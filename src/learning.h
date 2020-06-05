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
    //white, ply1
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
    { 0x422F5BC962E4FFD5, make_move(SQ_C2,SQ_C4)},  //Nf3, c5

    //black, ply2
    { 0x9D23AEE67D355B5C, make_move(SQ_D7,SQ_D6)},  //e4, c5, Nf3
    { 0xC040C1C32167C525, make_move(SQ_C8,SQ_C6)},  //e4, c5, Nc3
    { 0xE2C3C6E66290F1F4, make_move(SQ_D7,SQ_D5)},  //e4, c5, c3

    { 0xC058DE9FA60D09B1, make_move(SQ_B8,SQ_C6)},  //e4, e5, Nf3
    { 0x9D3BB1BAFA5F97C8, make_move(SQ_G8,SQ_F6)},  //e4, e5, Nc3
    { 0x409E1A6196F55BBF, make_move(SQ_G8,SQ_F6)}, //e4, e5, Bc4

    { 0x7033575329CFFE6E, make_move(SQ_D7,SQ_D5)}, //e4, e6,d4
    { 0x0E2D79DED1B261B7, make_move(SQ_D7,SQ_D5)}, //e4, e6,d3
    { 0x65A4D0C378246271, make_move(SQ_D7,SQ_D5)}, //e4, e6,Nf3

    { 0x0C15AD78216FC04E, make_move(SQ_E7,SQ_E6)}, //d4, Nf6, c4
    { 0xE72D59CE69C95929, make_move(SQ_G7,SQ_G6)}, //d4, Nf6, Nf3
    { 0xE2DC4A5B2FAC0C6A, make_move(SQ_F3,SQ_E4)}, //d4, Nf6, Bg5

    { 0x8CD881DD8BF57CAF, make_move(SQ_E7,SQ_E6)}, //d4, d5, c4
    { 0x67E0756BC353E5C8, make_move(SQ_G8,SQ_F6)}, //d4, d5, Nf3
    { 0x9333083C5AC65D44, make_move(SQ_G8,SQ_F6)}, //d4, d5, Bf4

    { 0xAEDF7EE793C802BA, make_move(SQ_G8,SQ_F6)}, //d4, e6, c4
    { 0x45E78A51DB6E9BDD, make_move(SQ_G8,SQ_F6)}, //d4, e6, Nf3
    { 0x7033575329CFFE6E, make_move(SQ_D7,SQ_D5)}, //d4, e6, e4

    { 0x19822AE870845C51, make_move(SQ_G7,SQ_G6)}, //Nf3, Nf6, c4
    { 0x15EE1B92C31A5E83, make_move(SQ_G7,SQ_G6)}, //Nf3, Nf6, g3
    { 0xE72D59CE69C95929, make_move(SQ_G7,SQ_G6)}, //Nf3, Nf6, d4

    { 0x952337376980E262, make_move(SQ_G8,SQ_F6)}, //Nf3, d5, g3
    { 0x67E0756BC353E5C8, make_move(SQ_G8,SQ_F6)}, //Nf3, d5, d4
    { 0x994F064DDA1EE0B0, make_move(SQ_C7,SQ_C6)}, //Nf3, d5, c4

    { 0x43CF8752C732A788, make_move(SQ_B8,SQ_C6)}, //Nf3, c5, c4
    { 0x4FA3B62874ACA55A, make_move(SQ_B8,SQ_C6)}, //Nf3, c5, g3
    { 0x9D23AEE67D355B5C, make_move(SQ_D7,SQ_D6)}, //Nf3, c5, e4
};

#endif // #ifndef LEARNING_H_INCLUDED

