#include <functions.h>
#include <components.h>
#include <settings.h>
#include "functions/sndplay.h"
#include "functions/timer.h"

int main()
{
	u8 scene = 0;

	Settings settings = {
		9,
		4, // music at half volume so sfx is more audible...
		0
	};
    initSystem();
    mp_init();  /* Initialize PCM audio before title screen (for menu SFX) */

    /* Map settings.sfx (0-9) to g_sfxVolume (0-15) */
    g_sfxVolume = (u8)((u16)settings.sfx * 15 / 9);

    /* Map settings.music (0-9) to g_musicVolume (0-15) */
    g_musicVolume = (u8)((u16)settings.music * 15 / 9);

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
			scene = gameLoop();
			/* When returning from game, switch back to title music */
			musicLoadSong(SONG_TITLE);
			musicStart();
    	}
    	if (scene == 4) {
			scene = optionsScreen(&settings);
			/* Update volumes from potentially changed settings */
			g_sfxVolume = (u8)((u16)settings.sfx * 15 / 9);
			g_musicVolume = (u8)((u16)settings.music * 15 / 9);
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