#include "bitboard.h"
#include "position.h"
#include <iostream>

void init_position(Position &pos)
{
    // Clear all boards first
    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 6; p++) {
            pos.pieces[c][p] = 0ULL;
        }
    }

    // WHITE PIECES
    pos.pieces[WHITE][PAWN]   = 0xFF00ULL;
    pos.pieces[WHITE][KNIGHT] = 0x42ULL;
    pos.pieces[WHITE][BISHOP] = 0x24ULL;
    pos.pieces[WHITE][ROOK]   = 0x81ULL;
    pos.pieces[WHITE][QUEEN]  = 0x8ULL;
    pos.pieces[WHITE][KING]   = 0x10ULL;

    // BLACK PIECES
    pos.pieces[BLACK][PAWN]   = 0xFF000000000000ULL;
    pos.pieces[BLACK][KNIGHT] = 0x4200000000000000ULL;
    pos.pieces[BLACK][BISHOP] = 0x2400000000000000ULL;
    pos.pieces[BLACK][ROOK]   = 0x8100000000000000ULL;
    pos.pieces[BLACK][QUEEN]  = 0x800000000000000ULL;
    pos.pieces[BLACK][KING]   = 0x1000000000000000ULL;

    // OCCUPANCIES
    pos.occupancy[WHITE] = 0ULL;
    pos.occupancy[BLACK] = 0ULL;
    for (int p = 0; p < 6; p++) {
        pos.occupancy[WHITE] |= pos.pieces[WHITE][p];
        pos.occupancy[BLACK] |= pos.pieces[BLACK][p];
    }
    pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];

    // GAME STATE
    pos.side_to_move = WHITE;
    pos.castling_rights = WK | WQ | BK | BQ;
    pos.en_passant_square = -1;
}

void print_board(Position &pos)
{
    const char piece_chars[2][6] = {
        {'P', 'N', 'B', 'R', 'Q', 'K'},
        {'p', 'n', 'b', 'r', 'q', 'k'}
    };

    for (int rank = 7; rank >= 0; rank--)
    {
        for (int file = 0; file < 8; file++)
        {
            int square_index = (rank * 8) + file;
            char sq_char = '.';

            for (int c = 0; c < 2; c++) {
                for (int p = 0; p < 6; p++) {
                    if (get_bit(pos.pieces[c][p], square_index)) {
                        sq_char = piece_chars[c][p];
                        break;
                    }
                }
                if (sq_char != '.') break;
            }

            std::cout << sq_char << " ";
        }
        std::cout << "\n";
    }
}