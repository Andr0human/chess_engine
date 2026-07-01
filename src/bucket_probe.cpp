
#include "bucket_probe.h"

#include <iomanip>
#include <ostream>
#include <sstream>

bool                          BucketProbe::enabled = false;
thread_local BucketProbe::Key BucketProbe::tlKey;
thread_local std::vector<std::string> BucketProbe::tlNames;
thread_local bool             BucketProbe::tlValid = false;

void
BucketTally::report(std::ostream& out, const std::string& title) const
{
  if (rows.empty())
    return;

  out << "--- " << title << " ---\n";

  out << "  key = (";
  for (size_t i = 0; i < names.size(); ++i)
    out << names[i] << (i + 1 < names.size() ? ", " : "");
  out << ")\n";

  out << "  bucket                 win      draw      loss"
         "   decided  dec:draw   verdict\n";

  uint64_t pureDrawTotal = 0;   // sum of draw column over PURE-DRAW buckets

  for (const auto& [key, arr] : rows)
  {
    const uint64_t win = arr[0], draw = arr[1], loss = arr[2];
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
  }

  out << "  PURE-DRAW total draws  : " << pureDrawTotal << '\n';
  out << '\n';
}
