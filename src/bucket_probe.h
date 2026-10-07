
#ifndef BUCKET_PROBE_H
#define BUCKET_PROBE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Feature buckets for the endgame recognizers. Used only by egvalidate.
//
// A tool for finding which feature buckets of a recognizer hold only draws and
// which are mixed, scored against the exact WDL oracle. It doesn't include the
// core engine headers (bitboard.h, types.h, ...), so it is easy to add or
// remove. Only two files use it:
//
//   * the recognizer (endgame.cpp), where it reaches its verdict:
//       if (BucketProbe::enabled)
//         BucketProbe::emit({{"feat0", v0}, {"feat1", v1}, ...});
//     Each feature is named where it is emitted, so the names sit next to the
//     values. When the probe is off this is one bool check and builds nothing,
//     so search never allocates.
//
//     A feature tagged TERM is in the same unit as the other TERMs (ranks,
//     files and distances, all counted in squares). TERMs are also used by the
//     signed-sum search, not only the subset search:
//         BucketProbe::emit({{"pawnR", pawnR, BucketProbe::TERM},
//                            {"kingInROS", ros}, ...});
//
//   * the harness (endgame_validation.cpp). It turns BucketProbe::enabled on
//     for the walk, calls BucketProbe::reset() before each recognizer call, and
//     adds BucketProbe::current() to a BucketTally when the probe fired.
// -----------------------------------------------------------------------------

// Passes features from the recognizer, deep in the call stack, to the harness.
// The state is thread_local, so each OpenMP thread in egvalidate has its own
// current bucket and there are no races.
class BucketProbe
{
  public:
  using Key = std::vector<int>;

  // What a feature can be used for. A FLAG is only bucketed on its value. A
  // TERM is also in the same unit as every other TERM, so the sum search can add
  // them into `+a -b -c >= t` rules. Only tag values that share a unit. A 0/1
  // flag among distances is fine (it acts as a one-square correction), but
  // adding a rank to a piece count means nothing.
  enum Role { FLAG, TERM };

  // One feature: its column name and this position's value. The recognizer
  // names each feature where it emits it, so there is no separate list of names
  // to keep in step. `role` defaults to FLAG, so `{"name", v}` also works.
  struct Feature { const char* name; int value; Role role = FLAG; };

  // Off by default, so search never reaches emit().
  static bool enabled;

  // Called by the recognizer: record this position's feature values, with the
  // names and roles (which are the same on every call).
  static void
  emit(std::initializer_list<Feature> feats)
  {
    tlKey.clear();
    tlNames.clear();
    tlRoles.clear();
    for (const Feature& f : feats)
    {
      tlKey.push_back(f.value);
      tlNames.emplace_back(f.name);
      tlRoles.push_back(f.role);
    }
    tlValid = true;
  }

  // Harness: clear the last result before calling the recognizer.
  static void reset() { tlValid = false; }

  // Harness: did the recognizer bucket the current position, and with what key?
  static bool       fired()   { return tlValid; }
  static const Key& current() { return tlKey; }

  // Harness: the names and roles of the current features.
  static const std::vector<std::string>& names() { return tlNames; }
  static const std::vector<Role>&        roles() { return tlRoles; }

  private:
  static thread_local Key                      tlKey;
  static thread_local std::vector<std::string> tlNames;
  static thread_local std::vector<Role>        tlRoles;
  static thread_local bool                     tlValid;
};

// For each feature vector, counts the oracle's wins, draws and losses over the
// call-set positions in it, and how many the recognizer already calls a draw.
// A bucket with no wins and no losses is PURE-DRAW: the recognizer could claim
// the whole bucket as a draw. A bucket with any win or loss is mixed, and can't
// be claimed as a whole.
class BucketTally
{
  public:
  using Key  = std::vector<int>;
  using Role = BucketProbe::Role;

  enum Result { WIN = 0, DRAW = 1, LOSS = 2 };

  // Example FENs kept per bucket, for draws and for decided positions. Enough to
  // look at a mixed bucket and see what feature is missing, and few enough to
  // cost nothing.
  static constexpr size_t MAX_SAMPLE = 3;

