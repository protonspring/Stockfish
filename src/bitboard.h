/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2019 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

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

#ifndef BITBOARD_H_INCLUDED
#define BITBOARD_H_INCLUDED

#include <string>

#include "types.h"

namespace Bitbases {

void init();
bool probe(Square wksq, Square wpsq, Square bksq, Color us);

}

namespace Bitboards {

void init();
const std::string pretty(Bitboard b);

}

constexpr Bitboard  AllSquares = ~Bitboard(0);
constexpr Bitboard DarkSquares = 0xAA55AA55AA55AA55ULL;
constexpr Bitboard     FileABB = 0x0101010101010101ULL;
constexpr Bitboard     Rank1BB = 0xFF;

extern uint8_t PopCnt16[1 << 16];
extern uint8_t SquareDistance[SQUARE_NB][SQUARE_NB];

extern Bitboard SquareBB[SQUARE_NB];
extern Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
extern Bitboard LineBB[SQUARE_NB][SQUARE_NB];
extern Bitboard DistanceRingBB[SQUARE_NB][8];
extern Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];
extern Bitboard PawnAttacks[COLOR_NB][SQUARE_NB];
extern Bitboard KingFlank[FILE_NB];

/// rbb() and fbb() return a bitboard representing squares on the given file or rank.
constexpr Bitboard rbb(  Rank r) { return Rank1BB << (8 * r); }
constexpr Bitboard fbb(  File f) { return FileABB << f;       }
constexpr Bitboard fbb(Square s) { return fbb(file_of(s));    }
constexpr Bitboard rbb(Square s) { return rbb(rank_of(s));    }

/// shift() moves a bitboard one step along direction D
template<Direction D>
constexpr Bitboard shift(Bitboard b) {
  return  D == NORTH      ?  b                 << 8 : D == SOUTH      ?  b                 >> 8
        : D == EAST       ? (b & ~fbb(FILE_H)) << 1 : D == WEST       ? (b & ~fbb(FILE_A)) >> 1
        : D == NORTH_EAST ? (b & ~fbb(FILE_H)) << 9 : D == NORTH_WEST ? (b & ~fbb(FILE_A)) << 7
        : D == SOUTH_EAST ? (b & ~fbb(FILE_H)) >> 7 : D == SOUTH_WEST ? (b & ~fbb(FILE_A)) >> 9
        : 0;
}

constexpr Bitboard QueenSide   =  fbb(FILE_A) | fbb(FILE_B)  |  fbb(FILE_C) | fbb(FILE_D);
constexpr Bitboard CenterFiles =  fbb(FILE_C) | fbb(FILE_D)  |  fbb(FILE_E) | fbb(FILE_F);
constexpr Bitboard KingSide    =  fbb(FILE_E) | fbb(FILE_F)  |  fbb(FILE_G) | fbb(FILE_H);
constexpr Bitboard Center      = (fbb(FILE_D) | fbb(FILE_E)) & (rbb(RANK_4) | rbb(RANK_5));


/// Magic holds all magic bitboards relevant data for a single square
struct Magic {
  Bitboard  mask;
  Bitboard  magic;
  Bitboard* attacks;
  unsigned  shift;

  // Compute the attack's index using the 'magic bitboards' approach
  unsigned index(Bitboard occupied) const {

    if (HasPext)
        return unsigned(pext(occupied, mask));

    if (Is64Bit)
        return unsigned(((occupied & mask) * magic) >> shift);

    unsigned lo = unsigned(occupied) & unsigned(mask);
    unsigned hi = unsigned(occupied >> 32) & unsigned(mask >> 32);
    return (lo * unsigned(magic) ^ hi * unsigned(magic >> 32)) >> shift;
  }
};

extern Magic RookMagics[SQUARE_NB];
extern Magic BishopMagics[SQUARE_NB];


/// Overloads of bitwise operators between a Bitboard and a Square for testing
/// whether a given bit is set in a bitboard, and for setting and clearing bits.

inline Bitboard operator&(Bitboard b, Square s) {
  assert(s >= SQ_A1 && s <= SQ_H8);
  return b & SquareBB[s];
}

