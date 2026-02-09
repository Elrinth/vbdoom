#include <libgccvb.h>
#include <mem.h>
#include <misc.h>
#include "text.h"
#include <video.h>
#include <world.h>
#include <constants.h>
#include <languages.h>
#include <stdint.h>
#include "doomstage.h"

typedef int64_t s64;

extern BYTE vb_doomMap[];
extern u16 e1m1Segs[];
extern int e1m1Vertices[];
extern int e1m1Things[];

extern u16 e1m1SegsLength;
extern u16 e1m1VerticesLength;
extern u16 e1m1ThingsLength;

struct xy { float x,y; } *vertex;

struct secx1x2 { int sec; int sx1,sx2; } now;
struct fsecx1x2 { s32 sec; s32 sx1,sx2; } fnow;
//now = { sectorno: player.sectorno, sx1: 0, sx2: W - 1 };

int sectorCeil = 128;
int sectorFloor = 0;

s32 fSectorCeil = ITOFIX23_9(128);
s32 fSectorFloor = ITOFIX23_9(0);

s32 fixedPlayerAngle;
s32 fixedPlayerX,fixedPlayerY,fixedPlayerZ;
s32 fixedAngleRatio = FTOFIX23_9(512.0f/360.0f); // 512.0f/
s32 fixedPlayerAngleCos;
s32 fixedPlayerAngleSin;
s32 fixedPlayerYaw;
s32 fixedNearZ = FTOFIX23_9(1e-4f);
s32 fixedFarZ = ITOFIX23_9(5);
s32 fixedNearside = FTOFIX23_9(1e-5f);
s32 fixedFarside = FTOFIX23_9(20.0f);

s32 fixed512 = FTOFIX23_9(512.0f);
s32 fixed90 = FTOFIX23_9(90.0f);


float playerAngle;
float playerX,playerY,playerZ;
float angleRatio = 512.0f/360.0f; // 1.42222
float playerAngleCos;
float playerAngleSin;
float playerYaw;
#define FixedYaw(y,z) (y + FIX23_9_MULT(z, fixedPlayerYaw))
#define Yaw(y,z) (y + z*playerYaw)
float nearz = 1e-4f;
float farz = 5;
float nearside = 1e-5f;
float farside = 20.0f;
/* Define window size */
#define EyeHeight 41
#define FixedEyeHeight ITOFIX23_9(EyeHeight)
#define W (384) // vb screen width
#define Wby2 (W/2)
#define H (224-32) // reduce by interface height which is 32..
#define Hby2 (H/2)


//#define hfov (0.73f*H)  // Affects the horizontal field of vision
//#define vfov (.2f*H)    // Affects the vertical field of vision
#define hfov (0.7f*H)  // Affects the horizontal field of vision
#define vfov (.8f*H)    // Affects the vertical field of vision

#define fhfov FTOFIX23_9(hfov)  // Affects the horizontal field of vision
#define fvfov FTOFIX23_9(vfov)    // Affects the vertical field of vision

int ytop[W];
int ybottom[H];

//secx1x2 now;

void initializeDoomStage() {

	now.sx1 = 0;
	now.sx2 = W-1;
	fnow.sx1 = ITOFIX23_9(0);
	fnow.sx2 = ITOFIX23_9(W-1);
	int x;
	for(x=0; x<W; x++) {
		ytop[x] = 0;
		ybottom[x] = H-1;
	}
	u16 i;
	for (i = 0; i < e1m1ThingsLength; i+=5) {
		// 0x,1y,2angle,3type,4flags
		if (e1m1Things[i+3] == 1) {
			// the player
			playerX = e1m1Things[i];
			playerY = e1m1Things[i+1];
			playerZ = 0;
			playerYaw = 0;

			fixedPlayerX = ITOFIX23_9(e1m1Things[i]);
			fixedPlayerY = ITOFIX23_9(e1m1Things[i+1]);
			fixedPlayerZ = ITOFIX23_9(0);
			fixedPlayerYaw = ITOFIX23_9(0);

			playerAngle = e1m1Things[i+2]*angleRatio;
			playerAngleCos = COSF((int)playerAngle);
			playerAngleSin = SINF((int)playerAngle);


			fixedPlayerAngle = FIX23_9_MULT(ITOFIX23_9(e1m1Things[i+2]), fixedAngleRatio);
			fixedPlayerAngleCos = COS(FIX23_9TOI(fixedPlayerAngle));
			fixedPlayerAngleSin = SIN(FIX23_9TOI(fixedPlayerAngle));
		}
	}
}
void clearDoomStage(u8 bgmap) {
	u16 pos = 0;
	u16 y = 0;
	u16 x = 0;
	for (y = 0; y < H; y+=8) {
		for (x = 0; x < W; x+=8) {
			//tilePosition = (((i2)/8) * 128) + ((iX*2)/8);
			pos = (y<<4) + (x>>2);
			copymem((void*)BGMap(bgmap)+pos, (void*)(vb_doomMap+251), 1);
		}
	}
}

