#ifndef _COMPONENTS_MENU_MULTIPLAYER_H
#define _COMPONENTS_MENU_MULTIPLAYER_H

#include <types.h>

/*
 * Multiplayer menu: HOST/JOIN selection, connection handshake,
 * level/mode selection.
 *
 * Returns:
 *   0 = cancelled (return to title)
 *   1-6 = level number to start (1=E1M1, ..., 4=E1M4, 7=DM Arena)
 *
 * Sets g_isMultiplayer, g_isHost, g_gameMode globals in link.h.
 */
u8 multiplayerMenu(void);

#endif
