/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2018 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

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
#include <cassert>

#include "bitboard.h"
#include "pawns.h"
#include "position.h"
#include "thread.h"

namespace {

  #define V Value
  #define S(mg, eg) make_score(mg, eg)

  // Isolated pawn penalty
  constexpr Score Isolated = S(13, 18);

  // Backward pawn penalty
  constexpr Score Backward = S(24, 12);

  // Connected pawn bonus by opposed, phalanx, #support and rank
  Score Connected[2][2][3][RANK_NB];

  // Doubled pawn penalty
  constexpr Score Doubled = S(18, 38);

  // Weakness of our pawn shelter in front of the king by [isKingFile][distance from edge][rank].
  // RANK_1 = 0 is used for files where we have no pawns or our pawn is behind our king.
  constexpr Value ShelterWeakness[][int(FILE_NB) / 2][RANK_NB] = {
    { { V(9* 98/8), V(9*20/8), V(9*11/8), V(9*42/8), V(9* 83/8), V(9* 84/8), V(9*101/8) }, // Not On King file
      { V(9*103/8), V(9* 8/8), V(9*33/8), V(9*86/8), V(9* 87/8), V(9*105/8), V(9*113/8) },
      { V(9*100/8), V(9* 2/8), V(9*65/8), V(9*95/8), V(9* 59/8), V(9* 89/8), V(9*115/8) },
      { V(9* 72/8), V(9* 6/8), V(9*52/8), V(9*74/8), V(9* 83/8), V(9* 84/8), V(9*112/8) } },
    { { V(9*105/8), V(9*19/8), V(9* 3/8), V(9*27/8), V(9* 85/8), V(9* 93/8), V(9* 84/8) }, // On King file
      { V(9*121/8), V(9* 7/8), V(9*33/8), V(9*95/8), V(9*112/8), V(9* 86/8), V(9* 72/8) },
      { V(9*121/8), V(9*26/8), V(9*65/8), V(9*90/8), V(9* 65/8), V(9* 76/8), V(9*117/8) },
      { V(9* 79/8), V(9* 0/8), V(9*45/8), V(9*65/8), V(9* 94/8), V(9* 92/8), V(9*105/8) } }
  };

  // Danger of enemy pawns moving toward our king by [type][distance from edge][rank].
  // For the unopposed and unblocked cases, RANK_1 = 0 is used when opponent has
  // no pawn on the given file, or their pawn is behind our king.
  constexpr Value StormDanger[][4][RANK_NB] = {
    { { V(9* 0/8),  V(9*-290/8), V(9*-274/8), V(9*57/8), V(9*41/8) },  // BlockedByKing
      { V(9* 0/8),  V(9*  60/8), V(9* 144/8), V(9*39/8), V(9*13/8) },
      { V(9* 0/8),  V(9*  65/8), V(9* 141/8), V(9*41), V(9*34/8) },
      { V(9* 0/8),  V(9*  53/8), V(9* 127/8), V(9*56/8), V(9*14/8) } },
    { { V(9* 4/8),  V(9*  73/8), V(9* 132/8), V(9*46/8), V(9*31/8) },  // Unopposed
      { V(9* 1/8),  V(9*  64/8), V(9* 143/8), V(9*26/8), V(9*13/8) },
      { V(9* 1/8),  V(9*  47/8), V(9* 110/8), V(9*44/8), V(9*24/8) },
      { V(9* 0/8),  V(9*  72/8), V(9* 127/8), V(9*50/8), V(9*31/8) } },
    { { V(9* 0/8),  V(9*   0/8), V(9*  19/8), V(9*23/8), V(9* 1/8) },  // BlockedByPawn
      { V(9* 0/8),  V(9*   0/8), V(9*  88/8), V(9*27/8), V(9* 2/8) },
      { V(9* 0/8),  V(9*   0/8), V(9* 101/8), V(9*16/8), V(9* 1/8) },
      { V(9* 0/8),  V(9*   0/8), V(9* 111/8), V(9*22/8), V(9*15/8) } },
    { { V(9*22/8),  V(9*  45/8), V(9* 104/8), V(9*62/8), V(9* 6/8) },  // Unblocked
      { V(9*31/8),  V(9*  30/8), V(9*  99/8), V(9*39/8), V(9*19/8) },
      { V(9*23/8),  V(9*  29/8), V(9*  96/8), V(9*41/8), V(9*15/8) },
      { V(9*21/8),  V(9*  23/8), V(9* 116/8), V(9*41/8), V(9*15/8) } }
  };