/*
s32 MM_ITOFIX23_9(n) {
  return (n << 19 >> 16);
}
s32 MM_FIX23_9_MULT(a, b) {
  return (a * b << 16) >> 19;
}
int MM_FIX23_9TOI(n) {
  return (n << 16 >> 19);
}*/

void fixedDrawDoomStage(u8 bgmap) {

	s32 fvx1=0;
	s32 fvx2=0;
	s32 fvy1=0;
	s32 fvy2=0;

	s32 ftx1=0;
	s32 ftz1=0;
	s32 ftx2=0;
	s32 ftz2=0;

	int fy1a = 0;
	int fy1b = 0;
	int fy2a = 0;
	int fy2b = 0;

	s32 fyceil = 0;
	s32 fyfloor = 0;
	int x = 0;
	int wallsNotDrawn = 0;
	int index = 0;

	u16 segNotDrawn1 = 0;
	u16 segNotDrawn2 = 0;
	u16 vertexIndex1 = 0;
	u16 vertexIndex2 = 0;

	s32 fixedWBy2 = ITOFIX23_9(Wby2);
	s32 fixedHBy2 = ITOFIX23_9(Hby2);
	s32 fixedZero = ITOFIX23_9(0);

	s32 fixedPlayerZPlusEyeHeight = (fixedPlayerZ + FixedEyeHeight);

	u16 i = 0;
	// iterate walls
	for (i = 0; i < e1m1SegsLength; i+= 2) {

		vertexIndex1 = e1m1Segs[i]<<1;
		vertexIndex2 = e1m1Segs[i+1]<<1;

		fvx1 = fixedPlayerX - ITOFIX23_9(e1m1Vertices[ vertexIndex1 ]);
		fvy1 = ITOFIX23_9(e1m1Vertices[ vertexIndex1+1 ]) - fixedPlayerY;

		fvx2 = fixedPlayerX - ITOFIX23_9(e1m1Vertices[ vertexIndex2 ]);
		fvy2 = ITOFIX23_9(e1m1Vertices[ vertexIndex2+1 ]) - fixedPlayerY;

		ftx1 = FIX23_9_MULT(fvx1, fixedPlayerAngleSin) - FIX23_9_MULT(fvy1, fixedPlayerAngleCos);
		ftz1 = FIX23_9_MULT(fvx1, fixedPlayerAngleCos) + FIX23_9_MULT(fvy1, fixedPlayerAngleSin);

		ftx2 = FIX23_9_MULT(fvx2, fixedPlayerAngleSin) - FIX23_9_MULT(fvy2, fixedPlayerAngleCos);
		ftz2 = FIX23_9_MULT(fvx2, fixedPlayerAngleCos) + FIX23_9_MULT(fvy2, fixedPlayerAngleSin);

		if (ftz1 <= fixedZero && ftz2 <= fixedZero) {
			//segNotDrawn1 = vertexIndex1;
			//segNotDrawn2 = vertexIndex2;
			wallsNotDrawn++;
			continue;
		}
		if (ftz1 <= ITOFIX23_9(0) || ftz2 <= ITOFIX23_9(0)) {
		  	struct xy fi1 = FixedIntersect(ftx1,ftz1,ftx2,ftz2, -fixedNearside, fixedNearZ, -fixedFarside, fixedNearZ);
		  	struct xy fi2 = FixedIntersect(ftx1,ftz1,ftx2,ftz2,  fixedNearside, fixedNearZ, fixedFarside, fixedFarZ);
			if (ftz1 < fixedNearZ) {
				if (fi1.y > fixedZero) {
					ftx1 = fi1.x;
					ftz1 = fi1.y;
				} else {
					ftx1 = fi2.x;
					ftz1 = fi2.y;
				}
			}
			if (ftz2 < fixedNearZ) {
				if (fi1.y > fixedZero) {
					ftx2 = fi1.x;
					ftz2 = fi1.y;
				} else {
					ftx2 = fi2.x;
					ftz2 = fi2.y;
				}
			}
		}

		/* Do perspective transformation */
		s32 fxscale1 = FIX23_9_DIV( fhfov, ftz1 );
		s32 fyscale1 = FIX23_9_DIV( fvfov, ftz1 );
		s32 fx1 = fixedWBy2 - FIX23_9_MULT(ftx1, fxscale1);
		s32 fxscale2 = FIX23_9_DIV(fhfov, ftz2);
		s32 fyscale2 = FIX23_9_DIV(fvfov, ftz2);
		s32 fx2 = fixedWBy2 - FIX23_9_MULT(ftx2, fxscale2);

		/*if(fx1 >= fx2 || fx2 < fnow.sx1 || fx1 > fnow.sx2) {
			wallsNotDrawn++;
			continue; // Only render if it's visible
		}*/
		fyceil = fSectorCeil - fixedPlayerZPlusEyeHeight;
		fyfloor = fSectorFloor - fixedPlayerZPlusEyeHeight;
		fy1a = fixedHBy2 - FIX23_9_MULT(FixedYaw(fyceil, ftz1), fyscale1);
		fy1b = fixedHBy2 - FIX23_9_MULT(FixedYaw(fyfloor, ftz1), fyscale1);
		fy2a = fixedHBy2 - FIX23_9_MULT(FixedYaw(fyceil, ftz2), fyscale2);
		fy2b = fixedHBy2 - FIX23_9_MULT(FixedYaw(fyfloor, ftz2), fyscale2);


		int beginx = 0;
		int endx = 0;
		beginx = max(FIX23_9TOI(fx1), FIX23_9TOI(fnow.sx1));
		endx = min(FIX23_9TOI(fx2), FIX23_9TOI(fnow.sx2));

		if (beginx > endx) {
		  const s32 tmp = beginx;
		  beginx = endx;
		  endx = tmp;
		}

		if (beginx < 0) {
			beginx = 0;
		}
		if (endx > W) {
			endx = W;
		}


		for(x = beginx; x < endx; x+=8)
		{
		copymem((void*)BGMap(1)+24, (void*)(vb_doomMap+2464), 8);
        		continue;
			/* Calculate the Z coordinate for this point. (Only used for lighting.) */
            //int z = ((x - x1) * (tz2-tz1) / (x2-x1) + tz1) * 8;
            /* Acquire the Y coordinates for our ceiling & floor for this X coordinate. Clamp them. */
			int ya = 0;
			int cya = 0; // top
			int yb = 0;
			int cyb = 0; // bottom

			s32 fx_x1 = ITOFIX23_9(x) - fx1;
			s32 fx2_x1 = fx2 - fx1;
			s32 fy2a_y1a = fy2a - fy1a;
			s32 fy2b_y1b = fy2b - fy1b;

 			// top
			ya = FIX23_9TOI( FIX23_9_DIV(FIX23_9_MULT(fx_x1, fy2a_y1a), fx2_x1) + fy1a );
			cya = clamp(ya, 0,H-1);
			// bottom
			yb = FIX23_9TOI( FIX23_9_DIV(FIX23_9_MULT(fx_x1, fy2b_y1b), fx2_x1) + fy1b );
			cyb = clamp(yb, 0,H-1);


			// round down to nearest 8 as we draw 8x8 tiles.
			int xPos = roundDown(x, 8);
			int cyaRoof = roundDown(cya-1, 8);
			if (cyaRoof < 0) {
				cyaRoof = 0;
			}
			int cybFloor = roundDown(cyb+1, 8);
			if (cybFloor > H) {
				cybFloor = H;
			}


			// draw roof
			//drawDoomStageTile(xPos, 0, cyaRoof, 2480);
			// draw floor
			//drawDoomStageTile(xPos, cybFloor, H-1, 2480);
			// draw wall
			//drawDoomStageTile(xPos, cyaRoof, cybFloor, 2484); //2472);

		}
	}
/*
	for (i = 0; i < e1m1ThingsLength; i+=5) {
		if (e1m1Things[i+3] != 1) { // monster or something else... let's assume monster for now... (1 is player)

			fvx1 = fixedPlayerX - ITOFIX23_9(e1m1Things[i]);
			fvy1 = ITOFIX23_9(e1m1Things[i+1]) - fixedPlayerY;
			// rotate around player view
			ftx1 = FIX23_9_MULT(fvx1, fixedPlayerAngleSin) - (fvy1, fixedPlayerAngleCos);
			ftz1 = FIX23_9_MULT(fvx1, fixedPlayerAngleCos) + (fvy1, fixedPlayerAngleSin);
			// only display if enemy is in Field-Of-View of player.
			if (ftz1 <= fixedZero && ftx1 <= fixedZero) {
				continue;
			}
			s32 fxscale1 = FIX23_9_DIV(fhfov, ftz1);
			s32 fyscale1 = FIX23_9_DIV(fvfov, ftz1);
			int x1 = FIX23_9TOI( fixedWBy2 - FIX23_9_MULT(ftx1, fxscale1) );
			int y1b = FIX23_9TOI(fixedHBy2 - (FIX23_9_MULT(FixedYaw(fyfloor, ftz1), fyscale1)));
			int FourtySixTimesYScale = FIX23_9TOI(FIX23_9_MULT(ITOFIX23_9(46),fyscale1));
			WA[29].gx = x1;//%8;
			WA[30].gx = x1;//%8;
			WA[29].gy = y1b - FourtySixTimesYScale;//%8;
			WA[30].gy = y1b - FourtySixTimesYScale;
			u16 startPos = (96*4) + 38; // startpos in image where the enemy is
			u16 blackStartPos = startPos + (96*34); // startpos in image where the enemy is with different palette
			u16 curCol = 0;
			u16 yPos = 0;
			u16 tilePosition = 0;

			u8 bgmap = 3;
			for (curCol = 0; curCol < 6; curCol++) {
				yPos = curCol<<7;
				copymem((void*)BGMap(3)+yPos, (void*)(vb_doomMap+startPos+(curCol*96)), 10);
				copymem((void*)BGMap(2)+yPos, (void*)(vb_doomMap+blackStartPos+(curCol*96)), 10);
			}
			// scale these two layers...
			affine_fast_scale_fixed(30, fyscale1);
			affine_fast_scale_fixed(29, fyscale1);
		}
	}*/

	// I forgot what this code does... but I think it's because I use a different palette for LAYER 2 for the enemy... which allows me to use all 4 colors for the enemy if i'd like
	u8 y;
	for (y = 0; y < 14; y++) {
		for (x = 0; x < 10; x++) {
			BGM_PALSET(2, x, y, BGM_PAL2);
		}
	}
}