  // Totals for a whole tally, used to rank feature subsets. pureDrawDraws is the
  // score: the draws that could be claimed if every PURE-DRAW bucket of this
  // feature set became a `return true`.
  struct Summary
  {
    uint64_t pureDrawDraws   = 0;
    uint64_t pureDrawBuckets = 0;
    uint64_t totalBuckets    = 0;
  };

  // Add one call-set position to its bucket, counts only. See wantSamples in
  // the harness.
  void
  add(const Key& key, Result result, bool heurDraw)
  { count(key, result, heurDraw); }

  // The same, but also keep the position as an example (draw or decided) if the
  // bucket has room. `makeFen` returns its FEN and is called only then, so most
  // positions never build one.
  template <typename MakeFen>
  void
  add(const Key& key, Result result, bool heurDraw, const MakeFen& makeFen)
  {
    Row& r = count(key, result, heurDraw);
    std::vector<std::string>& s = (result == DRAW) ? r.drawFens : r.decFens;
    if (s.size() < MAX_SAMPLE)
      s.push_back(makeFen());
  }

  // Record the feature names and roles, which are the same for every bucket.
  // Only the first call sets them; later calls do nothing.
  void
  setNames(const std::vector<std::string>& n) { if (names.empty()) names = n; }

  void
  setRoles(const std::vector<Role>& r) { if (roles.empty()) roles = r; }

  // Merge in another tally (a worker's into the generator's). The counts don't
  // depend on the order, so the parallel total equals the serial one.
  void
  merge(const BucketTally& other)
  {
    if (names.empty()) names = other.names;
    if (roles.empty()) roles = other.roles;
    for (const auto& [k, r] : other.rows)
    {
      Row& dst = rows[k];
      for (int i = 0; i < 4; ++i) dst.n[i] += r.n[i];
      takeSamples(dst.drawFens, r.drawFens);
      takeSamples(dst.decFens,  r.decFens);
    }
  }

  bool empty() const { return rows.empty(); }

  size_t featureCount() const { return names.size(); }
  size_t bucketCount()  const { return rows.size(); }

  const std::vector<std::string>& featureNames() const { return names; }
  const std::vector<Role>&        featureRoles() const { return roles; }

  // Indices of the features tagged TERM, which the signed-sum search can use.
  // Empty when nothing is tagged, meaning this endgame has no features for the
  // sum search yet.
  std::vector<size_t>
  termIndices() const
  {
    std::vector<size_t> idx;
    for (size_t i = 0; i < roles.size(); ++i)
      if (roles[i] == BucketProbe::TERM)
        idx.push_back(i);
    return idx;
  }

  // Total call-set positions added (win + draw + loss over every bucket).
  uint64_t positionCount() const;

  // One bucket cut down to what every verdict needs: its draws and its decided
  // positions. A bucket can be claimed exactly when decided == 0.
  struct Bucket { Key key; uint64_t draws = 0; uint64_t decided = 0; };

  // Every bucket, in key order. The sum search reads the TERM values thousands
  // of times, once per sign choice. remap() is too slow for that, since it
  // allocates a key per row.
  std::vector<Bucket> buckets() const;

  // Re-key the tally: `keyFn` maps each old key to a new one, and rows that land
  // on the same new key are added together. Rows are only counts, so this gives
  // exactly what a walk emitting the new key would have counted. So one cube is
  // enough for any feature computed from its features: project() keeps some of
  // them, and a signed sum adds them up.
  //
  // New features are FLAG unless `newRoles` says otherwise. A computed value
  // isn't in the same unit as anything by default, and tagging it TERM would let
  // the sum search add up sums.
  template <typename Fn>
  BucketTally
  remap(Fn keyFn, const std::vector<std::string>& newNames,
        const std::vector<Role>& newRoles = {}) const
  {
    BucketTally out;
    out.names = newNames;
    out.roles = newRoles.empty()
                  ? std::vector<Role>(newNames.size(), BucketProbe::FLAG)
                  : newRoles;

    for (const auto& [key, r] : rows)
    {
      Row& dst = out.rows[keyFn(key)];
      for (int i = 0; i < 4; ++i) dst.n[i] += r.n[i];
      takeSamples(dst.drawFens, r.drawFens);
      takeSamples(dst.decFens,  r.decFens);
    }
    return out;
  }

