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

#include <cassert>

#include "movegen.h"
#include "position.h"

namespace {

  template<GenType Type, Direction D>
  void make_promotions(MoveList &moveList, Square to, Square ksq) {

    if (Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS)
        moveList.push_back(make<PROMOTION>(to - D, to, QUEEN));

    if (Type == QUIETS || Type == EVASIONS || Type == NON_EVASIONS)
    {
        moveList.push_back(make<PROMOTION>(to - D, to, ROOK));
        moveList.push_back(make<PROMOTION>(to - D, to, BISHOP));
        moveList.push_back(make<PROMOTION>(to - D, to, KNIGHT));
    }

    // Knight promotion is the only promotion that can give a direct check
    // that's not already included in the queen promotion.
    if (Type == QUIET_CHECKS && (PseudoAttacks[KNIGHT][to] & ksq))
        moveList.push_back(make<PROMOTION>(to - D, to, KNIGHT));
    else
        (void)ksq; // Silence a warning under MSVC
  }


  template<Color Us, GenType Type>
  void generate_pawn_moves(const Position& pos, MoveList &moveList, Bitboard target) {

    constexpr Color     Them     = (Us == WHITE ? BLACK      : WHITE);
    constexpr Bitboard  TRank7BB = (Us == WHITE ? Rank7BB    : Rank2BB);
    constexpr Bitboard  TRank3BB = (Us == WHITE ? Rank3BB    : Rank6BB);
    constexpr Direction Up       = pawn_push(Us);
    constexpr Direction UpRight  = (Us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction UpLeft   = (Us == WHITE ? NORTH_WEST : SOUTH_EAST);

    const Square ksq = pos.square<KING>(Them);
    Bitboard emptySquares;

    Bitboard pawnsOn7    = pos.pieces(Us, PAWN) &  TRank7BB;
    Bitboard pawnsNotOn7 = pos.pieces(Us, PAWN) & ~TRank7BB;

    Bitboard enemies = (Type == EVASIONS ? pos.pieces(Them) & target:
                        Type == CAPTURES ? target : pos.pieces(Them));

    // Single and double pawn pushes, no promotions
    if (Type != CAPTURES)
    {
        emptySquares = (Type == QUIETS || Type == QUIET_CHECKS ? target : ~pos.pieces());

        Bitboard b1 = shift<Up>(pawnsNotOn7)   & emptySquares;
        Bitboard b2 = shift<Up>(b1 & TRank3BB) & emptySquares;

        if (Type == EVASIONS) // Consider only blocking squares
        {
            b1 &= target;
            b2 &= target;
        }

        if (Type == QUIET_CHECKS)
        {
            b1 &= pos.attacks_from<PAWN>(ksq, Them);
            b2 &= pos.attacks_from<PAWN>(ksq, Them);

            // Add pawn pushes which give discovered check. This is possible only
            // if the pawn is not on the same file as the enemy king, because we
            // don't generate captures. Note that a possible discovery check
            // promotion has been already generated amongst the captures.
            Bitboard dcCandidateQuiets = pos.blockers_for_king(Them) & pawnsNotOn7;
            if (dcCandidateQuiets)
            {
                Bitboard dc1 = shift<Up>(dcCandidateQuiets) & emptySquares & ~file_bb(ksq);
                Bitboard dc2 = shift<Up>(dc1 & TRank3BB) & emptySquares;

                b1 |= dc1;
                b2 |= dc2;
            }
        }

        while (b1)
        {
            Square to = pop_lsb(&b1);
            moveList.push_back(make_move(to - Up, to));
        }

        while (b2)
        {
            Square to = pop_lsb(&b2);
            moveList.push_back(make_move(to - Up - Up, to));
        }
    }

    // Promotions and underpromotions
    if (pawnsOn7)
    {
        if (Type == CAPTURES)
            emptySquares = ~pos.pieces();

        if (Type == EVASIONS)
            emptySquares &= target;

        Bitboard b1 = shift<UpRight>(pawnsOn7) & enemies;
        Bitboard b2 = shift<UpLeft >(pawnsOn7) & enemies;
        Bitboard b3 = shift<Up     >(pawnsOn7) & emptySquares;

        while (b1)
            make_promotions<Type, UpRight>(moveList, pop_lsb(&b1), ksq);

        while (b2)
            make_promotions<Type, UpLeft >(moveList, pop_lsb(&b2), ksq);

        while (b3)
            make_promotions<Type, Up     >(moveList, pop_lsb(&b3), ksq);
    }

    // Standard and en-passant captures
    if (Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS)
    {
        Bitboard b1 = shift<UpRight>(pawnsNotOn7) & enemies;
        Bitboard b2 = shift<UpLeft >(pawnsNotOn7) & enemies;

        while (b1)
        {
            Square to = pop_lsb(&b1);
            moveList.push_back(make_move(to - UpRight, to));
        }

        while (b2)
        {
            Square to = pop_lsb(&b2);
            moveList.push_back(make_move(to - UpLeft, to));
        }

        if (pos.ep_square() != SQ_NONE)
        {
            assert(rank_of(pos.ep_square()) == relative_rank(Us, RANK_6));

            // An en passant capture can be an evasion only if the checking piece
            // is the double pushed pawn and so is in the target. Otherwise this
            // is a discovery check and we are forced to do otherwise.
            if (Type == EVASIONS && !(target & (pos.ep_square() - Up)))
                return;

            b1 = pawnsNotOn7 & pos.attacks_from<PAWN>(pos.ep_square(), Them);

            assert(b1);

            while (b1)
                moveList.push_back(make<ENPASSANT>(pop_lsb(&b1), pos.ep_square()));
        }
    }
  }


  template<PieceType Pt, bool Checks>
  void generate_moves(const Position& pos, MoveList &moveList, Color us,
                          Bitboard target) {

    static_assert(Pt != KING && Pt != PAWN, "Unsupported piece type in generate_moves()");

    const Square* pl = pos.squares<Pt>(us);

    for (Square from = *pl; from != SQ_NONE; from = *++pl)
    {
        if (Checks)
        {
            if (    (Pt == BISHOP || Pt == ROOK || Pt == QUEEN)
                && !(PseudoAttacks[Pt][from] & target & pos.check_squares(Pt)))
                continue;

            if (pos.blockers_for_king(~us) & from)
                continue;
        }

        Bitboard b = pos.attacks_from<Pt>(from) & target;

        if (Checks)
            b &= pos.check_squares(Pt);

        while (b)
            moveList.push_back(make_move(from, pop_lsb(&b)));
    }
  }


  template<Color Us, GenType Type>
  void generate_all(const Position& pos, MoveList &moveList, Bitboard target) {

    constexpr CastlingRights OO  = Us & KING_SIDE;
    constexpr CastlingRights OOO = Us & QUEEN_SIDE;
    constexpr bool Checks = Type == QUIET_CHECKS; // Reduce template instantations

    generate_pawn_moves<Us, Type>(pos, moveList, target);
    generate_moves<KNIGHT, Checks>(pos, moveList, Us, target);
    generate_moves<BISHOP, Checks>(pos, moveList, Us, target);
    generate_moves<  ROOK, Checks>(pos, moveList, Us, target);
    generate_moves< QUEEN, Checks>(pos, moveList, Us, target);

    if (Type != QUIET_CHECKS && Type != EVASIONS)
    {
        Square ksq = pos.square<KING>(Us);
        Bitboard b = pos.attacks_from<KING>(ksq) & target;
        while (b)
            moveList.push_back(make_move(ksq, pop_lsb(&b)));

        if (Type != CAPTURES && pos.can_castle(CastlingRights(OO | OOO)))
        {
            if (!pos.castling_impeded(OO) && pos.can_castle(OO))
                moveList.push_back(make<CASTLING>(ksq, pos.castling_rook_square(OO)));

            if (!pos.castling_impeded(OOO) && pos.can_castle(OOO))
                moveList.push_back(make<CASTLING>(ksq, pos.castling_rook_square(OOO)));
        }
    }
  }

} // namespace


/// <CAPTURES>     Generates all pseudo-legal captures and queen promotions
/// <QUIETS>       Generates all pseudo-legal non-captures and underpromotions
/// <NON_EVASIONS> Generates all pseudo-legal captures and non-captures
///
/// Returns a pointer to the end of the move list.

template<GenType Type>
void generate(const Position& pos, MostList &moveList) {

  static_assert(Type == CAPTURES || Type == QUIETS || Type == NON_EVASIONS, "Unsupported type in generate()");
  assert(!pos.checkers());

  Color us = pos.side_to_move();

  Bitboard target =  Type == CAPTURES     ?  pos.pieces(~us)
                   : Type == QUIETS       ? ~pos.pieces()
                   : Type == NON_EVASIONS ? ~pos.pieces(us) : 0;

  if (us == WHITE) generate_all<WHITE, Type>(pos, moveList, target)
  else             generate_all<BLACK, Type>(pos, moveList, target);
}

// Explicit template instantiations
template void generate<CAPTURES>(const Position&, MoveList&);
template void generate<QUIETS>(const Position&, MoveList&);
template void generate<NON_EVASIONS>(const Position&, MoveList&);


/// generate<QUIET_CHECKS> generates all pseudo-legal non-captures and knight
/// underpromotions that give check. Returns a pointer to the end of the move list.
template<>
ExtMove* generate<QUIET_CHECKS>(const Position& pos, ExtMove* moveList) {

  assert(!pos.checkers());

  Color us = pos.side_to_move();
  Bitboard dc = pos.blockers_for_king(~us) & pos.pieces(us);

  while (dc)
  {
     Square from = pop_lsb(&dc);
     PieceType pt = type_of(pos.piece_on(from));

     if (pt == PAWN)
         continue; // Will be generated together with direct checks

     Bitboard b = pos.attacks_from(pt, from) & ~pos.pieces();

     if (pt == KING)
         b &= ~PseudoAttacks[QUEEN][pos.square<KING>(~us)];

     while (b)
         moveList.push_back(make_move(from, pop_lsb(&b)));
  }

  if (us == WHITE) generate_all<WHITE, QUIET_CHECKS>(pos, moveList, ~pos.pieces())
  else generate_all<BLACK, QUIET_CHECKS>(pos, moveList, ~pos.pieces());
}


/// generate<EVASIONS> generates all pseudo-legal check evasions when the side
/// to move is in check. Returns a pointer to the end of the move list.
template<>
void generate<EVASIONS>(const Position& pos, MoveList& moveList) {

  assert(pos.checkers());

  Color us = pos.side_to_move();
  Square ksq = pos.square<KING>(us);
  Bitboard sliderAttacks = 0;
  Bitboard sliders = pos.checkers() & ~pos.pieces(KNIGHT, PAWN);

  // Find all the squares attacked by slider checkers. We will remove them from
  // the king evasions in order to skip known illegal moves, which avoids any
  // useless legality checks later on.
  while (sliders)
  {
      Square checksq = pop_lsb(&sliders);
      sliderAttacks |= LineBB[checksq][ksq] ^ checksq;
  }

  // Generate evasions for king, capture and non capture moves
  Bitboard b = pos.attacks_from<KING>(ksq) & ~pos.pieces(us) & ~sliderAttacks;
  while (b)
      moveList.push_back(make_move(ksq, pop_lsb(&b)));

  if (more_than_one(pos.checkers()))
      return; // Double check, only a king move can save the day

  // Generate blocking evasions or captures of the checking piece
  Square checksq = lsb(pos.checkers());
  Bitboard target = between_bb(checksq, ksq) | checksq;

  if (us == WHITE) generate_all<WHITE, EVASIONS>(pos, moveList, target)
  else generate_all<BLACK, EVASIONS>(pos, moveList, target);
}


/// generate<LEGAL> generates all the legal moves in the given position

template<>
void generate<LEGAL>(const Position& pos, MoveList &moveList) {

  Color us = pos.side_to_move();
  Bitboard pinned = pos.blockers_for_king(us) & pos.pieces(us);
  Square ksq = pos.square<KING>(us);
  ExtMove* cur = moveList;

  if (pos.checkers()) generate<EVASIONS    >(pos, moveList)
  else generate<NON_EVASIONS>(pos, moveList);

  for (std::vector<ExtMove>::iterator cur = moveList.begin(); cur < moveList.end(); ++cur)
      if (   (pinned || from_sq(*cur) == ksq || type_of(*cur) == ENPASSANT)
          && !pos.legal(*cur))
      {
          *cur = v.back();
          v.pop_back();
      }
}
