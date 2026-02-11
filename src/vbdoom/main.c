#include <functions.h>
#include <components.h>
#include <settings.h>
#include "functions/sndplay.h"
#include "functions/timer.h"
#include "components/intermission.h"

int main()
{
	u8 scene = 0;

	Settings settings = {
		9,
		5,
		0
	};
    initSystem();
    setFrameTime(43);  /* ~23.3 fps target; further slowed to match hardware */
    mp_init();  /* Initialize PCM audio before title screen (for menu SFX) */

    /* Map settings to hardware volume levels */
    g_sfxVolume = (u8)((u16)settings.sfx * 15 / 9);
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
    		musicLoadSong(SONG_E1M1);
    		musicStart();
			/* Show the episode map before gameplay starts */
			showIntermission();
			scene = gameLoop();
			/* When returning from game, switch back to title music */
			musicLoadSong(SONG_TITLE);
			musicStart();
    	}
    	if (scene == 4) {
			scene = optionsScreen(&settings);
			/* Update volumes from potentially changed settings */
			g_sfxVolume = (u8)((u16)settings.sfx * 15 / 9);
			g_musicSetting = settings.music;
			g_musicVolume = musicVolFromSetting(settings.music);
			/* No stop/start needed -- sequencer keeps running silently at vol 0
			 * and resumes audibly when vol comes back. */
		}
    	if (scene == 5) {
    		scene = creditsScreen();
    	}
    }

	//precautionScreen();
	//languageSelectionScreen();



    return 0;
}