  // Cut down to a feature subset: add up every bucket whose key matches on the
  // features in `featIdx` (indices into the emitted vector). The kept features
  // keep their roles, because unlike a computed feature, each is the original.
  BucketTally project(const std::vector<size_t>& featIdx) const;

  // Verdict counts over all buckets (see Summary).
  Summary summarize() const;

  // Print the bucket table. The column names are the ones the recognizer
  // emitted (see setNames()).
  void
  report(std::ostream& out, const std::string& title) const;

  // Write every bucket as a tab-separated row: the feature values, then win,
  // draw and loss. The first line names the columns.
  void
  writeTsv(std::ostream& out) const;

  private:
  struct Row
  {
    std::array<uint64_t, 4>  n{};        // win, draw, loss, heurDraw
    std::vector<std::string> drawFens;   // <= MAX_SAMPLE draw examples
    std::vector<std::string> decFens;    // <= MAX_SAMPLE win/loss examples
  };

  Row&
  count(const Key& key, Result result, bool heurDraw)
  {
    Row& r = rows[key];
    ++r.n[static_cast<int>(result)];
    if (heurDraw) ++r.n[3];
    return r;
  }

  // Copy examples from `src` into `dst` until it holds MAX_SAMPLE. Callers
  // merge in task order, so the examples kept are the ones a serial run keeps.
  static void
  takeSamples(std::vector<std::string>& dst, const std::vector<std::string>& src)
  {
    for (const std::string& f : src)
    {
      if (dst.size() >= MAX_SAMPLE) break;
      dst.push_back(f);
    }
  }

  std::map<Key, Row>       rows;
  std::vector<std::string> names;
  std::vector<Role>        roles;
};

// Feature subset search. Given a `cube` counted over the recognizer's whole
// feature pool, cut it down to every subset of 1 to maxK features. For each
// size k, print the `topN` subsets by draws in PURE-DRAW buckets, and how much
// the best score gained over size k-1.
//
// Subsets are ranked within each size, not all together, because adding a
// feature never lowers the score. It only splits buckets further, and a split
// can't make a PURE-DRAW bucket mixed, only pull pure parts out of mixed ones.
// So the full pool would always come first. The real question is when one more
// feature stops being worth it.
void
reportSubsetSearch(std::ostream& out, const BucketTally& cube, size_t maxK,
                   size_t topN, const std::string& title);

// Signed-sum search: find rules of the form `+a -b -c >= t` (a halfspace) over
// the features tagged TERM. For each choice of a sign in {-1, 0, +1} per
// feature, the cube is re-keyed by the sum, and the scan finds the furthest
// threshold past which no bucket holds a decided position. That bucket gives the
// constant t, so one pass finds it instead of trying values one by one. The sum
// is never clamped. Clamping would merge the end buckets, which is where a
// threshold outside the expected range would show up.
//
// `maxL0` caps how many coefficients are nonzero. A 0 leaves a feature out, so
// the signs also pick the subset. The number of nonzero coefficients measures
// how simple a rule is (the bucket count can't, since a rule always gives two).
// Candidates are built as a subset and then signs for it, so the cap skips work
// instead of filtering afterwards. The first nonzero coefficient is always +1,
// because a sum and its negation split the positions the same way. That loses
// nothing only because each candidate is scanned in both directions (claim
// `>= t` and claim `<= t`). Without both, the search would miss every rule of
// that shape.
void
reportSumSearch(std::ostream& out, const BucketTally& cube, size_t maxL0,
                size_t topN, const std::string& title);

// The two searches together: find the best `freezeN` rules with the sum
// search, add each to the cube as a yes/no feature with remap(), and run the
// subset search over the larger pool. A rule packs several TERMs into one
// feature, so a subset of k features that includes one can express rules that
// no k plain features can. The bucket counts don't change: a rule depends only
// on features already in the key, so it can't merge two keys or split one.
//
// `maxK` caps both searches: the nonzero coefficients when finding rules, and
// the subset size afterwards. The rules kept are the strongest over all sizes,
// not the best of each size, since grouping by size is only there to judge how
// simple a rule is. Only one rule is kept per sign choice, because two
// thresholds on the same sum split the positions in nearly the same way.
void
reportFrozenSearch(std::ostream& out, const BucketTally& cube, size_t maxK,
                   size_t topN, size_t freezeN, const std::string& title);

#endif // BUCKET_PROBE_H
