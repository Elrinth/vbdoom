#ifndef __PICKUP_SHELLS_H__
#define __PICKUP_SHELLS_H__

/* Shotgun shells box (+20 shells), 1 static frame */
/* 32x24 px, 4x3 tiles, 48 u32 words per frame */

#define PICKUP_SHELLS_FRAMES 1
#define PICKUP_SHELLS_BYTES 192

extern const unsigned int pickup_shells_000Tiles[48];

static const unsigned int* const PICKUP_SHELLS_FRAME_TABLE[1] = {
    pickup_shells_000Tiles
};

#endif
