
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
// Endgame-recognizer bucket instrumentation (DEBUG / egvalidate only).
//
// Throwaway scaffolding for mining which feature buckets of an endgame
// recognizer are pure-draw vs mixed against the perfect-WDL oracle. Kept fully
// self-contained so it can be plugged in / pulled out without touching core
// engine headers (bitboard.h, types.h, ...). Only two translation units include
// it:
//
//   * the recognizer (endgame.cpp): at its verdict point,
//       if (BucketProbe::enabled)
//         BucketProbe::emit({{"feat0", v0}, {"feat1", v1}, ...});
//     Each feature is named inline, so the column labels live next to the
//     values. When disabled this is a single static-bool load -- nothing is
//     built, so real search stays allocation-free.
//
//     A feature may be tagged TERM to declare it commensurate with the other
//     TERMs (ranks, files, distances -- all measured in squares), which opts it
//     into the signed-sum search on top of the plain subset search:
//         BucketProbe::emit({{"pawnR", pawnR, BucketProbe::TERM},
//                            {"kingInROS", ros}, ...});
//
//   * the harness (endgame_validation.cpp): flips BucketProbe::enabled on around
//     the sweep, calls BucketProbe::reset() before each recognizer call, and
//     folds BucketProbe::current() into a BucketTally when the probe fired.
// -----------------------------------------------------------------------------

// Emit channel between the recognizer (deep in the call stack) and the harness.
// The scratch state is thread_local, so it is race-free when egvalidate drives
// the sweep across an OpenMP team: each thread owns its own current bucket.
class BucketProbe
{
  public:
  using Key = std::vector<int>;

  // What a feature may be used for. FLAG is a bare coordinate: bucket on its
  // value, nothing else. TERM additionally declares it commensurate with every
  // other TERM -- same unit, so a signed sum of them is meaningful -- which is
  // what lets the sum search combine them into `+a -b -c >= t` rules. Tag only
  // same-unit quantities; a 0/1 flag among distances is fine (it reads as a
  // one-square correction) but a rank added to a piece count is nonsense.
  enum Role { FLAG, TERM };

  // One feature of the vector: its column label plus this position's value.
  // The recognizer names each feature inline at the emit site, so the labels
  // travel with the data instead of a separate list the harness must keep in sync.
  // `role` defaults to FLAG, so a two-field `{"name", v}` emit stays valid.
  struct Feature { const char* name; int value; Role role = FLAG; };

  // Master switch. Off by default => emit() is never reached in real search.
  static bool enabled;

  // Recognizer -> harness: record this position's feature values, and (since
  // they are identical on every emit) the column labels and roles alongside them.
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

  // Harness: clear stale state before invoking the recognizer.
  static void reset() { tlValid = false; }

  // Harness: did the recognizer bucket the current position, and with what key?
  static bool       fired()   { return tlValid; }
  static const Key& current() { return tlKey; }

  // Harness: the column labels / roles matching the current feature vector.
  static const std::vector<std::string>& names() { return tlNames; }
  static const std::vector<Role>&        roles() { return tlRoles; }

  private:
  static thread_local Key                      tlKey;
  static thread_local std::vector<std::string> tlNames;
  static thread_local std::vector<Role>        tlRoles;
  static thread_local bool                     tlValid;
};

// Accumulates, per feature vector, the oracle WDL split of the call-set positions
// landing in it, plus how many the recognizer already labels draw. A bucket with
// zero wins and zero losses is a pure-draw class -- a candidate to recognize
// wholesale; any win/loss makes it MIXED (cannot blanket-claim draw).
class BucketTally
{
  public:
  using Key  = std::vector<int>;
  using Role = BucketProbe::Role;

  enum Result { WIN = 0, DRAW = 1, LOSS = 2 };

  // Example FENs kept per bucket, per class (draw / decided). Enough to eyeball a
  // mixed bucket and spot the missing discriminator; small enough to be free.
  static constexpr size_t MAX_SAMPLE = 3;

  // Scoring summary of a whole tally, used to rank one feature subset against
  // another. pureDrawDraws is the recall-mining objective: the draws that become
  // claimable if every PURE-DRAW bucket of this feature set is turned into a
  // `return true`.
  struct Summary
  {
    uint64_t pureDrawDraws   = 0;
    uint64_t pureDrawBuckets = 0;
    uint64_t totalBuckets    = 0;
  };

  // Fold one call-set position into its bucket. Pass `fen` to keep it as an
  // example of its class (draw vs decided) if the bucket still has room; pass
  // nullptr to tally counts only -- see wantSamples in the harness.
  void
  add(const Key& key, Result result, bool heurDraw, const std::string* fen = nullptr)
  {
    Row& r = rows[key];
    ++r.n[static_cast<int>(result)];
    if (heurDraw) ++r.n[3];

    if (fen == nullptr)
      return;

    std::vector<std::string>& s = (result == DRAW) ? r.drawFens : r.decFens;
    if (s.size() < MAX_SAMPLE)
      s.push_back(*fen);
  }

  // Record the feature column labels / roles (identical for every bucket). Set
  // once from the probe; later identical calls are no-ops.
  void
  setNames(const std::vector<std::string>& n) { if (names.empty()) names = n; }

  void
  setRoles(const std::vector<Role>& r) { if (roles.empty()) roles = r; }

  // Merge another tally (worker -> generator reduction). Order-independent, so
  // the parallel total equals the serial total exactly.
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

