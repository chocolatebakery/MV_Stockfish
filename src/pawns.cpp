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
#include <cassert>

#include "bitboard.h"
#include "pawns.h"
#include "position.h"
#include "thread.h"

namespace {

  #define V Value
  #define S(mg, eg) make_score(mg, eg)

  // Pawn penalties
  constexpr Score Backward[VARIANT_NB] = {
    S( 9, 24),
#ifdef ANTI
    S(26, 50),
#endif
#ifdef ATOMIC
    S(35, 15),
#endif
#ifdef CRAZYHOUSE
    S(41, 19),
#endif
#ifdef EXTINCTION
    S(17, 11),
#endif
#ifdef GRID
    S(17, 11),
#endif
#ifdef HORDE
    S(78, 14),
#endif
#ifdef KOTH
    S(41, 19),
#endif
#ifdef LOSERS
    S(26, 49),
#endif
#ifdef RACE
    S(0, 0),
#endif
#ifdef THREECHECK
    S(41, 19),
#endif
#ifdef TWOKINGS
    S(17, 11),
#endif
  };
  constexpr Score BlockedStorm  = S(82, 82);
  constexpr Score Doubled[VARIANT_NB] = {
    S(11, 56),
#ifdef ANTI
    S( 4, 51),
#endif
#ifdef ATOMIC
    S( 0,  0),
#endif
#ifdef CRAZYHOUSE
    S(13, 40),
#endif
#ifdef EXTINCTION
    S(13, 40),
#endif
#ifdef GRID
    S(13, 40),
#endif
#ifdef HORDE
    S(11, 83),
#endif
#ifdef KOTH
    S(13, 40),
#endif
#ifdef LOSERS
    S( 4, 54),
#endif
#ifdef RACE
    S( 0,  0),
#endif
#ifdef THREECHECK
    S(13, 40),
#endif
#ifdef TWOKINGS
    S(13, 40),
#endif
  };
  constexpr Score Isolated[VARIANT_NB] = {
    S( 5, 15),
#ifdef ANTI
    S(54, 69),
#endif
#ifdef ATOMIC
    S(24, 14),
#endif
#ifdef CRAZYHOUSE
    S(30, 27),
#endif
#ifdef EXTINCTION
    S(13, 16),
#endif
#ifdef GRID
    S(13, 16),
#endif
#ifdef HORDE
    S(16, 38),
#endif
#ifdef KOTH
    S(30, 27),
#endif
#ifdef LOSERS
    S(53, 69),
#endif
#ifdef RACE
    S(0, 0),
#endif
#ifdef THREECHECK
    S(30, 27),
#endif
#ifdef TWOKINGS
    S(13, 16),
#endif
  };
  constexpr Score WeakLever     = S( 0, 56);
  constexpr Score WeakUnopposed = S(13, 27);

  // Connected pawn bonus
  constexpr int Connected[RANK_NB] = { 0, 7, 8, 12, 29, 48, 86 };

