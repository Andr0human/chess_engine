
#ifndef BUCKET_PROBE_H
#define BUCKET_PROBE_H

#include <array>
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

  // Fold one call-set position into its bucket.
  void
  add(const Key& key, Result result, bool heurDraw)
  {
    Row& r = rows[key];
    ++r[static_cast<int>(result)];
    if (heurDraw) ++r[3];
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
      for (int i = 0; i < 4; ++i) dst[i] += r[i];
    }
  }

  bool empty() const { return rows.empty(); }

  // Print the bucket table. Column labels come from the names captured via
  // setNames() (emitted inline by the recognizer).
  void
  report(std::ostream& out, const std::string& title) const;

  private:
  using Row = std::array<uint64_t, 4>;   // win, draw, loss, heurDraw
  std::map<Key, Row>       rows;
  std::vector<std::string> names;
};

#endif // BUCKET_PROBE_H
