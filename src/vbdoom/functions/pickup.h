#ifndef _FUNCTIONS_PICKUP_H
#define _FUNCTIONS_PICKUP_H

#include <types.h>
#include <stdbool.h>

/*
 * Pickup/item system.
 * Up to MAX_PICKUPS items in the world, MAX_VISIBLE_PICKUPS rendered at once.
 * Rendering uses affine-scaled worlds (same technique as enemies but smaller).
 */

#define MAX_PICKUPS          16  /* total pickups that can exist in the world */
#define MAX_VISIBLE_PICKUPS  2   /* rendered simultaneously (1 world each) */

/* Pickup types */
#define PICKUP_AMMO_CLIP       0
#define PICKUP_HEALTH_SMALL    1
#define PICKUP_HEALTH_LARGE    2
#define PICKUP_WEAPON_SHOTGUN  3
#define PICKUP_HELMET          4  /* +1 armor */
#define PICKUP_ARMOR           5  /* 100 armor */
#define PICKUP_SHELLS          6  /* +20 shotgun shells */
#define PICKUP_WEAPON_ROCKET   7  /* rocket launcher pickup */
#define PICKUP_KEY_RED         8
#define PICKUP_KEY_YELLOW      9
#define PICKUP_KEY_BLUE        10
#define PICKUP_WEAPON_CHAINGUN 11

/* Gameplay constants */
#define PICKUP_RADIUS        80   /* collision radius (8.8 fixed-point units) */
#define AMMO_CLIP_AMOUNT     10   /* bullets per clip */
#define HEALTH_SMALL_AMOUNT  10   /* HP from small medkit */
#define HEALTH_LARGE_AMOUNT  25   /* HP from large medkit */
#define SHELLS_BOX_AMOUNT    20   /* shells per box */
#define MAX_SHELLS           50   /* max shotgun ammo */

/* Rendering constants */
#define PICKUP_CHAR_START   1303  /* after enemies: 983 + 5*64 = 1303 */
#define PICKUP_CHAR_STRIDE  12              /* 4x3 tiles = 12 chars per pickup */
#define PICKUP_BGMAP_START  8               /* BGMap(8), (9) */
#define PICKUP_TILE_W       4               /* tile grid width (32px / 8) */
#define PICKUP_TILE_H       3               /* tile grid height (24px / 8) */
#define PICKUP_PX_W         32              /* sprite width in pixels */
#define PICKUP_PX_H         24              /* sprite height in pixels */
#define PICKUP_FRAME_BYTES  192             /* 12 tiles * 16 bytes/tile */

typedef struct {
    u16  x;         /* 8.8 fixed-point position */
    u16  y;
    u8   type;      /* PICKUP_AMMO_CLIP ... PICKUP_SHELLS */
    u8   animFrame; /* current animation frame (for ping-pong pickups) */
    u8   animTimer; /* counts frames for animation */
    bool active;
    u8   respawnTimer; /* >0 = item collected, counts down to respawn (DM only) */
} Pickup;

extern Pickup g_pickups[MAX_PICKUPS];

/* Screen flash state (shared with gameLoop for palette override) */
extern u8 g_flashTimer;   /* >0 = flash active, counts down each frame */
extern u8 g_flashType;    /* 0 = pickup (bright), 1 = damage (red tint) */

/* Weapon pickup flag: set to weapon ID when a weapon is picked up (0 = none) */
extern u8 g_pickedUpWeapon;

/* Item tracking (for level stats screen) */
extern u8 g_itemsCollected;
extern u8 g_totalItems;

/* ---- API ---- */

/* Initialize pickups: place items on the map. Call once at level start. */
void initPickups(void);
void initPickupsE1M2(void);
void initPickupsE1M3(void);
void initPickupsE1M4(void);

/* Spawn a pickup at a position (e.g., ammo drop from dead enemy).
 * Returns true if spawned, false if no free slot. */
bool spawnPickup(u8 type, u16 x, u16 y);

/* Check player collision with pickups.
 * Modifies ammo/health/armor directly. Triggers flash on pickup.
 * Call once per frame. Returns true if something was picked up. */
bool updatePickups(u16 playerX, u16 playerY, u8 *ammo, u8 *health,
                   u16 *armor, u8 *armorType, u8 *shellAmmo);

/* Animate pickups: advance ping-pong animation for helmet/armor. Call per frame. */
void animatePickups(void);

/* Get the actual animation frame index (ping-pong resolved) */
u8 getPickupAnimFrame(Pickup *p);

#endif
