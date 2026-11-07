#include "movegen.h"
#include "bitboard.h"
#include <vector>
#include <iostream>

inline void serialize_pawn_moves(U64 board, int offset, int flag, MoveList &move_list)
{
    while (board > 0)
    {
        int to = pop_lsb(board);
        move_list.push_back(encode_move(to - offset, to, flag));
    }
}

inline void serialize_promotions(U64 board, int offset, int flag, MoveList &move_list)
{
    while (board > 0)
    {
        int to = pop_lsb(board);
        int from = to - offset;

        if (flag == CAPTURE)
        {
            move_list.push_back(encode_move(from, to, Q_PROMO_CAP));
            move_list.push_back(encode_move(from, to, R_PROMO_CAP));
            move_list.push_back(encode_move(from, to, B_PROMO_CAP));
            move_list.push_back(encode_move(from, to, N_PROMO_CAP));
        }
        else
        {
            move_list.push_back(encode_move(from, to, Q_PROMO));
            move_list.push_back(encode_move(from, to, R_PROMO));
            move_list.push_back(encode_move(from, to, B_PROMO));
            move_list.push_back(encode_move(from, to, N_PROMO));
        }
    }
}

U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask)
{
    U64 occupancy = 0ULL;

    for (int i = 0; i < bits_in_mask; i++)
    {
        int square = __builtin_ctzll(attack_mask);
        attack_mask &= attack_mask - 1;

        if (index & (1 << i))
        {
            occupancy |= 1ULL << square;
        }
    }

    return occupancy;
}

U64 bishop_attacks_on_the_fly(int square, U64 block)
{
    int file = square % 8;
    int rank = square / 8;

    U64 attacks = 0ULL;

    for (int curr_rank = rank + 1, curr_file = file + 1; curr_rank <= 7 && curr_file <= 7; curr_rank++, curr_file++)
    {
        attacks |= 1ULL << (curr_rank * 8 + curr_file);

        if (block & (1ULL << (curr_rank * 8 + curr_file)))
        {
            break;
        }
    }

    for (int curr_rank = rank - 1, curr_file = file + 1; curr_rank >= 0 && curr_file <= 7; curr_rank--, curr_file++)
    {
        attacks |= 1ULL << (curr_rank * 8 + curr_file);
        if (block & (1ULL << (curr_rank * 8 + curr_file)))
        {
            break;
        }
    }

    for (int curr_rank = rank + 1, curr_file = file - 1; curr_rank <= 7 && curr_file >= 0; curr_rank++, curr_file--)
    {
        attacks |= 1ULL << (curr_rank * 8 + curr_file);
        if (block & (1ULL << (curr_rank * 8 + curr_file)))
        {
            break;
        }
    }

    for (int curr_rank = rank - 1, curr_file = file - 1; curr_rank >= 0 && curr_file >= 0; curr_rank--, curr_file--)
    {
        attacks |= 1ULL << (curr_rank * 8 + curr_file);
        if (block & (1ULL << (curr_rank * 8 + curr_file)))
        {
            break;
        }
    }
    return attacks;
}

