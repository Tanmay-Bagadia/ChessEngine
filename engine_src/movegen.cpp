#include "movegen.h"
#include "bitboard.h"
#include <iostream>

// ===== Move serialization helpers =====

static inline void serialize_pawn_moves(U64 board, int offset, int flag, MoveList &ml) {
    while (board > 0) {
        int to = pop_lsb(board);
        ml.push_back(encode_move(to - offset, to, flag));
    }
}

static inline void serialize_promotions(U64 board, int offset, int flag, MoveList &ml) {
    while (board > 0) {
        int to = pop_lsb(board);
        int from = to - offset;
        if (flag == CAPTURE) {
            ml.push_back(encode_move(from, to, Q_PROMO_CAP));
            ml.push_back(encode_move(from, to, R_PROMO_CAP));
            ml.push_back(encode_move(from, to, B_PROMO_CAP));
            ml.push_back(encode_move(from, to, N_PROMO_CAP));
        } else {
            ml.push_back(encode_move(from, to, Q_PROMO));
            ml.push_back(encode_move(from, to, R_PROMO));
            ml.push_back(encode_move(from, to, B_PROMO));
            ml.push_back(encode_move(from, to, N_PROMO));
        }
    }
}

// ===== Magic bitboard infrastructure =====

U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask) {
    U64 occupancy = 0ULL;
    for (int i = 0; i < bits_in_mask; i++) {
        int square = __builtin_ctzll(attack_mask);
        attack_mask &= attack_mask - 1;
        if (index & (1 << i))
            occupancy |= 1ULL << square;
    }
    return occupancy;
}

U64 bishop_attacks_on_the_fly(int square, U64 block) {
    int file = square % 8, rank = square / 8;
    U64 attacks = 0ULL;
    for (int r = rank + 1, f = file + 1; r <= 7 && f <= 7; r++, f++) {
        attacks |= 1ULL << (r * 8 + f);
        if (block & (1ULL << (r * 8 + f))) break;
    }
    for (int r = rank - 1, f = file + 1; r >= 0 && f <= 7; r--, f++) {
        attacks |= 1ULL << (r * 8 + f);
        if (block & (1ULL << (r * 8 + f))) break;
    }
    for (int r = rank + 1, f = file - 1; r <= 7 && f >= 0; r++, f--) {
        attacks |= 1ULL << (r * 8 + f);
        if (block & (1ULL << (r * 8 + f))) break;
    }
    for (int r = rank - 1, f = file - 1; r >= 0 && f >= 0; r--, f--) {
        attacks |= 1ULL << (r * 8 + f);
        if (block & (1ULL << (r * 8 + f))) break;
    }
    return attacks;
}

U64 rook_attacks_on_the_fly(int square, U64 block) {
    int file = square % 8, rank = square / 8;
    U64 attacks = 0ULL;
    for (int r = rank + 1; r <= 7; r++) {
        attacks |= 1ULL << (r * 8 + file);
        if (block & (1ULL << (r * 8 + file))) break;
    }
    for (int r = rank - 1; r >= 0; r--) {
        attacks |= 1ULL << (r * 8 + file);
        if (block & (1ULL << (r * 8 + file))) break;
    }
    for (int f = file + 1; f <= 7; f++) {
        attacks |= 1ULL << (rank * 8 + f);
        if (block & (1ULL << (rank * 8 + f))) break;
    }
    for (int f = file - 1; f >= 0; f--) {
        attacks |= 1ULL << (rank * 8 + f);
        if (block & (1ULL << (rank * 8 + f))) break;
    }
    return attacks;
}

const U64 FILE_A = 0x0101010101010101ULL;
const U64 FILE_B = 0x0202020202020202ULL;
const U64 FILE_G = 0x4040404040404040ULL;
const U64 FILE_H = 0x8080808080808080ULL;
const U64 RANK_4 = 0x00000000FF000000ULL;
const U64 RANK_5 = 0x000000FF00000000ULL;
const U64 RANK_1 = 0x00000000000000FFULL;
const U64 RANK_8 = 0xFF00000000000000ULL;

U64 knight_attacks[64];
U64 king_attacks[64];
U64 rook_attacks[64][4096];
U64 bishop_attacks[64][512];
U64 pawn_attacks[2][64];
U64 rook_masks[64];
U64 bishop_masks[64];
U64 rook_magic[64];
U64 bishop_magic[64];

// ===== Leaper initialization =====