  // Strength of pawn shelter for our king by [distance from edge][rank].
  // RANK_1 = 0 is used for files where we have no pawn, or pawn is behind our king.
  constexpr Value ShelterStrength[VARIANT_NB][int(FILE_NB) / 2][RANK_NB] = {
  {
    { V( -6), V( 81), V( 93), V( 58), V( 39), V( 18), V(  25) },
    { V(-43), V( 61), V( 35), V(-49), V(-29), V(-11), V( -63) },
    { V(-10), V( 75), V( 23), V( -2), V( 32), V(  3), V( -45) },
    { V(-39), V(-13), V(-29), V(-52), V(-48), V(-67), V(-166) }
  },
#ifdef ANTI
  {},
#endif
#ifdef ATOMIC
  {
    { V( 7), V(76), V(84), V( 38), V( 7), V( 30), V(-19) },
    { V(-3), V(93), V(52), V(-17), V(12), V(-22), V(-35) },
    { V(-6), V(83), V(25), V(-24), V(15), V( 22), V(-39) },
    { V(11), V(83), V(19), V(  8), V(18), V(-21), V(-30) }
  },
#endif
#ifdef CRAZYHOUSE
  {
    { V(-48), V(138), V(80), V( 48), V( 5), V( -7), V(  9) },
    { V(-78), V(116), V(20), V( -2), V(14), V(  6), V(-36) },
    { V(-69), V( 99), V(12), V(-19), V(38), V( 22), V(-50) },
    { V( -6), V( 95), V( 9), V(  4), V(-2), V(  2), V(-37) }
  },
#endif
#ifdef EXTINCTION
  {},
#endif
#ifdef GRID
  {
    { V( 7), V(76), V(84), V( 38), V( 7), V( 30), V(-19) },
    { V(-3), V(93), V(52), V(-17), V(12), V(-22), V(-35) },
    { V(-6), V(83), V(25), V(-24), V(15), V( 22), V(-39) },
    { V(11), V(83), V(19), V(  8), V(18), V(-21), V(-30) }
  },
#endif
#ifdef HORDE
  {
    { V( 7), V(76), V(84), V( 38), V( 7), V( 30), V(-19) },
    { V(-3), V(93), V(52), V(-17), V(12), V(-22), V(-35) },
    { V(-6), V(83), V(25), V(-24), V(15), V( 22), V(-39) },
    { V(11), V(83), V(19), V(  8), V(18), V(-21), V(-30) }
  },
#endif
#ifdef KOTH
  {
    { V( 7), V(76), V(84), V( 38), V( 7), V( 30), V(-19) },
    { V(-3), V(93), V(52), V(-17), V(12), V(-22), V(-35) },
    { V(-6), V(83), V(25), V(-24), V(15), V( 22), V(-39) },
    { V(11), V(83), V(19), V(  8), V(18), V(-21), V(-30) }
  },
#endif
#ifdef LOSERS
  {
    { V( 7), V(76), V(84), V( 38), V( 7), V( 30), V(-19) },
    { V(-3), V(93), V(52), V(-17), V(12), V(-22), V(-35) },
    { V(-6), V(83), V(25), V(-24), V(15), V( 22), V(-39) },
    { V(11), V(83), V(19), V(  8), V(18), V(-21), V(-30) }
  },
#endif
#ifdef RACE
  {}, // There are no pawns in Racing Kings
#endif
#ifdef THREECHECK
  {
    { V( 7), V(76), V(84), V( 38), V( 7), V( 30), V(-19) },
    { V(-3), V(93), V(52), V(-17), V(12), V(-22), V(-35) },
    { V(-6), V(83), V(25), V(-24), V(15), V( 22), V(-39) },
    { V(11), V(83), V(19), V(  8), V(18), V(-21), V(-30) }
  },
#endif
#ifdef TWOKINGS
  {
    { V( 7), V(76), V(84), V( 38), V( 7), V( 30), V(-19) },
    { V(-3), V(93), V(52), V(-17), V(12), V(-22), V(-35) },
    { V(-6), V(83), V(25), V(-24), V(15), V( 22), V(-39) },
    { V(11), V(83), V(19), V(  8), V(18), V(-21), V(-30) }
  },
#endif
  };

  // Danger of enemy pawns moving toward our king by [distance from edge][rank].
  // RANK_1 = 0 is used for files where the enemy has no pawn, or their pawn
  // is behind our king. Note that UnblockedStorm[0][1-2] accommodate opponent pawn
  // on edge, likely blocked by our king.
  constexpr Value UnblockedStorm[int(FILE_NB) / 2][RANK_NB] = {
    { V( 85), V(-289), V(-166), V(97), V(50), V( 45), V( 50) },
    { V( 46), V( -25), V( 122), V(45), V(37), V(-10), V( 20) },
    { V( -6), V(  51), V( 168), V(34), V(-2), V(-22), V(-14) },
    { V(-15), V( -11), V( 101), V( 4), V(11), V(-15), V(-29) }
  };

#ifdef HORDE
  constexpr Score ImbalancedHorde = S(49, 39);
#endif
  #undef S
  #undef V

