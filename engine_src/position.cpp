#include "bitboard.h"
#include "position.h"
#include "movegen.h"
#include <iostream>
#include <sstream>

void init_position(Position &pos)
{
    // Clear all boards first
    for (int c = 0; c < 2; c++)
    {
        for (int p = 0; p < 6; p++)
        {
            pos.pieces[c][p] = 0ULL;
        }
    }

    // WHITE PIECES
    pos.pieces[WHITE][PAWN] = 0xFF00ULL;
    pos.pieces[WHITE][KNIGHT] = 0x42ULL;
    pos.pieces[WHITE][BISHOP] = 0x24ULL;
    pos.pieces[WHITE][ROOK] = 0x81ULL;
    pos.pieces[WHITE][QUEEN] = 0x8ULL;
    pos.pieces[WHITE][KING] = 0x10ULL;

    // BLACK PIECES
    pos.pieces[BLACK][PAWN] = 0xFF000000000000ULL;
    pos.pieces[BLACK][KNIGHT] = 0x4200000000000000ULL;
    pos.pieces[BLACK][BISHOP] = 0x2400000000000000ULL;
    pos.pieces[BLACK][ROOK] = 0x8100000000000000ULL;
    pos.pieces[BLACK][QUEEN] = 0x800000000000000ULL;
    pos.pieces[BLACK][KING] = 0x1000000000000000ULL;

    // OCCUPANCIES
    pos.occupancy[WHITE] = 0ULL;
    pos.occupancy[BLACK] = 0ULL;
    for (int p = 0; p < 6; p++)
    {
        pos.occupancy[WHITE] |= pos.pieces[WHITE][p];
        pos.occupancy[BLACK] |= pos.pieces[BLACK][p];
    }
    pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];

    // GAME STATE
    pos.side_to_move = WHITE;
    pos.castling_rights = WK | WQ | BK | BQ;
    pos.en_passant_square = -1;
    pos.halfmove_clock = 0;
    pos.fullmove_number = 1;
}

void print_board(Position &pos)
{
    const char piece_chars[2][6] = {
        {'P', 'N', 'B', 'R', 'Q', 'K'},
        {'p', 'n', 'b', 'r', 'q', 'k'}};

    for (int rank = 7; rank >= 0; rank--)
    {
        for (int file = 0; file < 8; file++)
        {
            int square_index = (rank * 8) + file;
            char sq_char = '.';

            for (int c = 0; c < 2; c++)
            {
                for (int p = 0; p < 6; p++)
                {
                    if (get_bit(pos.pieces[c][p], square_index))
                    {
                        sq_char = piece_chars[c][p];
                        break;
                    }
                }
                if (sq_char != '.')
                    break;
            }

            std::cout << sq_char << " ";
        }
        std::cout << "\n";
    }
}

void load_fen(Position &pos, const char *fen)
{
    // Clear all piece bitboards
    for (int c = 0; c < 2; c++)
    {
        for (int p = 0; p < 6; p++)
        {
            pos.pieces[c][p] = 0ULL;
        }
    }
    pos.occupancy[WHITE] = pos.occupancy[BLACK] = pos.occupancy[BOTH] = 0ULL;
    pos.side_to_move = WHITE;
    pos.castling_rights = 0;
    pos.en_passant_square = -1;
    pos.halfmove_clock = 0;
    pos.fullmove_number = 1;

    std::istringstream ss(fen);
    std::string piece_placement, active_color, castling, ep_square;
    int halfmove = 0, fullmove = 1;

    if (!(ss >> piece_placement))
        return;

    // 1. Piece placement (ranks 8 down to 1, files a to h)
    int rank = 7;
    int file = 0;

    for (char c : piece_placement)
    {
        if (c == '/')
        {
            rank--;
            file = 0;
        }
        else if (c >= '1' && c <= '8')
        {
            file += (c - '0');
        }
        else
        {
            int color = isupper(c) ? WHITE : BLACK;
            char upper = toupper(c);
            int piece_type = -1;

            switch (upper)
            {
            case 'P': piece_type = PAWN; break;
            case 'N': piece_type = KNIGHT; break;
            case 'B': piece_type = BISHOP; break;
            case 'R': piece_type = ROOK; break;
            case 'Q': piece_type = QUEEN; break;
            case 'K': piece_type = KING; break;
            }

            if (piece_type != -1 && rank >= 0 && rank < 8 && file >= 0 && file < 8)
            {
                int square = (rank * 8) + file;
                set_bit(pos.pieces[color][piece_type], square);
            }
            file++;
        }
    }

    // Populate occupancies
    for (int p = 0; p < 6; p++)
    {
        pos.occupancy[WHITE] |= pos.pieces[WHITE][p];
        pos.occupancy[BLACK] |= pos.pieces[BLACK][p];
    }
    pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];

    // 2. Active color
    if (ss >> active_color)
    {
        pos.side_to_move = (active_color == "b") ? BLACK : WHITE;
    }

    // 3. Castling rights
    if (ss >> castling)
    {
        for (char c : castling)
        {
            if (c == 'K') pos.castling_rights |= WK;
            else if (c == 'Q') pos.castling_rights |= WQ;
            else if (c == 'k') pos.castling_rights |= BK;
            else if (c == 'q') pos.castling_rights |= BQ;
        }
    }

    // 4. En passant target square
    if (ss >> ep_square)
    {
        if (ep_square != "-" && ep_square.length() >= 2)
        {
            int ep_file = ep_square[0] - 'a';
            int ep_rank = ep_square[1] - '1';
            if (ep_file >= 0 && ep_file < 8 && ep_rank >= 0 && ep_rank < 8)
            {
                pos.en_passant_square = (ep_rank * 8) + ep_file;
            }
        }
    }

    // 5. Halfmove clock
    if (ss >> halfmove)
    {
        pos.halfmove_clock = halfmove;
    }

    // 6. Fullmove number
    if (ss >> fullmove)
    {
        pos.fullmove_number = fullmove;
    }
}