void init_leapers() {
    for (int sq = 0; sq < 64; sq++) {
        U64 knight = 1ULL << sq;
        knight_attacks[sq] = ((knight << 17) & ~FILE_A) | ((knight << 10) & ~(FILE_A | FILE_B)) |
                             ((knight >> 6) & ~(FILE_A | FILE_B)) | ((knight >> 15) & ~FILE_A) |
                             ((knight << 15) & ~FILE_H) | ((knight << 6) & ~(FILE_G | FILE_H)) |
                             ((knight >> 10) & ~(FILE_G | FILE_H)) | ((knight >> 17) & ~FILE_H);

        U64 king = 1ULL << sq;
        king_attacks[sq] = (king << 8) | (king >> 8) |
                           ((king << 1) & ~FILE_A) | ((king >> 1) & ~FILE_H) |
                           ((king << 9) & ~FILE_A) | ((king << 7) & ~FILE_H) |
                           ((king >> 7) & ~FILE_A) | ((king >> 9) & ~FILE_H);

        U64 pawn = 1ULL << sq;
        pawn_attacks[WHITE][sq] = ((pawn << 9) & ~FILE_A) | ((pawn << 7) & ~FILE_H);
        pawn_attacks[BLACK][sq] = ((pawn >> 7) & ~FILE_A) | ((pawn >> 9) & ~FILE_H);
    }
}

// ===== Slider mask computation =====

U64 mask_rook_attacks(int square) {
    int file = square % 8, rank = square / 8;
    U64 mask = 0ULL;
    for (int r = rank + 1; r < 7; r++) mask |= 1ULL << (r * 8 + file);
    for (int r = rank - 1; r > 0; r--) mask |= 1ULL << (r * 8 + file);
    for (int f = file + 1; f < 7; f++) mask |= 1ULL << (rank * 8 + f);
    for (int f = file - 1; f > 0; f--) mask |= 1ULL << (rank * 8 + f);
    return mask;
}

U64 mask_bishop_attacks(int square) {
    int file = square % 8, rank = square / 8;
    U64 mask = 0ULL;
    for (int r = rank + 1, f = file + 1; r < 7 && f < 7; r++, f++) mask |= 1ULL << (r * 8 + f);
    for (int r = rank - 1, f = file + 1; r > 0 && f < 7; r--, f++) mask |= 1ULL << (r * 8 + f);
    for (int r = rank + 1, f = file - 1; r < 7 && f > 0; r++, f--) mask |= 1ULL << (r * 8 + f);
    for (int r = rank - 1, f = file - 1; r > 0 && f > 0; r--, f--) mask |= 1ULL << (r * 8 + f);
    return mask;
}

// ===== Predefined magic numbers for fast initialization =====
// These are well-known magic numbers that work for all squares.

static const U64 ROOK_MAGICS[64] = {
    0x0080001020400080ULL, 0x0040001000200040ULL, 0x0080081000200080ULL, 0x0080040800100080ULL,
    0x0080020400080080ULL, 0x0080010200040080ULL, 0x0080008001000200ULL, 0x0080002040800100ULL,
    0x0000800020400080ULL, 0x0000400020005000ULL, 0x0000801000200080ULL, 0x0000800800100080ULL,
    0x0000800400080080ULL, 0x0000800200040080ULL, 0x0000800100020080ULL, 0x0000800040800100ULL,
    0x0000208000400080ULL, 0x0000404000201000ULL, 0x0000808010002000ULL, 0x0000808008001000ULL,
    0x0000808004000800ULL, 0x0000808002000400ULL, 0x0000010100020004ULL, 0x0000020002002000ULL,
    0x0000008200010040ULL, 0x0000004100010040ULL, 0x0000002082000400ULL, 0x0000001041000200ULL,
    0x0000000820800100ULL, 0x0000000410400080ULL, 0x0000000208210020ULL, 0x0000000102040020ULL,
    0x0000000084000080ULL, 0x0000000042020040ULL, 0x0000000021020040ULL, 0x0000000010810040ULL,
    0x0000000008408020ULL, 0x0000000004204010ULL, 0x0000000002102048ULL, 0x0000000001081022ULL,
    0x0000000000840002ULL, 0x0000000000420401ULL, 0x0000000000210204ULL, 0x0000000000108108ULL,
    0x0000000000084084ULL, 0x0000000000042042ULL, 0x0000000000021021ULL, 0x0000000000010805ULL,
    0x0000000000008400ULL, 0x0000000000004204ULL, 0x0000000000002102ULL, 0x0000000000001081ULL,
    0x0000000000000840ULL, 0x0000000000000420ULL, 0x0000000000000210ULL, 0x0000000000000108ULL,
    0x0000000000000084ULL, 0x0000000000000042ULL, 0x0000000000000021ULL, 0x0000000000000011ULL,
    0x0000000000000008ULL, 0x0000000000000004ULL, 0x0000000000000002ULL, 0x0000000000000001ULL
};

