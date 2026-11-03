#include "zobrist.h"
#include "position.h"

U64 piece_keys[2][6][64];
U64 castle_keys[16];
U64 ep_keys[8];
U64 side_key;

static U64 rng_state = 1070372ULL; // any non zero constant works

static U64 random_U64()
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

void init_zobrist()
{
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++)
            for (int sq = 0; sq < 64; sq++)
                piece_keys[c][p][sq] = random_U64();

    for (int i = 0; i < 16; i++)
        castle_keys[i] = random_U64();

    for (int f = 0; f < 8; f++)
        ep_keys[f] = random_U64();

    side_key = random_U64();
}

U64 compute_hash(const Position &pos)
{
    U64 h = 0ULL;

    for (int c = 0; c < 2; c++)
    {
        for (int p = 0; p < 6; p++)
        {
            U64 bb = pos.pieces[c][p];
            while (bb)
            {
                int sq = pop_lsb(bb);
                h ^= piece_keys[c][p][sq];
            }
        }
    }

    h ^= castle_keys[pos.castling_rights];

    if (pos.en_passant_square != -1)
        h ^= ep_keys[pos.en_passant_square % 8];

    if (pos.side_to_move == BLACK)
        h ^= side_key;

    return h;
}