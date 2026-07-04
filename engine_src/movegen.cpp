#include "movegen.h"
#include "bitboard.h"
#include <vector>
#include <iostream>

inline void serialize_pawn_moves(U64 board, int offset, int flag, std::vector<Move> &move_list)
{

    while (board > 0)
    {
        int to = __builtin_ctzll(board);
        move_list.push_back(encode_move(to - offset, to, flag));
        clear_bit(board, to);
    }
}

U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask)
{

    U64 occupancy = 0ULL;

    for (int i = 0; i < bits_in_mask; i++)
    {
        int square = __builtin_ctzll(attack_mask);

        attack_mask &= attack_mask - 1;

        // Is the i-th bit of the index turned on?
        if (index & (1 << i))
        {
            occupancy |= 1Ull << square;
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
const U64 RANK_4 = 0xFF000000ULL;

// // Number of relevant bits in each rook mask (by square, a1=0, h8=63)
// const int rook_relevant_bits[64] = {
//     12, 11, 11, 11, 11, 11, 11, 12,
//     11, 10, 10, 10, 10, 10, 10, 11,
//     11, 10, 10, 10, 10, 10, 10, 11,
//     11, 10, 10, 10, 10, 10, 10, 11,
//     11, 10, 10, 10, 10, 10, 10, 11,
//     11, 10, 10, 10, 10, 10, 10, 11,
//     11, 10, 10, 10, 10, 10, 10, 11,
//     12, 11, 11, 11, 11, 11, 11, 12};

// // Number of relevant bits in each bishop mask
// const int bishop_relevant_bits[64] = {
//     6, 5, 5, 5, 5, 5, 5, 6,
//     5, 5, 5, 5, 5, 5, 5, 5,
//     5, 5, 7, 7, 7, 7, 5, 5,
//     5, 5, 7, 9, 9, 7, 5, 5,
//     5, 5, 7, 9, 9, 7, 5, 5,
//     5, 5, 7, 7, 7, 7, 5, 5,
//     5, 5, 5, 5, 5, 5, 5, 5,
//     6, 5, 5, 5, 5, 5, 5, 6};

U64 knight_attacks[64];
U64 king_attacks[64];
U64 rook_attacks[64][4096];  // 64 squares, max 4096 realities (12 bits)
U64 bishop_attacks[64][512]; // 64 squares, max 512 realities (9 bits)
U64 rook_masks[64];
U64 bishop_masks[64];

void generate_moves(Position &pos, std::vector<Move> &move_list)
{
    // PAWNS
    U64 valid_single_pushes = (pos.white_pawns << 8) & ~pos.master_occupancy;
    U64 valid_double_pushes = (valid_single_pushes << 8) & ~pos.master_occupancy & RANK_4;

    // prevention  of warping of pawns
    U64 valid_capture_east = (pos.white_pawns << 9) & ~FILE_A & pos.black_occupancy;
    U64 valid_capture_west = (pos.white_pawns << 7) & ~FILE_H & pos.black_occupancy;

    serialize_pawn_moves(valid_single_pushes, 8, 0, move_list);
    serialize_pawn_moves(valid_double_pushes, 16, 1, move_list);
    serialize_pawn_moves(valid_capture_east, 9, 4, move_list);
    serialize_pawn_moves(valid_capture_west, 7, 4, move_list);
}

void init_leapers()
{
    for (int square = 0; square < 64; square++)
    {
        // KNIGHT
        U64 knight = 1ULL << square;

        // EAST JUMPS
        U64 knight_NNE = (knight << 17) & ~FILE_A;            // Up 2, Right 1
        U64 knight_EEN = (knight << 10) & ~(FILE_A | FILE_B); // Up 1, Right 2
        U64 knight_EES = (knight >> 6) & ~(FILE_A | FILE_B);  // Down 1, Right 2
        U64 knight_SSE = (knight >> 15) & ~FILE_A;            // Down 2, Right 1

        // WEST JUMPS
        U64 knight_NNW = (knight << 15) & ~FILE_H;            // Up 2, Left 1
        U64 knight_WWN = (knight << 6) & ~(FILE_G | FILE_H);  // Up 1, Left 2
        U64 knight_WWS = (knight >> 10) & ~(FILE_G | FILE_H); // Down 1, Left 2
        U64 knight_SSW = (knight >> 17) & ~FILE_H;            // Down 2, Left 1

        knight_attacks[square] = knight_NNE | knight_EEN | knight_EES | knight_SSE | knight_NNW | knight_WWN | knight_WWS | knight_SSW;

        // KING
        U64 king = 1ULL << square;

        // VERTICAL JUMPS
        U64 jump_N = (king << 8);
        U64 jump_S = (king >> 8);

        // EAST JUMPS
        U64 jump_E = (king << 1) & ~FILE_A;
        U64 jump_NE = (king << 9) & ~FILE_A;
        U64 jump_SE = (king >> 7) & ~FILE_A;

        // WEST JUMPS
        U64 jump_W = (king >> 1) & ~FILE_H;
        U64 jump_NW = (king << 7) & ~FILE_H;
        U64 jump_SW = (king >> 9) & ~FILE_H;

        king_attacks[square] = jump_N | jump_S | jump_E | jump_W | jump_NE | jump_NW | jump_SE | jump_SW;
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

unsigned int random_state = 1804289383; // seed for generating pseudo-random numbers

unsigned int random_u32()
{
    // Xorshift algorithm by George Marsaglia - proven to produce good random numbers
    unsigned int number = random_state;

    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;

    random_state = number; // scramble the randome state to produce new number
    return number;
}

U64 random_u64()
{
    U64 num1 = (U64)(random_u32()) & 0xFFFF; // 0xFFFF  is 1111111111111111, this mask only keeps the lowest 16 bits
    U64 num2 = (U64)(random_u32()) & 0xFFFF; // 0xFFFF  is 1111111111111111, this mask only keeps the lowest 16 bits
    U64 num3 = (U64)(random_u32()) & 0xFFFF; // 0xFFFF  is 1111111111111111, this mask only keeps the lowest 16 bits
    U64 num4 = (U64)(random_u32()) & 0xFFFF; // 0xFFFF  is 1111111111111111, this mask only keeps the lowest 16 bits

    return num1 | num2 << 16 | num3 << 32 | num4 << 48;
}

U64 random_magic_candidate()
{
    return random_u64() & random_u64() & random_u64();
}

U64 rook_magic[64];
U64 bishop_magic[64];

U64 find_magic_number(int square, bool is_bishop)
{
    U64 mask = is_bishop ? bishop_masks[square] : rook_masks[square];
    int relevant_bits = __builtin_popcountll(mask);
    int occupancy_cnt = 1 << relevant_bits; // total no. of blocker arrangements

    U64 occupancies[4096];
    U64 attacks[4096];

    for (int i = 0; i < occupancy_cnt; i++)
    {
        occupancies[i] = set_occupancy(i, relevant_bits, mask);
        attacks[i] = is_bishop ? bishop_attacks_on_the_fly(square, occupancies[i]) : rook_attacks_on_the_fly(square, occupancies[i]);
    }

    for (int trials = 0; trials < 100000000; trials++)
    {
        U64 candidate = random_magic_candidate();

        // Quick filter: skip candidates that don't spread bits well
        if (__builtin_popcountll((mask * candidate) & 0xFF00000000000000ULL) < 6)
        {
            continue;
        }

        U64 used[4096] = {};
        bool failed = false;

        for (int i = 0; i < occupancy_cnt; i++)
        {
            // The magic formula: multiply and shift to get index
            int index = (int)((occupancies[i] * candidate) >> (64 - relevant_bits));

            if (used[index] == 0ULL)
            {
                used[index] = attacks[i];
            }
            else if (used[index] != attacks[i])
            {
                // Collision — two different attack patterns mapped to same index
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
