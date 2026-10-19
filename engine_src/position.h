#pragma once
#include "bitboard.h"

using Move = uint16_t;

enum Color { WHITE = 0, BLACK = 1, BOTH = 2 };
enum PieceType {
  PAWN = 0,
  KNIGHT = 1,
  BISHOP = 2,
  ROOK = 3,
  QUEEN = 4,
  KING = 5
};
enum Castling { WK = 1, WQ = 2, BK = 4, BQ = 8 };

constexpr const char *START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr const char *KIWIPETE_FEN =
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";

struct Position {
  U64 pieces[2][6]; // pieces[Color][PieceType]
  U64 occupancy[3]; // occupancy[WHITE], occupancy[BLACK], occupancy[BOTH]

  int side_to_move;
  int castling_rights;
  int en_passant_square;
  int halfmove_clock;
  int fullmove_number;
};

bool make_move(Position &pos, Move move);

void init_position(Position &pos);

void load_fen(Position &pos, const char *fen);

void print_board(Position &pos);