void drawDoomStage(u8 bgmap) {
	float vx1=0;
	float vx2=0;
	float vy1=0;
	float vy2=0;

	float tx1=0;
	float tz1=0;
	float tx2=0;
	float tz2=0;

	int y1a = 0;
	int y1b = 0;
	int y2a = 0;
	int y2b = 0;

	int yceil = 0;
	int yfloor = 0;
	int x = 0;
	int wallsNotDrawn = 0;
	int index = 0;

	u16 segNotDrawn1 = 0;
	u16 segNotDrawn2 = 0;
	u16 vertexIndex1 = 0;
	u16 vertexIndex2 = 0;

	float playerZPlusEyeHeight = playerZ + EyeHeight;

	u16 i = 0;
	// iterate walls
	for (i = 0; i < e1m1SegsLength; i+= 2) {
		vertexIndex1 = e1m1Segs[i]<<1;
		vertexIndex2 = e1m1Segs[i+1]<<1;

		vx1 = playerX - e1m1Vertices[ vertexIndex1 ];
		vy1 = e1m1Vertices[ vertexIndex1+1 ] - playerY;

		vx2 = playerX - e1m1Vertices[ vertexIndex2 ];
		vy2 = e1m1Vertices[ vertexIndex2+1 ] - playerY;

		tx1 = vx1 * playerAngleSin - vy1 * playerAngleCos;
		tz1 = vx1 * playerAngleCos + vy1 * playerAngleSin;

		tx2 = vx2 * playerAngleSin - vy2 * playerAngleCos;
		tz2 = vx2 * playerAngleCos + vy2 * playerAngleSin;

		if (tz1 <= 0 && tz2 <= 0) {
			segNotDrawn1 = vertexIndex1;
			segNotDrawn2 = vertexIndex2;
			wallsNotDrawn++;
			continue;
		}
		if (tz1 <= 0 || tz2 <= 0) {
		  struct xy i1 = Intersect(tx1,tz1,tx2,tz2, -nearside,nearz, -farside,farz);
		  struct xy i2 = Intersect(tx1,tz1,tx2,tz2,  nearside,nearz,  farside,farz);
		  if (tz1 < nearz) {
			if (i1.y > 0) {
			  tx1 = i1.x;
			  tz1 = i1.y;
			} else {
			  tx1 = i2.x;
			  tz1 = i2.y;
			}
		  }
		  if (tz2 < nearz) {
			if (i1.y > 0) {
			  tx2 = i1.x;
			  tz2 = i1.y;
			} else {
			  tx2 = i2.x;
			  tz2 = i2.y;
			}
		  }
		}

		/* Do perspective transformation */
		float xscale1 = hfov / tz1;
		float yscale1 = vfov / tz1;
		int x1 = Wby2 - (int)(tx1 * xscale1);
		float xscale2 = hfov / tz2;
		float yscale2 = vfov / tz2;
		int x2 = Wby2 - (int)(tx2 * xscale2);

		if(x1 >= x2 || x2 < now.sx1 || x1 > now.sx2) {
			wallsNotDrawn++;
			continue; // Only render if it's visible
		}

		yceil = sectorCeil - playerZPlusEyeHeight;
		yfloor = sectorFloor - playerZPlusEyeHeight;
		y1a = Hby2 - (int)(Yaw(yceil, tz1) * yscale1);
		y1b = Hby2 - (int)(Yaw(yfloor, tz1) * yscale1);
		y2a = Hby2 - (int)(Yaw(yceil, tz2) * yscale2);
		y2b = Hby2 - (int)(Yaw(yfloor, tz2) * yscale2);

		int beginx = 0;
		int endx = 0;
		beginx = max(x1, now.sx1);
		endx = min(x2, now.sx2);

		if (beginx > endx) {
		  const int tmp = beginx;
		  beginx = endx;
		  endx = tmp;
		}

		if (beginx < 0) {
			beginx = 0;
		}
		if (endx > W) {
			endx = W;
		}

		for(x = beginx; x < endx; x+=8)
		{
			/* Calculate the Z coordinate for this point. (Only used for lighting.) */
            //int z = ((x - x1) * (tz2-tz1) / (x2-x1) + tz1) * 8;
            /* Acquire the Y coordinates for our ceiling & floor for this X coordinate. Clamp them. */
            int ya = 0;
            int cya = 0; // top
            int yb = 0;
            int cyb = 0; // bottom

            int x_x1 = x - x1;
            int x2_x1 = x2 - x1;
            int y2a_y1a = y2a - y1a;
            int y2b_y1b = y2b - y1b;


			ya = (int)(x_x1 * y2a_y1a / x2_x1 + y1a);
			cya = clamp(ya, 0,H-1); // top

			yb = (int)(x_x1 * y2b_y1b / x2_x1 + y1b);
			cyb = clamp(yb, 0,H-1); // bottom


			// round down to nearest 8 as we draw 8x8 tiles.
            int xPos = roundDown(x, 8);
            int cyaRoof = roundDown(cya-1, 8);



			int cybFloor = roundDown(cyb+1, 8);




			// draw roof
			drawDoomStageTile(xPos, 0, cyaRoof, 2482);
			// draw floor
			drawDoomStageTile(xPos, cybFloor, H-1, 2480);
            // draw wall
            drawDoomStageTile(xPos, cyaRoof, cybFloor, 2484); //2472);

		}
	}

	for (i = 0; i < e1m1ThingsLength; i+=5) {
		if (e1m1Things[i+3] != 1) { // monster or something else... let's assume monster for now... (1 is player)

			vx1 = playerX - e1m1Things[i];
			vy1 = e1m1Things[i+1] - playerY;
			// rotate around player view
			tx1 = vx1 * playerAngleSin - vy1 * playerAngleCos;
			tz1 = vx1 * playerAngleCos + vy1 * playerAngleSin;
			// only display if enemy is in Field-Of-View of player.
			if (tz1 <= 0 && tx1 <= 0) {
				continue;
			}
			float xscale1 = hfov / tz1;
			float yscale1 = vfov / tz1;
			int x1 = Wby2 - (int)(tx1 * xscale1);
			int y1b = Hby2 - (int)(Yaw(yfloor, tz1) * yscale1);
			int FourtySixTimesYScale = 46*yscale1;
			WA[29].gx = x1;//%8;
			WA[30].gx = x1;//%8;
			WA[29].gy = y1b - FourtySixTimesYScale;//%8;
			WA[30].gy = y1b - FourtySixTimesYScale;//%8;
			u16 startPos = (96*4) + 38; // startpos in image where the enemy is
			u16 blackStartPos = startPos + (96*34); // startpos in image where the enemy is with different palette
			u16 curCol = 0;
			u16 yPos = 0;
			u16 tilePosition = 0;

			u8 bgmap = 3;
			for (curCol = 0; curCol < 6; curCol++) {
				yPos = curCol<<7;
				copymem((void*)BGMap(3)+yPos, (void*)(vb_doomMap+startPos+(curCol*96)), 10);
				copymem((void*)BGMap(2)+yPos, (void*)(vb_doomMap+blackStartPos+(curCol*96)), 10);
			}

			affine_fast_scale(30, yscale1);
			affine_fast_scale(29, yscale1);
		}
	}

	// I forgot what this code does... but I think it's because I use a different palette for LAYER 2 for the enemy... which allows me to use all 4 colors for the enemy if i'd like
	u8 y;
	for (y = 0; y < 14; y++) {
		for (x = 0; x < 10; x++) {
			BGM_PALSET(2, x, y, BGM_PAL2);
		}
	}
}