inline Bitboard operator|(Bitboard b, Square s) {
  assert(s >= SQ_A1 && s <= SQ_H8);
  return b | SquareBB[s];
}

inline Bitboard operator^(Bitboard b, Square s) {
  assert(s >= SQ_A1 && s <= SQ_H8);
  return b ^ SquareBB[s];
}

inline Bitboard& operator|=(Bitboard& b, Square s) {
  assert(s >= SQ_A1 && s <= SQ_H8);
  return b |= SquareBB[s];
}

inline Bitboard& operator^=(Bitboard& b, Square s) {
  assert(s >= SQ_A1 && s <= SQ_H8);
  return b ^= SquareBB[s];
}

constexpr bool more_than_one(Bitboard b) {
  return b & (b - 1);
}

inline bool opposite_colors(Square s1, Square s2) {
  return bool(DarkSquares & s1) != bool(DarkSquares & s2);
}


/// pawn_attacks_bb() returns the squares attacked by pawns of the given color
/// from the squares in the given bitboard.
template<Color C>
constexpr Bitboard pawn_attacks_bb(Bitboard b) {
  return C == WHITE ? shift<NORTH_WEST>(b) | shift<NORTH_EAST>(b)
                    : shift<SOUTH_WEST>(b) | shift<SOUTH_EAST>(b);
}


/// pawn_double_attacks_bb() returns the squares doubly attacked by pawns of the
/// given color from the squares in the given bitboard.
template<Color C>
constexpr Bitboard pawn_double_attacks_bb(Bitboard b) {
  return C == WHITE ? shift<NORTH_WEST>(b) & shift<NORTH_EAST>(b)
                    : shift<SOUTH_WEST>(b) & shift<SOUTH_EAST>(b);
}


/// adjacent_files_bb() returns a bitboard representing all the squares on the
/// adjacent files of the given one.
constexpr Bitboard adjacent_files_bb(File f) {
  return shift<EAST>(fbb(f)) | shift<WEST>(fbb(f));
}


/// between_bb() returns a bitboard representing all the squares between the two
/// given ones. For instance, between_bb(SQ_C4, SQ_F7) returns a bitboard with
/// the bits for square d5 and e6 set. If s1 and s2 are not on the same rank,
/// file or diagonal, 0 is returned.
inline Bitboard between_bb(Square s1, Square s2) {
  return BetweenBB[s1][s2];
}


/// forward_ranks_bb() returns a bitboard representing the squares on the ranks
/// in front of the given one, from the point of view of the given color. For instance,
/// forward_ranks_bb(BLACK, SQ_D3) will return the 16 squares on ranks 1 and 2.
constexpr Bitboard forward_ranks_bb(Color c, Square s) {
  return c == WHITE ? ~rbb(RANK_1) << 8 * (rank_of(s) - RANK_1)
                    : ~rbb(RANK_8) >> 8 * (RANK_8 - rank_of(s));
}

/// forward_file_bb() returns a bitboard representing all the squares along the
/// line in front of the given one, from the point of view of the given color.
constexpr Bitboard forward_file_bb(Color c, Square s) {
  return forward_ranks_bb(c, s) & fbb(s);
}

/// pawn_attack_span() returns a bitboard representing all the squares that can
/// be attacked by a pawn of the given color when it moves along its file,
/// starting from the given square.
constexpr Bitboard pawn_attack_span(Color c, Square s) {
  return forward_ranks_bb(c, s) & adjacent_files_bb(file_of(s));
}

/// passed_pawn_span() returns a bitboard which can be used to test if a pawn of
/// the given color and on the given square is a passed pawn.
constexpr Bitboard passed_pawn_span(Color c, Square s) {
  return forward_ranks_bb(c, s) & (adjacent_files_bb(file_of(s)) | fbb(s));
}

/// aligned() returns true if the squares s1, s2 and s3 are aligned either on a
/// straight or on a diagonal line.
inline bool aligned(Square s1, Square s2, Square s3) {
  return LineBB[s1][s2] & s3;
}

