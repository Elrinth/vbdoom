#ifndef __PICKUP_KEYCARD_H__
#define __PICKUP_KEYCARD_H__

#define KEYCARD_ANIM_FRAMES 6

extern const unsigned int pickup_keycard_000Tiles[48];
extern const unsigned int pickup_keycard_001Tiles[48];
extern const unsigned int pickup_keycard_002Tiles[48];
extern const unsigned int pickup_keycard_003Tiles[48];
extern const unsigned int pickup_keycard_004Tiles[48];
extern const unsigned int pickup_keycard_005Tiles[48];

/* Frame table for keycard animation */
static const unsigned int* const PICKUP_KEYCARD_FRAME_TABLE[6] = {
    pickup_keycard_000Tiles,
    pickup_keycard_001Tiles,
    pickup_keycard_002Tiles,
    pickup_keycard_003Tiles,
    pickup_keycard_004Tiles,
    pickup_keycard_005Tiles
};

#endif
