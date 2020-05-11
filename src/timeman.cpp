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

/// init() is called at the beginning of the search and calculates the allowed
/// thinking time out of the time control and current game ply. We support four
/// different kinds of time controls, passed in 'limits':

void TimeManagement::init(Search::LimitsType& limits, Color us, int ply) {

  TimePoint minThinkingTime = Options["Minimum Thinking Time"];
  TimePoint moveOverhead    = Options["Move Overhead"];
  TimePoint slowMover       = Options["Slow Mover"];
  TimePoint npmsec          = Options["nodestime"];
  double opt_scale, max_scale;

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

  TimePoint timeLeft =  std::max(TimePoint(1),
      limits.time[us] + limits.inc[us] * (mtg - 1) - moveOverhead * (2 + mtg));

  timeLeft = slowMover * timeLeft / 100;

  if (limits.movestogo == 0) /// x basetime (+ z increment)
  {
      opt_scale = 1.0 / std::max(2.0, 8.2 * (9.2 - std::log2(ply + 1)));

      max_scale = (1.0 / std::max(0.5, 1.7 * (8.0 - std::log2(ply + 1)))) / opt_scale;
  }
  else // x moves in y seconds (+ z increment)
  {
      double mid = (ply - 34.0) / 32.0;
      opt_scale = (1.7 / std::max(1.0, 1.7 - mid / (1 + std::abs(mid)))) / mtg;

      max_scale = std::min(6.3, 1.5 + 0.11 * mtg);
  }

  optimumTime = opt_scale * timeLeft;
  optimumTime = std::max(minThinkingTime, optimumTime);

  maximumTime = std::min(0.8 * limits.time[us] - moveOverhead, max_scale * optimumTime);

  if (Options["Ponder"])
      optimumTime += optimumTime / 4;
}
