#include "movegen.h"
#include "bitboard.h"
#include <vector>

inline void serialize_pawn_moves(U64 board, int offset, int flag, std::vector<Move> &move_list)
{

    while (board > 0)
    {
        int to = __builtin_ctzll(board);
        move_list.push_back(encode_move(to - offset, to, flag));
        clear_bit(board, to);
    }
}

const U64 FILE_A = 0x0101010101010101ULL;
const U64 FILE_B = 0x0202020202020202ULL;
const U64 FILE_G = 0x4040404040404040ULL;
const U64 FILE_H = 0x8080808080808080ULL;
const U64 RANK_4 = 0xFF000000ULL;

U64 knight_attacks[64];
U64 king_attacks[64];

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