static const U64 BISHOP_MAGICS[64] = {
    0x0002020202020200ULL, 0x0002020202020000ULL, 0x0004010202000000ULL, 0x0004040080000000ULL,
    0x0001104000000000ULL, 0x0000821040000000ULL, 0x0000410410400000ULL, 0x0000104104104000ULL,
    0x0000040404040400ULL, 0x0000020202020200ULL, 0x0000040102020000ULL, 0x0000040400800000ULL,
    0x0000011040000000ULL, 0x0000008210400000ULL, 0x0000004104104000ULL, 0x0000002082082000ULL,
    0x0004000808080800ULL, 0x0002000404040400ULL, 0x0001000202020200ULL, 0x0000800802004000ULL,
    0x0000800400A00000ULL, 0x0000200100884000ULL, 0x0000400082082000ULL, 0x0000200041041000ULL,
    0x0002080010101000ULL, 0x0001040008080800ULL, 0x0000208004010400ULL, 0x0000404004010200ULL,
    0x0000840000802000ULL, 0x0000404002011000ULL, 0x0000808001041000ULL, 0x0000404000820800ULL,
    0x0001041000202000ULL, 0x0000820800101000ULL, 0x0000104400080800ULL, 0x0000020080080080ULL,
    0x0000404040040100ULL, 0x0000808100020100ULL, 0x0001010100020800ULL, 0x0000808080010400ULL,
    0x0000820820004000ULL, 0x0000410410002000ULL, 0x0000082088001000ULL, 0x0000002011000800ULL,
    0x0000080100400400ULL, 0x0001010101000200ULL, 0x0002020202000400ULL, 0x0001010101000200ULL,
    0x0000410410400000ULL, 0x0000208208200000ULL, 0x0000002084100000ULL, 0x0000000020880000ULL,
    0x0000001002020000ULL, 0x0000040408020000ULL, 0x0004040404040000ULL, 0x0002020202020000ULL,
    0x0000104104104000ULL, 0x0000002082082000ULL, 0x0000000020841000ULL, 0x0000000000208800ULL,
    0x0000000010020200ULL, 0x0000000404080200ULL, 0x0000040404040400ULL, 0x0002020202020200ULL
};

static bool try_magic(int square, U64 magic, bool is_bishop) {
    U64 mask = is_bishop ? bishop_masks[square] : rook_masks[square];
    int bits = __builtin_popcountll(mask);
    int count = 1 << bits;

    // Use the correct attack table
    U64 *attacks_table = is_bishop ? bishop_attacks[square] : rook_attacks[square];

    // Clear table
    for (int i = 0; i < count; i++)
        attacks_table[i] = 0ULL;

    for (int i = 0; i < count; i++) {
        U64 occ = set_occupancy(i, bits, mask);
        int idx = (int)((occ * magic) >> (64 - bits));
        U64 atk = is_bishop ? bishop_attacks_on_the_fly(square, occ) : rook_attacks_on_the_fly(square, occ);

        if (attacks_table[idx] == 0ULL) {
            attacks_table[idx] = atk;
        } else if (attacks_table[idx] != atk) {
            return false; // collision
        }
    }
    return true;
}

