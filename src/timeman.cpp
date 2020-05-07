/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2020 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "search.h"
#include "timeman.h"
#include "uci.h"

TimeManagement Time; // Our global time management object

int t[9] = {5, 122, 244, 92, 20, 10, 55, 26, 8};
TUNE(t);

/// init() is called at the beginning of the search and calculates the allowed
/// thinking time out of the time control and current game ply. We support four
/// different kinds of time controls, passed in 'limits':

void TimeManagement::init(Search::LimitsType& limits, Color us, int ply) {

  TimePoint minThinkingTime = Options["Minimum Thinking Time"];
  TimePoint moveOverhead    = Options["Move Overhead"];
  TimePoint slowMover       = Options["Slow Mover"];
  TimePoint npmsec          = Options["nodestime"];
  double scale;

  // If we have to play in 'nodes as time' mode, then convert from time
  // to nodes, and use resulting values in time management formulas.
  // WARNING: to avoid time losses, the given npmsec (nodes per millisecond)
  // must be much lower than the real engine speed.
  if (npmsec)
  {
      if (!availableNodes) // Only once at game start
          availableNodes = npmsec * limits.time[us]; // Time is in msec

      // Convert from milliseconds to nodes
      limits.time[us] = TimePoint(availableNodes);
      limits.inc[us] *= npmsec;
      limits.npmsec = npmsec;
  }

  startTime = limits.startTime;

  int mtg = limits.movestogo ? std::min(limits.movestogo, 50) : 50;

  // Adjust moveOverhead to help with tiny increments (if needed)
  moveOverhead = std::max(10, std::min<int>(limits.inc[us] / 2, moveOverhead));

  TimePoint timeLeft =  std::max(TimePoint(0),
      limits.time[us] + limits.inc[us] * (mtg - 1) - moveOverhead * (2 + mtg));

  timeLeft = slowMover * timeLeft / 100;

  /// movestogo == 0 means: x basetime (+ z increment)
  if (limits.movestogo == 0)
  {
      scale = std::min(t[0]/10.0, (t[1]/1000.0) / std::max(t[2]/1000.0, t[3]/10.0 - std::log2(ply + 1)));
      optimumTime = std::min((t[4]/100.0) * limits.time[us], scale * timeLeft);
      optimumTime = std::max(minThinkingTime, optimumTime);

      scale = std::min(double(t[5]), t[6]/10.0 + ply / (double(t[7])));
      maximumTime = std::min((t[8]/10.0) * limits.time[us] - moveOverhead, scale * optimumTime);
  }

  /// movestogo != 0 means: x moves in y seconds (+ z increment)
  else
  {
      double mid = (ply - 32.0) / 32.0;
      scale = 1.6 / std::max(1.0, 1.6 - mid / (1 + std::abs(mid)));
      optimumTime = scale * timeLeft / mtg;
      optimumTime = std::max(minThinkingTime, optimumTime);

      scale = std::min<double>(5.5, 1.5 + 0.1 * mtg);
      maximumTime = std::min<int>(limits.time[us] - 2 * mtg * moveOverhead, scale * optimumTime);
  }

  if (Options["Ponder"])
      optimumTime += optimumTime / 4;
}
