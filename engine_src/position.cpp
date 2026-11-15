#include "bitboard.h"
#include "position.h"
#include "movegen.h"
#include "zobrist.h"
#include "evaluation.h"
#include <iostream>
#include <sstream>

// Castling update mask: AND with current rights when piece leaves/arrives at this square
static const int castling_mask[64] = {
    ~WQ & 15, 15, 15, 15, ~(WK|WQ) & 15, 15, 15, ~WK & 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    ~BQ & 15, 15, 15, 15, ~(BK|BQ) & 15, 15, 15, ~BK & 15,
};

// ---------- Helpers for incremental eval updates ----------

static inline void add_piece(Position &pos, int color, int piece, int sq) {
    set_bit(pos.pieces[color][piece], sq);
    pos.occupancy[color] |= 1ULL << sq;
    pos.occupancy[BOTH] |= 1ULL << sq;
    pos.hash ^= piece_keys[color][piece][sq];
    pos.mg_score[color] += get_mg_value(color, piece, sq);
    pos.eg_score[color] += get_eg_value(color, piece, sq);
    pos.game_phase += get_phase_weight(piece);
}

static inline void remove_piece(Position &pos, int color, int piece, int sq) {
    clear_bit(pos.pieces[color][piece], sq);
    pos.occupancy[color] &= ~(1ULL << sq);
    pos.occupancy[BOTH] &= ~(1ULL << sq);
    pos.hash ^= piece_keys[color][piece][sq];
    pos.mg_score[color] -= get_mg_value(color, piece, sq);
    pos.eg_score[color] -= get_eg_value(color, piece, sq);
    pos.game_phase -= get_phase_weight(piece);
}

static inline void move_piece(Position &pos, int color, int piece, int from, int to) {
    U64 fromto = (1ULL << from) | (1ULL << to);
    pos.pieces[color][piece] ^= fromto;
    pos.occupancy[color] ^= fromto;
    pos.occupancy[BOTH] ^= fromto;
    pos.hash ^= piece_keys[color][piece][from] ^ piece_keys[color][piece][to];
    pos.mg_score[color] += get_mg_value(color, piece, to) - get_mg_value(color, piece, from);
    pos.eg_score[color] += get_eg_value(color, piece, to) - get_eg_value(color, piece, from);
}

// ---------- Init / FEN ----------

void init_position(Position &pos) {
    load_fen(pos, START_FEN);
}

void load_fen(Position &pos, const char *fen) {
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++)
            pos.pieces[c][p] = 0ULL;
    pos.occupancy[WHITE] = pos.occupancy[BLACK] = pos.occupancy[BOTH] = 0ULL;
    pos.side_to_move = WHITE;
    pos.castling_rights = 0;
    pos.en_passant_square = -1;
    pos.halfmove_clock = 0;
    pos.fullmove_number = 1;
    pos.mg_score[0] = pos.mg_score[1] = 0;
    pos.eg_score[0] = pos.eg_score[1] = 0;
    pos.game_phase = 0;
    pos.hash = 0;

    std::istringstream ss(fen);
    std::string piece_placement, active_color, castling, ep_square;
    int halfmove = 0, fullmove = 1;

    if (!(ss >> piece_placement)) return;

    int rank = 7, file = 0;
    for (char c : piece_placement) {
        if (c == '/') { rank--; file = 0; }
        else if (c >= '1' && c <= '8') { file += (c - '0'); }
        else {
            int color = isupper(c) ? WHITE : BLACK;
            char upper = toupper(c);
            int piece_type = -1;
            switch (upper) {
                case 'P': piece_type = PAWN; break;
                case 'N': piece_type = KNIGHT; break;
                case 'B': piece_type = BISHOP; break;
                case 'R': piece_type = ROOK; break;
                case 'Q': piece_type = QUEEN; break;
                case 'K': piece_type = KING; break;
            }
            if (piece_type != -1 && rank >= 0 && rank < 8 && file >= 0 && file < 8) {
                int sq = rank * 8 + file;
                add_piece(pos, color, piece_type, sq);
            }
            file++;
        }
    }

    pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];

    if (ss >> active_color)
        pos.side_to_move = (active_color == "b") ? BLACK : WHITE;

    if (ss >> castling) {
        for (char c : castling) {
            if (c == 'K') pos.castling_rights |= WK;
            else if (c == 'Q') pos.castling_rights |= WQ;
            else if (c == 'k') pos.castling_rights |= BK;
            else if (c == 'q') pos.castling_rights |= BQ;
        }
    }

    if (ss >> ep_square && ep_square != "-" && ep_square.length() >= 2) {
        int ep_file = ep_square[0] - 'a';
        int ep_rank = ep_square[1] - '1';
        if (ep_file >= 0 && ep_file < 8 && ep_rank >= 0 && ep_rank < 8)
            pos.en_passant_square = ep_rank * 8 + ep_file;
    }

    if (ss >> halfmove) pos.halfmove_clock = halfmove;
    if (ss >> fullmove) pos.fullmove_number = fullmove;

    if (pos.game_phase > 24) pos.game_phase = 24;

    pos.hash = compute_hash(pos);
}

