#ifndef __FIREBALL_SPRITES_H__
#define __FIREBALL_SPRITES_H__

/*
 * Fireball sprites (32x32 px, 4x4 tiles)
 * Flight frames: 2
 * Explosion frames: 5
 * Tiles per frame: 16 (64 u32 words = 256 bytes)
 */

extern const unsigned int fireball_flight_000Tiles[64];
extern const unsigned int fireball_flight_001Tiles[64];
extern const unsigned int fireball_explode_000Tiles[64];
extern const unsigned int fireball_explode_001Tiles[64];
extern const unsigned int fireball_explode_002Tiles[64];
extern const unsigned int fireball_explode_003Tiles[64];
extern const unsigned int fireball_explode_004Tiles[64];

#define FIREBALL_FLIGHT_COUNT 2
#define FIREBALL_EXPLODE_COUNT 5
#define FIREBALL_FRAME_BYTES 256

static const unsigned int* const FIREBALL_FLIGHT_FRAMES[] = {
    fireball_flight_000Tiles,
    fireball_flight_001Tiles,
};

static const unsigned int* const FIREBALL_EXPLODE_FRAMES[] = {
    fireball_explode_000Tiles,
    fireball_explode_001Tiles,
    fireball_explode_002Tiles,
    fireball_explode_003Tiles,
    fireball_explode_004Tiles,
};

#endif
