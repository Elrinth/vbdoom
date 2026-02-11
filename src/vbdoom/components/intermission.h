#ifndef _COMPONENTS_INTERMISSION_H
#define _COMPONENTS_INTERMISSION_H

/* Show the intermission screen (le_map.png) with bleed-down reveal,
 * skull blinking animation, and bleed-down hide.
 * Overwrites VRAM -- caller must restore game display state afterwards. */
void showIntermission(void);

/* Show level completion statistics screen (kills, items, secrets, time).
 * Counting-up animation, waits for button press.
 * Overwrites VRAM -- caller must restore game display state afterwards. */
void showLevelStats(void);

#endif