// 64-element array mapping square index -> castling rights to keep.
// When a king or rook moves (or a rook is captured on its corner), rights &= castling_rights_mask[sq].
// Bitmask values: WK = 1, WQ = 2, BK = 4, BQ = 8 (Total = 15)
static const int castling_rights_mask[64] = {
    13, 15, 15, 15, 12, 15, 15, 14, // 0 - 7   (a1: 13 [~WQ], e1: 12 [~(WK|WQ)], h1: 14 [~WK])
    15, 15, 15, 15, 15, 15, 15, 15, // 8 - 15
    15, 15, 15, 15, 15, 15, 15, 15, // 16 - 23
    15, 15, 15, 15, 15, 15, 15, 15, // 24 - 31
    15, 15, 15, 15, 15, 15, 15, 15, // 32 - 39
    15, 15, 15, 15, 15, 15, 15, 15, // 40 - 47
    15, 15, 15, 15, 15, 15, 15, 15, // 48 - 55
     7, 15, 15, 15,  3, 15, 15, 11  // 56 - 63 (a8: 7 [~BQ],  e8: 3 [~(BK|BQ)],   h8: 11 [~BK])
};

bool make_move(Position &pos, Move move)
{
    int from = get_from(move);
    int to = get_to(move);
    int flag = get_flag(move);
    int side = pos.side_to_move;
    int enemy = side ^ 1;
    int piece_type = -1;

    for (int i = 0; i < 6; i++)
    {
        if (pos.pieces[side][i] & (1ULL << from))
        {
            piece_type = i;
            break;
        }
    }

    clear_bit(pos.pieces[side][piece_type], from);

    if (flag & 4)
    {
        int captured_piece = -1;
        int captured_square = to;
        for (int i = 0; i < 6; i++)
        {
            if (pos.pieces[enemy][i] & (1ULL << to))
            {
                captured_piece = i;
                break;
            }
        }

        // En passant: the captured pawn is not on the destination square.
        if (captured_piece == -1)
        {
            captured_piece = PAWN;
            captured_square = (side == WHITE) ? (to - 8) : (to + 8);
        }

        clear_bit(pos.pieces[enemy][captured_piece], captured_square);
    }

    // 4. PLACE PIECE AT DESTINATION
    // If promotion: add the PROMOTED piece type, not the pawn
    //   flag 8→Knight, 9→Bishop, 10→Rook, 11→Queen (and 12-15 for capture variants)
    // Otherwise: set_bit(pos.pieces[side][piece_type], to)
    if (flag >= 8)
    {
        int promoted_piece = -1;

        switch (flag & 3)
        {
        case 0:
            promoted_piece = KNIGHT;
            break;
        case 1:
            promoted_piece = BISHOP;
            break;
        case 2:
            promoted_piece = ROOK;
            break;
        case 3:
            promoted_piece = QUEEN;
            break;
        }

        set_bit(pos.pieces[side][promoted_piece], to);
    }
    else
    {
        set_bit(pos.pieces[side][piece_type], to);
    }

    // 5. HANDLE CASTLING ROOK
    // K_CASTLE (white): move rook from h1(7) to f1(5)
    // Q_CASTLE (white): move rook from a1(0) to d1(3)
    // K_CASTLE (black): move rook from h8(63) to f8(61)
    // Q_CASTLE (black): move rook from a8(56) to d8(59)

    // 6. UPDATE EN PASSANT SQUARE
    // If DOUBLE_PUSH: set en_passant to the skipped square (from + 8 for white, from - 8 for black)
    // Otherwise: set to -1

    // 7. UPDATE CASTLING RIGHTS
    // Use a simple trick — a 64-element table mapping square → rights to KEEP:
    //   castling_rights &= castling_update[from]
    //   castling_rights &= castling_update[to]
    // e.g., if from == 4 (e1), clear WK|WQ → keep BK|BQ only
    //        if from == 0 (a1), clear WQ → keep WK|BK|BQ
    //        if to == 63 (h8), clear BK (rook captured!) → keep WK|WQ|BQ
    //        all other squares → keep everything (0xF)

    // 8. RECOMPUTE OCCUPANCIES
    // pos.occupancy[WHITE] = all white pieces OR'd together
    // pos.occupancy[BLACK] = all black pieces OR'd together
    // pos.occupancy[BOTH]  = pos.occupancy[WHITE] | pos.occupancy[BLACK]

    // 9. LEGALITY CHECK
    // Find our king square: __builtin_ctzll(pos.pieces[side][KING])
    // If is_square_attacked(king_sq, enemy, pos) → return false (illegal!)

    // 10. SWITCH SIDE
    // pos.side_to_move ^= 1

    return true; // legal move
}