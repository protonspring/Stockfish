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

#ifndef LEARNING_H_INCLUDED
#define LEARNING_H_INCLUDED

#include "types.h"

const std::map<Key, Move> learnedPositions = {
    { 0xB4D30CD15A43432D, make_move(SQ_E2,SQ_E4)},  //opening position
    { 0x6BDFF9FE4592E7A4, make_move(SQ_D7,SQ_D6)},  //white, e4
    { 0x4B9CA36CE6D81E08, make_move(SQ_D7,SQ_D5)},  //white, d4
    { 0x5E0B24FCB7338217, make_move(SQ_B8,SQ_C6)},  //white, Nf3
};

#endif // #ifndef LEARNING_H_INCLUDED

