/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2020 The Stockfish developers (see AUTHORS file)

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

#include <cassert>

#include "bitboard.h"
#include "endgame.h"
#include "movegen.h"

namespace {

  // Used to drive the king towards the edge of the board
  // in KX vs K and KQ vs KR endgames.
  // Values range from 27 (center squares) to 90 (in the corners)
  inline int push_to_edge(Square s) {
      int rd = edge_distance(rank_of(s)), fd = edge_distance(file_of(s));
      return 90 - (7 * fd * fd / 2 + 7 * rd * rd / 2);
  }

  // Used to drive the king towards A1H8 corners in KBN vs K endgames.
  // Values range from 0 on A8H1 diagonal to 7 in A1H8 corners
  inline int push_to_corner(Square s) {
      return abs(7 - rank_of(s) - file_of(s));
  }

  // Drive a piece close to or away from another piece
  inline int push_close(Square s1, Square s2) { return 140 - 20 * distance(s1, s2); }
  inline int push_away(Square s1, Square s2) { return 120 - push_close(s1, s2); }

#ifndef NDEBUG
  bool verify_material(const Position& pos, Color c, Value npm, int pawnsCnt) {
    return pos.non_pawn_material(c) == npm && pos.count<PAWN>(c) == pawnsCnt;
  }
#endif

  // Map the square as if strongSide is white and strongSide's only pawn
  // is on the left half of the board.
  Square normalize(const Position& pos, Color strongSide, Square sq) {

    assert(pos.count<PAWN>(strongSide) == 1);

    if (file_of(pos.square<PAWN>(strongSide)) >= FILE_E)
        sq = flip_file(sq);

    return strongSide == WHITE ? sq : flip_rank(sq);
  }

} // namespace


namespace Endgames {

  std::pair<Map<Value>, Map<ScaleFactor>> maps;

  void init() {

    add<KPK>("KPK");
  }
}


/// KP vs K. This endgame is evaluated with the help of a bitbase
template<>
Value Endgame<KPK>::operator()(const Position& pos) const {

  assert(verify_material(pos, strongSide, VALUE_ZERO, 1));
  assert(verify_material(pos, weakSide, VALUE_ZERO, 0));

  // Assume strongSide is white and the pawn is on files A-D
  Square strongKing = normalize(pos, strongSide, pos.square<KING>(strongSide));
  Square strongPawn = normalize(pos, strongSide, pos.square<PAWN>(strongSide));
  Square weakKing   = normalize(pos, strongSide, pos.square<KING>(weakSide));

  Color us = strongSide == pos.side_to_move() ? WHITE : BLACK;

  if (!Bitbases::probe(strongKing, strongPawn, weakKing, us))
      return VALUE_DRAW;

  Value result = VALUE_KNOWN_WIN + PawnValueEg + Value(rank_of(strongPawn));

  return strongSide == pos.side_to_move() ? result : -result;
}