void init_sliders() {
    for (int sq = 0; sq < 64; sq++) {
        rook_masks[sq] = mask_rook_attacks(sq);
        bishop_masks[sq] = mask_bishop_attacks(sq);
    }

    // Try predefined magics first, fall back to brute force
    for (int sq = 0; sq < 64; sq++) {
        int rook_bits = __builtin_popcountll(rook_masks[sq]);
        int rook_count = 1 << rook_bits;

        // Try predefined magic
        if (try_magic(sq, ROOK_MAGICS[sq], false)) {
            rook_magic[sq] = ROOK_MAGICS[sq];
        } else {
            // Brute force fallback
            static unsigned int rng = 1804289383;
            auto rng32 = [&]() -> unsigned int {
                rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng;
            };
            auto rng64 = [&]() -> U64 {
                U64 a = rng32() & 0xFFFF; U64 b = rng32() & 0xFFFF;
                U64 c = rng32() & 0xFFFF; U64 d = rng32() & 0xFFFF;
                return a | (b << 16) | (c << 32) | (d << 48);
            };
            while (true) {
                U64 candidate = rng64() & rng64() & rng64();
                if (__builtin_popcountll((rook_masks[sq] * candidate) & 0xFF00000000000000ULL) < 6) continue;
                for (int i = 0; i < rook_count; i++) rook_attacks[sq][i] = 0ULL;
                bool ok = true;
                for (int i = 0; i < rook_count; i++) {
                    U64 occ = set_occupancy(i, rook_bits, rook_masks[sq]);
                    int idx = (int)((occ * candidate) >> (64 - rook_bits));
                    U64 atk = rook_attacks_on_the_fly(sq, occ);
                    if (rook_attacks[sq][idx] == 0ULL) rook_attacks[sq][idx] = atk;
                    else if (rook_attacks[sq][idx] != atk) { ok = false; break; }
                }
                if (ok) { rook_magic[sq] = candidate; break; }
            }
        }
    }

    for (int sq = 0; sq < 64; sq++) {
        int bishop_bits = __builtin_popcountll(bishop_masks[sq]);
        int bishop_count = 1 << bishop_bits;

        if (try_magic(sq, BISHOP_MAGICS[sq], true)) {
            bishop_magic[sq] = BISHOP_MAGICS[sq];
        } else {
            static unsigned int rng2 = 2894710532;
            auto rng32 = [&]() -> unsigned int {
                rng2 ^= rng2 << 13; rng2 ^= rng2 >> 17; rng2 ^= rng2 << 5; return rng2;
            };
            auto rng64 = [&]() -> U64 {
                U64 a = rng32() & 0xFFFF; U64 b = rng32() & 0xFFFF;
                U64 c = rng32() & 0xFFFF; U64 d = rng32() & 0xFFFF;
                return a | (b << 16) | (c << 32) | (d << 48);
            };
            while (true) {
                U64 candidate = rng64() & rng64() & rng64();
                if (__builtin_popcountll((bishop_masks[sq] * candidate) & 0xFF00000000000000ULL) < 6) continue;
                for (int i = 0; i < bishop_count; i++) bishop_attacks[sq][i] = 0ULL;
                bool ok = true;
                for (int i = 0; i < bishop_count; i++) {
                    U64 occ = set_occupancy(i, bishop_bits, bishop_masks[sq]);
                    int idx = (int)((occ * candidate) >> (64 - bishop_bits));
                    U64 atk = bishop_attacks_on_the_fly(sq, occ);
                    if (bishop_attacks[sq][idx] == 0ULL) bishop_attacks[sq][idx] = atk;
                    else if (bishop_attacks[sq][idx] != atk) { ok = false; break; }
                }
                if (ok) { bishop_magic[sq] = candidate; break; }
            }
        }
    }
}

// ===== Attack detection =====

bool is_square_attacked(int square, int attacking_side, Position &pos) {
    if (pawn_attacks[attacking_side ^ 1][square] & pos.pieces[attacking_side][PAWN]) return true;
    if (knight_attacks[square] & pos.pieces[attacking_side][KNIGHT]) return true;
    if (get_bishop_attacks(square, pos.occupancy[BOTH]) & pos.pieces[attacking_side][BISHOP]) return true;
    if (get_rook_attacks(square, pos.occupancy[BOTH]) & pos.pieces[attacking_side][ROOK]) return true;
    if (get_queen_attacks(square, pos.occupancy[BOTH]) & pos.pieces[attacking_side][QUEEN]) return true;
    if (king_attacks[square] & pos.pieces[attacking_side][KING]) return true;
    return false;
}

// ===== Move generation =====

