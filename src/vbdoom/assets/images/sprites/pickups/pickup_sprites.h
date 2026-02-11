#ifndef __PICKUP_SPRITES_H__
#define __PICKUP_SPRITES_H__

/*
 * Pickup sprite tile data
 * Tile grid: 32x24 px (4x3 tiles)
 * Tiles per frame: 12
 * Bytes per frame: 192
 */

/* Tile size constants defined in pickup.h */

extern const unsigned int pickup_ammo_clipTiles[48];
extern const unsigned int pickup_health_largeTiles[48];
extern const unsigned int pickup_health_smallTiles[48];
extern const unsigned int pickup_shotgunTiles[48];

/* New pickup types */
#include "pickup_helmet.h"
#include "pickup_armor.h"
#include "pickup_shells.h"
#include "pickup_rocket.h"

/* Indexed by pickup type: 0=clip, 1=health_sm, 2=health_lg, 3=shotgun,
 * 4=helmet, 5=armor, 6=shells, 7=rocket, 8=key_red, 9=key_yellow, 10=key_blue
 * Keys use placeholder art (ammo clip) until key sprites are added. */
static const unsigned int* const PICKUP_TILES[11] = {
    pickup_ammo_clipTiles,
    pickup_health_smallTiles,
    pickup_health_largeTiles,
    pickup_shotgunTiles,
    pickup_helmet_000Tiles,
    pickup_armor_000Tiles,
    pickup_shells_000Tiles,
    pickup_rocketTiles,
    pickup_ammo_clipTiles,     /* key red placeholder */
    pickup_health_smallTiles,  /* key yellow placeholder */
    pickup_health_largeTiles   /* key blue placeholder */
};

#endif /* __PICKUP_SPRITES_H__ */