  // Max bonus for king safety. Corresponds to start position with all the pawns
  // in front of the king and no enemy pawn on the horizon.
  constexpr Value MaxSafetyBonus = V(258);

  #undef S
  #undef V

  template<Color Us>
  Score evaluate(const Position& pos, Pawns::Entry* e) {

    constexpr Color     Them = (Us == WHITE ? BLACK : WHITE);
    constexpr Direction Up   = (Us == WHITE ? NORTH : SOUTH);

    Bitboard b, neighbours, stoppers, doubled, supported, phalanx;
    Bitboard lever, leverPush;
    Square s;
    bool opposed, backward;
    Score score = SCORE_ZERO;
    const Square* pl = pos.squares<PAWN>(Us);

    Bitboard ourPawns   = pos.pieces(  Us, PAWN);
    Bitboard theirPawns = pos.pieces(Them, PAWN);

    e->passedPawns[Us] = e->pawnAttacksSpan[Us] = e->weakUnopposed[Us] = 0;
    e->semiopenFiles[Us] = 0xFF;
    e->kingSquares[Us]   = SQ_NONE;
    e->pawnAttacks[Us]   = pawn_attacks_bb<Us>(ourPawns);
    e->pawnsOnSquares[Us][BLACK] = popcount(ourPawns & DarkSquares);
    e->pawnsOnSquares[Us][WHITE] = pos.count<PAWN>(Us) - e->pawnsOnSquares[Us][BLACK];

    // Loop through all pawns of the current color and score each pawn
    while ((s = *pl++) != SQ_NONE)
    {
        assert(pos.piece_on(s) == make_piece(Us, PAWN));

        File f = file_of(s);

        e->semiopenFiles[Us]   &= ~(1 << f);
        e->pawnAttacksSpan[Us] |= pawn_attack_span(Us, s);

        // Flag the pawn
        opposed    = theirPawns & forward_file_bb(Us, s);
        stoppers   = theirPawns & passed_pawn_mask(Us, s);
        lever      = theirPawns & PawnAttacks[Us][s];
        leverPush  = theirPawns & PawnAttacks[Us][s + Up];
        doubled    = ourPawns   & (s - Up);
        neighbours = ourPawns   & adjacent_files_bb(f);
        phalanx    = neighbours & rank_bb(s);
        supported  = neighbours & rank_bb(s - Up);

        // A pawn is backward when it is behind all pawns of the same color on the
        // adjacent files and cannot be safely advanced.
        if (!neighbours || lever || relative_rank(Us, s) >= RANK_5)
            backward = false;
        else
        {
            // Find the backmost rank with neighbours or stoppers
            b = rank_bb(backmost_sq(Us, neighbours | stoppers));

            // The pawn is backward when it cannot safely progress to that rank:
            // either there is a stopper in the way on this rank, or there is a
            // stopper on adjacent file which controls the way to that rank.
            backward = (b | shift<Up>(b & adjacent_files_bb(f))) & stoppers;

            assert(!(backward && (forward_ranks_bb(Them, s + Up) & neighbours)));
        }

        // Passed pawns will be properly scored in evaluation because we need
        // full attack info to evaluate them. Include also not passed pawns
        // which could become passed after one or two pawn pushes when are
        // not attacked more times than defended.
        if (   !(stoppers ^ lever ^ leverPush)
            && !(ourPawns & forward_file_bb(Us, s))
            && popcount(supported) >= popcount(lever) - 1
            && popcount(phalanx)   >= popcount(leverPush))
            e->passedPawns[Us] |= s;

        else if (   stoppers == SquareBB[s + Up]
                 && relative_rank(Us, s) >= RANK_5)
        {
            b = shift<Up>(supported) & ~theirPawns;
            while (b)
                if (!more_than_one(theirPawns & PawnAttacks[Us][pop_lsb(&b)]))
                    e->passedPawns[Us] |= s;
        }

        // Score this pawn
        if (supported | phalanx)
            score += Connected[opposed][bool(phalanx)][popcount(supported)][relative_rank(Us, s)];

        else if (!neighbours)
            score -= Isolated, e->weakUnopposed[Us] += !opposed;

        else if (backward)
            score -= Backward, e->weakUnopposed[Us] += !opposed;

        if (doubled && !supported)
            score -= Doubled;
    }

    return score;
  }

} // namespace

