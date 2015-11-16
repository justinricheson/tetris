#include "graphics.h"

// 4 bit bitmap (each pixel represented by 4 bits in row major order).
// Header is not used so was removed
const unsigned char block[] =
{
    0xFF, 0xFF,
    0xF5, 0x5F, // Shade center of block
    0xF5, 0x5F,
    0xFF, 0xFF
};

const unsigned char clearblock[] =
{
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00
};

// Shape definitions
const int SD_O[4] = { 1, 1, 1, 1 };
const int SD_I[16] = { 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0 };
const int SD_S[9] = { 0, 1, 1, 1, 1, 0, 0, 0, 0 };
const int SD_Z[9] = { 1, 1, 0, 0, 1, 1, 0, 0, 0 };
const int SD_L[9] = { 1, 0, 0, 1, 0, 0, 1, 1, 0 };
const int SD_J[9] = { 0, 1, 0, 0, 1, 0, 1, 1, 0 };
const int SD_T[9] = { 1, 1, 1, 0, 1, 0, 0, 0, 0 };