int roundUp(int num, int factor)
{
    return num + factor - 1 - (num + factor - 1) % factor;
}

int roundDown(int num, int factor)
{
	return num - (num % factor);
}

void drawDoomStageTile(int iX, int iStartY, int iEndY, u16 iStartPos) {

	//copymem((void*)BGMap(1)+0, (void*)(vb_doomMap+iStartPos), 2);
	//int tilePosition = (roundDown(iStartY, 8)<<4) + ((iX<<1)>>3);
	//copymem((void*)BGMap(1)+tilePosition, (void*)(vb_doomMap+iStartPos), 2);

	iStartY = clamp(iStartY, 0, H-1);
    iEndY = clamp(iEndY, 0, H-1);
    if (iStartY > iEndY) {
	  const u16 tmp = iStartY;
	  iStartY = iEndY;
	  iEndY = tmp;
	}

	int tilePosition = 0;
	u16 i2;
	for (i2 = iStartY; i2 < iEndY; i2+=8) {
		tilePosition = (roundDown(i2, 8)<<4) + ((iX<<1)>>3);
		copymem((void*)BGMap(1)+tilePosition, (void*)(vb_doomMap+iStartPos), 2);
	}
}


void jawPlayer(float iValue) {
	playerYaw += iValue;
}
void incPlayerAngle(float iValue) {
	if (playerAngle + iValue < 0) {
		playerAngle += 512.0f;
	} else if (playerAngle + iValue > 512.0f) {
		playerAngle -= 512.0f;
	}
	playerAngle = playerAngle + iValue;
	playerAngleCos = COSF((int)playerAngle);
	playerAngleSin = SINF((int)playerAngle);
}
void playerMoveForward(float iValue) {
	playerX -= playerAngleCos * iValue;
	playerY += playerAngleSin * iValue;
}
void playerStrafe(float iValue) {
	float tempangle = playerAngle;
	tempangle -= 90.0f*angleRatio;

	float tempAngleCos = COSF((int)tempangle);
	float tempAngleSin = SINF((int)tempangle);

	playerX += tempAngleCos * iValue*angleRatio;
	playerY -= tempAngleSin * iValue*angleRatio;
}

