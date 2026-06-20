#include "bitboard.h"

struct Position
{
    U64 white_pawns, white_knights, white_bishops, white_queens, white_rooks, white_king,
        black_pawns, black_knights, black_bishops, black_queens, black_rooks, black_king;

    U64 white_occupancy, black_occupancy, master_occupancy;
};

void init_position(Position &pos);

void print_board(Position &pos);