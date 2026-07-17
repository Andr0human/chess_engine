
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

  // One feature of the vector: its column label plus this position's value.
  // The recognizer names each feature inline at the emit site, so the labels
  // travel with the data instead of a separate list the harness must keep in sync.
  struct Feature { const char* name; int value; };

  // Master switch. Off by default => emit() is never reached in real search.
  static bool enabled;

  // Recognizer -> harness: record this position's feature values, and (since
  // they are identical on every emit) the column labels alongside them.
  static void
  emit(std::initializer_list<Feature> feats)
  {
    tlKey.clear();
    tlNames.clear();
    for (const Feature& f : feats)
    {
      tlKey.push_back(f.value);
      tlNames.emplace_back(f.name);
    }
    tlValid = true;
  }

  // Harness: clear stale state before invoking the recognizer.
  static void reset() { tlValid = false; }

  // Harness: did the recognizer bucket the current position, and with what key?
  static bool       fired()   { return tlValid; }
  static const Key& current() { return tlKey; }

  // Harness: the column labels matching the current feature vector.
  static const std::vector<std::string>& names() { return tlNames; }

  private:
  static thread_local Key                      tlKey;
  static thread_local std::vector<std::string> tlNames;
  static thread_local bool                     tlValid;
};

// Accumulates, per feature vector, the oracle WDL split of the call-set positions
// landing in it, plus how many the recognizer already labels draw. A bucket with
// zero wins and zero losses is a pure-draw class -- a candidate to recognize
// wholesale; any win/loss makes it MIXED (cannot blanket-claim draw).
class BucketTally
{
  public:
  using Key = std::vector<int>;

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

  // Record the feature column labels (identical for every bucket). Set once from
  // the probe; later identical calls are no-ops.
  void
  setNames(const std::vector<std::string>& n) { if (names.empty()) names = n; }

  // Merge another tally (worker -> generator reduction). Order-independent, so
  // the parallel total equals the serial total exactly.
  void
  merge(const BucketTally& other)
  {
    if (names.empty()) names = other.names;
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

  // Total call-set positions folded in (win + draw + loss over every bucket).
  uint64_t positionCount() const;

  // Marginalize onto a feature subset: sum every bucket whose key agrees on the
  // features in `featIdx` (indices into the emitted vector). Because bucket rows
  // are pure counts, the result is *exactly* what a dedicated sweep emitting only
  // those features would have tallied -- which is what lets one sweep serve every
  // subset instead of one sweep per subset.
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

#endif // BUCKET_PROBE_H
