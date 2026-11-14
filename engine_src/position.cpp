#include "bitboard.h"
#include "position.h"
#include "movegen.h"
#include "zobrist.h"
#include <iostream>
#include <sstream>

void init_position(Position &pos)
{
    for (int c = 0; c < 2; c++)
    {
        for (int p = 0; p < 6; p++)
        {
            pos.pieces[c][p] = 0ULL;
        }
    }

    pos.pieces[WHITE][PAWN] = 0xFF00ULL;
    pos.pieces[WHITE][KNIGHT] = 0x42ULL;
    pos.pieces[WHITE][BISHOP] = 0x24ULL;
    pos.pieces[WHITE][ROOK] = 0x81ULL;
    pos.pieces[WHITE][QUEEN] = 0x8ULL;
    pos.pieces[WHITE][KING] = 0x10ULL;

    pos.pieces[BLACK][PAWN] = 0xFF000000000000ULL;
    pos.pieces[BLACK][KNIGHT] = 0x4200000000000000ULL;
    pos.pieces[BLACK][BISHOP] = 0x2400000000000000ULL;
    pos.pieces[BLACK][ROOK] = 0x8100000000000000ULL;
    pos.pieces[BLACK][QUEEN] = 0x800000000000000ULL;
    pos.pieces[BLACK][KING] = 0x1000000000000000ULL;

    pos.occupancy[WHITE] = 0ULL;
    pos.occupancy[BLACK] = 0ULL;
    for (int p = 0; p < 6; p++)
    {
        pos.occupancy[WHITE] |= pos.pieces[WHITE][p];
        pos.occupancy[BLACK] |= pos.pieces[BLACK][p];
    }
    pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];

    pos.side_to_move = WHITE;
    pos.castling_rights = WK | WQ | BK | BQ;
    pos.en_passant_square = -1;
    pos.halfmove_clock = 0;
    pos.fullmove_number = 1;
    pos.hash = compute_hash(pos);
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
            case 'P':
                piece_type = PAWN;
                break;
            case 'N':
                piece_type = KNIGHT;
                break;
            case 'B':
                piece_type = BISHOP;
                break;
            case 'R':
                piece_type = ROOK;
                break;
            case 'Q':
                piece_type = QUEEN;
                break;
            case 'K':
                piece_type = KING;
                break;
            }

            if (piece_type != -1 && rank >= 0 && rank < 8 && file >= 0 && file < 8)
            {
                int square = (rank * 8) + file;
                set_bit(pos.pieces[color][piece_type], square);
            }
            file++;
        }
    }

    for (int p = 0; p < 6; p++)
    {
        pos.occupancy[WHITE] |= pos.pieces[WHITE][p];
        pos.occupancy[BLACK] |= pos.pieces[BLACK][p];
    }
    pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];

    if (ss >> active_color)
    {
        pos.side_to_move = (active_color == "b") ? BLACK : WHITE;
    }

    if (ss >> castling)
    {
        for (char c : castling)
        {
            if (c == 'K')
                pos.castling_rights |= WK;
            else if (c == 'Q')
                pos.castling_rights |= WQ;
            else if (c == 'k')
                pos.castling_rights |= BK;
            else if (c == 'q')
                pos.castling_rights |= BQ;
        }
    }

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

    if (ss >> halfmove)
    {
        pos.halfmove_clock = halfmove;
    }

    if (ss >> fullmove)
    {
        pos.fullmove_number = fullmove;
    }
    pos.hash = compute_hash(pos);
}

