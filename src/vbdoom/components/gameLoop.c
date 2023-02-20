#include <functions.h>
#include <math.h>
#include <languages.h>
#include "doomstage.h"
#include "doomgfx.h"
#include <input.h>
#include "gameLoop.h"
extern BYTE FontTiles[];

s16 fixedAngleRatioz = FTOFIX13_3(512.0f/360.0f);
s16 fixed0point05 = 0; // calced below
s16 fixed2point13 = 0;

u8 switchWeapon(u8 iFrom, u8 iTo)
{
	if (iTo > 1 || iTo < 8) {
		return iTo;
	}
	return iTo;
}

u8 gameLoop()
{
	u8 isFixed = 0;

	fixed0point05 = FIX13_3_MULT( FTOFIX13_3(0.05f), fixedAngleRatioz);
	fixed2point13 = FIX13_3_MULT( FTOFIX13_3(2.13f), fixedAngleRatioz);

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
	vbSetWorld(31, WRLD_ON|1, 0, 0, 0, 0, 0, 0, 384, 224); // stage background

	// monster bgmaps

	u16 EnemyScale = 4; // max scale
	u16 EnemyScaleX = 6; // max scale
	vbSetWorld(30, WRLD_ON|2|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 40*EnemyScaleX, 56*EnemyScale);
	vbSetWorld(29, WRLD_ON|3|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 40*EnemyScaleX, 56*EnemyScale);
	vbSetWorld(28, WRLD_ON|4|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 40*EnemyScaleX, 56*EnemyScale);
	vbSetWorld(27, WRLD_ON|5|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 40*EnemyScaleX, 56*EnemyScale);
	vbSetWorld(26, WRLD_ON|6|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 40*EnemyScaleX, 56*EnemyScale);
	vbSetWorld(25, WRLD_ON|7|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 40*EnemyScaleX, 56*EnemyScale);

	vbSetWorld(24, WRLD_ON|8, 					0, 0, 0, 0, 0, 0, 20*EnemyScale, 26*EnemyScale); // more bullets
	vbSetWorld(23, WRLD_ON|LAYER_BULLET, 		0, 0, 0, 0, 0, 0, 20*EnemyScale, 26*EnemyScale); // bullets
	vbSetWorld(22, WRLD_ON|LAYER_WEAPON_BLACK, 	0, 0, 0, 0, 0, 0, 160, 184); // weapon black
	vbSetWorld(21, WRLD_ON|LAYER_WEAPON, 		0, 0, 0, 0, 0, 0, 160, 184); // weapon
	vbSetWorld(20, WRLD_ON|LAYER_UI_BLACK, 		0, 0, 0, 0, 0, 0, 384, 32); // ui black
	vbSetWorld(19, WRLD_ON|LAYER_UI, 			0, 0, 0, 0, 0, 0, 384, 32); // ui
	//vbSetWorld(18, WRLD_ON|14, 0, 0, 0, 0, 0, 0, 384, 224); // stage background
	//vbSetWorld(17, WRLD_ON|15, 0, 0, 0, 0, 0, 0, 384, 224); // stage background

	WA[20].gy = 192;
	WA[19].gy = 192;


	//vbSetWorld(29, WRLD_ON|2|WRLD_AFFINE, 0, 0, 0, 0, 0, 0, 384, 224);
	//affine_clr_param(29);



	//affine_scale(u8 world, s16 centerX, s16 centerY, u16 imageW, u16 imageH, float scaleX, float scaleY)
	WA[18].head = WRLD_END;
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
	//drawDoomUI(0, 0, 24);
	//drawDoomGuy(0, 44, 24, 0);

    vbDisplayShow();


	//vbFXFadeIn(0);

	u8 doomface = 0;
	u8 updateDoomfaceTime = 0;
	u8 updateDoomfaceCount = 0;
	u8 updatePistolCount = 0;
	u8 weaponAnimation = 0;

	u16 ammo = 50;
	u8 currentWeapon = 2;
	u8 currentAmmoType = 1;
	u8 currentHealth = 100;
	u8 currentArmour = 0;


	// we only need to draw doom ui once...
	//drawUpdatedAmmo(ammo, currentAmmoType);
	//drawHealth(currentHealth);
	//drawArmour(currentArmour);

	//affine_clr_param(30);
	//affine_fast_scale(30, 1.0f);

	VIP_REGS[GPLT0] = 0xE4;	/* Set all eight palettes to: 11100100 */
	VIP_REGS[GPLT1] = 0x00;	/* (i.e. "Normal" dark to light progression.) */
	VIP_REGS[GPLT2] = 0x84;
	/*VIP_REGS[GPLT2] = 0xC2;
	VIP_REGS[GPLT3] = 0xA2;
	VIP_REGS[JPLT0] = 0xE4;
	VIP_REGS[JPLT1] = 0xE4;
	VIP_REGS[JPLT2] = 0xE4;
	VIP_REGS[JPLT3] = 0xE4;*/

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

		drawDoomUI(LAYER_UI, 0, 0);
		drawUpdatedAmmo(ammo, currentAmmoType);

		drawHealth(currentHealth);
		drawArmour(currentArmour);
		drawDoomFace(LAYER_UI, 44, 0, doomface);
			drawWeapon(currentWeapon, weaponAnimation);
	u16 keyInputs;
	while(1) {
	// clear 1 (weapon layer) and 2 (enemy layer) each frame...
		/*for (worldCount = 1; worldCount < 7; worldCount++) {
			setmem((void*)BGMap(worldCount), 0x0000, 8192);
		}*/
		// 8192
		// 56 x 96 (384/4 * 224/4) = 5376
		// 10 x 14 (40/4 * 56/4) = 140 (Enemy layer)
		// 40 x 46 (160/4 x 184/4) = 1840 (Weapon layer)
		setmem((void*)BGMap(2), 0x0000, 140); // clear enemy layer 1...
		setmem((void*)BGMap(3), 0x0000, 140); // clear enemy layer 1...
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
			if (isFixed) {
				fixedPlayerMoveForward(ITOFIX13_3(3));
			} else {
				playerMoveForward(3);
			}
		} else if (keyInputs & K_LD) {// Left Pad, Down
			if (isFixed) {
				fixedPlayerMoveForward(ITOFIX13_3(-3));
			 } else {
				playerMoveForward(-3);
			}
		}
		if (keyInputs & K_LL) {// Left Pad, Left'
			if (isFixed) {
				fixedPlayerStrafe(ITOFIX13_3(-3));
			} else {
				playerStrafe(-3);
			}
		} else if (keyInputs & K_LR) {// Left Pad, Right
			if (isFixed) {
				fixedPlayerStrafe(ITOFIX13_3(+3));
			} else {
				playerStrafe(+3);
			}
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
			if (isFixed) {
				incFixedPlayerAngle(-fixed2point13);
			} else {
				// inc player angle and shiz.
				incPlayerAngle(-2.13f);
			}
		} else if(keyInputs & K_RR) {// Right Pad, Right
			// inc player
			if (isFixed) {
				incFixedPlayerAngle(fixed2point13);
			} else {
				incPlayerAngle(2.13f);
			}

		}

		if (keyInputs & (K_B | K_RT) && updatePistolCount == 0 && weaponAnimation == 0) {
			drawUpdatedAmmo(ammo, currentAmmoType);
			if (currentWeapon > 1) {
				if (ammo > 0) {
					weaponAnimation = 1; // shoot
					ammo--;
					//drawUpdatedAmmo(ammo, currentAmmoType);
				}
				else {
					currentWeapon = switchWeapon(currentWeapon, currentWeapon-1);
					if (currentWeapon == 1) {
						currentAmmoType = 0;
					}
				}
			} else {
				weaponAnimation = 1; // punch
			}
			setmem((void*)BGMap(LAYER_WEAPON_BLACK), 0, 1840);
			setmem((void*)BGMap(LAYER_WEAPON), 0, 1840);
			drawWeapon(currentWeapon, weaponAnimation);
		}
 		//printString(0, 14, 10, getString(STR_TESTICUS));
 		//printString(0, 34, 10, "hello");
		//drawDoomGuy(0, 44, 24, doomface);
		if (isFixed) {
			fixedDrawDoomStage(0);
		} else {
			drawDoomStage(0);
		}

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
			drawDoomFace(LAYER_UI, 44, 0, doomface);
		} else {
			updateDoomfaceCount++;
		}
		//VIP_REGS[GPLT0] = 0xD2;
		//VIP_REGS[GPLT0] = 0xE4;

		// only need to draw if changed...
		//drawDoomPistol(0,42,16, pistolAnimation);

		//vbSetObject(u16 n, u8 header, s16 x, s16 p, s16 y, u16 chr)
		//vbSetObject(	  1024, 		 OBJ_ON, 	16,	   0,	  16,	   8);

		if (weaponAnimation == 1) {
			updatePistolCount++;
			if (updatePistolCount > 4) {
				updatePistolCount = 0;
				weaponAnimation = 0;
			}
			setmem((void*)BGMap(LAYER_WEAPON_BLACK), 0, 1840);
			setmem((void*)BGMap(LAYER_WEAPON), 0, 1840);
			drawWeapon(currentWeapon, weaponAnimation);
		}
		//copymem(0x1000, (void*)FontTiles, 1024);
		//setmem((void*)CharSeg0, 0x0000, 2048);
        	//setmem((void*)BGMap(0), 0x0000, 8192);
		//copymem((void*)(CharSeg3+0x1000), (void*)FontTiles, 256*16);

		//setmem((void*)FontTiles, 0x1000, 1024);
       	//setmem((void*)BGMap(0), 0x0000, 8192);
		//printString(1, 30, 20, getString(STR_HELLO_WORLD));

		vbWaitFrame(0);
	}
	return 0;
}