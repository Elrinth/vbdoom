#include <functions.h>
#include <math.h>
#include <languages.h>
#include "doomgfx.h"
#include <input.h>
#include "gameLoop.h"
#include "sndplay.h"
#include "doomstage.h"
#include "RayCasterRenderer.h"
#include "enemy.h"
#include "pickup.h"
#include "../assets/images/sprites/zombie/zombie_sprites.h"
#include "../assets/images/sprites/pickups/pickup_sprites.h"
extern BYTE FontTiles[];
#include <stdint.h>
#include <stdbool.h>

s32 fixedAngleRatioz = FTOFIX23_9(512.0f/360.0f);
s32 fixed0point05 = 0; // calced below
s32 fixed2point13 = 0;

s16 fPlayerAng = 0;
u16 fPlayerX = 236;
u16 fPlayerY = 244;
s16 turnRate = 10;
s16 movespeed = 40;
s16 swayY = 0;
s16 swayX = 0;
u8 weaponChangeTimer = 0;
u8 weaponChangeTime = 20;
bool isChangingWeapon = false;
bool comboChangeAndFire = false;
bool isPlayMusicBool = false;
u8 currentWeapon = 2;
u8 nextWeapon = 2;
u8 updatePistolCount = 0;

#define W_CHAINSAW 0
#define W_FISTS 1
#define W_PISTOL 2
#define W_SHOTGUN 3
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
{ true, W_PISTOL, true, 50,1,1},
{ false, W_SHOTGUN, true, 0,2,1},
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

u8 switchWeapon(u8 iFrom, u8 iTo)
{
	if (iTo > 1 || iTo < 8)
		return iTo;
	return iTo;
}
void cycleNextWeapon() {
	currentWeapon++;
	if (currentWeapon > 3)
		currentWeapon = 0;
	if (weapons[currentWeapon].hasWeapon == false || (weapons[currentWeapon].requiresAmmo && weapons[currentWeapon].ammo == 0)) {
		cycleNextWeapon();
	} else {
		if (nextWeapon != currentWeapon) {
			drawUpdatedAmmo(weapons[currentWeapon].ammo, weapons[currentWeapon].ammoType);
			// reset timers and shit...
			weaponAnimation = 0;
			updatePistolCount = 0;
			weaponChangeTimer = 0;
			isShooting = false;
			isChangingWeapon = true;
		}
	}
}
void cycleWeapons()
{
	if (isChangingWeapon) {
		return;
	}

	cycleNextWeapon();
}