  // Indices of the features the recognizer tagged TERM -- the pool the signed-sum
  // search may draw on. Empty when nothing is tagged, which is the signal that
  // this endgame has no sum-eligible vocabulary declared yet.
  std::vector<size_t>
  termIndices() const
  {
    std::vector<size_t> idx;
    for (size_t i = 0; i < roles.size(); ++i)
      if (roles[i] == BucketProbe::TERM)
        idx.push_back(i);
    return idx;
  }

  // Total call-set positions folded in (win + draw + loss over every bucket).
  uint64_t positionCount() const;

  // One bucket flattened down to what every verdict actually rests on: draws vs
  // decided. A bucket is claimable exactly when decided == 0.
  struct Bucket { Key key; uint64_t draws = 0; uint64_t decided = 0; };

  // Every bucket, in key order. The sum search needs to rescan the cube's TERM
  // coordinates thousands of times (once per sign vector), which remap() cannot
  // serve -- it allocates a key per row, right for a single re-key but ruinous
  // across a search. Key order also means a 1-D re-key comes out already sorted,
  // so the threshold sweep is a straight walk.
  std::vector<Bucket> buckets() const;

  // Re-key the tally: rebuild it with `keyFn` mapping each existing key to a new
  // one, summing the rows that collide. Sound for the same reason project() is --
  // rows are pure counts, so any many-to-one re-key yields *exactly* what a
  // dedicated sweep emitting that key would have tallied. That is what makes one
  // cube a sufficient statistic for any derived coordinate: project() is the case
  // where keyFn selects coordinates, a signed sum is the case where it adds them.
  //
  // Derived coordinates are FLAG unless `newRoles` says otherwise: a computed
  // value is not commensurate with anything by default, and silently re-tagging
  // one TERM would admit sums of sums into the sum search.
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

  // Marginalize onto a feature subset: sum every bucket whose key agrees on the
  // features in `featIdx` (indices into the emitted vector). The subset's features
  // keep the roles they were emitted with -- unlike a derived coordinate, a
  // projected one *is* the original.
  BucketTally project(const std::vector<size_t>& featIdx) const;

  // Verdict counts over all buckets (see Summary).
  Summary summarize() const;

  // Print the bucket table. Column labels come from the names captured via
  // setNames() (emitted inline by the recognizer).
  void
  report(std::ostream& out, const std::string& title) const;

  private:
  struct Row
  {
    std::array<uint64_t, 4>  n{};        // win, draw, loss, heurDraw
    std::vector<std::string> drawFens;   // <= MAX_SAMPLE draw examples
    std::vector<std::string> decFens;    // <= MAX_SAMPLE win/loss examples
  };

  // Drain examples from `src` into `dst` until the cap. Callers merge in task
  // order, so the samples that survive are the ones a serial run would have kept.
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

// Automated feature-set search. Given a `cube` tallied over the recognizer's full
// candidate feature pool, marginalize onto every feature subset of size 1..maxK
// and print, per size k, the `topN` subsets by claimable PURE-DRAW draws together
// with the best score's gain over size k-1.
//
// Ranking is grouped by k rather than flat because the score is monotone --
// adding a feature only splits buckets finer, and a split can never make a
// PURE-DRAW bucket impure, only rescue pure fragments out of mixed ones. So the
// full pool always wins a flat leaderboard, and the real question is where the
// marginal gain stops paying for the extra feature.
void
reportSubsetSearch(std::ostream& out, const BucketTally& cube, size_t maxK,
                   size_t topN, const std::string& title);

// Automated signed-sum search: mine rules of the shape `+a -b -c >= t` over the
// features the recognizer tagged TERM. For each sign vector in {-1,0,+1}^m the
// cube is re-keyed by the raw sum, and the sweep scanned for the extreme
// threshold past which no bucket holds a decided position -- that row *is* the
// constant, so one pass yields the whole threshold range instead of one guess.
// The sum is kept raw, never clamped: clamping merges the end buckets, which is
// precisely where a threshold outside the assumed window would show itself.
//
// `maxL0` caps the nonzero coefficients: a coefficient of 0 excludes a term, so
// signs subsume subset selection, and L0 -- not the bucket count, which saturates
// at 2 for a halfspace -- is what parsimony means here. Candidates are enumerated
// as subset-mask x signs so the cap prunes rather than filters, and the first
// nonzero coefficient is pinned to +1: sigma and -sigma cut the same partition.
// That halving is lossless only because each candidate is scanned in *both*
// directions (claim `>= t` and claim `<= t`) -- the two opposite sides of the one
// cut. Drop either half and the search silently loses every rule of that shape.
void
reportSumSearch(std::ostream& out, const BucketTally& cube, size_t maxL0,
                size_t topN, const std::string& title);

// The two searches composed: mine the best `freezeN` halfspaces with the sum
// search, append each to the cube as a boolean coordinate via remap(), and run the
// subset search over the widened pool. No third search -- the point is that a
// halfspace collapses several TERMs into one feature, so a size-k subset holding
// one reaches rules no raw size-k subset can express. Bucket counts are untouched:
// a halfspace is a function of coordinates already in the key, so it can neither
// collide two keys nor split one.
//
// `maxK` caps both searches (L0 for the mining, subset size for the search over the
// result). Slots go to the strongest halfspaces across every L0 rather than the
// best per L0 -- a frozen slot wants the best discriminator going, and the per-L0
// grouping exists to price parsimony, which a frozen rule has already paid for --
// but only one per sign vector: two thresholds on one cut are the same
// discriminator twice.
void
reportFrozenSearch(std::ostream& out, const BucketTally& cube, size_t maxK,
                   size_t topN, size_t freezeN, const std::string& title);

#endif // BUCKET_PROBE_H
