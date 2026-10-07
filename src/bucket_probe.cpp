
#include "bucket_probe.h"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <iomanip>
#include <ostream>
#include <sstream>

bool                          BucketProbe::enabled = false;
thread_local BucketProbe::Key BucketProbe::tlKey;
thread_local std::vector<std::string>        BucketProbe::tlNames;
thread_local std::vector<BucketProbe::Role>  BucketProbe::tlRoles;
thread_local bool             BucketProbe::tlValid = false;

namespace {

// Write a feature vector as "(v0,v1,...)" or a name list as "(n0, n1, ...)".
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

// The reports show only what the recognizer emits, so an empty tally means it
// has no emit() call. That is normal when no mining is going on, since the emit
// calls are taken out before pushing. Say so instead of printing nothing, which
// would look like a broken tool.
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

// The sum search uses only TERM features, so having none isn't an error. It
// means the recognizer tagged no features that can be added up. Say how to tag
// them instead of printing an empty table.
void
reportNoTerms(std::ostream& out, const std::string& title)
{
  out << "--- " << title << " ---\n"
      << "  no TERM features: every emitted feature is a bare FLAG, so there is\n"
         "  nothing commensurate to add up. Tag the same-unit ones (ranks, files,\n"
         "  distances -- all measured in squares) at the emit site:\n"
         "      BucketProbe::emit({{\"pawnR\", pawnR, BucketProbe::TERM}, ...});\n\n";
}

// Write a signed sum as the rule it stands for: "+pawnR -kPromoD >= 4".
std::string
ruleString(const std::vector<std::string>& termNames, const std::vector<int>& sigma,
           bool upper, int t)
{
  std::ostringstream os;
  bool first = true;
  for (size_t i = 0; i < sigma.size(); ++i)
  {
    if (sigma[i] == 0)
      continue;
    if (!first) os << ' ';
    os << (sigma[i] > 0 ? '+' : '-') << termNames[i];
    first = false;
  }
  os << (upper ? " >= " : " <= ") << t;
  return os.str();
}

// One rule found by the sum search: a coefficient per TERM, a threshold, and
// which side of the threshold the rule claims as drawn.
struct SumCand
{
  std::vector<int> sigma;          // coefficient per TERM, in {-1,0,+1}
  int              t     = 0;
  bool             upper = true;   // claim `sum >= t` vs `sum <= t`
  uint64_t         score = 0;      // draws claimable on that side
};

// What the sum search finds in one cube: the TERM features it used, and every
// valid rule for each count of nonzero coefficients (L0), best first. Kept apart
// from the printing so the frozen search can use the same rules as features
// instead of printing them.
struct SumMining
{
  std::vector<size_t>               pool;        // cube indices of the TERMs
  std::vector<std::string>          termNames;   // labels, parallel to pool
  std::vector<std::vector<SumCand>> byL0;        // byL0[k-1]: candidates at L0 == k
};

// Does the rule hold for this full cube key? `sigma` is indexed by TERM, and
// `pool` maps each TERM back to its place in the key.
bool
holds(const SumCand& c, const std::vector<size_t>& pool, const BucketTally::Key& key)
{
  int s = 0;
  for (size_t j = 0; j < pool.size(); ++j)
    s += c.sigma[j] * key[pool[j]];
  return c.upper ? (s >= c.t) : (s <= c.t);
}

// The search itself: for every sign choice over the TERM features, re-key the
// cube by the sum and find the furthest threshold past which no bucket holds a
// decided position. reportSumSearch's comment in bucket_probe.h explains why
// the sum isn't clamped, why the first nonzero coefficient is always +1, and why
// both directions are scanned.
SumMining
mineSums(const BucketTally& cube, size_t maxL0)
{
  SumMining out;
  out.pool     = cube.termIndices();
  const size_t m = out.pool.size();
  if (m == 0)
    return out;

  out.termNames.reserve(m);
  for (size_t i : out.pool)
    out.termNames.push_back(cube.featureNames()[i]);

  maxL0 = std::min(maxL0, m);
  if (maxL0 == 0)
    return out;

  // Copy the cube's TERM values into one flat array once. Every sign choice
  // reads all of it, so it must be fast. remap() allocates a key per row, which
  // is fine for one re-key (like the sum table in reportSumSearch) but not for
  // thousands.
  const std::vector<BucketTally::Bucket> all = cube.buckets();
  const size_t                           R   = all.size();

  std::vector<int>      vals(R * m);
  std::vector<uint64_t> drawOf(R), decOf(R);
  int                   maxAbs = 0;
  for (size_t r = 0; r < R; ++r)
  {
    for (size_t j = 0; j < m; ++j)
    {
      const int v = all[r].key[out.pool[j]];
      vals[r * m + j] = v;
      maxAbs = std::max(maxAbs, std::abs(v));
    }
    drawOf[r] = all[r].draws;
    decOf[r]  = all[r].decided;
  }

  // A sum of maxL0 terms lies between -maxL0 * maxAbs and +maxL0 * maxAbs, so
  // the sums index one small array. No map is needed, and nothing is clamped or
  // lost at the ends.
  const int    off  = static_cast<int>(maxL0) * maxAbs;
  const size_t span = static_cast<size_t>(2 * off + 1);

  std::vector<uint64_t> dr(span), dc(span);
  out.byL0.resize(maxL0);

  for (size_t k = 1; k <= maxL0; ++k)
  {
    std::vector<SumCand>& cands = out.byL0[k - 1];

    for (uint64_t mask = 1; mask < (uint64_t{1} << m); ++mask)
    {
      if (static_cast<size_t>(std::popcount(mask)) != k)
        continue;

      std::vector<size_t> bits;
      for (size_t j = 0; j < m; ++j)
        if (mask & (uint64_t{1} << j))
          bits.push_back(j);

      // Signs for the chosen terms, with the first always +1. Its negation
      // splits the positions the same way, and both sides are scanned below.
      for (uint64_t sgn = 0; sgn < (uint64_t{1} << (k - 1)); ++sgn)
      {
        std::vector<int> sigma(m, 0);
        for (size_t b = 0; b < bits.size(); ++b)
          sigma[bits[b]] =
            (b == 0 || ((sgn >> (b - 1)) & 1) == 0) ? 1 : -1;

        std::fill(dr.begin(), dr.end(), 0);
        std::fill(dc.begin(), dc.end(), 0);
        size_t loIdx = span, hiIdx = 0;

        for (size_t r = 0; r < R; ++r)
        {
          int s = 0;
          for (size_t b : bits)
            s += sigma[b] * vals[r * m + b];

          const size_t idx = static_cast<size_t>(s + off);
          dr[idx] += drawOf[r];
          dc[idx] += decOf[r];
          loIdx = std::min(loIdx, idx);
          hiIdx = std::max(hiIdx, idx);
        }

        // A rule can only start where the decided positions stop. Claiming
        // `sum >= t` is valid exactly when t is above the highest decided sum.
        size_t hiDec = loIdx, loDec = hiIdx;
        bool   anyDec = false;
        for (size_t i = loIdx; i <= hiIdx; ++i)
          if (dc[i] != 0)
          {
            if (!anyDec) loDec = i;
            hiDec  = i;
            anyDec = true;
          }

        auto claim = [&](bool upper, size_t from, size_t to, int t) {
          uint64_t sc = 0;
          for (size_t i = from; i <= to; ++i)
            sc += dr[i];
          if (sc != 0)
            cands.push_back({sigma, t, upper, sc});
        };

        if (!anyDec)
        {
          // No decided position at all: every position in the call set is a
          // draw, so the sum tells nothing apart. Add one row that claims it
          // all, instead of two rules for the two directions.
          claim(true, loIdx, hiIdx, static_cast<int>(loIdx) - off);
          continue;
        }

        if (hiDec < hiIdx)
          claim(true, hiDec + 1, hiIdx, static_cast<int>(hiDec + 1) - off);
        if (loDec > loIdx)
          claim(false, loIdx, loDec - 1, static_cast<int>(loDec - 1) - off);
      }
    }

    // Highest score first, then a fixed order. The subset search breaks ties by
    // bucket count, but that means nothing here, since a rule always gives two.
    std::sort(cands.begin(), cands.end(), [](const SumCand& a, const SumCand& b) {
      if (a.score != b.score) return a.score > b.score;
      if (a.sigma != b.sigma) return a.sigma < b.sigma;
      return a.upper > b.upper;
    });
  }

  return out;
}

// The most any search over this cube could claim, printed at the top of both
// reports. No rule over these features can claim more than the cube's draws.
// Seeing it makes a score of 0 easier to judge.
std::string
cubeCeiling(const BucketTally& cube)
{
  uint64_t draws = 0, decided = 0;
  for (const BucketTally::Bucket& b : cube.buckets())
  {
    draws   += b.draws;
    decided += b.decided;
  }

  std::ostringstream os;
  os << "  ceiling: " << draws << " draws vs " << decided
     << " decided -- no rule over this pool can claim more than the draws\n";
  return os.str();
}

}   // namespace