// Mask table for updating castling rights when a king or rook moves or is captured.
static const int castling_rights_mask[64] = {
    13, 15, 15, 15, 12, 15, 15, 14,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    7, 15, 15, 15, 3, 15, 15, 11};

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

    if (piece_type == -1)
        return false;

    clear_bit(pos.pieces[side][piece_type], from);
    pos.hash ^= piece_keys[side][piece_type][from];

    if (flag & 4)
    {
        int captured_piece = -1;
        int captured_square = to;

        if (flag == EP_CAPTURE)
        {
            captured_piece = PAWN;
            captured_square = (side == WHITE) ? (to - 8) : (to + 8);
        }
        else
        {
            for (int i = 0; i < 6; i++)
            {
                if (pos.pieces[enemy][i] & (1ULL << to))
                {
                    captured_piece = i;
                    break;
                }
            }
        }

        if (captured_piece != -1)
        {
            clear_bit(pos.pieces[enemy][captured_piece], captured_square);
            pos.hash ^= piece_keys[enemy][captured_piece][captured_square];
        }
    }

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
        pos.hash ^= piece_keys[side][promoted_piece][to];
    }
    else
    {
        set_bit(pos.pieces[side][piece_type], to);
        pos.hash ^= piece_keys[side][piece_type][to];
    }

    // Castling: move the corresponding rook across the king
    if (flag == K_CASTLE)
    {
        if (side == WHITE)
        {
            clear_bit(pos.pieces[WHITE][ROOK], 7);
            set_bit(pos.pieces[WHITE][ROOK], 5);
            pos.hash ^= piece_keys[WHITE][ROOK][7] ^ piece_keys[WHITE][ROOK][5];
        }
        else
        {
            clear_bit(pos.pieces[BLACK][ROOK], 63);
            set_bit(pos.pieces[BLACK][ROOK], 61);
            pos.hash ^= piece_keys[BLACK][ROOK][63] ^ piece_keys[BLACK][ROOK][61];
        }
    }
    else if (flag == Q_CASTLE)
    {
        if (side == WHITE)
        {
            clear_bit(pos.pieces[WHITE][ROOK], 0);
            set_bit(pos.pieces[WHITE][ROOK], 3);
            pos.hash ^= piece_keys[WHITE][ROOK][0] ^ piece_keys[WHITE][ROOK][3];
        }
        else
        {
            clear_bit(pos.pieces[BLACK][ROOK], 56);
            set_bit(pos.pieces[BLACK][ROOK], 59);
            pos.hash ^= piece_keys[BLACK][ROOK][56] ^ piece_keys[BLACK][ROOK][59];
        }
    }

    // Remove the old en passant key (if any) before the square changes
    if (pos.en_passant_square != -1)
        pos.hash ^= ep_keys[pos.en_passant_square % 8];

    // Double pawn push: set en passant target square behind destination
    if (flag == DOUBLE_PUSH)
    {
        pos.en_passant_square = (side == WHITE) ? (to - 8) : (to + 8);
    }
    else
    {
        pos.en_passant_square = -1;
    }
    // Add the new en passant key (if any)
    if (pos.en_passant_square != -1)
        pos.hash ^= ep_keys[pos.en_passant_square % 8];

    // Castling rights: XOR out the old combination, XOR in the new one
    pos.hash ^= castle_keys[pos.castling_rights];
    pos.castling_rights &= castling_rights_mask[from];
    pos.castling_rights &= castling_rights_mask[to];
    pos.hash ^= castle_keys[pos.castling_rights];

    pos.occupancy[WHITE] = 0ULL;
    pos.occupancy[BLACK] = 0ULL;
    for (int p = 0; p < 6; p++)
    {
        pos.occupancy[WHITE] |= pos.pieces[WHITE][p];
        pos.occupancy[BLACK] |= pos.pieces[BLACK][p];
    }
    pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];

    int king_sq = __builtin_ctzll(pos.pieces[side][KING]);
    if (is_square_attacked(king_sq, enemy, pos))
    {
        return false;
    }

    pos.side_to_move ^= 1;
    pos.hash ^= side_key;

    if (piece_type == PAWN || (flag & 4))
        pos.halfmove_clock = 0;
    else
        pos.halfmove_clock++;

    if (side == BLACK)
        pos.fullmove_number++;

    return true;
}

std::string export_fen(const Position &pos)
{
    const char piece_chars[2][6] = {
        {'P', 'N', 'B', 'R', 'Q', 'K'},
        {'p', 'n', 'b', 'r', 'q', 'k'}};

    std::string fen = "";
    for (int rank = 7; rank >= 0; rank--)
    {
        int empty_count = 0;
        for (int file = 0; file < 8; file++)
        {
            int square = rank * 8 + file;
            char piece_char = 0;
            for (int c = 0; c < 2; c++)
            {
                for (int p = 0; p < 6; p++)
                {
                    if (pos.pieces[c][p] & (1ULL << square))
                    {
                        piece_char = piece_chars[c][p];
                        break;
                    }
                }
                if (piece_char) break;
            }

            if (piece_char)
            {
                if (empty_count > 0)
                {
                    fen += std::to_string(empty_count);
                    empty_count = 0;
                }
                fen += piece_char;
            }
            else
            {
                empty_count++;
            }
        }
        if (empty_count > 0)
            fen += std::to_string(empty_count);
        if (rank > 0)
            fen += '/';
    }

    fen += (pos.side_to_move == WHITE) ? " w " : " b ";

    std::string castling = "";
    if (pos.castling_rights & WK) castling += 'K';
    if (pos.castling_rights & WQ) castling += 'Q';
    if (pos.castling_rights & BK) castling += 'k';
    if (pos.castling_rights & BQ) castling += 'q';
    if (castling.empty()) castling = "-";
    fen += castling + " ";

    if (pos.en_passant_square != -1)
    {
        char file_char = 'a' + (pos.en_passant_square % 8);
        char rank_char = '1' + (pos.en_passant_square / 8);
        fen += file_char;
        fen += rank_char;
    }
    else
    {
        fen += "-";
    }

    fen += " " + std::to_string(pos.halfmove_clock) + " " + std::to_string(pos.fullmove_number);
    return fen;
}
