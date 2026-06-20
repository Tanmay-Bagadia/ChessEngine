#include "movegen.h"
#include "bitboard.h"
#include <vector>

const U64 FILE_A = 0x0101010101010101ULL;
const U64 FILE_H = 0x8080808080808080ULL;
const U64 RANK_4 = 0xFF000000ULL;

void generate_moves(Position &pos, std::vector<Move> &move_list)
{

    U64 valid_single_pushes = (pos.white_pawns << 8) & ~pos.master_occupancy;
    U64 valid_double_pushes = (valid_single_pushes << 8) & ~pos.master_occupancy & RANK_4;

    // prevent warping of pawn
    U64 valid_capture_east = (pos.white_pawns << 9) & ~FILE_A & pos.black_occupancy;
    U64 valid_capture_west = (pos.white_pawns << 7) & ~FILE_H & pos.black_occupancy;

    U64 board;

    // Single Pushes (Flag 0)
    board = valid_single_pushes;
    while (board > 0)
    {
        int to = __builtin_ctzll(board);
        move_list.push_back(encode_move(to - 8, to, 0));
        clear_bit(board, to);
    }

    // Double Pushes (Flag 1)
    board = valid_double_pushes;
    while (board > 0)
    {
        int to = __builtin_ctzll(board);
        move_list.push_back(encode_move(to - 16, to, 1));
        clear_bit(board, to);
    }

    // Captures East (Flag 4)
    board = valid_capture_east;
    while (board > 0)
    {
        int to = __builtin_ctzll(board);
        move_list.push_back(encode_move(to - 9, to, 4));
        clear_bit(board, to);
    }

    // Captures West (Flag 4)
    board = valid_capture_west;
    while (board > 0)
    {
        int to = __builtin_ctzll(board);
        move_list.push_back(encode_move(to - 7, to, 4));
        clear_bit(board, to);
    }
}