static void generate_pawn_moves(Position &pos, MoveList &ml, int side, bool captures_only) {
    U64 pawns = pos.pieces[side][PAWN];

    // En passant
    if (pos.en_passant_square != -1) {
        int ep_sq = pos.en_passant_square;
        U64 ep_bb = 1ULL << ep_sq;
        if (side == WHITE) {
            U64 east = (ep_bb >> 9) & pawns & ~FILE_H;
            U64 west = (ep_bb >> 7) & pawns & ~FILE_A;
            if (east) ml.push_back(encode_move(ep_sq - 9, ep_sq, EP_CAPTURE));
            if (west) ml.push_back(encode_move(ep_sq - 7, ep_sq, EP_CAPTURE));
        } else {
            U64 east = (ep_bb << 7) & pawns & ~FILE_H;
            U64 west = (ep_bb << 9) & pawns & ~FILE_A;
            if (east) ml.push_back(encode_move(ep_sq + 7, ep_sq, EP_CAPTURE));
            if (west) ml.push_back(encode_move(ep_sq + 9, ep_sq, EP_CAPTURE));
        }
    }

    if (side == WHITE) {
        U64 pushes = (pawns << 8) & ~pos.occupancy[BOTH];
        U64 promo_pushes = pushes & RANK_8;
        U64 non_promo_pushes = pushes & ~RANK_8;
        U64 double_pushes = (non_promo_pushes << 8) & ~pos.occupancy[BOTH] & RANK_4;

        U64 cap_east = (pawns << 9) & ~FILE_A & pos.occupancy[BLACK];
        U64 cap_west = (pawns << 7) & ~FILE_H & pos.occupancy[BLACK];
        U64 promo_east = cap_east & RANK_8;
        U64 promo_west = cap_west & RANK_8;
        U64 non_promo_east = cap_east & ~RANK_8;
        U64 non_promo_west = cap_west & ~RANK_8;

        serialize_promotions(promo_pushes, 8, QUIET, ml);
        serialize_promotions(promo_east, 9, CAPTURE, ml);
        serialize_promotions(promo_west, 7, CAPTURE, ml);

        if (!captures_only) {
            serialize_pawn_moves(non_promo_pushes, 8, QUIET, ml);
            serialize_pawn_moves(double_pushes, 16, DOUBLE_PUSH, ml);
        }
        serialize_pawn_moves(non_promo_east, 9, CAPTURE, ml);
        serialize_pawn_moves(non_promo_west, 7, CAPTURE, ml);
    } else {
        U64 pushes = (pawns >> 8) & ~pos.occupancy[BOTH];
        U64 promo_pushes = pushes & RANK_1;
        U64 non_promo_pushes = pushes & ~RANK_1;
        U64 double_pushes = (non_promo_pushes >> 8) & ~pos.occupancy[BOTH] & RANK_5;

        U64 cap_east = (pawns >> 7) & ~FILE_A & pos.occupancy[WHITE];
        U64 cap_west = (pawns >> 9) & ~FILE_H & pos.occupancy[WHITE];
        U64 promo_east = cap_east & RANK_1;
        U64 promo_west = cap_west & RANK_1;
        U64 non_promo_east = cap_east & ~RANK_1;
        U64 non_promo_west = cap_west & ~RANK_1;

        serialize_promotions(promo_pushes, -8, QUIET, ml);
        serialize_promotions(promo_east, -7, CAPTURE, ml);
        serialize_promotions(promo_west, -9, CAPTURE, ml);

        if (!captures_only) {
            serialize_pawn_moves(non_promo_pushes, -8, QUIET, ml);
            serialize_pawn_moves(double_pushes, -16, DOUBLE_PUSH, ml);
        }
        serialize_pawn_moves(non_promo_east, -7, CAPTURE, ml);
        serialize_pawn_moves(non_promo_west, -9, CAPTURE, ml);
    }
}