void print_board(Position &pos) {
    const char piece_chars[2][6] = {
        {'P', 'N', 'B', 'R', 'Q', 'K'},
        {'p', 'n', 'b', 'r', 'q', 'k'}
    };
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            char ch = '.';
            for (int c = 0; c < 2; c++)
                for (int p = 0; p < 6; p++)
                    if (pos.pieces[c][p] & (1ULL << sq))
                        ch = piece_chars[c][p];
            std::cout << ch << " ";
        }
        std::cout << "\n";
    }
    std::cout << "FEN: " << export_fen(pos) << "\n";
    std::cout << "Hash: " << std::hex << pos.hash << std::dec << "\n";
}

std::string export_fen(const Position &pos) {
    const char piece_chars[2][6] = {
        {'P', 'N', 'B', 'R', 'Q', 'K'},
        {'p', 'n', 'b', 'r', 'q', 'k'}
    };
    std::string fen;
    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            char pc = 0;
            for (int c = 0; c < 2; c++)
                for (int p = 0; p < 6; p++)
                    if (pos.pieces[c][p] & (1ULL << sq))
                        pc = piece_chars[c][p];
            if (pc) {
                if (empty > 0) { fen += std::to_string(empty); empty = 0; }
                fen += pc;
            } else {
                empty++;
            }
        }
        if (empty > 0) fen += std::to_string(empty);
        if (rank > 0) fen += '/';
    }

    fen += (pos.side_to_move == WHITE) ? " w " : " b ";
    std::string cr;
    if (pos.castling_rights & WK) cr += 'K';
    if (pos.castling_rights & WQ) cr += 'Q';
    if (pos.castling_rights & BK) cr += 'k';
    if (pos.castling_rights & BQ) cr += 'q';
    if (cr.empty()) cr = "-";
    fen += cr + " ";

    if (pos.en_passant_square != -1) {
        fen += (char)('a' + pos.en_passant_square % 8);
        fen += (char)('1' + pos.en_passant_square / 8);
    } else {
        fen += "-";
    }
    fen += " " + std::to_string(pos.halfmove_clock) + " " + std::to_string(pos.fullmove_number);
    return fen;
}

// ---------- make_move (with undo) ----------

bool make_move(Position &pos, Move move, UndoInfo &undo) {
    int from = get_from(move);
    int to = get_to(move);
    int flag = get_flag(move);
    int us = pos.side_to_move;
    int them = us ^ 1;

    // Find moving piece
    int piece = find_piece_at(pos, us, from);
    if (piece == -1) return false;

    // Save undo state
    undo.castling_rights = pos.castling_rights;
    undo.en_passant_square = pos.en_passant_square;
    undo.halfmove_clock = pos.halfmove_clock;
    undo.hash = pos.hash;
    undo.move = move;
    undo.captured_piece = -1;

    // Remove old en passant key
    if (pos.en_passant_square != -1)
        pos.hash ^= ep_keys[pos.en_passant_square % 8];

    // Remove old castling key
    pos.hash ^= castle_keys[pos.castling_rights];

    // Handle captures
    int captured_piece = -1;
    int captured_sq = to;

    if (flag == EP_CAPTURE) {
        captured_piece = PAWN;
        captured_sq = (us == WHITE) ? (to - 8) : (to + 8);
        remove_piece(pos, them, PAWN, captured_sq);
        undo.captured_piece = PAWN;
    }
    else if (flag & 4) { // CAPTURE or PROMOTION_CAPTURE
        captured_piece = find_piece_at(pos, them, to);
        if (captured_piece != -1) {
            remove_piece(pos, them, captured_piece, to);
            undo.captured_piece = captured_piece;
        }
    }

    // Move the piece
    remove_piece(pos, us, piece, from);

    // Handle promotions
    if (flag >= 8) {
        int promo_piece;
        switch (flag & 3) {
            case 0: promo_piece = KNIGHT; break;
            case 1: promo_piece = BISHOP; break;
            case 2: promo_piece = ROOK; break;
            case 3: promo_piece = QUEEN; break;
            default: promo_piece = QUEEN; break;
        }
        add_piece(pos, us, promo_piece, to);
    } else {
        add_piece(pos, us, piece, to);
    }

    // Handle castling rook movement
    if (flag == K_CASTLE) {
        if (us == WHITE) {
            move_piece(pos, WHITE, ROOK, 7, 5);
        } else {
            move_piece(pos, BLACK, ROOK, 63, 61);
        }
    } else if (flag == Q_CASTLE) {
        if (us == WHITE) {
            move_piece(pos, WHITE, ROOK, 0, 3);
        } else {
            move_piece(pos, BLACK, ROOK, 56, 59);
        }
    }

    // Update en passant square
    if (flag == DOUBLE_PUSH) {
        pos.en_passant_square = (us == WHITE) ? (to - 8) : (to + 8);
    } else {
        pos.en_passant_square = -1;
    }

    // Update castling rights
    pos.castling_rights &= castling_mask[from];
    pos.castling_rights &= castling_mask[to];

    // Add back hash keys
    if (pos.en_passant_square != -1)
        pos.hash ^= ep_keys[pos.en_passant_square % 8];
    pos.hash ^= castle_keys[pos.castling_rights];

    // Recompute BOTH occupancy (fast with OR of all piece bitboards)
    pos.occupancy[WHITE] = 0;
    pos.occupancy[BLACK] = 0;
    for (int p = 0; p < 6; p++) {
        pos.occupancy[WHITE] |= pos.pieces[WHITE][p];
        pos.occupancy[BLACK] |= pos.pieces[BLACK][p];
    }
    pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];

    // Check legality: our king must not be in check
    int king_sq = lsb(pos.pieces[us][KING]);
    if (is_square_attacked(king_sq, them, pos)) {
        int piece_on_to = find_piece_at(pos, us, to);
        if (flag >= 8) {
            if (piece_on_to != -1)
                remove_piece(pos, us, piece_on_to, to);
            add_piece(pos, us, PAWN, from);
        } else {
            if (piece_on_to != -1)
                move_piece(pos, us, piece_on_to, to, from);
        }

        if (flag == EP_CAPTURE) {
            int cap_sq = (us == WHITE) ? (to - 8) : (to + 8);
            add_piece(pos, them, PAWN, cap_sq);
        } else if (undo.captured_piece != -1) {
            add_piece(pos, them, undo.captured_piece, to);
        }

        if (flag == K_CASTLE) {
            if (us == WHITE) move_piece(pos, WHITE, ROOK, 5, 7);
            else move_piece(pos, BLACK, ROOK, 61, 63);
        } else if (flag == Q_CASTLE) {
            if (us == WHITE) move_piece(pos, WHITE, ROOK, 3, 0);
            else move_piece(pos, BLACK, ROOK, 59, 56);
        }

        pos.castling_rights = undo.castling_rights;
        pos.en_passant_square = undo.en_passant_square;
        pos.halfmove_clock = undo.halfmove_clock;
        pos.hash = undo.hash;

        pos.occupancy[WHITE] = 0;
        pos.occupancy[BLACK] = 0;
        for (int p = 0; p < 6; p++) {
            pos.occupancy[WHITE] |= pos.pieces[WHITE][p];
            pos.occupancy[BLACK] |= pos.pieces[BLACK][p];
        }
        pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];

        return false;
    }

    // Switch side
    pos.side_to_move ^= 1;
    pos.hash ^= side_key;

    // Update clocks
    if (piece == PAWN || captured_piece != -1)
        pos.halfmove_clock = 0;
    else
        pos.halfmove_clock++;

    if (us == BLACK)
        pos.fullmove_number++;

    return true;
}

