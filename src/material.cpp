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
#include <cstring>   // For std::memset

#include "material.h"
#include "thread.h"

using namespace std;

namespace {

  // Endgame evaluation and scaling functions are accessed directly and not through
  // the function maps because they correspond to more than one material hash key.
  Endgame<KXK>    EvaluateKXK[] = { Endgame<KXK>(WHITE),    Endgame<KXK>(BLACK) };

  Endgame<KBPsK>  ScaleKBPsK[]  = { Endgame<KBPsK>(WHITE),  Endgame<KBPsK>(BLACK) };
  Endgame<KQKRPs> ScaleKQKRPs[] = { Endgame<KQKRPs>(WHITE), Endgame<KQKRPs>(BLACK) };
  Endgame<KPsK>   ScaleKPsK[]   = { Endgame<KPsK>(WHITE),   Endgame<KPsK>(BLACK) };
  Endgame<KPKP>   ScaleKPKP[]   = { Endgame<KPKP>(WHITE),   Endgame<KPKP>(BLACK) };

  // Helper used to detect a given material distribution
  bool is_KXK(const Position& pos, Color us) {
    return  !more_than_one(pos.pieces(~us))
          && pos.non_pawn_material(us) >= RookValueMg;
  }

  bool is_KBPsK(const Position& pos, Color us) {
    return   pos.non_pawn_material(us) == BishopValueMg
          && pos.count<PAWN  >(us) >= 1;
  }

  bool is_KQKRPs(const Position& pos, Color us) {
    return  !pos.count<PAWN>(us)
          && pos.non_pawn_material(us) == QueenValueMg
          && pos.count<ROOK>(~us) == 1
          && pos.count<PAWN>(~us) >= 1;
  }

} // namespace

namespace Material {


/// Material::probe() looks up the current position's material configuration in
/// the material hash table. It returns a pointer to the Entry if the position
/// is found. Otherwise a new Entry is computed and stored there, so we don't
/// have to recompute all when the same material configuration occurs again.

Entry* probe(const Position& pos) {

  Key key = pos.material_key();
  Entry* e = pos.this_thread()->materialTable[key];

  if (e->key == key)
      return e;

  std::memset(e, 0, sizeof(Entry));
  e->key = key;
  e->factor[WHITE] = e->factor[BLACK] = (uint8_t)SCALE_FACTOR_NORMAL;

  Value npm_w = pos.non_pawn_material(WHITE);
  Value npm_b = pos.non_pawn_material(BLACK);
  Value npm   = Utility::clamp(npm_w + npm_b, EndgameLimit, MidgameLimit);

  // Map total non-pawn material into [PHASE_ENDGAME, PHASE_MIDGAME]
  e->gamePhase = Phase(((npm - EndgameLimit) * PHASE_MIDGAME) / (MidgameLimit - EndgameLimit));

  // Let's look if we have a specialized evaluation function for this particular
  // material configuration. Firstly we look for a fixed configuration one, then
  // for a generic one if the previous search failed.
  if ((e->evaluationFunction = Endgames::probe<Value>(key)) != nullptr)
      return e;

  for (Color c : { WHITE, BLACK })
      if (is_KXK(pos, c))
      {
          e->evaluationFunction = &EvaluateKXK[c];
          return e;
      }

  // OK, we didn't find any special evaluation function for the current material
  // configuration. Is there a suitable specialized scaling function?
  const auto* sf = Endgames::probe<ScaleFactor>(key);

  if (sf)
  {
      e->scalingFunction[sf->strongSide] = sf; // Only strong color assigned
      return e;
  }

  // We didn't find any specialized scaling function, so fall back on generic
  // ones that refer to more than one material distribution. Note that in this
  // case we don't return after setting the function.
  for (Color c : { WHITE, BLACK })
  {
    if (is_KBPsK(pos, c))
        e->scalingFunction[c] = &ScaleKBPsK[c];

    else if (is_KQKRPs(pos, c))
        e->scalingFunction[c] = &ScaleKQKRPs[c];
  }

  if (npm_w + npm_b == VALUE_ZERO && pos.pieces(PAWN)) // Only pawns on the board
  {
      if (!pos.count<PAWN>(BLACK))
      {
          assert(pos.count<PAWN>(WHITE) >= 2);

          e->scalingFunction[WHITE] = &ScaleKPsK[WHITE];
      }
      else if (!pos.count<PAWN>(WHITE))
      {
          assert(pos.count<PAWN>(BLACK) >= 2);

          e->scalingFunction[BLACK] = &ScaleKPsK[BLACK];
      }
      else if (pos.count<PAWN>(WHITE) == 1 && pos.count<PAWN>(BLACK) == 1)
      {
          // This is a special case because we set scaling functions
          // for both colors instead of only one.
          e->scalingFunction[WHITE] = &ScaleKPKP[WHITE];
          e->scalingFunction[BLACK] = &ScaleKPKP[BLACK];
      }
  }

  // Zero or just one pawn makes it difficult to win, even with a small material
  // advantage. This catches some trivial draws like KK, KBK and KNK and gives a
  // drawish scale factor for cases such as KRKBP and KmmKm (except for KBBKN).
  if (!pos.count<PAWN>(WHITE) && npm_w - npm_b <= BishopValueMg)
      e->factor[WHITE] = uint8_t(npm_w <  RookValueMg   ? SCALE_FACTOR_DRAW :
                                 npm_b <= BishopValueMg ? 4 : 14);

  if (!pos.count<PAWN>(BLACK) && npm_b - npm_w <= BishopValueMg)
      e->factor[BLACK] = uint8_t(npm_b <  RookValueMg   ? SCALE_FACTOR_DRAW :
                                 npm_w <= BishopValueMg ? 4 : 14);

  return e;
}

} // namespace Material
