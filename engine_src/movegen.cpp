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
const U64 FILE_H = 0x8080808080808080ULL;
const U64 RANK_4 = 0xFF000000ULL;

void generate_moves(Position &pos, std::vector<Move> &move_list)
{

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