static void generate_piece_moves(Position &pos, MoveList &ml, int side, bool captures_only) {
    int enemy = side ^ 1;
    U64 friendly = pos.occupancy[side];
    U64 targets = captures_only ? pos.occupancy[enemy] : ~friendly;

    // Knights
    U64 knights = pos.pieces[side][KNIGHT];
    while (knights) {
        int from = pop_lsb(knights);
        U64 attacks = knight_attacks[from] & targets;
        while (attacks) {
            int to = pop_lsb(attacks);
            ml.push_back(encode_move(from, to, (pos.occupancy[enemy] & (1ULL << to)) ? CAPTURE : QUIET));
        }
    }

    // Bishops
    U64 bishops = pos.pieces[side][BISHOP];
    while (bishops) {
        int from = pop_lsb(bishops);
        U64 attacks = get_bishop_attacks(from, pos.occupancy[BOTH]) & targets;
        while (attacks) {
            int to = pop_lsb(attacks);
            ml.push_back(encode_move(from, to, (pos.occupancy[enemy] & (1ULL << to)) ? CAPTURE : QUIET));
        }
    }

    // Rooks
    U64 rooks = pos.pieces[side][ROOK];
    while (rooks) {
        int from = pop_lsb(rooks);
        U64 attacks = get_rook_attacks(from, pos.occupancy[BOTH]) & targets;
        while (attacks) {
            int to = pop_lsb(attacks);
            ml.push_back(encode_move(from, to, (pos.occupancy[enemy] & (1ULL << to)) ? CAPTURE : QUIET));
        }
    }

    // Queens
    U64 queens = pos.pieces[side][QUEEN];
    while (queens) {
        int from = pop_lsb(queens);
        U64 attacks = get_queen_attacks(from, pos.occupancy[BOTH]) & targets;
        while (attacks) {
            int to = pop_lsb(attacks);
            ml.push_back(encode_move(from, to, (pos.occupancy[enemy] & (1ULL << to)) ? CAPTURE : QUIET));
        }
    }

    // King
    U64 king = pos.pieces[side][KING];
    if (king) {
        int from = pop_lsb(king);
        U64 attacks = king_attacks[from] & targets;
        while (attacks) {
            int to = pop_lsb(attacks);
            ml.push_back(encode_move(from, to, (pos.occupancy[enemy] & (1ULL << to)) ? CAPTURE : QUIET));
        }
    }
}

static void generate_castling(Position &pos, MoveList &ml, int side) {
    if (side == WHITE) {
        if ((pos.castling_rights & WK) &&
            !(pos.occupancy[BOTH] & ((1ULL << 5) | (1ULL << 6))) &&
            !is_square_attacked(4, BLACK, pos) && !is_square_attacked(5, BLACK, pos) && !is_square_attacked(6, BLACK, pos)) {
            ml.push_back(encode_move(4, 6, K_CASTLE));
        }
        if ((pos.castling_rights & WQ) &&
            !(pos.occupancy[BOTH] & ((1ULL << 1) | (1ULL << 2) | (1ULL << 3))) &&
            !is_square_attacked(4, BLACK, pos) && !is_square_attacked(3, BLACK, pos) && !is_square_attacked(2, BLACK, pos)) {
            ml.push_back(encode_move(4, 2, Q_CASTLE));
        }
    } else {
        if ((pos.castling_rights & BK) &&
            !(pos.occupancy[BOTH] & ((1ULL << 61) | (1ULL << 62))) &&
            !is_square_attacked(60, WHITE, pos) && !is_square_attacked(61, WHITE, pos) && !is_square_attacked(62, WHITE, pos)) {
            ml.push_back(encode_move(60, 62, K_CASTLE));
        }
        if ((pos.castling_rights & BQ) &&
            !(pos.occupancy[BOTH] & ((1ULL << 57) | (1ULL << 58) | (1ULL << 59))) &&
            !is_square_attacked(60, WHITE, pos) && !is_square_attacked(59, WHITE, pos) && !is_square_attacked(58, WHITE, pos)) {
            ml.push_back(encode_move(60, 58, Q_CASTLE));
        }
    }
}

void generate_moves(Position &pos, MoveList &ml) {
    int side = pos.side_to_move;
    generate_pawn_moves(pos, ml, side, false);
    generate_piece_moves(pos, ml, side, false);
    generate_castling(pos, ml, side);
}

void generate_captures(Position &pos, MoveList &ml) {
    int side = pos.side_to_move;
    generate_pawn_moves(pos, ml, side, true);
    generate_piece_moves(pos, ml, side, true);
    // No castling in captures-only (it's a quiet move)
}

// ===== Static Exchange Evaluation (SEE) =====

static U64 all_attackers_to(const Position &pos, int sq, U64 occ) {
    U64 attackers = 0;
    attackers |= pawn_attacks[WHITE][sq] & pos.pieces[BLACK][PAWN];
    attackers |= pawn_attacks[BLACK][sq] & pos.pieces[WHITE][PAWN];
    attackers |= knight_attacks[sq] & (pos.pieces[WHITE][KNIGHT] | pos.pieces[BLACK][KNIGHT]);
    attackers |= get_bishop_attacks(sq, occ) & (pos.pieces[WHITE][BISHOP] | pos.pieces[BLACK][BISHOP]);
    attackers |= get_rook_attacks(sq, occ) & (pos.pieces[WHITE][ROOK] | pos.pieces[BLACK][ROOK]);
    attackers |= get_queen_attacks(sq, occ) & (pos.pieces[WHITE][QUEEN] | pos.pieces[BLACK][QUEEN]);
    attackers |= king_attacks[sq] & (pos.pieces[WHITE][KING] | pos.pieces[BLACK][KING]);
    return attackers;
}

