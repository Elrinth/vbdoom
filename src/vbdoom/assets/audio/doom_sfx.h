#ifndef _DOOM_SFX_H_
#define _DOOM_SFX_H_

#include <types.h>

/* Sound effect IDs */
enum {
	SFX_PUNCH = 0,
	SFX_PISTOL = 1,
	SFX_SHOTGUN = 2,
	SFX_SHOTGUN_COCK = 3,
	SFX_ITEM_UP = 4,
	SFX_PLAYER_UMF = 5,
	SFX_PLAYER_PAIN = 6,
	SFX_PLAYER_DEATH = 7,
	SFX_POSSESSED_SIGHT1 = 8,
	SFX_POSSESSED_DEATH1 = 9,
	SFX_POSSESSED_PAIN = 10,
	SFX_POSSESSED_ACTIVITY = 11,
	SFX_CLAW_ATTACK = 12,
	SFX_PROJECTILE = 13,
	SFX_PROJECTILE_CONTACT = 14,
	SFX_IMP_SIGHT1 = 15,
	SFX_IMP_DEATH1 = 16,
	SFX_IMP_ACTIVITY = 17,
	SFX_PINKY_ATTACK = 18,
	SFX_PINKY_DEATH = 19,
	SFX_PINKY_SIGHT = 20,
	SFX_DOOR_OPEN = 21,
	SFX_DOOR_CLOSE = 22,
	SFX_SWITCH_ON = 23,
	SFX_ROCKET_LAUNCH = 24,
	SFX_BARREL_EXPLODE = 25,
	SFX_ELEVATOR_STP = 26,
	SFX_STONE_MOVE = 27,
	SFX_COUNT = 28
};

/* Sound entry: pointer to packed 4-bit data + sample count */
typedef struct {
	const u8 *data;    /* packed nibble pairs */
	u16 length;        /* total samples (not bytes) */
} SFXEntry;

/* Lookup table */
extern const SFXEntry sfx_table[];

/* Individual sample arrays */
extern const u8 sfx_punch[];
extern const u8 sfx_pistol[];
extern const u8 sfx_shotgun[];
extern const u8 sfx_shotgun_cock[];
extern const u8 sfx_item_up[];
extern const u8 sfx_player_umf[];
extern const u8 sfx_player_pain[];
extern const u8 sfx_player_death[];
extern const u8 sfx_possessed_sight1[];
extern const u8 sfx_possessed_death1[];
extern const u8 sfx_possessed_pain[];
extern const u8 sfx_possessed_activity[];
extern const u8 sfx_claw_attack[];
extern const u8 sfx_projectile[];
extern const u8 sfx_projectile_contact[];
extern const u8 sfx_imp_sight1[];
extern const u8 sfx_imp_death1[];
extern const u8 sfx_imp_activity[];
extern const u8 sfx_pinky_attack[];
extern const u8 sfx_pinky_death[];
extern const u8 sfx_pinky_sight[];
extern const u8 sfx_door_open[];
extern const u8 sfx_door_close[];
extern const u8 sfx_switch_on[];
extern const u8 sfx_rocket_launch[];
extern const u8 sfx_barrel_explode[];
extern const u8 sfx_elevator_stp[];
extern const u8 sfx_stone_move[];

#endif
