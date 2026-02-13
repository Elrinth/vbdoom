#include <functions.h>
#include <components.h>
#include <settings.h>
#include "functions/sndplay.h"
#include "functions/timer.h"
#include "functions/link.h"
#include "components/intermission.h"
#include "components/menu_multiplayer.h"

/* Volume conversion LUT: setting (0-9) -> hardware (0-15) */
static const u8 g_settingToHw[10] = {0,1,3,5,6,8,10,11,13,15};

int main()
{
	u8 scene = 0;

	Settings settings = {
		9,
		3,
		0
	};
    initSystem();
    setFrameTime(43);  /* ~23.3 fps target; further slowed to match hardware */
    mp_init();  /* Initialize PCM audio before title screen (for menu SFX) */

    /* Map settings to hardware volume levels */
    g_sfxVolume = g_settingToHw[settings.sfx];
    g_musicSetting = settings.music;
    g_musicVolume = musicVolFromSetting(settings.music);

    /* Start title screen music */
    musicLoadSong(SONG_TITLE);
    musicStart();

    while(scene != 5) {
    	if (scene == 0)
    		scene = titleScreen();
    	if  (scene == 1) {
    		/* Switch to level music when entering the game */
    		g_startLevel = 1;
    		musicLoadSong(SONG_E1M1);
    		musicStart();
			/* Show the episode map before gameplay starts */
			showIntermission();
			scene = gameLoop();
			/* When returning from game, switch back to title music */
			musicLoadSong(SONG_TITLE);
			musicStart();
    	}
    	if (scene == 3) {
    		/* Multiplayer menu: returns level number (1-7) or 0 (cancel) */
    		u8 mpLevel = multiplayerMenu();
    		if (mpLevel > 0) {
    			/* Start multiplayer game with selected level */
    			g_startLevel = mpLevel;
    			musicLoadSong(SONG_E1M1);
    			musicStart();
    			if (g_gameMode != GAMEMODE_DEATHMATCH)
    				showIntermission();
    			scene = gameLoop();
    			/* Reset multiplayer state when returning */
    			g_isMultiplayer = false;
    			musicLoadSong(SONG_TITLE);
    			musicStart();
    		} else {
    			scene = 0;  /* cancelled, return to title */
    		}
    	}
    	if (scene == 4) {
			scene = optionsScreen(&settings);
			/* Update volumes from potentially changed settings */
			g_sfxVolume = g_settingToHw[settings.sfx];
			g_musicSetting = settings.music;
			g_musicVolume = musicVolFromSetting(settings.music);
			g_rumbleSetting = settings.rumble;
			/* Sync music with volume setting */
			if (g_musicVolume == 0) {
				musicStop();
			} else if (!isMusicPlaying()) {
				musicStart();
			}
		}
    	if (scene == 5) {
    		scene = creditsScreen();
    	}
    }
    return 0;
}