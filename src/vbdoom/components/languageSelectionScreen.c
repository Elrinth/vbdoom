#include <constants.h>
#include <controller.h>
#include <mem.h>
#include <video.h>
#include <world.h>
#include <functions.h>
#include <languages.h>
#include "languageSelectionScreen.h"

extern u8 currentLanguage;

void languageSelectionScreen()
{
	u8 i = 0;
	int numLangs = 0;

    clearScreen();

	WA[31].head = WRLD_ON|WRLD_OVR;
	WA[31].w = 384;
	WA[31].h = 224;
	WA[30].head = WRLD_END;

    printString(0, 17, 10 + currentLanguage, ">");

	for(i = 0; __LANGUAGES[i]; i++) {
		printString(0, 18, 10 + i, (char *)__LANGUAGES[i][STR_LANGUAGE]);
		numLangs++;
	}

	vbFXFadeIn(0);
	vbWaitFrame(20);

	u16 keyInputs;

	while(1) {
		keyInputs = vbReadPad();
		if(keyInputs & (K_LU) && (currentLanguage > 0)) {
            currentLanguage--;
            printString(0, 17, 10 + currentLanguage + 1, " ");
            printString(0, 17, 10 + currentLanguage, ">");
            vbWaitFrame(3);
		} else if(keyInputs & (K_LD) && (currentLanguage < (numLangs-1))) {
            currentLanguage++;
            printString(0, 17, 10 + currentLanguage - 1, " ");
            printString(0, 17, 10 + currentLanguage, ">");
            vbWaitFrame(3);
		} else if(keyInputs & K_STA || keyInputs & K_A) {
			break;
		}
		vbWaitFrame(3);
	}

	vbFXFadeOut(0);
}