void unmake_move(Position &pos, Move move, const UndoInfo &undo) {
    int from = get_from(move);
    int to = get_to(move);
    int flag = get_flag(move);

    // Switch side back
    pos.side_to_move ^= 1;
    int us = pos.side_to_move;
    int them = us ^ 1;

    // Determine what piece is now on 'to' square
    int piece_on_to = find_piece_at(pos, us, to);

    // Handle promotion: remove promoted piece from 'to', put pawn back on 'from'
    if (flag >= 8) {
        if (piece_on_to != -1)
            remove_piece(pos, us, piece_on_to, to);
        add_piece(pos, us, PAWN, from);
    } else {
        // Normal move: move piece back
        if (piece_on_to != -1)
            move_piece(pos, us, piece_on_to, to, from);
    }

    // Restore captured piece
    if (flag == EP_CAPTURE) {
        int cap_sq = (us == WHITE) ? (to - 8) : (to + 8);
        add_piece(pos, them, PAWN, cap_sq);
    } else if (undo.captured_piece != -1) {
        add_piece(pos, them, undo.captured_piece, to);
    }

    // Undo castling rook movement
    if (flag == K_CASTLE) {
        if (us == WHITE) move_piece(pos, WHITE, ROOK, 5, 7);
        else move_piece(pos, BLACK, ROOK, 61, 63);
    } else if (flag == Q_CASTLE) {
        if (us == WHITE) move_piece(pos, WHITE, ROOK, 3, 0);
        else move_piece(pos, BLACK, ROOK, 59, 56);
    }

    // Restore state from undo
    pos.castling_rights = undo.castling_rights;
    pos.en_passant_square = undo.en_passant_square;
    pos.halfmove_clock = undo.halfmove_clock;
    pos.hash = undo.hash;

    if (us == BLACK)
        pos.fullmove_number--;

    // Recompute occupancy
    pos.occupancy[WHITE] = 0;
    pos.occupancy[BLACK] = 0;
    for (int p = 0; p < 6; p++) {
        pos.occupancy[WHITE] |= pos.pieces[WHITE][p];
        pos.occupancy[BLACK] |= pos.pieces[BLACK][p];
    }
    pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];
}

// ---------- Legacy copy-based make_move ----------

bool make_move(Position &pos, Move move) {
    UndoInfo undo;
    return make_move(pos, move, undo);
}
