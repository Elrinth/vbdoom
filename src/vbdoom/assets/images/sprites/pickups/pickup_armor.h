#ifndef __PICKUP_ARMOR_H__
#define __PICKUP_ARMOR_H__

/* Armor pickup (100 armor), 3 ping-pong animation frames */
/* 32x24 px, 4x3 tiles, 48 u32 words per frame */

#define PICKUP_ARMOR_FRAMES 3
#define PICKUP_ARMOR_BYTES 192

extern const unsigned int pickup_armor_000Tiles[48];
extern const unsigned int pickup_armor_001Tiles[48];
extern const unsigned int pickup_armor_002Tiles[48];

static const unsigned int* const PICKUP_ARMOR_FRAME_TABLE[3] = {
    pickup_armor_000Tiles,
    pickup_armor_001Tiles,
    pickup_armor_002Tiles
};

#endif