void incFixedPlayerAngle(s32 iValue) {
	if (FIX23_9TOI(fixedPlayerAngle + iValue) < 0) {
		fixedPlayerAngle += fixed512;
	} else if (FIX23_9TOI(fixedPlayerAngle + iValue) >= 512) {
		fixedPlayerAngle -= fixed512;
	}
	/*
	playerAngle = playerAngle + iValue;
	playerAngleCos = COSF((int)playerAngle);
	playerAngleSin = SINF((int)playerAngle);*/
	fixedPlayerAngle = fixedPlayerAngle + iValue;
	fixedPlayerAngleCos = COS(FIX23_9TOI(fixedPlayerAngle));
	fixedPlayerAngleSin = SIN(FIX23_9TOI(fixedPlayerAngle));
}

void jawFixedPlayer(s32 iValue) {
	fixedPlayerYaw += iValue;
}

void fixedPlayerMoveForward(s32 iValue) {
	//playerX -= FIX23_9_MULT(fixedPlayerAngleCos, iValue);
	//playerY += FIX23_9_MULT(fixedPlayerAngleSin, iValue);
	fixedPlayerX -= FIX23_9_MULT(fixedPlayerAngleCos, iValue);
	fixedPlayerY += FIX23_9_MULT(fixedPlayerAngleSin, iValue);
}