  template<Color Us>
  Score evaluate(const Position& pos, Pawns::Entry* e) {

    constexpr Color     Them = ~Us;
    constexpr Direction Up   = pawn_push(Us);

    Bitboard neighbours, stoppers, support, phalanx, opposed;
    Bitboard lever, leverPush, blocked;
    Square s;
    bool backward, passed, doubled;
    Score score = SCORE_ZERO;
    const Square* pl = pos.squares<PAWN>(Us);

    Bitboard ourPawns   = pos.pieces(  Us, PAWN);
    Bitboard theirPawns = pos.pieces(Them, PAWN);

    Bitboard doubleAttackThem = pawn_double_attacks_bb<Them>(theirPawns);

    e->passedPawns[Us] = 0;
    e->kingSquares[Us] = SQ_NONE;
    e->pawnAttacks[Us] = e->pawnAttacksSpan[Us] = pawn_attacks_bb<Us>(ourPawns);
    e->blockedCount += popcount(shift<Up>(ourPawns) & (theirPawns | doubleAttackThem));

#ifdef HORDE
    if (pos.is_horde() && pos.is_horde_color(Us))
    {
        int l = 0, m = 0, r = popcount(ourPawns & file_bb(FILE_A));
        for (File f1 = FILE_A; f1 <= FILE_H; ++f1)
        {
            l = m; m = r; r = popcount(ourPawns & shift<EAST>(file_bb(f1)));
            score -= ImbalancedHorde * m / (1 + l * r);
        }
    }
#endif
    // Loop through all pawns of the current color and score each pawn
    while ((s = *pl++) != SQ_NONE)
    {
        assert(pos.piece_on(s) == make_piece(Us, PAWN));

        Rank r = relative_rank(Us, s);

        // Flag the pawn
        opposed    = theirPawns & forward_file_bb(Us, s);
        blocked    = theirPawns & (s + Up);
        stoppers   = theirPawns & passed_pawn_span(Us, s);
        lever      = theirPawns & PawnAttacks[Us][s];
        leverPush  = theirPawns & PawnAttacks[Us][s + Up];
#ifdef HORDE
        if (pos.is_horde() && relative_rank(Us, s) == RANK_1)
            doubled = 0;
        else
#endif
        doubled    = ourPawns   & (s - Up);
        neighbours = ourPawns   & adjacent_files_bb(s);
        phalanx    = neighbours & rank_bb(s);
#ifdef HORDE
        if (pos.is_horde() && relative_rank(Us, s) == RANK_1)
            support = 0;
        else
#endif
        support    = neighbours & rank_bb(s - Up);

        // A pawn is backward when it is behind all pawns of the same color on
        // the adjacent files and cannot safely advance.
        backward =  !(neighbours & forward_ranks_bb(Them, s + Up))
                  && (leverPush | blocked);

        // Compute additional span if pawn is not backward nor blocked
        if (!backward && !blocked)
            e->pawnAttacksSpan[Us] |= pawn_attack_span(Us, s);

        // A pawn is passed if one of the three following conditions is true:
        // (a) there is no stoppers except some levers
        // (b) the only stoppers are the leverPush, but we outnumber them
        // (c) there is only one front stopper which can be levered.
        //     (Refined in Evaluation::passed)
        passed =   !(stoppers ^ lever)
                || (   !(stoppers ^ leverPush)
                    && popcount(phalanx) >= popcount(leverPush))
                || (   stoppers == blocked && r >= RANK_5
                    && (shift<Up>(support) & ~(theirPawns | doubleAttackThem)));

        passed &= !(forward_file_bb(Us, s) & ourPawns);

        // Passed pawns will be properly scored later in evaluation when we have
        // full attack info.
        if (passed)
            e->passedPawns[Us] |= s;

        // Score this pawn
        if (support | phalanx)
        {
            int v =  Connected[r] * (4 + 2 * bool(phalanx) - 2 * bool(opposed) - bool(blocked)) / 2
                   + 21 * popcount(support);

            score += make_score(v, v * (r - 2) / 4);
        }

        else if (!neighbours)
        {
            score -=   Isolated[pos.variant()]
                     + WeakUnopposed * !opposed;

            if (   (ourPawns & forward_file_bb(Them, s))
                && popcount(opposed) == 1
                && !(theirPawns & adjacent_files_bb(s)))
                score -= Doubled[pos.variant()];
        }

        else if (backward)
            score -=   Backward[pos.variant()]
                     + WeakUnopposed * !opposed;

#ifdef HORDE
        if (!support || pos.is_horde())
#else
        if (!support)
#endif
            score -= Doubled[pos.variant()] * doubled
                     + WeakLever * more_than_one(lever);
    }

    return score;
  }

} // namespace

