#include <functions.h>
#include <math.h>
#include <languages.h>
#include "doomgfx.h"
#include <input.h>
#include "gameLoop.h"
#include "sndplay.h"
#include "../assets/audio/doom_sfx.h"
#include "doomstage.h"
#include "RayCaster.h"
#include "RayCasterRenderer.h"
#include "RayCasterFixed.h"
#include "enemy.h"
#include "pickup.h"
#include "particle.h"
#include "timer.h"
#include "../assets/images/sprites/zombie/zombie_sprites.h"
#include "../assets/images/sprites/zombie_sgt/zombie_sgt_sprites.h"
#include "../assets/images/sprites/imp/imp_sprites.h"
#include "../assets/images/sprites/demon/demon_sprites.h"
#include "../assets/images/sprites/pickups/pickup_sprites.h"
#include "../assets/images/sprites/faces/face_sprites.h"
#include "projectile.h"
#include "door.h"
#include "intermission.h"
#include "menu_options.h"
extern BYTE FontTiles[];
#include <stdint.h>
#include <stdbool.h>

int rand(void);  /* forward declaration -- avoids pulling in full stdlib */

/* Map data (defined in RayCasterData.h, compiled via RayCasterFixed.c) */
extern u8 g_map[];
extern const u8 e1m1_map[];
extern const u8 e1m2_map[];
extern const u8 e1m3_map[];
extern const u8 e1m4_map[];

/* Player spawn positions (must match RayCasterData.h / e1m2.h) */
#define E1M1_SPAWN_X  (15 * 256 + 128)
#define E1M1_SPAWN_Y  (28 * 256 + 128)
#define E1M2_SPAWN_X  (15 * 256 + 128)
#define E1M2_SPAWN_Y  (28 * 256 + 128)
#define E1M3_SPAWN_X  (15 * 256 + 128)
#define E1M3_SPAWN_Y  (28 * 256 + 128)
/* VBDOOM_LEVEL_FORMAT 1 */
/* e1m4 spawn and level data */
#define E1M4_SPAWN_X  (30 * 256 + 128)
#define E1M4_SPAWN_Y  (30 * 256 + 128)
#define E1M4_SPAWN2_X (30 * 256 + 128)
#define E1M4_SPAWN2_Y (27 * 256 + 128)

/* Precomputed constants -- no runtime float math needed.
 * fixedAngleRatioz = FTOFIX23_9(512.0f/360.0f) = 729
 * fixed0point05 = FIX23_9_MULT(FTOFIX23_9(0.05f), 729) = 37
 * fixed2point13 = FIX23_9_MULT(FTOFIX23_9(2.13f), 729) = 1553
 */
static const s32 fixedAngleRatioz = FTOFIX23_9(512.0f/360.0f);
static const s32 fixed0point05 = FIX23_9_MULT(FTOFIX23_9(0.05f), FTOFIX23_9(512.0f/360.0f));
static const s32 fixed2point13 = FIX23_9_MULT(FTOFIX23_9(2.13f), FTOFIX23_9(512.0f/360.0f));

s16 fPlayerAng = 512;  /* face south (180 degrees) at start */
u16 fPlayerX = 3968;   /* tile (15, 28) center - start room */
u16 fPlayerY = 7296;
s16 turnRate = 10;
s16 movespeed = 40;
s16 swayY = 0;
s16 swayX = 0;
u8 weaponChangeTimer = 0;
u8 weaponChangeTime = 20;
bool isChangingWeapon = false;
bool pendingAutoSwitch = false;
bool isPlayMusicBool = true;
u16 g_levelFrames = 0;  /* frames elapsed this level (for stats screen time) */

/* Secret tracking */
u8 g_secretsFound = 0;
u8 g_totalSecrets = 0;

/* Secret sector definitions: each is a tile position (tx, ty).
 * When the player enters a secret tile for the first time, it's counted. */
#define MAX_SECRETS 4
typedef struct { u8 tx; u8 ty; bool found; } SecretSector;
SecretSector g_secrets[MAX_SECRETS];
u8 currentWeapon = 2;
u8 nextWeapon = 2;
u8 updatePistolCount = 0;

#define W_CHAINSAW 0
#define W_FISTS 1
#define W_PISTOL 2
#define W_SHOTGUN 3
#define W_ROCKET 4
typedef struct {
	bool hasWeapon;
	u8 iWeapon;
	bool requiresAmmo;
	u8 ammo;
	u8 ammoType;
	u8 attackFrames;
} Weapon;

Weapon weapons[] = {
{ false, W_CHAINSAW, false, 0,0,1},
{ true, W_FISTS, false, 0,0,4},
{ true, W_PISTOL, true, 50,1,4},   /* attackFrames=4: ~12 frame refire delay (Doom-like) */
{ false, W_SHOTGUN, true, 0,2,5},
{ false, W_ROCKET, true, 0,3,5},   /* rocket launcher: ammo type 3 = RCKT */
};

// 0 = chainsaw, 1 = fists, 2 = pistol, 3 = shotgun, 4 =
u16 ammos[] = {
	 50, 30, 0, 0
};
bool isRunning = false;
bool isMoving = false;
bool isShooting = false;
u8 weaponAnimation = 0;

u16 weaponSwayIndex = 0;
const s16 swayXTBL[] =     {0,-1,-2,-3,-4,-3,-2,-1, 0, 1, 2, 3, 4, 3, 2, 1};
const s16 swayYTBL[] =     {0, 0, 1, 2, 2, 2, 1, 0, 0, 0, 1, 2, 2, 2, 1, 0};

u16 walkSwayIndex = 0;
const s16 walkSwayYTBL[] = {0, 0, 1, 1, 1, 1, 1, 0, 0,-1,-1,-1,-1};

/*
 * Smart weapon switching: priority shotgun > pistol > fists.
 * findBestWeapon: find best weapon with ammo (priority order: 3,2,1)
 */
static u8 findBestWeapon(void) {
	/* Priority: rocket (4) > shotgun (3) > pistol (2) > fists (1) */
	if (weapons[W_ROCKET].hasWeapon && weapons[W_ROCKET].ammo > 0) return W_ROCKET;
	if (weapons[W_SHOTGUN].hasWeapon && weapons[W_SHOTGUN].ammo > 0) return W_SHOTGUN;
	if (weapons[W_PISTOL].hasWeapon && weapons[W_PISTOL].ammo > 0) return W_PISTOL;
	return W_FISTS; /* always available */
}

static void switchToWeapon(u8 newWeapon) {
	if (newWeapon == currentWeapon) return;
	currentWeapon = newWeapon;
	drawUpdatedAmmo(weapons[currentWeapon].ammo, weapons[currentWeapon].ammoType);
	drawWeaponSlotNumbers(weapons[W_PISTOL].hasWeapon,
	                     weapons[W_SHOTGUN].hasWeapon,
	                     weapons[W_ROCKET].hasWeapon,
	                     currentWeapon);
	highlightWeaponHUD(currentWeapon);
	weaponAnimation = 0;
	updatePistolCount = 0;
	weaponChangeTimer = 0;
	isShooting = false;
	isChangingWeapon = true;
}

void cycleNextWeapon() {
	u8 start = currentWeapon;
	u8 next = currentWeapon;
	do {
		next++;
		if (next > 4) next = 1; /* skip 0 (chainsaw) */
		if (weapons[next].hasWeapon &&
		    (!weapons[next].requiresAmmo || weapons[next].ammo > 0)) {
			switchToWeapon(next);
			return;
		}
	} while (next != start);
}

void cycleWeapons()
{
	if (isChangingWeapon) return;
	cycleNextWeapon();
}

/* Auto-switch when current weapon runs out of ammo */
static void autoSwitchOnEmpty(void) {
	if (weapons[currentWeapon].requiresAmmo && weapons[currentWeapon].ammo == 0) {
		u8 best = findBestWeapon();
		switchToWeapon(best);
	}
}

/* Set to 4 (or 2, 3, etc.) to start at that level for debugging. Use 1 for normal play. */
#define START_LEVEL 4

u8 currentLevel = 1;

/* Restore all game worlds, palettes, BGMaps, and VRAM tile data.
 * Called during initial gameLoop() setup and after the intermission screen
 * (which overwrites VRAM with le_map tiles). */