void fixedPlayerStrafe(s32 iValue) {
	f16 tempangle = fixedPlayerAngle;
	tempangle -= FIX23_9_MULT(fixed90, fixedAngleRatio);

	f16 tempAngleCos = COS(FIX23_9TOI(tempangle));
	f16 tempAngleSin = SIN(FIX23_9TOI(tempangle));

	//playerX += tempAngleCos * iValue*angleRatio;
	fixedPlayerX += FIX23_9_MULT(tempAngleCos, FIX23_9_MULT(iValue, fixedAngleRatio));
	//playerY -= tempAngleSin * iValue*angleRatio;
	fixedPlayerY -= FIX23_9_MULT(tempAngleSin, FIX23_9_MULT(iValue, fixedAngleRatio));
}
/*
fixedPlayerAngle = FIX23_9_MULT(ITOFIX23_9(e1m1Things[i+2]), fixedAngleRatio);
			fixedPlayerAngleCos = COS(FIX23_9TOI(fixedPlayerAngle));
			fixedPlayerAngleSin = SIN(FIX23_9TOI(fixedPlayerAngle));*/


void affine_fast_scale_fixed(u8 world, s32 scale) {
	int i,tmp;
	s16 *param;
	f16 XScl,YScl;
	f32 YSrc;

	tmp = (world<<4);
	param = (s16*)((WAM[tmp+9]<<1)+0x00020000);

	XScl = YScl = scale;
	YSrc = 0;

	i=0;
	while (i < (int)(WAM[tmp+8]<<3)) {
		param[i++] = 0;			//XSrc
		i++;					//Prlx
		param[i++] = (YSrc>>6);	//YSrc -- convert from 7.9 to 13.3 fixed
		param[i++] = XScl; 		//XScl
		i+=4; //skip remaining entries

		YSrc += YScl; //grab value of next scanline
	}
}