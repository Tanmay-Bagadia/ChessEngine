#pragma once
#include "bitboard.h"

enum Color { WHITE = 0, BLACK = 1, BOTH = 2 };
enum PieceType { PAWN = 0, KNIGHT = 1, BISHOP = 2, ROOK = 3, QUEEN = 4, KING = 5 };
enum Castling { WK = 1, WQ = 2, BK = 4, BQ = 8 };

struct Position
{
    U64 pieces[2][6];   // pieces[Color][PieceType]
    U64 occupancy[3];   // occupancy[WHITE], occupancy[BLACK], occupancy[BOTH]
    
    int side_to_move;
    int castling_rights;
    int en_passant_square;
};

void init_position(Position &pos);

void print_board(Position &pos);