std::vector<BucketTally::Bucket>
BucketTally::buckets() const
{
  std::vector<Bucket> out;
  out.reserve(rows.size());
  for (const auto& [key, r] : rows)
    out.push_back({key, r.n[1], r.n[0] + r.n[2]});
  return out;
}

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

// A draw example from a merged bucket is still a draw of the new bucket, and
// the same goes for decided ones, so remap() keeps the examples. Here there are
// none anyway: the searches are the only callers, and they turn examples off.
BucketTally
BucketTally::project(const std::vector<size_t>& featIdx) const
{
  std::vector<std::string> subNames;
  std::vector<Role>        subRoles;
  subNames.reserve(featIdx.size());
  subRoles.reserve(featIdx.size());
  for (size_t i : featIdx)
  {
    subNames.push_back(names[i]);
    if (i < roles.size()) subRoles.push_back(roles[i]);
  }

  return remap([&featIdx](const Key& key) {
    Key k;
    k.reserve(featIdx.size());
    for (size_t i : featIdx)
      k.push_back(key[i]);
    return k;
  }, subNames, subRoles);
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
BucketTally::writeTsv(std::ostream& out) const
{
  for (const std::string& n : names)
    out << n << '\t';
  out << "win\tdraw\tloss\n";

  for (const auto& [key, r] : rows)
  {
    for (int v : key)
      out << v << '\t';
    out << r.n[0] << '\t' << r.n[1] << '\t' << r.n[2] << '\n';
  }
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

    // Decided positions per draw: how many false draws a `return false` for
    // this whole bucket would remove for each draw it gives up. A bucket with no
    // draws prints "inf".
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

    // Examples of each kind under their bucket. On a mixed bucket, comparing
    // the two lists shows what feature is missing.
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
  out << cubeCeiling(cube);
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

    // Go through every subset of size k, as bitmasks with k bits set. The pool
    // has only a few features, so scanning all 2^n masks costs little next to
    // the projections.
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

    // Best score first. Ties go to the subset with fewer buckets (a simpler
    // rule), then to pool order, so the output is always the same.
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

void
reportSumSearch(std::ostream& out, const BucketTally& cube, size_t maxL0,
                size_t topN, const std::string& title)
{
  if (cube.empty())
  {
    reportNoFeatures(out, title);
    return;
  }

  // The sum search uses only TERM features. FLAGs are left out by their tag, so
  // they are never part of a candidate.
  const SumMining          mined     = mineSums(cube, maxL0);
  const std::vector<size_t>& pool     = mined.pool;
  const std::vector<std::string>& termNames = mined.termNames;
  const size_t             m         = pool.size();
  if (m == 0)
  {
    reportNoTerms(out, title);
    return;
  }

  if (mined.byL0.empty() || topN == 0)
    return;

  out << "--- " << title << " ---\n";
  out << "  terms = " << joinTuple(termNames, ", ") << "   (" << m << " TERM of "
      << cube.featureCount() << " emitted)\n";
  out << "  cube  : " << cube.bucketCount() << " distinct full-vector buckets over "
      << cube.positionCount() << " call-set positions\n";
  out << cubeCeiling(cube);
  out << "  rule  = a signed sum vs a threshold; score = draws on the claimed side,\n"
         "          which is admissible only if that side holds zero decided positions\n";
  out << "  L0 = nonzero coefficients, 1.." << mined.byL0.size() << ", top " << topN
      << " per L0.\n\n";

  uint64_t prevBest = 0;
  bool     havePrev = false;

  for (size_t k = 1; k <= mined.byL0.size(); ++k)
  {
    const std::vector<SumCand>& cands = mined.byL0[k - 1];

    // Unlike in the subset search, finding nothing at a size is common here. A
    // rule claims everything past its threshold, so decided positions at both
    // ends rule out every threshold. Say so, since printing nothing would look
    // like a broken search.
    if (cands.empty())
    {
      out << "  L0=" << k << "   no admissible rule: every halfspace at this size"
             " holds decided positions on both sides.\n\n";
      continue;
    }

    out << "  L0=" << k;
    if (havePrev && cands[0].score > prevBest)
      out << "   best gain over L0<" << k << ": +" << (cands[0].score - prevBest);
    out << '\n';

    out << "    " << std::left << std::setw(46) << "rule" << std::right
        << std::setw(12) << "score" << '\n';

    for (size_t i = 0; i < std::min(topN, cands.size()); ++i)
      out << "    " << std::left << std::setw(46)
          << ruleString(termNames, cands[i].sigma, cands[i].upper, cands[i].t)
          << std::right << std::setw(12) << cands[i].score << '\n';
    out << '\n';

    // Print the best rule's full table: the cube re-keyed by its sum with
    // remap(). Reading down it shows where the threshold comes from: it is the
    // row where the PURE-DRAW buckets stop.
    const SumCand& best = cands[0];
    const std::vector<int>& sigma = best.sigma;
    cube.remap(
          [&](const BucketTally::Key& key) {
            int s = 0;
            for (size_t j = 0; j < m; ++j)
              s += sigma[j] * key[pool[j]];
            return BucketTally::Key{s};
          },
          {ruleString(termNames, sigma, best.upper, best.t)})
      .report(out, "L0=" + std::to_string(k) + " best: sum sweep");

    prevBest = std::max(prevBest, cands[0].score);
    havePrev = true;
  }
}

void
reportFrozenSearch(std::ostream& out, const BucketTally& cube, size_t maxK,
                   size_t topN, size_t freezeN, const std::string& title)
{
  if (cube.empty())
  {
    reportNoFeatures(out, title);
    return;
  }

  const SumMining mined = mineSums(cube, maxK);
  if (mined.pool.empty())
  {
    reportNoTerms(out, title);
    return;
  }

  // Pick from every L0 at once. The per-L0 tables are there to show when one
  // more coefficient stops being worth it. A frozen rule just needs to be the
  // strongest one available.
  std::vector<SumCand> ranked;
  for (const std::vector<SumCand>& cs : mined.byL0)
    ranked.insert(ranked.end(), cs.begin(), cs.end());

  std::sort(ranked.begin(), ranked.end(), [](const SumCand& a, const SumCand& b) {
    if (a.score != b.score) return a.score > b.score;
    if (a.sigma != b.sigma) return a.sigma < b.sigma;
    return a.upper > b.upper;
  });

  // One rule per sign choice. Two thresholds on the same sum split the
  // positions in nearly the same way, and the copy would push out a different
  // rule.
  std::vector<SumCand> frozen;
  for (const SumCand& c : ranked)
  {
    if (frozen.size() >= freezeN)
      break;
    const bool dup = std::any_of(frozen.begin(), frozen.end(),
                                 [&c](const SumCand& f) { return f.sigma == c.sigma; });
    if (!dup)
      frozen.push_back(c);
  }

  out << "--- " << title << " ---\n";

  if (frozen.empty())
  {
    out << "  nothing frozen: the sum search found no admissible halfspace at L0 <= "
        << maxK << ",\n  so this is the plain subset search over the emitted pool.\n\n";
    reportSubsetSearch(out, cube, maxK, topN, title + ": subsets");
    return;
  }

  out << "  Frozen halfspaces -- each mined by the sum search, then handed to the\n"
         "  subset search below as one boolean coordinate. That is the point of the\n"
         "  pass: a halfspace collapses several TERMs into a single feature, so a\n"
         "  size-k subset holding one reaches rules no raw size-k subset can express.\n";
  for (const SumCand& f : frozen)
    out << "    " << std::left << std::setw(46)
        << ruleString(mined.termNames, f.sigma, f.upper, f.t) << std::right
        << "claims " << f.score << " draws\n";
  out << '\n';

  // Add the frozen rules to each key. The bucket count doesn't change: each rule
  // depends only on TERMs already in the key, so no two keys merge and none
  // splits. The subset search can now pick the rules as features.
  std::vector<std::string>       newNames = cube.featureNames();
  std::vector<BucketTally::Role> newRoles = cube.featureRoles();
  newRoles.resize(cube.featureCount(), BucketProbe::FLAG);
  for (const SumCand& f : frozen)
  {
    newNames.push_back(ruleString(mined.termNames, f.sigma, f.upper, f.t));
    // A frozen rule is 0 or 1, so it is a FLAG. As a TERM, a later sum could
    // add it to distances, which would have no unit.
    newRoles.push_back(BucketProbe::FLAG);
  }

  const std::vector<size_t>& pool = mined.pool;
  const BucketTally extended = cube.remap(
    [&frozen, &pool](const BucketTally::Key& key) {
      BucketTally::Key k = key;
      for (const SumCand& f : frozen)
        k.push_back(holds(f, pool, key) ? 1 : 0);
      return k;
    },
    newNames, newRoles);

  reportSubsetSearch(out, extended, maxK, topN, title + ": subsets over pool + frozen");
}
