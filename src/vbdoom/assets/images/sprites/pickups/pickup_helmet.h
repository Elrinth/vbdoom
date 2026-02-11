#ifndef __PICKUP_HELMET_H__
#define __PICKUP_HELMET_H__

/* Helmet pickup (+1 armor), 4 ping-pong animation frames */
/* 32x24 px, 4x3 tiles, 48 u32 words per frame */

#define PICKUP_HELMET_FRAMES 4
#define PICKUP_HELMET_BYTES 192

extern const unsigned int pickup_helmet_000Tiles[48];
extern const unsigned int pickup_helmet_001Tiles[48];
extern const unsigned int pickup_helmet_002Tiles[48];
extern const unsigned int pickup_helmet_003Tiles[48];

static const unsigned int* const PICKUP_HELMET_FRAME_TABLE[4] = {
    pickup_helmet_000Tiles,
    pickup_helmet_001Tiles,
    pickup_helmet_002Tiles,
    pickup_helmet_003Tiles
};

#endif
