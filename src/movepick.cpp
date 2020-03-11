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

#include "movepick.h"

namespace {

  enum Stages {
    MAIN_TT, CAPTURE_INIT, GOOD_CAPTURE, REFUTATION, QUIET_INIT, QUIET, BAD_CAPTURE,
    EVASION_TT, EVASION_INIT, EVASION,
    PROBCUT_TT, PROBCUT_INIT, PROBCUT,
    QSEARCH_TT, QCAPTURE_INIT, QCAPTURE, QCHECK_INIT, QCHECK
  };

  // partial_insertion_sort() sorts moves in descending order up to and including
  // a given limit. The order of moves smaller than the limit is left unspecified.
  void partial_insertion_sort(ExtMove* begin, ExtMove* end, int limit) {

    for (ExtMove *sortedEnd = begin, *p = begin + 1; p < end; ++p)
        if (p->value >= limit)
        {
            ExtMove tmp = *p, *q;
            *p = *++sortedEnd;
            for (q = sortedEnd; q != begin && *(q - 1) < tmp; --q)
                *q = *(q - 1);
            *q = tmp;
        }
  }

} // namespace


/// Constructors of the MovePicker class. As arguments we pass information
/// to help it to return the (presumably) good moves first, to decide which
/// moves to return (in the quiescence search, for instance, we only want to
/// search captures, promotions, and some checks) and how important good move
/// ordering is at the current node.

/// MovePicker constructor for the main search
MovePicker::MovePicker(const Position& p, Move ttm, Depth d, const ButterflyHistory* mh, const LowPlyHistory* lp,
                       const CapturePieceToHistory* cph, const PieceToHistory** ch, Move cm, Move* killers, int pl)
           : pos(p), mainHistory(mh), lowPlyHistory(lp), captureHistory(cph), continuationHistory(ch),
             depth(d) , ply(pl) {

  assert(d > 0);

  refutations.push_back(ExtMove(killers[0]));
  refutations.push_back(ExtMove(killers[1]));
  refutations.push_back(ExtMove(cm));

  stage = pos.checkers() ? EVASION_TT : MAIN_TT;
  ttMove = ttm && pos.pseudo_legal(ttm) ? ttm : MOVE_NONE;
  stage += (ttMove == MOVE_NONE);

  moves.reserve(MAX_MOVES);
}

/// MovePicker constructor for quiescence search
MovePicker::MovePicker(const Position& p, Move ttm, Depth d, const ButterflyHistory* mh,
                       const CapturePieceToHistory* cph, const PieceToHistory** ch, Square rs)
           : pos(p), mainHistory(mh), captureHistory(cph), continuationHistory(ch), recaptureSquare(rs), depth(d) {

  assert(d <= 0);

  stage = pos.checkers() ? EVASION_TT : QSEARCH_TT;
  ttMove =   ttm
          && (depth > DEPTH_QS_RECAPTURES || to_sq(ttm) == recaptureSquare)
          && pos.pseudo_legal(ttm) ? ttm : MOVE_NONE;
  stage += (ttMove == MOVE_NONE);

  moves.reserve(MAX_MOVES);
}

/// MovePicker constructor for ProbCut: we generate captures with SEE greater
/// than or equal to the given threshold.
MovePicker::MovePicker(const Position& p, Move ttm, Value th, const CapturePieceToHistory* cph)
           : pos(p), captureHistory(cph), threshold(th) {

  assert(!pos.checkers());

  stage = PROBCUT_TT;
  ttMove =   ttm
          && pos.capture(ttm)
          && pos.pseudo_legal(ttm)
          && pos.see_ge(ttm, threshold) ? ttm : MOVE_NONE;
  stage += (ttMove == MOVE_NONE);

  moves.reserve(MAX_MOVES);
}

/// MovePicker::score() assigns a numerical value to each move in a list, used
/// for sorting. Captures are ordered by Most Valuable Victim (MVV), preferring
/// captures with a good history. Quiets moves are ordered using the histories.
template<GenType Type>
void MovePicker::score() {

  static_assert(Type == CAPTURES || Type == QUIETS || Type == EVASIONS, "Wrong type");

  for (auto& m : moves)
      if (Type == CAPTURES)
          m.value =  int(PieceValue[MG][pos.piece_on(to_sq(m))]) * 6
                   + (*captureHistory)[pos.moved_piece(m)][to_sq(m)][type_of(pos.piece_on(to_sq(m)))];

      else if (Type == QUIETS)
          m.value =      (*mainHistory)[pos.side_to_move()][from_to(m.move)]
                   + 2 * (*continuationHistory[0])[pos.moved_piece(m.move)][to_sq(m.move)]
                   + 2 * (*continuationHistory[1])[pos.moved_piece(m.move)][to_sq(m.move)]
                   + 2 * (*continuationHistory[3])[pos.moved_piece(m.move)][to_sq(m.move)]
                   +     (*continuationHistory[5])[pos.moved_piece(m.move)][to_sq(m.move)]
                   + (ply < MAX_LPH ?  4 * (*lowPlyHistory)[ply][from_to(m.move)] : 0);

      else // Type == EVASIONS
      {
          if (pos.capture(m.move))
              m.value =  PieceValue[MG][pos.piece_on(to_sq(m.move))]
                       - Value(type_of(pos.moved_piece(m.move)));
          else
              m.value =  (*mainHistory)[pos.side_to_move()][from_to(m.move)]
                       + (*continuationHistory[0])[pos.moved_piece(m.move)][to_sq(m.move)]
                       - (1 << 28);
      }
}

