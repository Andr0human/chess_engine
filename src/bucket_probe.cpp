
#include "bucket_probe.h"

#include <algorithm>
#include <bit>
#include <iomanip>
#include <ostream>
#include <sstream>

bool                          BucketProbe::enabled = false;
thread_local BucketProbe::Key BucketProbe::tlKey;
thread_local std::vector<std::string> BucketProbe::tlNames;
thread_local bool             BucketProbe::tlValid = false;

namespace {

// Render a feature vector as "(v0,v1,...)" / a name list as "(n0, n1, ...)".
template <typename T>
std::string
joinTuple(const std::vector<T>& v, const char* sep)
{
  std::ostringstream os;
  os << '(';
  for (size_t i = 0; i < v.size(); ++i)
  {
    os << v[i];
    if (i + 1 < v.size()) os << sep;
  }
  os << ')';
  return os.str();
}

// Both reports are driven entirely by what the recognizer emits, so an empty
// tally means no emit site is planted -- the normal state between mining
// sessions, since the scaffolding is stripped back out before each push. Say so
// instead of printing nothing, which reads as a broken tool.
void
reportNoFeatures(std::ostream& out, const std::string& title)
{
  out << "--- " << title << " ---\n"
      << "  no features emitted: the recognizer never called BucketProbe::emit(),\n"
         "  so there is nothing to bucket. Plant one at its verdict point to mine\n"
         "  this endgame, naming each candidate feature inline:\n"
         "      if (BucketProbe::enabled)\n"
         "        BucketProbe::emit({{\"feat0\", v0}, {\"feat1\", v1}, ...});\n\n";
}

}   // namespace

uint64_t
BucketTally::positionCount() const
{
  uint64_t n = 0;
  for (const auto& [key, r] : rows)
  {
    (void)key;
    n += r.n[0] + r.n[1] + r.n[2];
  }
  return n;
}

BucketTally
BucketTally::project(const std::vector<size_t>& featIdx) const
{
  BucketTally sub;
  sub.names.reserve(featIdx.size());
  for (size_t i : featIdx)
    sub.names.push_back(names[i]);

  Key k;
  k.reserve(featIdx.size());
  for (const auto& [key, r] : rows)
  {
    k.clear();
    for (size_t i : featIdx)
      k.push_back(key[i]);

    // A part's draw example is still a draw of the union, and likewise for the
    // decided class, so samples carry across the marginalization unchanged. In
    // practice they are empty here: `combos` (the only caller) turns sampling off.
    Row& dst = sub.rows[k];
    for (int j = 0; j < 4; ++j)
      dst.n[j] += r.n[j];
    takeSamples(dst.drawFens, r.drawFens);
    takeSamples(dst.decFens,  r.decFens);
  }
  return sub;
}

BucketTally::Summary
BucketTally::summarize() const
{
  Summary s;
  for (const auto& [key, r] : rows)
  {
    (void)key;
    const uint64_t draw = r.n[1];
    const uint64_t decided = r.n[0] + r.n[2];
    ++s.totalBuckets;
    if (decided == 0 && draw != 0)
    {
      ++s.pureDrawBuckets;
      s.pureDrawDraws += draw;
    }
  }
  return s;
}

void
BucketTally::report(std::ostream& out, const std::string& title) const
{
  if (rows.empty())
  {
    reportNoFeatures(out, title);
    return;
  }

  out << "--- " << title << " ---\n";

  out << "  key = (";
  for (size_t i = 0; i < names.size(); ++i)
    out << names[i] << (i + 1 < names.size() ? ", " : "");
  out << ")\n";

  out << "  bucket                 win      draw      loss"
         "   decided  dec:draw   verdict\n";

  uint64_t pureDrawTotal = 0;   // sum of draw column over PURE-DRAW buckets

  for (const auto& [key, r] : rows)
  {
    const uint64_t win = r.n[0], draw = r.n[1], loss = r.n[2];
    const uint64_t decided = win + loss;   // false-draws if this bucket is claimed
    const bool pureDraw    = (decided == 0 && draw != 0);
    const bool pureDecided = (draw == 0 && decided != 0);

    if (pureDraw)
      pureDrawTotal += draw;

    // Decided-to-draw ratio: carve-out efficiency (false-draws killed per draw
    // sacrificed if this whole bucket is turned into a return-false). A pure-
    // decided bucket (no draws) is infinitely favourable -> "inf".
    std::ostringstream ratioStr;
    if (draw == 0)
      ratioStr << (decided == 0 ? "0.00" : "inf");
    else
      ratioStr << std::fixed << std::setprecision(2)
               << (static_cast<double>(decided) / static_cast<double>(draw));

    std::string keyStr = "(";
    for (size_t i = 0; i < key.size(); ++i)
      keyStr += std::to_string(key[i]) + (i + 1 < key.size() ? "," : "");
    keyStr += ")";

    out << "  " << std::left << std::setw(18) << keyStr << std::right
        << std::setw(10) << win
        << std::setw(10) << draw
        << std::setw(10) << loss
        << std::setw(10) << decided
        << std::setw(10) << ratioStr.str()
        << "   " << (pureDraw ? "PURE-DRAW"
                    : pureDecided ? "PURE-DECIDED" : "mixed") << '\n';

    // Examples of each class beneath their bucket. On a `mixed` bucket, reading
    // the two lists against each other is what reveals the missing feature.
    for (const std::string& f : r.drawFens)
      out << "      draw> " << f << '\n';
    for (const std::string& f : r.decFens)
      out << "      dec > " << f << '\n';
  }

  out << "  PURE-DRAW total draws  : " << pureDrawTotal << '\n';
  out << '\n';
}