namespace Pawns {

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
  e->blockedCount = 0;
  e->scores[WHITE] = evaluate<WHITE>(pos, e);
  e->scores[BLACK] = evaluate<BLACK>(pos, e);

  return e;
}


/// Entry::evaluate_shelter() calculates the shelter bonus and the storm
/// penalty for a king, looking at the king file and the two closest files.

template<Color Us>
Score Entry::evaluate_shelter(const Position& pos, Square ksq) {

  constexpr Color Them = ~Us;

  Bitboard b = pos.pieces(PAWN) & ~forward_ranks_bb(Them, ksq);
  Bitboard ourPawns = b & pos.pieces(Us);
  Bitboard theirPawns = b & pos.pieces(Them);

  Score bonus = make_score(5, 5);

  File center = Utility::clamp(file_of(ksq), FILE_B, FILE_G);
  for (File f = File(center - 1); f <= File(center + 1); ++f)
  {
      b = ourPawns & file_bb(f);
      int ourRank = b ? relative_rank(Us, frontmost_sq(Them, b)) : 0;

      b = theirPawns & file_bb(f);
      int theirRank = b ? relative_rank(Us, frontmost_sq(Them, b)) : 0;

      int d = edge_distance(f);
      bonus += make_score(ShelterStrength[pos.variant()][d][ourRank], 0);

      if (ourRank && (ourRank == theirRank - 1))
          bonus -= BlockedStorm * int(theirRank == RANK_3);
      else
          bonus -= make_score(UnblockedStorm[d][theirRank], 0);
  }

  return bonus;
}


/// Entry::do_king_safety() calculates a bonus for king safety. It is called only
/// when king square changes, which is about 20% of total king_safety() calls.

template<Color Us>
Score Entry::do_king_safety(const Position& pos) {

  Square ksq = pos.square<KING>(Us);
  kingSquares[Us] = ksq;
  castlingRights[Us] = pos.castling_rights(Us);
  auto compare = [](Score a, Score b) { return mg_value(a) < mg_value(b); };

  Score shelter = evaluate_shelter<Us>(pos, ksq);

  // If we can castle use the bonus after castling if it is bigger

  if (pos.can_castle(Us & KING_SIDE))
      shelter = std::max(shelter, evaluate_shelter<Us>(pos, relative_square(Us, SQ_G1)), compare);

  if (pos.can_castle(Us & QUEEN_SIDE))
      shelter = std::max(shelter, evaluate_shelter<Us>(pos, relative_square(Us, SQ_C1)), compare);

  // In endgame we like to bring our king near our closest pawn
  Bitboard pawns = pos.pieces(Us, PAWN);
  int minPawnDist = 6;

  if (pawns & PseudoAttacks[KING][ksq])
      minPawnDist = 1;
  else while (pawns)
      minPawnDist = std::min(minPawnDist, distance(ksq, pop_lsb(&pawns)));

  return shelter - make_score(0, 16 * minPawnDist);
}

// Explicit template instantiation
template Score Entry::do_king_safety<WHITE>(const Position& pos);
template Score Entry::do_king_safety<BLACK>(const Position& pos);

} // namespace Pawns
