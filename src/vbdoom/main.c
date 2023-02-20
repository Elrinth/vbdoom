#include <functions.h>
#include <components.h>

int main()
{
	u8 scene = 0;
    initSystem();
    gameLoop();
    while(1) {
    	if (scene == 0)
    		scene = titleScreen();
    	if  (scene == 1) {
			scene = gameLoop();
    	}
    	if (scene == 4) {
    		scene = creditsScreen();
    	}
    }

	//precautionScreen();
	//adjustmentScreen();
	//languageSelectionScreen();



    return 0;
}