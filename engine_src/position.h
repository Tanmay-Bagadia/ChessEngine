#pragma once
#include "bitboard.h"
#include <string>

using Move = uint16_t;

enum Color {
    WHITE = 0,
    BLACK = 1,
    BOTH = 2
};

enum PieceType {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5
};

enum Castling {
    WK = 1,
    WQ = 2,
    BK = 4,
    BQ = 8
};

constexpr const char *START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr const char *KIWIPETE_FEN = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";

// Stores all reversible state for unmake_move
struct UndoInfo {
    int castling_rights;
    int en_passant_square;
    int halfmove_clock;
    U64 hash;
    Move move;
    int captured_piece; // -1 if no capture
};

struct Position {
    U64 pieces[2][6];       // pieces[Color][PieceType]
    U64 occupancy[3];       // occupancy[WHITE], occupancy[BLACK], occupancy[BOTH]

    U64 hash;

    int side_to_move;
    int castling_rights;
    int en_passant_square;
    int halfmove_clock;
    int fullmove_number;

    // Incremental material + PST scores
    int mg_score[2];
    int eg_score[2];
    int game_phase;

    // Incremental NNUE accumulator (768 -> 256 per perspective)
    alignas(64) int16_t nnue_acc[2][256];
    bool nnue_acc_valid;
};

bool make_move(Position &pos, Move move, UndoInfo &undo);
void unmake_move(Position &pos, Move move, const UndoInfo &undo);

// Legacy copy-based make_move for perft and UCI compatibility
bool make_move(Position &pos, Move move);

void init_position(Position &pos);
void load_fen(Position &pos, const char *fen);
std::string export_fen(const Position &pos);
void print_board(Position &pos);

// Find piece type at a given square for a given color
inline int find_piece_at(const Position &pos, int color, int sq) {
    U64 bit = 1ULL << sq;
    for (int p = 0; p < 6; p++)
        if (pos.pieces[color][p] & bit)
            return p;
    return -1;
}

// Check if side to move has non-pawn material
inline bool has_non_pawn_material(const Position &pos, int color) {
    return (pos.pieces[color][KNIGHT] | pos.pieces[color][BISHOP] |
            pos.pieces[color][ROOK] | pos.pieces[color][QUEEN]) != 0;
}