void
reportSubsetSearch(std::ostream& out, const BucketTally& cube, size_t maxK,
                   size_t topN, const std::string& title)
{
  const size_t n = cube.featureCount();
  if (n == 0 || cube.empty())
  {
    reportNoFeatures(out, title);
    return;
  }

  maxK = std::min(maxK, n);
  if (maxK == 0 || topN == 0)
    return;

  out << "--- " << title << " ---\n";
  out << "  pool  = " << joinTuple(cube.featureNames(), ", ")
      << "   (" << n << " features)\n";
  out << "  cube  : " << cube.bucketCount() << " distinct full-vector buckets over "
      << cube.positionCount() << " call-set positions\n";
  out << "  score = draws claimable from that subset's PURE-DRAW buckets"
         " (higher = better recall)\n";
  out << "  subsets of size 1.." << maxK << ", top " << topN << " per size.\n\n";

  // One candidate subset's score. `idx` are indices into the emitted pool.
  struct Cand
  {
    uint64_t            score = 0;
    uint64_t            pureBuckets = 0;
    uint64_t            totalBuckets = 0;
    std::vector<size_t> idx;
  };

  uint64_t prevBest = 0;
  bool     havePrev = false;

  for (size_t k = 1; k <= maxK; ++k)
  {
    std::vector<Cand> cands;

    // Enumerate every size-k subset of the pool via popcount over bitmasks. The
    // pool is small (a handful of features), so the 2^n scan is negligible next
    // to the projection work it drives.
    for (uint64_t mask = 1; mask < (uint64_t{1} << n); ++mask)
    {
      if (static_cast<size_t>(std::popcount(mask)) != k)
        continue;

      Cand c;
      for (size_t i = 0; i < n; ++i)
        if (mask & (uint64_t{1} << i))
          c.idx.push_back(i);

      const BucketTally::Summary s = cube.project(c.idx).summarize();
      c.score        = s.pureDrawDraws;
      c.pureBuckets  = s.pureDrawBuckets;
      c.totalBuckets = s.totalBuckets;
      cands.push_back(std::move(c));
    }

    if (cands.empty())
      continue;

    // Best score first; ties broken toward the coarser subset (fewer buckets =
    // simpler rule), then by pool order so the output is deterministic.
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
      if (a.score != b.score)               return a.score > b.score;
      if (a.totalBuckets != b.totalBuckets) return a.totalBuckets < b.totalBuckets;
      return a.idx < b.idx;
    });

    out << "  k=" << k;
    if (havePrev)
      out << "   best gain over k=" << (k - 1) << ": +"
          << (cands[0].score - prevBest);
    out << '\n';

    out << "    " << std::left << std::setw(46) << "subset" << std::right
        << std::setw(12) << "score"
        << std::setw(10) << "pureBkts"
        << std::setw(10) << "buckets" << '\n';

    for (size_t i = 0; i < std::min(topN, cands.size()); ++i)
    {
      const Cand& c = cands[i];
      std::vector<std::string> nm;
      for (size_t j : c.idx)
        nm.push_back(cube.featureNames()[j]);

      out << "    " << std::left << std::setw(46) << joinTuple(nm, ",") << std::right
          << std::setw(12) << c.score
          << std::setw(10) << c.pureBuckets
          << std::setw(10) << c.totalBuckets << '\n';
    }
    out << '\n';

    prevBest = cands[0].score;
    havePrev = true;
  }
}