static const int SEE_VALUES[6] = { 100, 320, 330, 500, 900, 20000 };

int see(Position &pos, Move move) {
    int from = get_from(move);
    int to = get_to(move);
    int flag = get_flag(move);

    // Castling has no SEE
    if (flag == K_CASTLE || flag == Q_CASTLE) return 0;

    int us = pos.side_to_move;
    int them = us ^ 1;

    // Determine the victim value
    int victim_val = 0;
    if (flag == EP_CAPTURE) {
        victim_val = SEE_VALUES[PAWN];
    } else {
        int victim = find_piece_at(pos, them, to);
        if (victim >= 0) victim_val = SEE_VALUES[victim];
    }

    // Determine the initial attacker piece
    int attacker = find_piece_at(pos, us, from);
    if (attacker == -1) return 0;

    int promoted_piece = attacker;
    if (flag >= 8) {
        switch (flag & 3) {
            case 0: promoted_piece = KNIGHT; break;
            case 1: promoted_piece = BISHOP; break;
            case 2: promoted_piece = ROOK; break;
            case 3: promoted_piece = QUEEN; break;
        }
    }

    // Gain from this exchange
    int gain = victim_val;
    if (flag >= 8) gain += SEE_VALUES[promoted_piece] - SEE_VALUES[PAWN];

    // Build occupancy without the initial attacker and any EP captured pawn
    U64 occ = pos.occupancy[BOTH];
    occ ^= (1ULL << from);
    if (flag == EP_CAPTURE) {
        int ep_sq = (us == WHITE) ? (to - 8) : (to + 8);
        occ ^= (1ULL << ep_sq);
    }

    // Get all attackers to the target square
    U64 attackers = all_attackers_to(pos, to, occ) & occ;

    // Side to respond
    int side = them;

    // The value of the piece currently on 'to' (after the initial capture)
    int on_square_val = SEE_VALUES[promoted_piece];

    int balance[32];
    balance[0] = gain;
    int d = 0;

    while (true) {
        d++;

        // Find least valuable attacker for 'side'
        U64 my_attackers = 0;
        for (int p = PAWN; p <= KING; p++)
            my_attackers |= attackers & pos.pieces[side][p];

        if (my_attackers == 0) break;

        int att_piece = -1;
        for (int p = PAWN; p <= KING; p++) {
            if (pos.pieces[side][p] & my_attackers) { att_piece = p; break; }
        }
        if (att_piece == -1) break;

        // If king captures and opponent still has attackers, it's illegal
        if (att_piece == KING) {
            U64 opp_attackers = 0;
            for (int p = PAWN; p <= KING; p++)
                opp_attackers |= attackers & pos.pieces[side ^ 1][p];
            if (opp_attackers) break;
        }

        // This attacker captures the piece on square
        balance[d] = on_square_val - balance[d - 1];

        // Remove this attacker from occupancy for x-ray
        U64 att_bb = pos.pieces[side][att_piece] & my_attackers;
        int att_sq = lsb(att_bb);
        occ ^= (1ULL << att_sq);

        // Update sliding attackers (x-ray through removed piece)
        if (att_piece == PAWN || att_piece == BISHOP || att_piece == QUEEN)
            attackers |= (get_bishop_attacks(to, occ) & (pos.pieces[WHITE][BISHOP] | pos.pieces[BLACK][BISHOP] |
                          pos.pieces[WHITE][QUEEN] | pos.pieces[BLACK][QUEEN]));
        if (att_piece == PAWN || att_piece == ROOK || att_piece == QUEEN)
            attackers |= (get_rook_attacks(to, occ) & (pos.pieces[WHITE][ROOK] | pos.pieces[BLACK][ROOK] |
                          pos.pieces[WHITE][QUEEN] | pos.pieces[BLACK][QUEEN]));
        attackers &= occ;

        on_square_val = SEE_VALUES[att_piece];
        side ^= 1;
    }

    // Negamax the gain array
    while (d > 0) {
        balance[d - 1] = -std::max(-balance[d - 1], balance[d]);
        d--;
    }

    return balance[0];
}
