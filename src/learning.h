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

    //black, ply1
    { 0x6BDFF9FE4592E7A4, make_move(SQ_E7,SQ_E5)},  //e4
    { 0x4B9CA36CE6D81E08, make_move(SQ_D7,SQ_D5)},  //d4
    { 0x5E0B24FCB7338217, make_move(SQ_B8,SQ_C6)},  //Nf3

    //white, ply2
    { 0x77FB86CB90459A66, make_move(SQ_G1,SQ_F3)},  //e4, c5
    { 0x2A80F6B24B7DC88B, make_move(SQ_G1,SQ_F3)},  //e4, e5
    { 0x8F7CF8EE9554A34B, make_move(SQ_D2,SQ_D4)},  //e4, e6

    { 0x0DF571E384B99813, make_move(SQ_C2,SQ_C4)},  //d4, Nf6
    { 0x8D385D462E2324F2, make_move(SQ_C2,SQ_C4)},  //d4, d5
    { 0xAF3FA27C361E5AE7, make_move(SQ_C2,SQ_C4)},  //d4, e6

    { 0x1862F673D552040C, make_move(SQ_C2,SQ_C4)},  //Nf3, Nf6
    { 0x98AFDAD67FC8B8ED, make_move(SQ_G2,SQ_G3)},  //Nf3, d5
    { 0x422F5BC962E4FFD5, make_move(SQ_C2,SQ_C4)}   //Nf3, c5
};

#endif // #ifndef LEARNING_H_INCLUDED

