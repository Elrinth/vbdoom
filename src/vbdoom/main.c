#include <functions.h>
#include <components.h>
#include <settings.h>

int main()
{
	u8 scene = 1;

	Settings settings = {
		9,
		4, // music at half volume so sfx is more audible...
		0
	};
	//u8 sfxVol = 9;
	//u8 musicVol = 9;
	//u8 rumble = 0;
    initSystem();
    //gameLoop();
    while(scene != 5) {
    	if (scene == 0)
    		scene = titleScreen();
    	if  (scene == 1) {
			scene = gameLoop();
    	}
    	if (scene == 4) {
			scene = optionsScreen(&settings); // settings ska skicka med överallt?!
		}
    	if (scene == 5) {
    		scene = creditsScreen();
    	}
    }

	//precautionScreen();
	//adjustmentScreen();
	//languageSelectionScreen();



    return 0;
}