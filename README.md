# vbdoom
Virtual Boy Doom Project

Currently trying to get fixed-point math work (it's commited like NOT running with fixed-point atm, incase anyone wanted to experience it).
Simply flip switch in top of gameLoop.c where it says: u8 isFixed = 0; and set that to 1 instead.

![Image of VBDoom](https://raw.githubusercontent.com/Elrinth/vbdoom/main/2023-02-20_screen.png)

# to build:

pre requisieigiwsetion:
1. Install VBDE in c:\vbde (you can download here: https://www.virtual-boy.com/tools/vbde/downloads/980946/
2. put this project so it's in "C:\vbde\my_projects\vbdoom" (src and libs folder should be under that folder)
3. Open project in VBDE
4. Click the wrench with play icon
