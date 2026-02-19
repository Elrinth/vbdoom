#ifndef _COMPONENTS_MENU_OPTIONS_H
#define _COMPONENTS_MENU_OPTIONS_H

#include <stdbool.h>
#include "settings.h"

struct SaveState;
/* inGame: true when opened from in-game pause. saveState: non-NULL to show SAVE GAME and write on confirm. */
u8 optionsScreen(Settings *settings, bool inGame, const struct SaveState *saveState);

#endif