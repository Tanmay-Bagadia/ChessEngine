#pragma once
#include "position.h"

using Move = uint16_t;

// The bit-packing macro
inline Move encode_move(int from, int to, int flag)
{
    return (flag << 12) | (to << 6) | from;
}