#ifndef _FUNCTIONS_PICKUP_H
#define _FUNCTIONS_PICKUP_H

#include <types.h>
#include <stdbool.h>

/*
 * Pickup/item system.
 * Up to MAX_PICKUPS items in the world, MAX_VISIBLE_PICKUPS rendered at once.
 * Rendering uses affine-scaled worlds (same technique as enemies but smaller).
 */

#define MAX_PICKUPS          8   /* total pickups that can exist in the world */
#define MAX_VISIBLE_PICKUPS  3   /* rendered simultaneously (1 world each) */

/* Pickup types (index into PICKUP_TILES[] in pickup_sprites.h) */
#define PICKUP_AMMO_CLIP       0
#define PICKUP_HEALTH_SMALL    1
#define PICKUP_HEALTH_LARGE    2
#define PICKUP_WEAPON_SHOTGUN  3

/* Gameplay constants */
#define PICKUP_RADIUS        80   /* collision radius (8.8 fixed-point units) */
#define AMMO_CLIP_AMOUNT     10   /* bullets per clip */
#define HEALTH_SMALL_AMOUNT  10   /* HP from small medkit */
#define HEALTH_LARGE_AMOUNT  25   /* HP from large medkit */

/* Rendering constants */
#define PICKUP_CHAR_START   1175  /* after enemies: 983 + 3*64 = 1175 */
#define PICKUP_CHAR_STRIDE  12              /* 4x3 tiles = 12 chars per pickup */
#define PICKUP_BGMAP_START  6               /* BGMap(6), (7), (8) */
#define PICKUP_TILE_W       4               /* tile grid width (32px / 8) */
#define PICKUP_TILE_H       3               /* tile grid height (24px / 8) */
#define PICKUP_PX_W         32              /* sprite width in pixels */
#define PICKUP_PX_H         24              /* sprite height in pixels */
#define PICKUP_FRAME_BYTES  192             /* 12 tiles * 16 bytes/tile */

typedef struct {
    u16  x;         /* 8.8 fixed-point position */
    u16  y;
    u8   type;      /* PICKUP_AMMO_CLIP, PICKUP_HEALTH_SMALL, PICKUP_HEALTH_LARGE */
    bool active;
} Pickup;

extern Pickup g_pickups[MAX_PICKUPS];

/* Screen flash state (shared with gameLoop for palette override) */
extern u8 g_flashTimer;   /* >0 = flash active, counts down each frame */
extern u8 g_flashType;    /* 0 = pickup (bright), 1 = damage (red tint) */

/* Weapon pickup flag: set to weapon ID when a weapon is picked up (0 = none) */
extern u8 g_pickedUpWeapon;

/* ---- API ---- */

/* Initialize pickups: place static health packs on the map. Call once at game start. */
void initPickups(void);

/* Spawn a pickup at a position (e.g., ammo drop from dead enemy).
 * Returns true if spawned, false if no free slot. */
bool spawnPickup(u8 type, u16 x, u16 y);

/* Check player collision with pickups.
 * Modifies *ammo and *health directly. Triggers flash on pickup.
 * Call once per frame. Returns true if something was picked up. */
bool updatePickups(u16 playerX, u16 playerY, u8 *ammo, u8 *health);

#endif
