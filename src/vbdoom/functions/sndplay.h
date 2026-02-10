#ifndef __SNDPLAY_
#define __SNDPLAY_
#include <stdbool.h>


void playSnd(bool *isFiring, u8 *iWeaponType, bool *isPlayMusic);
void mp_init();

/* Pickup sound: play a short blip on channel 1 (WAVE2).
 * Call once when player picks up an item. */
void playPickupSound(void);

#endif