static void restoreGameDisplay(u8 doomface, u8 weapon) {
	u16 EnemyScale = 4;
	u16 EnemyScaleX = 6;

	/* ---- World configuration ---- */
	vbSetWorld(31, WRLD_ON|1, 0, 0, 0, 0, 0, 0, 384, 192);  /* stage background */

	/* Enemy worlds (5 slots, affine) */
	vbSetWorld(30, ENEMY_BGMAP_START|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	vbSetWorld(29, (ENEMY_BGMAP_START+1)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	vbSetWorld(28, (ENEMY_BGMAP_START+2)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	vbSetWorld(27, (ENEMY_BGMAP_START+3)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	vbSetWorld(26, (ENEMY_BGMAP_START+4)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	WORLD_PARAM(30, BGMap(ENEMY_BGMAP_START)   + 0x1000);
	WORLD_PARAM(29, BGMap(ENEMY_BGMAP_START+1) + 0x1000);
	WORLD_PARAM(28, BGMap(ENEMY_BGMAP_START+2) + 0x1000);
	WORLD_PARAM(27, BGMap(ENEMY_BGMAP_START+3) + 0x1000);
	WORLD_PARAM(26, BGMap(ENEMY_BGMAP_START+4) + 0x1000);

	/* Pickup worlds (2 slots, affine) */
	vbSetWorld(25, PICKUP_BGMAP_START|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 32*EnemyScaleX, 24*EnemyScale);
	vbSetWorld(24, (PICKUP_BGMAP_START+1)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 32*EnemyScaleX, 24*EnemyScale);
	WORLD_PARAM(25, BGMap(PICKUP_BGMAP_START)   + 0x1000);
	WORLD_PARAM(24, BGMap(PICKUP_BGMAP_START+1) + 0x1000);

	/* Particle world (affine) */
	vbSetWorld(PARTICLE_WORLD, PARTICLE_BGMAP|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 32*EnemyScaleX, 32*EnemyScale);
	WORLD_PARAM(PARTICLE_WORLD, BGMap(ENEMY_BGMAP_START) + 0x1800);

	/* Weapon + UI worlds */
	vbSetWorld(22, WRLD_ON|LAYER_WEAPON_BLACK, 0, 0, 0, 0, 0, 0, 136, 128);
	vbSetWorld(21, WRLD_ON|LAYER_WEAPON,       0, 0, 0, 0, 0, 0, 136, 128);
	vbSetWorld(20, WRLD_ON|LAYER_UI_BLACK,     0, 0, 0, 0, 0, 0, 384, 32);
	vbSetWorld(19, WRLD_ON|LAYER_UI,           0, 0, 0, 0, 0, 0, 384, 32);
	vbSetWorld(18, 0, 0, 0, 0, 0, 0, 0, 384, 32);  /* disabled: was third HUD layer (BGMap 14), caused even-tile glitch on hardware */

	WA[20].gy = 192;
	WA[19].gy = 192;
	WA[18].gy = 192;
	WA[17].head = WRLD_END;

	/* ---- Palettes ---- */
	VIP_REGS[GPLT0] = 0xE4;
	VIP_REGS[GPLT1] = 0x00;
	VIP_REGS[GPLT2] = 0x84;
	VIP_REGS[GPLT3] = 0xE4;
	VIP_REGS[JPLT0] = 0xE4;
	VIP_REGS[JPLT1] = 0xE4;
	VIP_REGS[JPLT2] = 0xE4;
	VIP_REGS[JPLT3] = 0xE4;

	/* ---- Clear all BGMaps ---- */
	{
		u8 m;
		for (m = 0; m < 14; m++) {
			setmem((void*)BGMap(m), 0, 8192);
		}
	}

	/* ---- Reload VRAM tile data ---- */
	loadDoomGfxToMem();
	loadParticleTiles();
	loadWallTextures();

	/* Enemy BGMaps + default frames */
	initEnemyBGMaps();
	loadEnemyFrame(0, Zombie_000Tiles);
	loadEnemyFrame(1, Zombie_000Tiles);
	loadEnemyFrame(2, Zombie_Sergeant_000Tiles);

	/* Pickup BGMaps + default frames */
	initPickupBGMaps();
	loadPickupFrame(0, PICKUP_TILES[PICKUP_AMMO_CLIP]);
	loadPickupFrame(1, PICKUP_TILES[PICKUP_HEALTH_SMALL]);
	/* Slot 2 removed: overlaps WALL_TEX_CHAR_START (char 1327) */

	/* Face + weapon sprites */
	loadFaceFrame(doomface);
	if (weapon == W_FISTS)
		loadFistSprites();
	else if (weapon == W_SHOTGUN)
		loadShotgunSprites();
	else if (weapon == W_ROCKET)
		loadRocketLauncherSprites();
	else
		loadPistolSprites();
}

/* Load a level: copy map, place enemies/pickups, register doors/switches, set spawn.
 * Call at game start and on each level transition. */
void loadLevel(u8 levelNum) {
	currentLevel = levelNum;
	g_levelComplete = 0;

	if (levelNum == 1) {
		{ u16 row; for (row = 0; row < 32; row++) copymem((u8*)g_map + row * MAP_X, (const u8*)e1m1_map + row * 32, 32); }
		{ u16 i; for (i = 32 * MAP_X; i < MAP_CELLS; i++) ((u8*)g_map)[i] = 0; }
		{ u16 row; for (row = 0; row < 32; row++) { u16 c; for (c = 32; c < MAP_X; c++) ((u8*)g_map)[row * MAP_X + c] = 0; } }
		fPlayerX = E1M1_SPAWN_X;
		fPlayerY = E1M1_SPAWN_Y;
		fPlayerAng = 512;  /* face south */
		initEnemies();
		initPickups();
		initDoors();
		registerDoor(15, 19);  /* center passage */
		registerDoor(5, 11);   /* armor room entrance */
		registerSwitch(27, 3, SW_EXIT, 0);   /* exit switch */
		registerSwitch(18, 10, SW_DOOR, 1);  /* opens armor room door */
	} else if (levelNum == 2) {
		{ u16 row; for (row = 0; row < 32; row++) copymem((u8*)g_map + row * MAP_X, (const u8*)e1m2_map + row * 32, 32); }
		{ u16 i; for (i = 32 * MAP_X; i < MAP_CELLS; i++) ((u8*)g_map)[i] = 0; }
		{ u16 row; for (row = 0; row < 32; row++) { u16 c; for (c = 32; c < MAP_X; c++) ((u8*)g_map)[row * MAP_X + c] = 0; } }
		fPlayerX = E1M2_SPAWN_X;
		fPlayerY = E1M2_SPAWN_Y;
		fPlayerAng = 512;  /* face south */
		initEnemiesE1M2();
		initPickupsE1M2();
		initDoors();
		registerDoor(15, 13);  /* center passage */
		registerDoor(8, 16);   /* weapon closet (opened by switch) */
		registerDoor(5, 19);   /* west barracks */
		registerDoor(26, 19);  /* east armory */
		registerSwitch(28, 3, SW_EXIT, 0);   /* exit switch */
		registerSwitch(4, 4, SW_DOOR, 1);    /* opens weapon closet door */
	} else if (levelNum == 3) {
		{ u16 row; for (row = 0; row < 32; row++) copymem((u8*)g_map + row * MAP_X, (const u8*)e1m3_map + row * 32, 32); }
		{ u16 i; for (i = 32 * MAP_X; i < MAP_CELLS; i++) ((u8*)g_map)[i] = 0; }
		{ u16 row; for (row = 0; row < 32; row++) { u16 c; for (c = 32; c < MAP_X; c++) ((u8*)g_map)[row * MAP_X + c] = 0; } }
		fPlayerX = E1M3_SPAWN_X;
		fPlayerY = E1M3_SPAWN_Y;
		fPlayerAng = 0;  /* face north */
		initEnemiesE1M3();
		initPickupsE1M3();
		initDoors();
		registerDoor(15, 21);  /* center passage south corridor to central area */
		registerDoor(5, 14);   /* west wing entrance */
		registerDoor(26, 14);  /* east wing entrance */
		registerDoor(15, 7);   /* north gate arena to command center */
		registerDoor(4, 7);    /* hidden armory entrance (switch-opened) */
		registerDoor(28, 5);   /* locked tech room (switch-opened) */
		registerSwitch(15, 3, SW_EXIT, 0);   /* exit switch */
		registerSwitch(10, 10, SW_DOOR, 4);  /* opens hidden armory door */
		registerSwitch(20, 10, SW_DOOR, 5);  /* opens locked tech room door */
	} else if (levelNum == 4) {
		{ u16 row; for (row = 0; row < 32; row++) copymem((u8*)g_map + row * MAP_X, (const u8*)e1m4_map + row * 32, 32); }
    	{ u16 i; for (i = 32 * MAP_X; i < MAP_CELLS; i++) ((u8*)g_map)[i] = 0; }
    	{ u16 row; for (row = 0; row < 32; row++) { u16 c; for (c = 32; c < MAP_X; c++) ((u8*)g_map)[row * MAP_X + c] = 0; } }
    	/* E1M4: use SPAWN 1 only (tile 30,30). E1M4_SPAWN2_X/Y (30,27) is unused. */
    	fPlayerX = E1M4_SPAWN_X;
    	fPlayerY = E1M4_SPAWN_Y;
    	fPlayerAng = 768;
    	initEnemiesE1M4();
    	initPickupsE1M4();
    	initDoors();
    	registerDoor(13, 1);
    	registerDoor(25, 10);   /* was 26,10: door tile is at (25,10) in map */
    	registerDoor(13, 11);
    	registerDoor(22, 16);
    	registerDoor(14, 17);
    	registerDoor(28, 18);   /* was 29,18: door tile is at (28,18) in map */
    	registerDoor(16, 19);
    	registerDoor(23, 19);
    	registerDoor(6, 21);
    	registerDoor(17, 21);
    	registerDoor(3, 23);
    	registerDoor(18, 24);
    	registerDoor(13, 25);
    	registerDoor(15, 25);
    	registerDoor(16, 25);
    	registerDoor(23, 26);
    	registerDoor(27, 28);   /* door index 16: spawn room exit (27,28) in map */
    	registerDoor(17, 29);
    	registerSwitch(29, 0, SW_DOOR, 5);
    	registerSwitch(0, 21, SW_EXIT, 0);
	} else if (levelNum == 5 || levelNum == 6) {
     		/* Placeholder: reuse E1M3 until e1m4/e1m5/e1m6 maps and inits are added */
     		{ u16 row; for (row = 0; row < 32; row++) copymem((u8*)g_map + row * MAP_X, (const u8*)e1m3_map + row * 32, 32); }
     		{ u16 i; for (i = 32 * MAP_X; i < MAP_CELLS; i++) ((u8*)g_map)[i] = 0; }
     		{ u16 row; for (row = 0; row < 32; row++) { u16 c; for (c = 32; c < MAP_X; c++) ((u8*)g_map)[row * MAP_X + c] = 0; } }
     		fPlayerX = E1M3_SPAWN_X;
     		fPlayerY = E1M3_SPAWN_Y;
     		fPlayerAng = 0;
     		initEnemiesE1M3();
     		initPickupsE1M3();
     		initDoors();
     		registerDoor(15, 21);
     		registerDoor(5, 14);
     		registerDoor(26, 14);
     		registerDoor(15, 7);
     		registerDoor(4, 7);
     		registerDoor(28, 5);
     		registerSwitch(15, 3, SW_EXIT, 0);
     		registerSwitch(10, 10, SW_DOOR, 4);
     		registerSwitch(20, 10, SW_DOOR, 5);
     	}

	/* Reset particles and projectiles for the new level */
	initParticles();
	initProjectiles();

	/* Reset level timer and counters for stats screen */
	g_levelFrames = 0;
	g_enemiesKilled = 0;
	g_totalEnemies = 0;
	g_secretsFound = 0;
	g_totalSecrets = 0;
	{
		u8 si;
		for (si = 0; si < MAX_SECRETS; si++) {
			g_secrets[si].tx = 0;
			g_secrets[si].ty = 0;
			g_secrets[si].found = false;
		}
	}

	/* Define secrets per level */
	if (levelNum == 1) {
		/* E1M1: armor room (4,9) is a secret area */
		g_secrets[0].tx = 4;  g_secrets[0].ty = 9;
		g_totalSecrets = 1;
	} else if (levelNum == 2) {
		/* E1M2: weapon closet (3,16) is a secret */
		g_secrets[0].tx = 3;  g_secrets[0].ty = 16;
		g_totalSecrets = 1;
	} else if (levelNum == 3) {
		/* E1M3: hidden armory (2,4) and locked tech room (29,3) */
		g_secrets[0].tx = 2;  g_secrets[0].ty = 4;
		g_secrets[1].tx = 29; g_secrets[1].ty = 3;
		g_totalSecrets = 2;
	}
	{
		u8 ei;
		for (ei = 0; ei < MAX_ENEMIES; ei++) {
			if (g_enemies[ei].active) g_totalEnemies++;
		}
	}
	g_itemsCollected = 0;
	g_totalItems = 0;
	{
		u8 pi;
		for (pi = 0; pi < MAX_PICKUPS; pi++) {
			if (g_pickups[pi].active) {
				u8 t = g_pickups[pi].type;
				if (t == PICKUP_WEAPON_SHOTGUN || t == PICKUP_HELMET || t == PICKUP_WEAPON_ROCKET)
					g_totalItems++;
			}
		}
	}
}

u8 gameLoop()
{
	/* fixed0point05 and fixed2point13 are now static const -- no runtime init needed */

	initializeDoomStage();
    clearScreen();

	vbSetWorld(31, WRLD_ON|1, 0, 0, 0, 0, 0, 0, 384, 192); // stage background

	u16 EnemyScale = 4; // max scale
	u16 EnemyScaleX = 6; // max scale

	// 1 world per enemy (5 slots), start DISABLED (renderer enables when visible)
	vbSetWorld(30, ENEMY_BGMAP_START|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	vbSetWorld(29, (ENEMY_BGMAP_START+1)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	vbSetWorld(28, (ENEMY_BGMAP_START+2)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	vbSetWorld(27, (ENEMY_BGMAP_START+3)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	vbSetWorld(26, (ENEMY_BGMAP_START+4)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	/* Affine param tables at 0x1000+ within each enemy's tile BGMap (safe: tiles < 912 bytes) */
	WORLD_PARAM(30, BGMap(ENEMY_BGMAP_START)   + 0x1000);  /* enemy 0 */
	WORLD_PARAM(29, BGMap(ENEMY_BGMAP_START+1) + 0x1000);  /* enemy 1 */
	WORLD_PARAM(28, BGMap(ENEMY_BGMAP_START+2) + 0x1000);  /* enemy 2 */
	WORLD_PARAM(27, BGMap(ENEMY_BGMAP_START+3) + 0x1000);  /* enemy 3 */
	WORLD_PARAM(26, BGMap(ENEMY_BGMAP_START+4) + 0x1000);  /* enemy 4 */
	/* Pickup worlds: 2 slots, start DISABLED (renderer enables when visible) */
	vbSetWorld(25, PICKUP_BGMAP_START|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 32*EnemyScaleX, 24*EnemyScale);
	vbSetWorld(24, (PICKUP_BGMAP_START+1)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 32*EnemyScaleX, 24*EnemyScale);
	/* Affine param tables for pickups (at 0x1000+ within pickup tile BGMaps) */
	WORLD_PARAM(25, BGMap(PICKUP_BGMAP_START)   + 0x1000);  /* pickup 0 */
	WORLD_PARAM(24, BGMap(PICKUP_BGMAP_START+1) + 0x1000);  /* pickup 1 */

	/* Particle world: affine, starts DISABLED (renderer enables when visible) */
	vbSetWorld(PARTICLE_WORLD, PARTICLE_BGMAP|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 32*EnemyScaleX, 32*EnemyScale);
	WORLD_PARAM(PARTICLE_WORLD, BGMap(ENEMY_BGMAP_START) + 0x1800);  /* particle param in enemy 0 BGMap */
	vbSetWorld(22, WRLD_ON|LAYER_WEAPON_BLACK, 				0, 0, 0, 0, 0, 0, 136, 128); // weapon black
	vbSetWorld(21, WRLD_ON|LAYER_WEAPON, 					0, 0, 0, 0, 0, 0, 136, 128); // weapon
	vbSetWorld(20, WRLD_ON|LAYER_UI_BLACK, 					0, 0, 0, 0, 0, 0, 384, 32); // ui black
	vbSetWorld(19, WRLD_ON|LAYER_UI, 						0, 0, 0, 0, 0, 0, 384, 32); // ui
	vbSetWorld(18, 0, 0, 0, 0, 0, 0, 0, 384, 32);  /* disabled: third HUD layer caused even-tile glitch on hardware */

	// ui position starts at pixel y 192, we use 2 layers because we wanna use all 4 colors
	WA[20].gy = 192;
	WA[19].gy = 192;
	WA[18].gy = 192;

	WA[17].head = WRLD_END;

	loadDoomGfxToMem();
	loadLevel(START_LEVEL);     /* START_LEVEL: 1=normal, 4=E1M4 debug, etc. */
	loadParticleTiles();

    vbDisplayShow();

	u8 doomface = 1;  /* start with center-looking idle face */
	u8 updateDoomfaceTime = 9;
	u8 updateDoomfaceCount = 0;
	u8 lookDir = 1;            /* 0=left, 1=center, 2=right */
	u8 ouchTimer = 0;          /* >0 = show ouch face */
	u8 killFaceTimer = 0;      /* >0 = show evil grin */

	u16 ammo = 50;
	u8 currentAmmoType = 1;
	u8 currentHealth = 100;
	u16 currentArmour = 0;
	u8 armorType = 0;  /* 0=none, 1=green (1/3 absorb), 2=blue (1/2 absorb) */

	/* HUD dirty tracking: only redraw when value changed (avoids redundant BGMap/copymem) */
	u16 lastHealth = 0xFFFF;
	u16 lastArmour = 0xFFFF;
	u8 lastHasPistol = 0xFF;
	u8 lastHasShotgun = 0xFF;
	u8 lastHasRocket = 0xFF;
	u8 lastCurrentWeapon = 0xFF;
	u16 lastCurrentAmmo = 0xFFFF;
	u16 lastPistolAmmo = 0xFFFF;
	u16 lastShotgunAmmo = 0xFFFF;
	u16 lastRocketAmmo = 0xFFFF;

	VIP_REGS[GPLT0] = 0xE4;	/* Set 1st palettes to: 11100100 */
	VIP_REGS[GPLT1] = 0x00;	/* (i.e. "Normal" dark to light progression.) */
	VIP_REGS[GPLT2] = 0x84;

	VIP_REGS[GPLT3] = 0xE4;	/* Wall palette (PAL3): same as GPLT0 for now */
	VIP_REGS[JPLT0] = 0xE4;
	VIP_REGS[JPLT1] = 0xE4;
	VIP_REGS[JPLT2] = 0xE4;
	VIP_REGS[JPLT3] = 0xE4;

	setmem((void*)BGMap(0), 0, 8192);
	setmem((void*)BGMap(1), 0, 8192);
	setmem((void*)BGMap(2), 0, 8192);
	setmem((void*)BGMap(3), 0, 8192);
	setmem((void*)BGMap(4), 0, 8192);
	setmem((void*)BGMap(5), 0, 8192);
	setmem((void*)BGMap(6), 0, 8192);
	setmem((void*)BGMap(7), 0, 8192);
	setmem((void*)BGMap(8), 0, 8192);
	setmem((void*)BGMap(9), 0, 8192);
	setmem((void*)BGMap(10), 0, 8192);
	setmem((void*)BGMap(11), 0, 8192);
	setmem((void*)BGMap(12), 0, 8192);
	setmem((void*)BGMap(13), 0, 8192);

	/* Enemy BGMap/char init MUST be after setmem clears BGMaps */
	initEnemyBGMaps();                          /* set up BGMap(3), (4), (5) tile entries */
	loadEnemyFrame(0, Zombie_000Tiles);         /* load frame 0 into enemy 0 (zombieman) */
	loadEnemyFrame(1, Zombie_000Tiles);         /* load frame 0 into enemy 1 (zombieman) */
	loadEnemyFrame(2, Zombie_Sergeant_000Tiles); /* load frame 0 into enemy 2 (sergeant) */

	/* Pickup BGMap/char init */
	initPickupBGMaps();
	/* Pre-load one of each pickup type into char slots */
	loadPickupFrame(0, PICKUP_TILES[PICKUP_AMMO_CLIP]);
	loadPickupFrame(1, PICKUP_TILES[PICKUP_HEALTH_SMALL]);
	/* Slot 2 removed: overlaps WALL_TEX_CHAR_START (char 1327) */

	/* Load default face tiles into char memory (dynamic per-face loading) */
	loadFaceFrame(1);  /* start with center-looking idle face */
	loadPistolSprites();  /* default weapon = pistol (2) */

	/* Load wall texture tiles into char memory */
	loadWallTextures();

	drawDoomUI(LAYER_UI, 0, 0);
	drawUpdatedAmmo(ammo, currentAmmoType);
	/* Initialize right-side digits for all ammo types */
	drawSmallAmmo(weapons[W_PISTOL].ammo, 1);  /* BULL */
	drawSmallAmmo(weapons[W_SHOTGUN].ammo, 2); /* SHEL */
	drawSmallAmmo(weapons[W_ROCKET].ammo, 3);  /* RCKT */
	drawWeaponSlotNumbers(weapons[W_PISTOL].hasWeapon,
	                     weapons[W_SHOTGUN].hasWeapon,
	                     weapons[W_ROCKET].hasWeapon,
	                     currentWeapon);
	drawHealth(currentHealth);
	drawArmour((u8)currentArmour);
	highlightWeaponHUD(currentWeapon);
	lastHealth = currentHealth;
	lastArmour = currentArmour;
	lastHasPistol = weapons[W_PISTOL].hasWeapon;
	lastHasShotgun = weapons[W_SHOTGUN].hasWeapon;
	lastHasRocket = weapons[W_ROCKET].hasWeapon;
	lastCurrentWeapon = currentWeapon;
	lastCurrentAmmo = weapons[currentWeapon].ammo;
	lastPistolAmmo = weapons[W_PISTOL].ammo;
	lastShotgunAmmo = weapons[W_SHOTGUN].ammo;
	lastRocketAmmo = weapons[W_ROCKET].ammo;

	drawDoomFace(&doomface);
	resetWeaponDrawState();  /* force full redraw after VRAM init */
	drawWeapon(currentWeapon, swayXTBL[weaponSwayIndex], swayYTBL[weaponSwayIndex], weaponAnimation, 0);
	u16 keyInputs;
	u16 prevKeyInputs = 0;
	u16 keyPressed; /* newly pressed this frame (edge detection) */

	/* Settings struct for in-game options screen (pause) */
	Settings pauseSettings;
	pauseSettings.sfx = (u8)((u16)g_sfxVolume * 9 / 15);
	pauseSettings.music = g_musicSetting;
	pauseSettings.rumble = 0;

	/* mp_init() already called in main.c before title screen */
	while(1) {
		//drawDoomFace(LAYER_UI, 44, 0, doomface);
		isMoving = false;
	// clear 1 (weapon layer) and 2 (enemy layer) each frame...
		/*for (worldCount = 1; worldCount < 7; worldCount++) {
			setmem((void*)BGMap(worldCount), 0x0000, 8192);
		}*/
		// 8192
		// 56 x 96 (384/4 * 224/4) = 5376
		// 10 x 14 (40/4 * 56/4) = 140 (Enemy layer)
		// 40 x 46 (160/4 x 184/4) = 1840 (Weapon layer)
		//setmem((void*)BGMap(2), 0x0000, 140); // clear enemy layer 1...
		//setmem((void*)BGMap(3), 0x0000, 140); // clear enemy layer 1...
		/*
		setmem((void*)BGMap(3), 0x0000, 140); // clear enemy layer 1...
		setmem((void*)BGMap(4), 0x0000, 140); // clear enemy layer 1...
		setmem((void*)BGMap(5), 0x0000, 140); // clear enemy layer 1...
		setmem((void*)BGMap(6), 0x0000, 140); // clear enemy layer 1...
		setmem((void*)BGMap(7), 0x0000, 140); // clear enemy layer 1...
		*/
		//setmem((void*)BGMap(LAYER_BULLET), 0, 8192);
		//setmem((void*)BGMap(UI_LAYER), 0, 8192);

		keyInputs = vbReadPad();
		keyPressed = keyInputs & ~prevKeyInputs; /* newly pressed this frame */
		// handle input last...
		if (keyInputs & K_LU) {// Left Pad, Up
			isMoving = true;
			fPlayerMoveForward(&fPlayerX, &fPlayerY, fPlayerAng, (isRunning?movespeed*2:movespeed));
			//playerMoveForward(3);
		} else if (keyInputs & K_LD) {// Left Pad, Down
			isMoving = true;
			fPlayerMoveForward(&fPlayerX, &fPlayerY, fPlayerAng, -(isRunning?movespeed*2:movespeed));
			//playerMoveForward(-3);
		}
		if (keyInputs & K_LL) {// Left Pad, Left'
			isMoving = true;
			fPlayerStrafe(&fPlayerX, &fPlayerY, fPlayerAng, -(isRunning?movespeed*2:movespeed));
		} else if (keyInputs & K_LR) {// Left Pad, Right
			isMoving = true;
			fPlayerStrafe(&fPlayerX, &fPlayerY, fPlayerAng, (isRunning?movespeed*2:movespeed));
		}
		if  (isMoving) {
			swayWeapon();
		} else {
			walkSwayIndex = 0;
			weaponSwayIndex = 0;
			//WA[31].gy = -8;
		}
		if(keyInputs & K_RU) {// Right Pad, Up
			jawFixedPlayer(-fixed0point05);
		} else if(keyInputs & K_RD) {// Right Pad, Down
			jawFixedPlayer(fixed0point05);
		}

		if(keyInputs & K_RL) {// Right Pad, Left
			fPlayerAng -= turnRate;
			if (fPlayerAng < 0)
				fPlayerAng = 1023;
		} else if(keyInputs & K_RR) {// Right Pad, Right
			fPlayerAng += turnRate;
			if (fPlayerAng > 1023)
				fPlayerAng = 0;

		}
		isRunning = keyInputs & K_LT;

		if (keyInputs & K_A && isShooting == false) {
			cycleWeapons();
		}

		if (keyInputs & (K_B | K_RT) && updatePistolCount == 0 && weaponAnimation == 0 && isChangingWeapon == false) {
			if (weapons[currentWeapon].requiresAmmo) {
				if (weapons[currentWeapon].ammo > 0) {
				weaponAnimation = 1; // shoot
				weapons[currentWeapon].ammo--;
				isShooting = true;
				/* Trigger PCM weapon fire sound */
				if (currentWeapon == W_ROCKET) {
					playPlayerSFX(SFX_ROCKET_LAUNCH);
				} else if (currentWeapon == W_SHOTGUN)
					playPlayerSFX(SFX_SHOTGUN);
				else
					playPlayerSFX(SFX_PISTOL);
				drawUpdatedAmmo(weapons[currentWeapon].ammo, weapons[currentWeapon].ammoType);
				if (currentWeapon == W_ROCKET) {
					/* Rocket launcher: spawn projectile, no hitscan */
					spawnRocket(fPlayerX, fPlayerY, fPlayerAng);
				} else {
					u8 hitIdx = playerShoot(fPlayerX, fPlayerY, fPlayerAng, currentWeapon);
						if (ENEMY_JUST_KILLED(hitIdx)) {
							killFaceTimer = 30; /* show evil grin for ~1.5s */
						}
						if (hitIdx == 255) {
							/* Bullet hit wall -- cast ray to find exact wall hit position */
							s16 wallHitX, wallHitY;
							CastRayHitPos(fPlayerX, fPlayerY, (u16)fPlayerAng & 1023, &wallHitX, &wallHitY);
							if (currentWeapon == W_SHOTGUN) {
								spawnShotgunGroup(wallHitX, wallHitY);
							} else {
								spawnPuff(wallHitX, wallHitY);
							}
						}
					}
				/* Defer auto-switch until shoot animation finishes */
				if (weapons[currentWeapon].ammo == 0) pendingAutoSwitch = true;
				} else {
					/* No ammo left -- smart switch to best available */
					autoSwitchOnEmpty();
				}
		} else {
		 	weaponAnimation = 1; // punch!
			isShooting = true;
			playPlayerSFX(SFX_PUNCH);
			/* Try to hit an enemy first */
			{
				u8 hitIdx = playerShoot(fPlayerX, fPlayerY, fPlayerAng, currentWeapon);
				if (ENEMY_JUST_KILLED(hitIdx)) {
					killFaceTimer = 30;
				}
				if (hitIdx == 255) {
					/* No enemy hit -- check wall for puff within melee range */
					s16 wallHitX, wallHitY;
					s16 dx, dy;
					s32 dist2;
					CastRayHitPos(fPlayerX, fPlayerY, (u16)fPlayerAng & 1023, &wallHitX, &wallHitY);
					dx = wallHitX - (s16)fPlayerX;
					dy = wallHitY - (s16)fPlayerY;
					dist2 = (s32)dx * dx + (s32)dy * dy;
					if (dist2 < (s32)80 * 80) {
						spawnPuff(wallHitX, wallHitY);
					}
				}
			}
		}
			/*
			if (currentWeapon > 1) {
				if (ammo > 0) {
					weaponAnimation = 1; // shoot
					isShooting = true;
					ammo--;
					//drawUpdatedAmmo(ammo, currentAmmoType);
				}
				else {
					cycleWeapons();
					if (currentWeapon == 1) {
						currentAmmoType = 0;
					}
				}
			} else {
				weaponAnimation = 1; // punch
			}*/

			//drawWeapon(currentWeapon, weaponSwayX, weaponSwayY, weaponAnimation);
		}
 		//printString(0, 14, 10, getString(STR_TESTICUS));
 		//printString(0, 34, 10, "hello");
		//drawDoomGuy(0, 44, 24, doomface);


		//drawDoomStage(0);




		/*if (isFixed) {
			fixedDrawDoomStage(0);
		} else {
			drawDoomStage(0);
		}*/
		/* Door/switch activation (Select button -- requires fresh press) */
		UpdateCenterRay(fPlayerX, fPlayerY, fPlayerAng);
		if (keyPressed & K_SEL) {
			u8 activateResult = playerActivate(fPlayerX, fPlayerY, fPlayerAng, currentLevel);
			if (activateResult == 2) {
				/* Hit a non-interactive wall -- play "umf" sound */
				playPlayerSFX(SFX_PLAYER_UMF);
			}
			/* activateResult == 1: door/switch activated (sound in door.c) */
			/* activateResult == 0: nothing in range, no sound */
		}

		/* In-game pause: Start button opens options screen */
		if (keyPressed & K_STA) {
			optionsScreen(&pauseSettings);

			/* Update volumes from potentially changed settings */
			g_sfxVolume = (u8)((u16)pauseSettings.sfx * 15 / 9);
			g_musicSetting = pauseSettings.music;
			g_musicVolume = musicVolFromSetting(pauseSettings.music);

			/* Restore all game VRAM, worlds, palettes, BGMaps */
			restoreGameDisplay(doomface, currentWeapon);

			/* Re-draw the full HUD (dirty flags will force health/armour/slots on next frame) */
			drawDoomUI(LAYER_UI, 0, 0);
			drawUpdatedAmmo(weapons[currentWeapon].ammo, weapons[currentWeapon].ammoType);
			drawSmallAmmo(weapons[W_PISTOL].ammo, 1);
			drawSmallAmmo(weapons[W_SHOTGUN].ammo, 2);
			drawSmallAmmo(weapons[W_ROCKET].ammo, 3);
			lastHealth = 0xFFFF;
			lastArmour = 0xFFFF;
			lastHasPistol = 0xFF;
			lastHasShotgun = 0xFF;
			lastHasRocket = 0xFF;
			lastCurrentWeapon = 0xFF;
			lastCurrentAmmo = weapons[currentWeapon].ammo;
			lastPistolAmmo = weapons[W_PISTOL].ammo;
			lastShotgunAmmo = weapons[W_SHOTGUN].ammo;
			lastRocketAmmo = weapons[W_ROCKET].ammo;
			highlightWeaponHUD(currentWeapon);
			drawDoomFace(&doomface);
			resetWeaponDrawState();  /* force full redraw after pause return */
			drawWeapon(currentWeapon, swayXTBL[weaponSwayIndex], swayYTBL[weaponSwayIndex], weaponAnimation, 0);

			/* Re-apply palettes */
			VIP_REGS[GPLT0] = 0xE4;
			VIP_REGS[GPLT1] = 0x00;
			VIP_REGS[GPLT2] = 0x84;
			VIP_REGS[GPLT3] = 0xE4;
			VIP_REGS[JPLT0] = 0xE4;
			VIP_REGS[JPLT1] = 0xE4;
			VIP_REGS[JPLT2] = 0xE4;
			VIP_REGS[JPLT3] = 0xE4;

			/* Reset input state to avoid leftover button press */
			prevKeyInputs = 0xFFFF;
		}

		/* Update door animations */
		updateDoors();

		// we actually only need to call this if x,y,ang changed...
		updateEnemies(fPlayerX, fPlayerY, fPlayerAng);

		/* Update particle animations */
		updateParticles();

		/* Animate pickups (ping-pong for helmet/armor) */
		animatePickups();

		/* Update projectiles (fireballs) */
		updateProjectiles(fPlayerX, fPlayerY, fPlayerAng);

		/* Check pickup collisions */
		{
			u8 pickupAmmo = weapons[2].ammo;  /* pistol bullets */
			u8 shellAmmo = weapons[3].ammo;   /* shotgun shells */
		if (updatePickups(fPlayerX, fPlayerY, &pickupAmmo, &currentHealth,
		                  &currentArmour, &armorType, &shellAmmo)) {
			/* Something was picked up - update ammo and HUD (health/armour redrawn by per-frame dirty check) */
			weapons[2].ammo = pickupAmmo;
			weapons[3].ammo = shellAmmo;
			/* Update big numbers (left side) for current weapon */
			drawUpdatedAmmo(weapons[currentWeapon].ammo, weapons[currentWeapon].ammoType);
			/* Update right-side digits for all ammo types (non-equipped too) */
			drawSmallAmmo(weapons[2].ammo, 1);  /* BULL */
			drawSmallAmmo(weapons[3].ammo, 2);  /* SHEL */
			drawSmallAmmo(weapons[4].ammo, 3);  /* RCKT */
		}
			/* Handle weapon pickups */
			if (g_pickedUpWeapon > 0) {
				weapons[g_pickedUpWeapon].hasWeapon = true;
				if (g_pickedUpWeapon == W_ROCKET)
					weapons[g_pickedUpWeapon].ammo = 2;  /* start with 2 rockets */
				else
					weapons[g_pickedUpWeapon].ammo = 8;  /* start with 8 shells */
				currentWeapon = g_pickedUpWeapon;
				nextWeapon = g_pickedUpWeapon;
				if (g_pickedUpWeapon == W_ROCKET)
					loadRocketLauncherSprites();
				else
					loadShotgunSprites();
				drawUpdatedAmmo(weapons[currentWeapon].ammo, weapons[currentWeapon].ammoType);
				lastHasPistol = 0xFF;
				lastHasShotgun = 0xFF;
				lastHasRocket = 0xFF;
				lastCurrentWeapon = 0xFF;
				highlightWeaponHUD(currentWeapon);
				weaponAnimation = 0;
				updatePistolCount = 0;
				isShooting = false;
				playPlayerSFX(SFX_SHOTGUN_COCK);
				g_pickedUpWeapon = 0;
			}
		}

		/* Check secret sectors */
		{
			u8 playerTX = (u8)(fPlayerX >> 8);
			u8 playerTY = (u8)(fPlayerY >> 8);
			u8 si;
			for (si = 0; si < g_totalSecrets; si++) {
				if (!g_secrets[si].found &&
				    playerTX == g_secrets[si].tx &&
				    playerTY == g_secrets[si].ty) {
					g_secrets[si].found = true;
					g_secretsFound++;
				}
			}
		}

		/* Update enemy sprite frames in VRAM -- distance-sorted rendering */
		/* Compute 3 closest active enemies for rendering.
		 * Alive enemies always have priority over dead ones. */
		{
			static u8 lastSlotEnemy[MAX_VISIBLE_ENEMIES] = {255,255,255,255,255};
			/* Cached line-of-sight results: only recompute every 4 frames */
			static bool losCache[MAX_ENEMIES];
			static u8 losInited = 0;
			u8 ei, vi;
			u32 dists[MAX_ENEMIES];
			u8 sorted[MAX_ENEMIES];
			u8 activeCount = 0;
			u8 doLosCheck;
			u8 doVisibilityRefresh;

			/* New level: invalidate visibility/LOS caches cleanly. */
			if (g_levelFrames == 0) {
				losInited = 0;
				for (ei = 0; ei < MAX_ENEMIES; ei++) {
					losCache[ei] = false;
				}
			}
			doLosCheck = ((g_levelFrames & 3) == 0) || !losInited;
			doVisibilityRefresh = ((g_levelFrames & 1) == 0) || (g_numVisibleEnemies == 0) || !losInited;
			if (!losInited) losInited = 1;

			/* Forward vector from player angle (octant approximation).
			 * Used to detect enemies behind the player so they don't
			 * waste render slots over visible dead enemies. */
			static const s8 fwdX[8] = { 0, 1, 1, 1, 0,-1,-1,-1};
			static const s8 fwdY[8] = { 1, 1, 0,-1,-1,-1, 0, 1};
			u8 fwdOct = ((u16)(fPlayerAng + 64) >> 7) & 7;

			if (doVisibilityRefresh) {
				/* Compute squared distance for all active enemies.
				 * Priority tiers (lower = higher priority):
				 *   visible alive       -> raw distance
				 *   visible dead        -> distance + 0x40000000
				 *   not-visible alive   -> distance + 0x80000000
				 *   not-visible dead    -> distance + 0xC0000000
				 * "Not-visible" = behind player OR blocked by a wall.
				 * hasLineOfSight is expensive (Bresenham walk); only recheck every 4 frames. */
				for (ei = 0; ei < MAX_ENEMIES; ei++) {
					if (!g_enemies[ei].active) continue;
					{
						s16 dx = (s16)g_enemies[ei].x - (s16)fPlayerX;
						s16 dy = (s16)g_enemies[ei].y - (s16)fPlayerY;
						u32 d = ((u32)((s32)dx * dx + (s32)dy * dy)) >> 8;
						bool visible = true;
						/* Cheap check first: behind player? */
						s32 dot = (s32)dx * fwdX[fwdOct] + (s32)dy * fwdY[fwdOct];
						if (dot <= 0) {
							visible = false;
						} else if (doLosCheck) {
							/* Full LOS check: only every 4 frames */
							if (!hasLineOfSight(fPlayerX, fPlayerY,
										g_enemies[ei].x, g_enemies[ei].y)) {
								visible = false;
							}
							losCache[ei] = visible;
						} else {
							/* Use cached result */
							visible = losCache[ei];
						}
						if (g_enemies[ei].state == ES_DEAD)
							d += visible ? 0x40000000u : 0xC0000000u;
						else if (!visible)
							d += 0x80000000u;
						dists[activeCount] = d;
						sorted[activeCount] = ei;
						activeCount++;
					}
				}

				/* Simple selection sort for top N closest */
				{
					u8 j, minIdx;
					u32 minDist;
					for (vi = 0; vi < activeCount && vi < MAX_VISIBLE_ENEMIES; vi++) {
						minIdx = vi;
						minDist = dists[vi];
						for (j = vi + 1; j < activeCount; j++) {
							if (dists[j] < minDist) {
								minDist = dists[j];
								minIdx = j;
							}
						}
						if (minIdx != vi) {
							/* Swap */
							u32 tmpD = dists[vi]; dists[vi] = dists[minIdx]; dists[minIdx] = tmpD;
							u8 tmpI = sorted[vi]; sorted[vi] = sorted[minIdx]; sorted[minIdx] = tmpI;
						}
					}
				}
			} else {
				/* Reuse previous visible ordering on alternate frames. */
				activeCount = g_numVisibleEnemies;
				for (vi = 0; vi < g_numVisibleEnemies && vi < MAX_VISIBLE_ENEMIES; vi++) {
					sorted[vi] = g_visibleEnemies[vi];
				}
			}

			/* Build visible list and load frames */
			g_numVisibleEnemies = (activeCount < MAX_VISIBLE_ENEMIES) ? activeCount : MAX_VISIBLE_ENEMIES;
			for (vi = 0; vi < MAX_VISIBLE_ENEMIES; vi++) {
				if (vi < g_numVisibleEnemies) {
					u8 realIdx = sorted[vi];
					u8 frameIdx;
					const unsigned int *frameData;
					u8 slotChanged = (lastSlotEnemy[vi] != realIdx);
					g_visibleEnemies[vi] = realIdx;

					frameIdx = getEnemySpriteFrame(realIdx, fPlayerX, fPlayerY, fPlayerAng);
					if (slotChanged || frameIdx != g_enemies[realIdx].lastRenderedFrame) {
						if (g_enemies[realIdx].enemyType == ETYPE_DEMON)
							frameData = DEMON_FRAMES[frameIdx];
						else if (g_enemies[realIdx].enemyType == ETYPE_IMP)
							frameData = IMP_FRAMES[frameIdx];
						else if (g_enemies[realIdx].enemyType == ETYPE_SERGEANT)
							frameData = ZOMBIE_SGT_FRAMES[frameIdx];
						else
							frameData = ZOMBIE_FRAMES[frameIdx];
						loadEnemyFrame(vi, frameData);
						g_enemies[realIdx].lastRenderedFrame = frameIdx;
					}
					lastSlotEnemy[vi] = realIdx;
				} else {
					g_visibleEnemies[vi] = 255;
					lastSlotEnemy[vi] = 255;
				}
			}
		}

		TraceFrame(&fPlayerX, &fPlayerY, &fPlayerAng);


		//drawFixedDoomStage(0);
		/*for (worldCount = 0; worldCount < 1; worldCount++) {
			setmem((void*)BGMap(worldCount), 0x0000, 8192);
		}*/
		//setmem((void*)BGMap(0), 0x0000, 8192);
		//setmem((void*)BGMap(1), 0x0000, 8192);
		/* --- Doom face selection (health/damage-based) --- */
		{
			u8 healthBracket = 0;
			u8 newFace;
			if (currentHealth >= 80)      healthBracket = 0;
			else if (currentHealth >= 60) healthBracket = 1;
			else if (currentHealth >= 40) healthBracket = 2;
			else if (currentHealth >= 20) healthBracket = 3;
			else                          healthBracket = 4;

			/* Process enemy damage this frame */
			if (g_lastEnemyDamage > 0) {
				u8 dmg = g_lastEnemyDamage;
				g_lastEnemyDamage = 0;
			/* Doom-style armor absorption */
				if (currentArmour > 0 && armorType > 0) {
					u8 armorAbsorb;
					if (armorType >= 2)
						armorAbsorb = dmg >> 1;     /* blue: absorb 1/2 */
					else
						armorAbsorb = (dmg + 2) / 3;  /* green: absorb 1/3, round up */
					if (armorAbsorb > (u8)currentArmour) armorAbsorb = (u8)currentArmour;
					currentArmour -= armorAbsorb;
					dmg -= armorAbsorb;
					if (currentArmour == 0) armorType = 0;
				}

				/* Apply damage to player health (HUD redrawn by per-frame dirty check) */
				if (dmg >= currentHealth)
					currentHealth = 0;
				else
					currentHealth -= dmg;

				/* Player pain/death sound */
				if (currentHealth == 0)
					playPlayerSFX(SFX_PLAYER_DEATH);
				else
					playPlayerSFX(SFX_PLAYER_PAIN);

				/* Screen flash for damage */
				g_flashTimer = 3;
				g_flashType = 1;

				/* Show ouch face: severe if >20 damage at once */
				ouchTimer = 20;
				if (dmg > 20)
					ouchTimer |= 0x80;
			}

			/* Decrement timers */
			if (ouchTimer > 0) {
				if (ouchTimer & 0x80) {
					ouchTimer = (ouchTimer & 0x7F) - 1;
					if ((ouchTimer & 0x7F) == 0) ouchTimer = 0;
					else ouchTimer |= 0x80;
				} else {
					ouchTimer--;
				}
			}
			if (killFaceTimer > 0) killFaceTimer--;

			/* Select face */
			if (currentHealth == 0) {
				newFace = FACE_DEAD;
			} else if (killFaceTimer > 0) {
				newFace = FACE_EVIL_GRIN + healthBracket;
			} else if (ouchTimer > 0) {
				if (ouchTimer & 0x80) {
					newFace = FACE_OUCH_SEVERE + healthBracket;
				} else {
					/* All damage directions use front ouch (left/right removed) */
					newFace = FACE_OUCH_FRONT + healthBracket;
				}
			} else {
				/* Idle: cycle left/center/right on a timer */
				if (updateDoomfaceCount > updateDoomfaceTime) {
					{ /* Fast mod-3: avoid division on V810 */
					u8 r3 = (u8)rand();
					lookDir = r3 - ((u16)(r3 * 171u) >> 9) * 3u;  /* 0=left, 1=center, 2=right */
				}
					updateDoomfaceCount = 0;
					updateDoomfaceTime = 9 + (rand() & 7);
				} else {
					updateDoomfaceCount++;
				}
				newFace = FACE_IDLE_BASE + healthBracket * 3 + lookDir;
			}

			if (newFace != doomface) {
				doomface = newFace;
				loadFaceFrame(doomface);  /* load new face tiles into VRAM */
				drawDoomFace(&doomface);
			}
		}
		//VIP_REGS[GPLT0] = 0xD2;
		//VIP_REGS[GPLT0] = 0xE4;

		// only need to draw if changed...
		//drawDoomPistol(0,42,16, pistolAnimation);

		//vbSetObject(u16 n, u8 header, s32 x, s32 p, s32 y, u16 chr)
		//vbSetObject(	  1024, 		 OBJ_ON, 	16,	   0,	  16,	   8);

		if (weaponAnimation >= 1) {
			if (weaponAnimation >= 2) { // punch animation
				isShooting = true;
			}
			updatePistolCount++;
			if (updatePistolCount > 1) {
				weaponAnimation++;
				if (weaponAnimation >= weapons[currentWeapon].attackFrames) {
					isShooting = false;
					weaponAnimation = 0;
					/* Switch weapon after full shoot animation if ammo ran out */
					if (pendingAutoSwitch) {
						pendingAutoSwitch = false;
						autoSwitchOnEmpty();
					}
				}
				updatePistolCount = 0;
			}

		}
		//setmem((void*)BGMap(LAYER_WEAPON_BLACK), 0, 1840);
		//setmem((void*)BGMap(LAYER_WEAPON), 0, 1840);
		drawWeapon(nextWeapon, swayXTBL[weaponSwayIndex], swayYTBL[weaponSwayIndex], weaponAnimation, weaponChangeTimer);
		//copymem(0x1000, (void*)FontTiles, 1024);
		//setmem((void*)CharSeg0, 0x0000, 2048);
        	//setmem((void*)BGMap(0), 0x0000, 8192);
		//copymem((void*)(CharSeg3+0x1000), (void*)FontTiles, 256*16);

		//setmem((void*)FontTiles, 0x1000, 1024);
       	//setmem((void*)BGMap(0), 0x0000, 8192);
		//printString(1, 30, 20, getString(STR_HELLO_WORLD));
		if (isMoving) {
			weaponSwayIndex++;
			walkSwayIndex++;
			//if (weaponSwayIndex > sizeof(swayXTBL)) {
			if (weaponSwayIndex >= 16) {
				weaponSwayIndex = 0;
			}
			if (walkSwayIndex >= 13) {
				walkSwayIndex = 0;
			}
		}
		if (isChangingWeapon) {
			if (weaponChangeTimer > 10 && nextWeapon != currentWeapon) {
				nextWeapon = currentWeapon; // switch weapon after half time...
				/* Now load tiles so they match the weapon drawWeapon() will use */
				if (currentWeapon == W_FISTS)
					loadFistSprites();
				else if (currentWeapon == W_PISTOL)
					loadPistolSprites();
				else if (currentWeapon == W_SHOTGUN)
					loadShotgunSprites();
				else if (currentWeapon == W_ROCKET)
					loadRocketLauncherSprites();
			}
			if (weaponChangeTimer >= weaponChangeTime) {
				weaponChangeTimer = 0;
				isChangingWeapon = false;
			}
			weaponChangeTimer++;
		}
		/* Update background music sequencer */
		updateMusic(isPlayMusicBool);

		/* Debug: draws player X, Y, angle over the HUD.
		 * Uncomment to re-enable: drawPlayerInfo(&fPlayerX, &fPlayerY, &fPlayerAng); */
		drawUseTargetDebug(g_centerWallTileX, g_centerWallTileY,
			(g_centerWallType == 4 || (g_centerWallType >= 6 && g_centerWallType <= 11)) ? 1u : 0u);

		/* Screen flash effect (pickup or damage) */
		if (g_flashTimer > 0) {
			g_flashTimer--;
			if (g_flashType == 0) {
				/* Pickup flash: all bright red */
				VIP_REGS[GPLT0] = 0xFF;  /* all indices -> brightest */
				VIP_REGS[GPLT1] = 0xFF;
				VIP_REGS[GPLT2] = 0xFF;
				VIP_REGS[GPLT3] = 0xFF;  /* walls flash too */
			} else {
				/* Damage flash: red tint (shift palette darker) */
				VIP_REGS[GPLT0] = 0xFE;
				VIP_REGS[GPLT1] = 0xFE;
				VIP_REGS[GPLT2] = 0xFE;
				VIP_REGS[GPLT3] = 0xFE;  /* walls flash too */
			}
		} else {
			/* Restore normal palettes */
			VIP_REGS[GPLT0] = 0xE4;
			VIP_REGS[GPLT1] = 0x00;
			VIP_REGS[GPLT2] = 0x84;
			VIP_REGS[GPLT3] = 0xE4;  /* wall palette */
		}

		//vbWaitFrame(0); /* Sync to VBlank for tear-free display */

		/* ---- Level transition ---- */
		if (g_levelComplete && currentLevel < 6) {
			/* Stop level music before transition */
			musicStop();
			/* Fade out current level */
			vbFXFadeOut(0);
			vbWaitFrame(10);

			/* Show level completion stats first (kills, items, secrets, time) */
			showLevelStats();

			/* Advance currentLevel so the intermission map skull shows the NEXT level */
			currentLevel++;

			/* Then show intermission map (le_map with bleed-down effect) */
			showIntermission();

			/* Switch music to next level's song */
			{
				if (currentLevel == 2) musicLoadSong(SONG_E1M2);
				else if (currentLevel == 3) musicLoadSong(SONG_E1M3);
				else if (currentLevel == 4) musicLoadSong(SONG_E1M3);  /* E1M4 uses E1M3 until e1m4.mid added */
				else if (currentLevel == 5) musicLoadSong(SONG_E1M5);
				else if (currentLevel == 6) musicLoadSong(SONG_E1M6);
				musicStart();
			}

			/* Load next level (map, enemies, pickups, doors, particles) */
			loadLevel(currentLevel);

			/* Restore all game VRAM, worlds, palettes, BGMaps
			 * (intermission overwrote them with le_map data) */
			restoreGameDisplay(doomface, currentWeapon);

			/* Reset weapon animation state */
			weaponAnimation = 0;
			updatePistolCount = 0;
			isShooting = false;
			weaponSwayIndex = 0;
			walkSwayIndex = 0;

			/* Re-draw HUD (weapons, ammo, health carry over; health/armour/slots via per-frame dirty check) */
			drawDoomUI(LAYER_UI, 0, 0);
			drawUpdatedAmmo(weapons[currentWeapon].ammo, weapons[currentWeapon].ammoType);
			drawSmallAmmo(weapons[W_PISTOL].ammo, 1);
			drawSmallAmmo(weapons[W_SHOTGUN].ammo, 2);
			drawSmallAmmo(weapons[W_ROCKET].ammo, 3);
			lastHealth = 0xFFFF;
			lastArmour = 0xFFFF;
			lastHasPistol = 0xFF;
			lastHasShotgun = 0xFF;
			lastHasRocket = 0xFF;
			lastCurrentWeapon = 0xFF;
			lastCurrentAmmo = weapons[currentWeapon].ammo;
			lastPistolAmmo = weapons[W_PISTOL].ammo;
			lastShotgunAmmo = weapons[W_SHOTGUN].ammo;
			lastRocketAmmo = weapons[W_ROCKET].ammo;
			highlightWeaponHUD(currentWeapon);
			drawDoomFace(&doomface);
			resetWeaponDrawState();  /* force full redraw after level transition */
			drawWeapon(currentWeapon, swayXTBL[weaponSwayIndex], swayYTBL[weaponSwayIndex], weaponAnimation, 0);

			/* Fade in to new level */
			vbFXFadeIn(0);

			/* Re-apply palettes after fade-in (vbFXFadeIn may set generic values) */
			VIP_REGS[GPLT0] = 0xE4;
			VIP_REGS[GPLT1] = 0x00;
			VIP_REGS[GPLT2] = 0x84;
			VIP_REGS[GPLT3] = 0xE4;
			VIP_REGS[JPLT0] = 0xE4;
			VIP_REGS[JPLT1] = 0xE4;
			VIP_REGS[JPLT2] = 0xE4;
			VIP_REGS[JPLT3] = 0xE4;
		}

		/* ---- Episode ending (completed E1M6) ---- */
		if (g_levelComplete && currentLevel == 6) {
			musicStop();
			vbFXFadeOut(0);
			vbWaitFrame(10);
			showLevelStats();
			/* Return to title screen (scene 0 triggers title in main loop) */
			currentLevel = 1;
			return 0;
		}

		/* HUD: redraw health/armour/weapon slots only when values changed */
		if (currentHealth != lastHealth) {
			drawHealth(currentHealth);
			lastHealth = currentHealth;
		}
		if (currentArmour != lastArmour) {
			drawArmour((u8)currentArmour);
			lastArmour = currentArmour;
		}
		if (weapons[W_PISTOL].hasWeapon != lastHasPistol ||
		    weapons[W_SHOTGUN].hasWeapon != lastHasShotgun ||
		    weapons[W_ROCKET].hasWeapon != lastHasRocket ||
		    currentWeapon != lastCurrentWeapon) {
			drawWeaponSlotNumbers(weapons[W_PISTOL].hasWeapon,
			                     weapons[W_SHOTGUN].hasWeapon,
			                     weapons[W_ROCKET].hasWeapon,
			                     currentWeapon);
			lastHasPistol = weapons[W_PISTOL].hasWeapon;
			lastHasShotgun = weapons[W_SHOTGUN].hasWeapon;
			lastHasRocket = weapons[W_ROCKET].hasWeapon;
			lastCurrentWeapon = currentWeapon;
		}

		/* Ammo HUD: redraw only when values changed (saves VRAM writes on real hardware) */
		if (weapons[currentWeapon].ammo != lastCurrentAmmo) {
			drawUpdatedAmmo(weapons[currentWeapon].ammo, weapons[currentWeapon].ammoType);
			lastCurrentAmmo = weapons[currentWeapon].ammo;
		}
		if (weapons[W_PISTOL].ammo != lastPistolAmmo) {
			drawSmallAmmo(weapons[W_PISTOL].ammo, 1);
			lastPistolAmmo = weapons[W_PISTOL].ammo;
		}
		if (weapons[W_SHOTGUN].ammo != lastShotgunAmmo) {
			drawSmallAmmo(weapons[W_SHOTGUN].ammo, 2);
			lastShotgunAmmo = weapons[W_SHOTGUN].ammo;
		}
		if (weapons[W_ROCKET].ammo != lastRocketAmmo) {
			drawSmallAmmo(weapons[W_ROCKET].ammo, 3);
			lastRocketAmmo = weapons[W_ROCKET].ammo;
		}

		/* Timer-based frame pacing: keeps emulator speed closer to hardware.
		 * If a frame is already slow on hardware, waitForFrameTimer() returns immediately. */
		g_levelFrames++;
		prevKeyInputs = keyInputs;
		waitForFrameTimer();
	}
	return 0;
}
void swayWeapon() {
	//WA[31].gy = walkSwayYTBL[walkSwayIndex]-8;
}