namespace Pawns {

/// Pawns::init() initializes some tables needed by evaluation. Instead of using
/// hard-coded tables, when makes sense, we prefer to calculate them with a formula
/// to reduce independent parameters and to allow easier tuning and better insight.

void init() {

  static constexpr int Seed[RANK_NB] = { 0, 13, 24, 18, 76, 100, 175, 330 };

  for (int opposed = 0; opposed <= 1; ++opposed)
      for (int phalanx = 0; phalanx <= 1; ++phalanx)
          for (int support = 0; support <= 2; ++support)
              for (Rank r = RANK_2; r < RANK_8; ++r)
  {
      int v = 17 * support;
      v += (Seed[r] + (phalanx ? (Seed[r + 1] - Seed[r]) / 2 : 0)) >> opposed;

      Connected[opposed][phalanx][support][r] = make_score(v, v * (r - 2) / 4);
  }
}


/// Pawns::probe() looks up the current position's pawns configuration in
/// the pawns hash table. It returns a pointer to the Entry if the position
/// is found. Otherwise a new Entry is computed and stored there, so we don't
/// have to recompute all when the same pawns configuration occurs again.

Entry* probe(const Position& pos) {

  Key key = pos.pawn_key();
  Entry* e = pos.this_thread()->pawnsTable[key];

  if (e->key == key)
      return e;

  e->key = key;
  e->scores[WHITE] = evaluate<WHITE>(pos, e);
  e->scores[BLACK] = evaluate<BLACK>(pos, e);
  e->openFiles = popcount(e->semiopenFiles[WHITE] & e->semiopenFiles[BLACK]);
  e->asymmetry = popcount(  (e->passedPawns[WHITE]   | e->passedPawns[BLACK])
                          | (e->semiopenFiles[WHITE] ^ e->semiopenFiles[BLACK]));

  return e;
}


/// Entry::shelter_storm() calculates shelter and storm penalties for the file
/// the king is on, as well as the two closest files.

template<Color Us>
Value Entry::shelter_storm(const Position& pos, Square ksq) {

  constexpr Color Them = (Us == WHITE ? BLACK : WHITE);

  enum { BlockedByKing, Unopposed, BlockedByPawn, Unblocked };

  File center = std::max(FILE_B, std::min(FILE_G, file_of(ksq)));
  Bitboard b =   pos.pieces(PAWN)
               & (forward_ranks_bb(Us, ksq) | rank_bb(ksq))
               & (adjacent_files_bb(center) | file_bb(center));
  Bitboard ourPawns = b & pos.pieces(Us);
  Bitboard theirPawns = b & pos.pieces(Them);
  Value safety = MaxSafetyBonus;

  for (File f = File(center - 1); f <= File(center + 1); ++f)
  {
      b = ourPawns & file_bb(f);
      Rank rkUs = b ? relative_rank(Us, backmost_sq(Us, b)) : RANK_1;

      b = theirPawns & file_bb(f);
      Rank rkThem = b ? relative_rank(Us, frontmost_sq(Them, b)) : RANK_1;

      int d = std::min(f, ~f);
      safety -=  ShelterWeakness[f == file_of(ksq)][d][rkUs]
               + StormDanger
                 [f == file_of(ksq) && rkThem == relative_rank(Us, ksq) + 1 ? BlockedByKing  :
                  rkUs   == RANK_1                                          ? Unopposed :
                  rkThem == rkUs + 1                                        ? BlockedByPawn  : Unblocked]
                 [d][rkThem];
  }

  return safety;
}


/// Entry::do_king_safety() calculates a bonus for king safety. It is called only
/// when king square changes, which is about 20% of total king_safety() calls.

template<Color Us>
Score Entry::do_king_safety(const Position& pos, Square ksq) {

  kingSquares[Us] = ksq;
  castlingRights[Us] = pos.can_castle(Us);
  int minKingPawnDistance = 0;

  Bitboard pawns = pos.pieces(Us, PAWN);
  if (pawns)
      while (!(DistanceRingBB[ksq][minKingPawnDistance++] & pawns)) {}

  Value bonus = shelter_storm<Us>(pos, ksq);

  // If we can castle use the bonus after the castling if it is bigger
  if (pos.can_castle(MakeCastling<Us, KING_SIDE>::right))
      bonus = std::max(bonus, shelter_storm<Us>(pos, relative_square(Us, SQ_G1)));

  if (pos.can_castle(MakeCastling<Us, QUEEN_SIDE>::right))
      bonus = std::max(bonus, shelter_storm<Us>(pos, relative_square(Us, SQ_C1)));

  return make_score(bonus, -16 * minKingPawnDistance);
}

// Explicit template instantiation
template Score Entry::do_king_safety<WHITE>(const Position& pos, Square ksq);
template Score Entry::do_king_safety<BLACK>(const Position& pos, Square ksq);

} // namespace Pawns
