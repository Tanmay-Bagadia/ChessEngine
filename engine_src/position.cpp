#include "bitboard.h"
#include "position.h"
#include <iostream>

void init_position(Position &pos)
{
    // WHITE PIECES
    pos.white_pawns = 0xFF00ULL;
    pos.white_knights = 0x42ULL;
    pos.white_bishops = 0x24ULL;
    pos.white_rooks = 0x81ULL;
    pos.white_queens = 0x8ULL;
    pos.white_king = 0x10ULL;

    // BLACK PIECES
    pos.black_pawns = 0xFF000000000000ULL;
    pos.black_knights = 0x4200000000000000ULL;
    pos.black_bishops = 0x2400000000000000ULL;
    pos.black_rooks = 0x8100000000000000ULL;
    pos.black_queens = 0x800000000000000ULL;
    pos.black_king = 0x1000000000000000ULL;

    // OCCUPANCIES
    pos.white_occupancy = pos.white_bishops | pos.white_king | pos.white_knights | pos.white_pawns | pos.white_queens | pos.white_rooks;

    pos.black_occupancy = pos.black_bishops | pos.black_king | pos.black_knights | pos.black_pawns | pos.black_queens | pos.black_rooks;

    pos.master_occupancy = pos.white_occupancy | pos.black_occupancy;
}

void print_board(Position &pos)
{
    for (int rank = 7; rank >= 0; rank--)
    {
        for (int file = 0; file < 8; file++)
        {
            int square_index = (rank * 8) + file;

            // WHITE PIECES
            if (get_bit(pos.white_pawns, square_index))
                std::cout << "P";
            else if (get_bit(pos.white_knights, square_index))
                std::cout << "N";
            else if (get_bit(pos.white_bishops, square_index))
                std::cout << "B";
            else if (get_bit(pos.white_rooks, square_index))
                std::cout << "R";
            else if (get_bit(pos.white_queens, square_index))
                std::cout << "Q";
            else if (get_bit(pos.white_king, square_index))
                std::cout << "K";

            // BLACK PIECES
            else if (get_bit(pos.black_pawns, square_index))
                std::cout << "p";
            else if (get_bit(pos.black_knights, square_index))
                std::cout << "n";
            else if (get_bit(pos.black_bishops, square_index))
                std::cout << "b";
            else if (get_bit(pos.black_rooks, square_index))
                std::cout << "r";
            else if (get_bit(pos.black_queens, square_index))
                std::cout << "q";
            else if (get_bit(pos.black_king, square_index))
                std::cout << "k";
            else
                std::cout << ".";
        }
        std::cout << "\n";
    }
}