/// MovePicker::select() returns the next move satisfying a predicate function.
/// It never returns the TT move.
template<MovePicker::PickType T, typename Pred>
Move MovePicker::select(MoveList &moveList, Pred filter)
{
  while (cur < moveList.end())
  {
      if (T == Best)
          std::swap(*cur, *std::max_element(cur, moveList.back()));

      if (cur->move != ttMove && filter())
          return *cur++;

      cur++;
  }
  return MOVE_NONE;
}

/// MovePicker::next_move() is the most important method of the MovePicker class. It
/// returns a new pseudo legal move every time it is called until there are no more
/// moves left, picking the move with the highest score from a list of generated moves.
Move MovePicker::next_move(bool skipQuiets) {

top:
  switch (stage) {

  case MAIN_TT:
  case EVASION_TT:
  case QSEARCH_TT:
  case PROBCUT_TT:
      ++stage;
      return ttMove;

  case CAPTURE_INIT:
  case PROBCUT_INIT:
  case QCAPTURE_INIT:
      moves.clear();
      generate<CAPTURES>(pos, moves);
      cur = moves.begin();

      score<CAPTURES>();
      ++stage;
      goto top;

  case GOOD_CAPTURE:
      if (select<Best>(moves, [&](){
                       return pos.see_ge(cur->move, Value(-55 * cur->value / 1024)) ?
                              // Move losing capture to endBadCaptures to be tried later
                              true : (badCaptures.push_back(*cur), false); }))
          return *(cur - 1);

      // Prepare the pointers to loop over the refutations array
      cur = refutations.begin();

      // If the countermove is the same as a killer, skip it
      if (   refutations[0].move == refutations[2].move
          || refutations[1].move == refutations[2].move)
	  refutations.pop_back();

      ++stage;
      /* fallthrough */

  case REFUTATION:
      if (select<Next>(refutations, [&](){ return    cur->move != MOVE_NONE
                                    && !pos.capture(cur->move)
                                    &&  pos.pseudo_legal(cur->move); }))
          return *(cur - 1);
      ++stage;
      /* fallthrough */

  case QUIET_INIT:
      if (!skipQuiets)
      {
	  moves.clear();
          generate<QUIETS>(pos, moves);
	  cur = moves.begin();

          score<QUIETS>();
          //partial_insertion_sort(cur, endMoves, -3000 * depth);
	  //std::sort(moves);
      }

      ++stage;
      /* fallthrough */

  case QUIET:
      return MOVE_NONE;
      if (   !skipQuiets
          && select<Next>(moves, [&](){return   cur->move != refutations[0].move
                                      && cur->move != refutations[1].move
                                      && cur->move != refutations[2].move;}))
          return *(cur - 1);

      // Prepare the pointers to loop over the bad captures
      cur = badCaptures.begin();//moves;

      ++stage;
      /* fallthrough */

  case BAD_CAPTURE:
      return select<Next>(badCaptures, [](){ return true; });

  case EVASION_INIT:
      moves.clear();
      generate<EVASIONS>(pos, moves);
      cur = moves.begin();

      score<EVASIONS>();
      ++stage;
      /* fallthrough */

  case EVASION:
      return select<Best>(moves, [](){ return true; });

  case PROBCUT:
      return select<Best>(moves, [&](){ return pos.see_ge(cur->move, threshold); });

  case QCAPTURE:
      if (select<Best>(moves, [&](){ return   depth > DEPTH_QS_RECAPTURES
                                    || to_sq(cur->move) == recaptureSquare; }))
          return *(cur - 1);

      // If we did not find any move and we do not try checks, we have finished
      if (depth != DEPTH_QS_CHECKS)
          return MOVE_NONE;

      ++stage;
      /* fallthrough */

  case QCHECK_INIT:
      moves.clear();
      generate<QUIET_CHECKS>(pos, moves);
      cur = moves.begin();

      ++stage;
      /* fallthrough */

  case QCHECK:
      return select<Next>(moves, [](){ return true; });
  }

  assert(false);
  return MOVE_NONE; // Silence warning
}