/// distance() functions return the distance between x and y, defined as the
/// number of steps for a king in x to reach y. Works with squares, ranks, files.

template<typename T> constexpr int distance(T x, T y) { return std::abs(x - y); }
template<> inline int distance<Square>(Square x, Square y) { return SquareDistance[x][y]; }

template<typename T1, typename T2> constexpr int distance(T2 x, T2 y);
template<> constexpr int distance<File>(Square x, Square y) { return distance(file_of(x), file_of(y)); }
template<> constexpr int distance<Rank>(Square x, Square y) { return distance(rank_of(x), rank_of(y)); }


/// attacks_bb() returns a bitboard representing all the squares attacked by a
/// piece of type Pt (bishop or rook) placed on 's'.

template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Bitboard occupied) {

  const Magic& m = Pt == ROOK ? RookMagics[s] : BishopMagics[s];
  return m.attacks[m.index(occupied)];
}

inline Bitboard attacks_bb(PieceType pt, Square s, Bitboard occupied) {

  assert(pt != PAWN);

  switch (pt)
  {
  case BISHOP: return attacks_bb<BISHOP>(s, occupied);
  case ROOK  : return attacks_bb<  ROOK>(s, occupied);
  case QUEEN : return attacks_bb<BISHOP>(s, occupied) | attacks_bb<ROOK>(s, occupied);
  default    : return PseudoAttacks[pt][s];
  }
}


/// popcount() counts the number of non-zero bits in a bitboard

inline int popcount(Bitboard b) {

#ifndef USE_POPCNT

  union { Bitboard bb; uint16_t u[4]; } v = { b };
  return PopCnt16[v.u[0]] + PopCnt16[v.u[1]] + PopCnt16[v.u[2]] + PopCnt16[v.u[3]];

#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)

  return (int)_mm_popcnt_u64(b);

#else // Assumed gcc or compatible compiler

  return __builtin_popcountll(b);

#endif
}


/// lsb() and msb() return the least/most significant bit in a non-zero bitboard

#if defined(__GNUC__)  // GCC, Clang, ICC

inline Square lsb(Bitboard b) {
  assert(b);
  return Square(__builtin_ctzll(b));
}

inline Square msb(Bitboard b) {
  assert(b);
  return Square(63 ^ __builtin_clzll(b));
}

#elif defined(_MSC_VER)  // MSVC

#ifdef _WIN64  // MSVC, WIN64

inline Square lsb(Bitboard b) {
  assert(b);
  unsigned long idx;
  _BitScanForward64(&idx, b);
  return (Square) idx;
}

inline Square msb(Bitboard b) {
  assert(b);
  unsigned long idx;
  _BitScanReverse64(&idx, b);
  return (Square) idx;
}

#else  // MSVC, WIN32

inline Square lsb(Bitboard b) {
  assert(b);
  unsigned long idx;

  if (b & 0xffffffff) {
      _BitScanForward(&idx, int32_t(b));
      return Square(idx);
  } else {
      _BitScanForward(&idx, int32_t(b >> 32));
      return Square(idx + 32);
  }
}

inline Square msb(Bitboard b) {
  assert(b);
  unsigned long idx;

  if (b >> 32) {
      _BitScanReverse(&idx, int32_t(b >> 32));
      return Square(idx + 32);
  } else {
      _BitScanReverse(&idx, int32_t(b));
      return Square(idx);
  }
}

#endif

#else  // Compiler is neither GCC nor MSVC compatible

#error "Compiler not supported."

#endif


/// pop_lsb() finds and clears the least significant bit in a non-zero bitboard

inline Square pop_lsb(Bitboard* b) {
  const Square s = lsb(*b);
  *b &= *b - 1;
  return s;
}


/// frontmost_sq() and backmost_sq() return the square corresponding to the
/// most/least advanced bit relative to the given color.

inline Square frontmost_sq(Color c, Bitboard b) { return c == WHITE ? msb(b) : lsb(b); }
inline Square  backmost_sq(Color c, Bitboard b) { return c == WHITE ? lsb(b) : msb(b); }

#endif // #ifndef BITBOARD_H_INCLUDED