u8 gameLoop()
{
	u8 isFixed = 0;

	fixed0point05 = FIX23_9_MULT( FTOFIX23_9(0.05f), fixedAngleRatioz);
	fixed2point13 = FIX23_9_MULT( FTOFIX23_9(2.13f), fixedAngleRatioz);

	initializeDoomStage();
    clearScreen();

	u8 worlds = 14;
	u8 worldCount = 0;
	//for (worldCount = 0; worldCount < worlds; worldCount++) {
		//vbSetWorld(31 - worldCount, WRLD_ON|(worldCount+1), 0, 0, 0, 0, 0, 0, 384, 224);
	//}
	// enemy layers must be able to scale.
	//vbSetWorld(2, WRLD_ON|0|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 384, 224);
	//vbSetWorld(1, WRLD_ON|1, 0, 0, 0, 0, 0, 0, 384, 224);
	//vbSetWorld(31, WRLD_ON|1, 0, 0, 0, 0, 0, 0, 384, 208); // stage background
	vbSetWorld(31, WRLD_ON|1, 0, 0, 0, 0, 0, 0, 384, 192); // stage background

	// monster bgmaps

	u16 EnemyScale = 4; // max scale
	u16 EnemyScaleX = 6; // max scale

	// 1 world per enemy, start DISABLED (renderer enables when visible)
	vbSetWorld(30, ENEMY_BGMAP_START|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	vbSetWorld(29, (ENEMY_BGMAP_START+1)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	/* Affine param tables -- each needs up to 224*16=3584 bytes */
	WORLD_PARAM(30, BGMap(0));              /* enemy 0: 0x20000 */
	WORLD_PARAM(29, BGMap(0) + 0x1000);    /* enemy 1: 0x21000 */
	WORLD_PARAM(28, BGMap(2));              /* enemy 2: 0x24000 */
	/* Enemy 2 world: disabled until renderer enables it */
	vbSetWorld(28, (ENEMY_BGMAP_START+2)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 64*EnemyScaleX, 64*EnemyScale);
	/* Pickup worlds: 3 slots, start DISABLED (renderer enables when visible) */
	vbSetWorld(27, PICKUP_BGMAP_START|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 32*EnemyScaleX, 24*EnemyScale);
	vbSetWorld(26, (PICKUP_BGMAP_START+1)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 32*EnemyScaleX, 24*EnemyScale);
	vbSetWorld(25, (PICKUP_BGMAP_START+2)|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 32*EnemyScaleX, 24*EnemyScale);
	/* Affine param tables for pickups (in free space of BGMap 2 and BGMap 6) */
	WORLD_PARAM(27, BGMap(2) + 0x1000);   /* pickup 0: 0x25000 */
	WORLD_PARAM(26, BGMap(2) + 0x1800);   /* pickup 1: 0x25800 */
	WORLD_PARAM(25, BGMap(6) + 0x1000);   /* pickup 2: 0x2D000 */

	vbSetWorld(24, LAYER_ENEMY_START+6, 0, 0, 0, 0, 0, 0, 0, 0); /* disabled (was more bullets) */

	vbSetWorld(23, WRLD_ON|LAYER_BULLET, 					0, 0, 0, 0, 0, 0, 20*EnemyScale, 26*EnemyScale); // bullets
	vbSetWorld(22, WRLD_ON|LAYER_WEAPON_BLACK, 				0, 0, 0, 0, 0, 0, 136, 128); // weapon black
	vbSetWorld(21, WRLD_ON|LAYER_WEAPON, 					0, 0, 0, 0, 0, 0, 136, 128); // weapon
	vbSetWorld(20, WRLD_ON|LAYER_UI_BLACK, 					0, 0, 0, 0, 0, 0, 384, 32); // ui black
	vbSetWorld(19, WRLD_ON|LAYER_UI, 						0, 0, 0, 0, 0, 0, 384, 32); // ui

	vbSetWorld(18, WRLD_ON|LAYER_UI+1, 						0, 0, 0, 0, 0, 0, 384, 32); // ui


	// ui position starts at pixel y 192, we use 2 layers because we wanna use all 4 colors
	WA[20].gy = 192;
	WA[19].gy = 192;
	WA[18].gy = 192;


	//vbSetWorld(29, WRLD_ON|2|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 384, 224);
	//affine_clr_param(29);



	//affine_scale(u8 world, s32 centerX, s32 centerY, u16 imageW, u16 imageH, float scaleX, float scaleY)
	WA[17].head = WRLD_END;
	//WA[16].head = WRLD_END;
	//WA[16].head = WRLD_END;
	/*
	WA[30].head = WRLD_ON;
	WA[30].w = 384;
	WA[30].h = 224;
	WA[29].head = WRLD_ON;
	WA[29].w = 384;
	WA[29].h = 224;*/
	/*
	WA[28].head = WRLD_ON;
	WA[28].w = 384;
	WA[28].h = 224;

	//WA[30].head = WRLD_END;
	WA[27].head = WRLD_END;*/
	/*
	WA[29].head = WRLD_ON | WRLD_OVR;
	WA[29].w = 384;
	WA[29].h = 224;
	WA[28].head = WRLD_END;*/

	/*
	SND_REGS[1].SxLRV = 0;
	SND_REGS[2].SxLRV = 0;
	SND_REGS[3].SxLRV = 0;
	SND_REGS[4].SxLRV = 0;*/

    //vbSetWorld(31, WRLD_ON | WRLD_OBJ, 0, 0, 0, 0, 0, 0, 382, 224);
    //vbSetWorld(30, WRLD_END, 0, 0, 0, 0, 0, 0, 0, 0);
/*
	VIP_REGS[SPT3] = 1023;
    VIP_REGS[SPT2] = 1012;// 1023 - 10 - 1
    VIP_REGS[SPT1] = 991;// 1012 - 20 - 1
*/
	loadDoomGfxToMem();
	initEnemies();
	//drawDoomUI(0, 0, 24);
	//drawDoomGuy(0, 44, 24, 0);

    vbDisplayShow();


	//vbFXFadeIn(0);

	u8 doomface = 0;
	u8 updateDoomfaceTime = 0;
	u8 updateDoomfaceCount = 0;

	u16 ammo = 50;
	u8 currentAmmoType = 1;
	u8 currentHealth = 100;
	u8 currentArmour = 0;





	// we only need to draw doom ui once...
	//drawUpdatedAmmo(ammo, currentAmmoType);
	//drawHealth(currentHealth);
	//drawArmour(currentArmour);

	//affine_clr_param(30);
	//affine_fast_scale(30, 1.0f);

	VIP_REGS[GPLT0] = 0xE4;	/* Set 1st palettes to: 11100100 */
	VIP_REGS[GPLT1] = 0x00;	/* (i.e. "Normal" dark to light progression.) */
	VIP_REGS[GPLT2] = 0x84; // VIP_REGS[GPLT2] = 0xC2;

	VIP_REGS[GPLT3] = 0xA2;
	VIP_REGS[JPLT0] = 0xE4; // object palettes (we dont use objects...)
	VIP_REGS[JPLT1] = 0xE4;
	VIP_REGS[JPLT2] = 0xE4;
	VIP_REGS[JPLT3] = 0xE4;

	int x,y,i;
	i = 32; // 8 och 9 och 12 påverkar...
	// om i = 14, 31 så blir de svart...

	//for (worldCount = 0; worldCount < 9; worldCount++) {
/*
		for (y = 0; y < 32; y++) {
			for (x = 0; x < 32; x++) {
				BGM_PALSET(1, x, y, GPLT1);
			}
		}*/
		//BGM_PALSET(1,1,2,BGM_PAL3);
		//SET_GPLT(worldCount, GPLT1);
	//}
	//SET_GPLT(GPLT0, 0xE4);
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
	//setmem((void*)BGMap(14), 0, 384*327);

	/* Enemy BGMap/char init MUST be after setmem clears BGMaps */
	initEnemyBGMaps();                          /* set up BGMap(3), (4), (5) tile entries */
	loadEnemyFrame(0, Zombie_000Tiles);         /* load frame 0 into enemy 0 char memory */
	loadEnemyFrame(1, Zombie_000Tiles);         /* load frame 0 into enemy 1 char memory */
	loadEnemyFrame(2, Zombie_000Tiles);         /* load frame 0 into enemy 2 char memory */

	/* Pickup BGMap/char init */
	initPickups();
	initPickupBGMaps();
	/* Pre-load one of each pickup type into char slots */
	loadPickupFrame(0, PICKUP_TILES[PICKUP_AMMO_CLIP]);
	loadPickupFrame(1, PICKUP_TILES[PICKUP_HEALTH_SMALL]);
	loadPickupFrame(2, PICKUP_TILES[PICKUP_HEALTH_LARGE]);

	drawDoomUI(LAYER_UI, 0, 0);
	drawUpdatedAmmo(ammo, currentAmmoType);

	drawHealth(currentHealth);
	drawArmour(currentArmour);



	drawDoomFace(&doomface);
	drawWeapon(currentWeapon, swayXTBL[weaponSwayIndex], swayYTBL[weaponSwayIndex], weaponAnimation, 0);
	u16 keyInputs;

	mp_init();
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

			if (isFixed) {
			// inc player angle and shiz.
				jawFixedPlayer(-fixed0point05);
			} else {
				jawPlayer (-0.05f);
			}

		} else if(keyInputs & K_RD) {// Right Pad, Down

			if (isFixed) {
			// inc player
				jawFixedPlayer(fixed0point05);
			} else {
				jawPlayer(0.05f);
			}
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
					drawUpdatedAmmo(weapons[currentWeapon].ammo, weapons[currentWeapon].ammoType);
					playerShoot(fPlayerX, fPlayerY, fPlayerAng); /* hitscan + alerts all enemies */
				} else {
					cycleWeapons();
				}
			} else {
			 	weaponAnimation = 1; // punch!?
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
		// we actually only need to call this if x,y,ang changed...
		updateEnemies(fPlayerX, fPlayerY, fPlayerAng);

		/* Check pickup collisions */
		{
			u8 pickupAmmo = weapons[currentWeapon].ammo;
			if (updatePickups(fPlayerX, fPlayerY, &pickupAmmo, &currentHealth)) {
				/* Something was picked up - update HUD */
				weapons[currentWeapon].ammo = pickupAmmo;
				drawUpdatedAmmo(weapons[currentWeapon].ammo, weapons[currentWeapon].ammoType);
				drawHealth(currentHealth);
			}
		}

		/* Update enemy sprite frames in VRAM (only if frame changed) */
		{
			u8 ei;
			for (ei = 0; ei < MAX_ENEMIES; ei++) {
				u8 frameIdx;
				if (!g_enemies[ei].active) continue;
				frameIdx = getEnemySpriteFrame(ei, fPlayerX, fPlayerY, fPlayerAng);
				if (frameIdx != g_enemies[ei].lastRenderedFrame) {
					loadEnemyFrame(ei, ZOMBIE_FRAMES[frameIdx]);
					g_enemies[ei].lastRenderedFrame = frameIdx;
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
		// draw each frame, but really only necessary if it's not the same as previous frame
		if (updateDoomfaceCount > updateDoomfaceTime) {
			doomface = (rand()%3); // randomize next facial expression
			updateDoomfaceCount = 0;
			updateDoomfaceTime = 9+rand()%8; // randomize delay for next update
			drawDoomFace(&doomface);
		} else {
			updateDoomfaceCount++;
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
			if (updatePistolCount > 3) {
				weaponAnimation++;
				if (weaponAnimation >= weapons[currentWeapon].attackFrames) {
					isShooting = false;
					weaponAnimation = 0;
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
			if (weaponChangeTimer > 10) {
				nextWeapon = currentWeapon; // switch weapon after half time...
			}
			if (weaponChangeTimer >= weaponChangeTime) {
				weaponChangeTimer = 0;
				isChangingWeapon = false;
			}
			weaponChangeTimer++;
		}
		comboChangeAndFire= isShooting && isChangingWeapon == false;
		playSnd(&comboChangeAndFire, &currentWeapon, &isPlayMusicBool);

		drawPlayerInfo(&fPlayerX, &fPlayerY, &fPlayerAng);

		/* Screen flash effect (pickup or damage) */
		if (g_flashTimer > 0) {
			g_flashTimer--;
			if (g_flashType == 0) {
				/* Pickup flash: all bright red */
				VIP_REGS[GPLT0] = 0xFF;  /* all indices -> brightest */
				VIP_REGS[GPLT1] = 0xFF;
				VIP_REGS[GPLT2] = 0xFF;
			} else {
				/* Damage flash: red tint (shift palette darker) */
				VIP_REGS[GPLT0] = 0xFE;
				VIP_REGS[GPLT1] = 0xFE;
				VIP_REGS[GPLT2] = 0xFE;
			}
		} else {
			/* Restore normal palettes */
			VIP_REGS[GPLT0] = 0xE4;
			VIP_REGS[GPLT1] = 0x00;
			VIP_REGS[GPLT2] = 0x84;
		}

		vbWaitFrame(0); /* Sync to VBlank: ~50fps on real hardware.
		                 * 0 = wait 1 VBlank (~50fps), 1 = wait 2 VBlanks (~25fps).
		                 * Without this, emulators run the loop uncapped. */
	}
	return 0;
}
void swayWeapon() {
	//WA[31].gy = walkSwayYTBL[walkSwayIndex]-8;
}