U64 rook_attacks_on_the_fly(int square, U64 block)
{
    int file = square % 8;
    int rank = square / 8;

    U64 attacks = 0ULL;

    for (int curr_rank = rank + 1; curr_rank <= 7; curr_rank++)
    {
        attacks |= 1ULL << (curr_rank * 8 + file);
        if (block & (1ULL << (curr_rank * 8 + file)))
        {
            break;
        }
    }

    for (int curr_rank = rank - 1; curr_rank >= 0; curr_rank--)
    {
        attacks |= 1ULL << (curr_rank * 8 + file);
        if (block & (1ULL << (curr_rank * 8 + file)))
        {
            break;
        }
    }

    for (int curr_file = file + 1; curr_file <= 7; curr_file++)
    {
        attacks |= 1ULL << (rank * 8 + curr_file);
        if (block & (1ULL << (rank * 8 + curr_file)))
        {
            break;
        }
    }

    for (int curr_file = file - 1; curr_file >= 0; curr_file--)
    {
        attacks |= 1ULL << (rank * 8 + curr_file);
        if (block & (1ULL << (rank * 8 + curr_file)))
        {
            break;
        }
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

void generate_moves(Position &pos, MoveList &move_list)
{
    int side = pos.side_to_move;
    int enemy = side ^ 1;

    // Pawn moves
    U64 pawns = pos.pieces[side][PAWN];

    if (pos.en_passant_square != -1)
    {
        int ep_sq = pos.en_passant_square;

        if (side == WHITE)
        {
            U64 ep_board = (1ULL << ep_sq);

            U64 attackers_east = (ep_board >> 9) & pawns & ~FILE_H;
            U64 attackers_west = (ep_board >> 7) & pawns & ~FILE_A;

            if (attackers_east != 0)
            {
                move_list.push_back(encode_move(ep_sq - 9, ep_sq, EP_CAPTURE));
            }
            if (attackers_west != 0)
            {
                move_list.push_back(encode_move(ep_sq - 7, ep_sq, EP_CAPTURE));
            }
        }
        else
        {
            U64 ep_board = (1ULL << ep_sq);

            U64 attackers_east = (ep_board << 7) & pawns & ~FILE_H;
            U64 attackers_west = (ep_board << 9) & pawns & ~FILE_A;

            if (attackers_east != 0)
            {
                move_list.push_back(encode_move(ep_sq + 7, ep_sq, EP_CAPTURE));
            }
            if (attackers_west != 0)
            {
                move_list.push_back(encode_move(ep_sq + 9, ep_sq, EP_CAPTURE));
            }
        }
    }

    if (side == WHITE)
    {
        U64 valid_single_pushes = (pawns << 8) & ~pos.occupancy[BOTH];
        U64 promo_pushes = valid_single_pushes & RANK_8;
        U64 non_promo_pushes = valid_single_pushes & ~RANK_8;

        U64 valid_double_pushes = (valid_single_pushes << 8) & ~pos.occupancy[BOTH] & RANK_4;

        U64 valid_capture_east = (pawns << 9) & ~FILE_A & pos.occupancy[BLACK];
        U64 promo_captures_east = valid_capture_east & RANK_8;
        U64 non_promo_captures_east = valid_capture_east & ~RANK_8;

        U64 valid_capture_west = (pawns << 7) & ~FILE_H & pos.occupancy[BLACK];
        U64 promo_captures_west = valid_capture_west & RANK_8;
        U64 non_promo_captures_west = valid_capture_west & ~RANK_8;

        serialize_pawn_moves(non_promo_pushes, 8, QUIET, move_list);
        serialize_promotions(promo_pushes, 8, QUIET, move_list);

        serialize_pawn_moves(valid_double_pushes, 16, DOUBLE_PUSH, move_list);

        serialize_pawn_moves(non_promo_captures_east, 9, CAPTURE, move_list);
        serialize_promotions(promo_captures_east, 9, CAPTURE, move_list);

        serialize_pawn_moves(non_promo_captures_west, 7, CAPTURE, move_list);
        serialize_promotions(promo_captures_west, 7, CAPTURE, move_list);
    }
    else
    {
        U64 valid_single_pushes = (pawns >> 8) & ~pos.occupancy[BOTH];
        U64 promo_pushes = valid_single_pushes & RANK_1;
        U64 non_promo_pushes = valid_single_pushes & ~RANK_1;

        U64 valid_double_pushes = (valid_single_pushes >> 8) & ~pos.occupancy[BOTH] & RANK_5;

        U64 valid_capture_east = (pawns >> 7) & ~FILE_A & pos.occupancy[WHITE];
        U64 promo_captures_east = valid_capture_east & RANK_1;
        U64 non_promo_captures_east = valid_capture_east & ~RANK_1;

        U64 valid_capture_west = (pawns >> 9) & ~FILE_H & pos.occupancy[WHITE];
        U64 promo_captures_west = valid_capture_west & RANK_1;
        U64 non_promo_captures_west = valid_capture_west & ~RANK_1;

        serialize_pawn_moves(non_promo_pushes, -8, QUIET, move_list);
        serialize_promotions(promo_pushes, -8, QUIET, move_list);

        serialize_pawn_moves(valid_double_pushes, -16, DOUBLE_PUSH, move_list);

        serialize_pawn_moves(non_promo_captures_east, -7, CAPTURE, move_list);
        serialize_promotions(promo_captures_east, -7, CAPTURE, move_list);

        serialize_pawn_moves(non_promo_captures_west, -9, CAPTURE, move_list);
        serialize_promotions(promo_captures_west, -9, CAPTURE, move_list);
    }

    // Knight moves
    U64 knights = pos.pieces[side][KNIGHT];
    while (knights != 0)
    {
        int from_square = pop_lsb(knights);
        U64 attacks = knight_attacks[from_square] & ~pos.occupancy[side];

        while (attacks != 0)
        {
            int to_square = pop_lsb(attacks);

            if (pos.occupancy[enemy] & (1ULL << to_square))
            {
                move_list.push_back(encode_move(from_square, to_square, CAPTURE));
            }
            else
            {
                move_list.push_back(encode_move(from_square, to_square, QUIET));
            }
        }
    }

    // King moves
    U64 king = pos.pieces[side][KING];
    while (king != 0)
    {
        int from_square = pop_lsb(king);
        U64 attacks = king_attacks[from_square] & ~pos.occupancy[side];

        while (attacks != 0)
        {
            int to_square = pop_lsb(attacks);

            if (pos.occupancy[enemy] & (1ULL << to_square))
            {
                move_list.push_back(encode_move(from_square, to_square, CAPTURE));
            }
            else
            {
                move_list.push_back(encode_move(from_square, to_square, QUIET));
            }
        }
    }

    // Bishop moves
    U64 bishops = pos.pieces[side][BISHOP];
    while (bishops != 0)
    {
        int from_square = pop_lsb(bishops);
        U64 attacks = get_bishop_attacks(from_square, pos.occupancy[BOTH]) & ~pos.occupancy[side];

        while (attacks != 0)
        {
            int to_square = pop_lsb(attacks);

            if (pos.occupancy[enemy] & (1ULL << to_square))
            {
                move_list.push_back(encode_move(from_square, to_square, CAPTURE));
            }
            else
            {
                move_list.push_back(encode_move(from_square, to_square, QUIET));
            }
        }
    }

    // Rook moves
    U64 rooks = pos.pieces[side][ROOK];
    while (rooks != 0)
    {
        int from_square = pop_lsb(rooks);
        U64 attacks = get_rook_attacks(from_square, pos.occupancy[BOTH]) & ~pos.occupancy[side];

        while (attacks != 0)
        {
            int to_square = pop_lsb(attacks);

            if (pos.occupancy[enemy] & (1ULL << to_square))
            {
                move_list.push_back(encode_move(from_square, to_square, CAPTURE));
            }
            else
            {
                move_list.push_back(encode_move(from_square, to_square, QUIET));
            }
        }
    }

    // Queen moves
    U64 queens = pos.pieces[side][QUEEN];
    while (queens != 0)
    {
        int from_square = pop_lsb(queens);
        U64 attacks = get_queen_attacks(from_square, pos.occupancy[BOTH]) & ~pos.occupancy[side];

        while (attacks != 0)
        {
            int to_square = pop_lsb(attacks);

            if (pos.occupancy[enemy] & (1ULL << to_square))
            {
                move_list.push_back(encode_move(from_square, to_square, CAPTURE));
            }
            else
            {
                move_list.push_back(encode_move(from_square, to_square, QUIET));
            }
        }
    }

    // Castling
    if (side == WHITE)
    {
        if (pos.castling_rights & WK)
        {
            if (!(pos.occupancy[BOTH] & ((1ULL << 5) | (1ULL << 6))))
            {
                if (!is_square_attacked(4, BLACK, pos) && !is_square_attacked(5, BLACK, pos))
                {
                    move_list.push_back(encode_move(4, 6, K_CASTLE));
                }
            }
        }
        if (pos.castling_rights & WQ)
        {
            if (!(pos.occupancy[BOTH] & ((1ULL << 1) | (1ULL << 2) | (1ULL << 3))))
            {
                if (!is_square_attacked(4, BLACK, pos) && !is_square_attacked(3, BLACK, pos))
                {
                    move_list.push_back(encode_move(4, 2, Q_CASTLE));
                }
            }
        }
    }
    else
    {
        if (pos.castling_rights & BK)
        {
            if (!(pos.occupancy[BOTH] & ((1ULL << 61) | (1ULL << 62))))
            {
                if (!is_square_attacked(60, WHITE, pos) && !is_square_attacked(61, WHITE, pos))
                {
                    move_list.push_back(encode_move(60, 62, K_CASTLE));
                }
            }
        }
        if (pos.castling_rights & BQ)
        {
            if (!(pos.occupancy[BOTH] & ((1ULL << 57) | (1ULL << 58) | (1ULL << 59))))
            {
                if (!is_square_attacked(60, WHITE, pos) && !is_square_attacked(59, WHITE, pos))
                {
                    move_list.push_back(encode_move(60, 58, Q_CASTLE));
                }
            }
        }
    }
}

void init_leapers()
{
    for (int square = 0; square < 64; square++)
    {
        U64 knight = 1ULL << square;

        U64 knight_NNE = (knight << 17) & ~FILE_A;
        U64 knight_EEN = (knight << 10) & ~(FILE_A | FILE_B);
        U64 knight_EES = (knight >> 6) & ~(FILE_A | FILE_B);
        U64 knight_SSE = (knight >> 15) & ~FILE_A;

        U64 knight_NNW = (knight << 15) & ~FILE_H;
        U64 knight_WWN = (knight << 6) & ~(FILE_G | FILE_H);
        U64 knight_WWS = (knight >> 10) & ~(FILE_G | FILE_H);
        U64 knight_SSW = (knight >> 17) & ~FILE_H;

        knight_attacks[square] = knight_NNE | knight_EEN | knight_EES | knight_SSE |
                                 knight_NNW | knight_WWN | knight_WWS | knight_SSW;

        U64 king = 1ULL << square;

        U64 jump_N = (king << 8);
        U64 jump_S = (king >> 8);
        U64 jump_E = (king << 1) & ~FILE_A;
        U64 jump_NE = (king << 9) & ~FILE_A;
        U64 jump_SE = (king >> 7) & ~FILE_A;
        U64 jump_W = (king >> 1) & ~FILE_H;
        U64 jump_NW = (king << 7) & ~FILE_H;
        U64 jump_SW = (king >> 9) & ~FILE_H;

        king_attacks[square] = jump_N | jump_S | jump_E | jump_W | jump_NE | jump_NW | jump_SE | jump_SW;

        U64 pawn = 1ULL << square;
        pawn_attacks[WHITE][square] = ((pawn << 9) & ~FILE_A) | ((pawn << 7) & ~FILE_H);
        pawn_attacks[BLACK][square] = ((pawn >> 7) & ~FILE_A) | ((pawn >> 9) & ~FILE_H);
    }
}

U64 mask_rook_attacks(int square)
{
    int file = square % 8;
    int rank = square / 8;

    U64 mask = 0ULL;

    for (int curr_rank = rank + 1; curr_rank < 7; curr_rank++)
    {
        mask |= 1ULL << (curr_rank * 8 + file);
    }

    for (int curr_rank = rank - 1; curr_rank > 0; curr_rank--)
    {
        mask |= 1ULL << (curr_rank * 8 + file);
    }

    for (int curr_file = file + 1; curr_file < 7; curr_file++)
    {
        mask |= 1ULL << (rank * 8 + curr_file);
    }

    for (int curr_file = file - 1; curr_file > 0; curr_file--)
    {
        mask |= 1ULL << (rank * 8 + curr_file);
    }

    return mask;
}
U64 mask_bishop_attacks(int square)
{
    int file = square % 8;
    int rank = square / 8;

    U64 mask = 0ULL;

    for (int curr_rank = rank + 1, curr_file = file + 1; curr_rank < 7 && curr_file < 7; curr_rank++, curr_file++)
    {
        mask |= 1ULL << (curr_rank * 8 + curr_file);
    }

    for (int curr_rank = rank - 1, curr_file = file + 1; curr_rank > 0 && curr_file < 7; curr_rank--, curr_file++)
    {
        mask |= 1ULL << (curr_rank * 8 + curr_file);
    }

    for (int curr_rank = rank + 1, curr_file = file - 1; curr_rank < 7 && curr_file > 0; curr_rank++, curr_file--)
    {
        mask |= 1ULL << (curr_rank * 8 + curr_file);
    }

    for (int curr_rank = rank - 1, curr_file = file - 1; curr_rank > 0 && curr_file > 0; curr_rank--, curr_file--)
    {
        mask |= 1ULL << (curr_rank * 8 + curr_file);
    }

    return mask;
}

static unsigned int random_state = 1804289383;

static unsigned int random_u32()
{
    unsigned int number = random_state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    random_state = number;
    return number;
}

static U64 random_u64()
{
    U64 num1 = (U64)(random_u32()) & 0xFFFF;
    U64 num2 = (U64)(random_u32()) & 0xFFFF;
    U64 num3 = (U64)(random_u32()) & 0xFFFF;
    U64 num4 = (U64)(random_u32()) & 0xFFFF;

    return num1 | (num2 << 16) | (num3 << 32) | (num4 << 48);
}

static U64 random_magic_candidate()
{
    return random_u64() & random_u64() & random_u64();
}

U64 rook_magic[64];
U64 bishop_magic[64];

U64 find_magic_number(int square, bool is_bishop)
{
    U64 mask = is_bishop ? bishop_masks[square] : rook_masks[square];
    int relevant_bits = __builtin_popcountll(mask);
    int occupancy_cnt = 1 << relevant_bits;

    U64 occupancies[4096];
    U64 attacks[4096];

    for (int i = 0; i < occupancy_cnt; i++)
    {
        occupancies[i] = set_occupancy(i, relevant_bits, mask);
        attacks[i] = is_bishop ? bishop_attacks_on_the_fly(square, occupancies[i])
                               : rook_attacks_on_the_fly(square, occupancies[i]);
    }

    for (int trials = 0; trials < 100000000; trials++)
    {
        U64 candidate = random_magic_candidate();

        if (__builtin_popcountll((mask * candidate) & 0xFF00000000000000ULL) < 6)
        {
            continue;
        }

        U64 used[4096] = {};
        bool failed = false;

        for (int i = 0; i < occupancy_cnt; i++)
        {
            int index = (int)((occupancies[i] * candidate) >> (64 - relevant_bits));

            if (used[index] == 0ULL)
            {
                used[index] = attacks[i];
            }
            else if (used[index] != attacks[i])
            {
                failed = true;
                break;
            }
        }

        if (!failed)
        {
            return candidate;
        }
    }

    std::cout << "ERROR: no magic candidate found for square: " << square << "\n";
    return 0ULL;
}

void init_sliders()
{
    for (int square = 0; square < 64; square++)
    {

        rook_masks[square] = mask_rook_attacks(square);
        bishop_masks[square] = mask_bishop_attacks(square);

        rook_magic[square] = find_magic_number(square, false);
        bishop_magic[square] = find_magic_number(square, true);

        int rook_bits = __builtin_popcountll(rook_masks[square]);
        for (int i = 0; i < (1 << rook_bits); i++)
        {
            U64 occupancy = set_occupancy(i, rook_bits, rook_masks[square]);
            int index = (int)((occupancy * rook_magic[square]) >> (64 - rook_bits));
            rook_attacks[square][index] = rook_attacks_on_the_fly(square, occupancy);
        }

        int bishop_bits = __builtin_popcountll(bishop_masks[square]);
        for (int i = 0; i < (1 << bishop_bits); i++)
        {
            U64 occupancy = set_occupancy(i, bishop_bits, bishop_masks[square]);
            int index = (int)((occupancy * bishop_magic[square]) >> (64 - bishop_bits));
            bishop_attacks[square][index] = bishop_attacks_on_the_fly(square, occupancy);
        }
    }
}

bool is_square_attacked(int square, int attacking_side, Position &pos)
{
    if (pawn_attacks[attacking_side ^ 1][square] & pos.pieces[attacking_side][PAWN])
    {
        return true;
    }

    if (knight_attacks[square] & pos.pieces[attacking_side][KNIGHT])
    {
        return true;
    }

    if (get_bishop_attacks(square, pos.occupancy[BOTH]) & pos.pieces[attacking_side][BISHOP])
    {
        return true;
    }

    if (get_rook_attacks(square, pos.occupancy[BOTH]) & pos.pieces[attacking_side][ROOK])
    {
        return true;
    }

    if (get_queen_attacks(square, pos.occupancy[BOTH]) & pos.pieces[attacking_side][QUEEN])
    {
        return true;
    }

    if (king_attacks[square] & pos.pieces[attacking_side][KING])
    {
        return true;
    }

    return false;
}