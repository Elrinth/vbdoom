#ifndef _COMPONENTS_GAME_LOOP_H
#define _COMPONENTS_GAME_LOOP_H

extern u8 g_startLevel;  /* set before calling gameLoop() to choose start level */

u8 gameLoop();
u8 switchWeapon(u8 iFrom, u8 iTo);
void cycleWeapons();
void cycleNextWeapon();
void move(int m, int r);
void swayWeapon();

#endif