#define _USE_MATH_DEFINES

#include "RayCaster.h"
#include "RayCasterRenderer.h"
#include "RayCasterFixed.h"
#include <constants.h>
#include <math.h>
#include "enemy.h"
#include "pickup.h"
#include "particle.h"
#include "doomgfx.h"
#include "../assets/images/sprites/pickups/pickup_sprites.h"
#include "../assets/images/particle_sprites.h"
#include "../assets/images/wall_textures.h"

extern BYTE vb_doomMap[];

/* Trig lookup tables (defined in RayCasterTables.h, compiled in RayCasterFixed.c) */
extern const u8 g_cos[];
extern const u8 g_sin[];

s32 diffX = 0;
s32 diffY = 0;

/* Forward declarations */
void drawTile(u16 *iX, u8 *iStartY, u16 *iStartPos);
void affine_enemy_scale(u8 world, float scale, s16 mxOffset);

s32 sqrt_i32(s32 v) {
    uint32_t b = 1<<30, q = 0, r = v;
    while (b > r)
        b >>= 2;
    while( b > 0 ) {
        uint32_t t = q + b;
        q >>= 1;
        if( r >= t ) {
            r -= t;
            q += b;
        }
        b >>= 2;
    }
    return q;
}

/* Forward declarations */
void drawTile(u16 *iX, u8 *iStartY, u16 *iStartPos);
void drawTileChar(u16 *iX, u8 *iStartY, u16 charIdx);

u16   x;
u8   sso;
u8   tc;
u8   tn;
u16  tso;
u16  tst;
u16 ssoX2;
u8 tv;
u8 wsPercent8;
s16 ws;
u8 y;
u8 curY;
u8 clampedY;
u16 tile;
u16 vanligTile;
bool drawnTop;
bool drawnBottom;
u16 wallTexChar;
u32 texAccum;     /* texture Y accumulator for row selection */
u16 wallTexBase;  /* per-column base: CHAR_START + (wt-1)*128 + tv*64 + col */
u8  texRow;
u8  wt;           /* wall type for this column (1-4) */
u16 transChar;    /* textured transition tile char index */
u8  bottomFill;   /* wall pixels in bottom partial tile (ssoX2 % 8) */

/* Per-column wall half-height for enemy occlusion (48 columns at 8px each) */
u8 g_wallSso[48];

void TraceFrame(u16 *playerX, u16 *playerY, s16 *playerA)
{
	//copymem((void*)BGMap(10)+10, (void*)(vb_doomMap+2482), 4); // fungerar fint

	//setmem((void*)BGMap(1), 0, 8192); // clear screen

	Start(*playerX, *playerY, *playerA);
	/*
    Start((u16)(playerX * 256.0),
               (u16)(playerY * 256.0),
               (s16)(playerA / (2.0 * M_PI) * 1024.0));*/

    for(x = 0; x < SCREEN_WIDTH; x+=8)
    {
    	// reset
		drawnTop = false;
		drawnBottom = false;

        Trace(x, &sso, &tn, &tc, &tso, &tst);
		ssoX2 = sso<<1;
		tv = tn == 1?0:1;
		if (sso < 20) tv = 1;  /* far walls always dark */

        //const int tx = (int)(tc >> 2);
        ws = HORIZON_HEIGHT - sso;
        if(ws < 0) {
		  ws  = 0;
		  sso = HORIZON_HEIGHT;
		}
		g_wallSso[x >> 3] = sso;  /* store wall depth for enemy occlusion */
        wsPercent8 = ws%8;
        //u16 to = tso;
        //u16 ts = tst;
		y = 0;
		curY = 0;
		clampedY = 0;
		tile = 0;
		drawnTop = false;
		drawnBottom = false;

		/* Compute wall type early so ceiling transitions can use it */
		wt = g_lastWallType;
		if (wt < 1 || wt > WALL_TEX_COUNT) wt = 1;
		wallTexBase = WALL_TEX_CHAR_START + (u16)(wt - 1) * WALL_TEX_PER_TYPE
		            + (u16)tv * WALL_TEX_LIT_BLOCK + (tc >> 5);

        for(y = 0; y < ws; y+=8) /* draw ceiling */
        {
            if (y+7 >= ws && wsPercent8 > 0) {
            	/* Textured ceiling/wall transition tile */
            	drawnTop = true;
				transChar = TRANS_TEX_CHAR_START + (u16)(wt - 1) * 14
				          + (u16)tv * 7 + (7 - wsPercent8);
				drawTileChar(&x, &curY, transChar);
            } else {
				tile = 0;
				drawTile(&x, &curY, &tile);
			}
			curY+=8;
        }
		vanligTile = 0;
		texAccum = (u32)tso;

		bottomFill = ssoX2 & 7;  /* wall pixels in bottom partial tile */

        for(y = 0; y < ssoX2; y+=8) /* draw walls */
        {
			/* Compute row from textureY accumulator */
			texRow = (u8)((texAccum >> 13) & 7);
			wallTexChar = wallTexBase + (u16)texRow * WALL_TEX_COLS;

            if (y == 0 && ws != 0 && drawnTop == false) {
				/* Top partial: wsPercent8 is 0 here, wall starts on tile boundary */
				drawTileChar(&x, &curY, wallTexChar);
            } else if (y + 7 >= ssoX2 && bottomFill > 0) {
				/* Bottom partial: textured transition tile with V-flip */
				transChar = TRANS_TEX_CHAR_START + (u16)(wt - 1) * 14
				          + (u16)tv * 7 + (bottomFill - 1);
				drawTileChar(&x, &curY, transChar | 0x1000);  /* V-flip */
				drawnBottom = true;
            } else {
				/* Full textured wall tile */
				drawTileChar(&x, &curY, wallTexChar);
			}
			texAccum += (u32)tst << 3;  /* advance textureY by tst * 8 scanlines */
            curY+=8;
        }
        /*
        // draw roof
        			drawDoomStageTile(xPos, 0, cyaRoof, 2482);
        			// draw floor
        			drawDoomStageTile(xPos, cybFloor, H-1, 2480);
        			// draw wall
        			drawDoomStageTile(xPos, cyaRoof, cybFloor, 2484); //2472);
        			*/



        for(y = 0; y < ws; y+=8) /* draw floor */
        {
        	if (y == 0 && drawnBottom == false && bottomFill > 0) {
				/* Floor/wall transition: textured V-flip */
				transChar = TRANS_TEX_CHAR_START + (u16)(wt - 1) * 14
				          + (u16)tv * 7 + (bottomFill - 1);
				drawTileChar(&x, &curY, transChar | 0x1000);  /* V-flip */
			} else {
				tile = 0;
				drawTile(&x, &curY, &tile);
			}
            curY+=8;
        }
    }

    // === ENEMY RENDERING ===
    // Single layer per enemy using dedicated zombie sprite character memory.
    // BGMap entries set once at init (initEnemyBGMaps), tile data loaded at init (loadEnemyFrame).
    // Disable world FIRST, do ALL setup, enable world LAST.
    {
        u8 ei;
        s16 dX, dY, viewZ, viewX;
        s16 scrX, scrY, scaledW, scaledH;
        float enemyScale;
        u8 worldNum, bgmapIdx;
        /* Debug: uncomment to show enemy 0 viewZ/scrX/scrY on HUD:
         * u8 ones, tens, hundreds, thousands;
         * u16 startPos, drawPos;
         * s16 dbgViewZ = 0, dbgScrX = 0, dbgScrY = 0; */

        for (ei = 0; ei < MAX_ENEMIES; ei++) {
            EnemyState *e = &g_enemies[ei];

            /* 1 world per enemy, 1 BGMap per enemy */
            worldNum = 30 - ei;                    /* enemy 0 = world 30, enemy 1 = world 29 */
            bgmapIdx = ENEMY_BGMAP_START + ei;     /* enemy 0 = BGMap 3, enemy 1 = BGMap 4 */

            scrX = 0;
            scrY = 0;
            viewZ = 0;
            viewX = 0;

            /* STEP 0: Disable world immediately (prevents ghost rendering) */
            WAM[worldNum << 4] = bgmapIdx | WRLD_AFFINE;

            if (!e->active) continue;

            /* Compute delta from player to enemy */
            dX = (s16)e->x - (s16)_playerX;
            dY = (s16)e->y - (s16)_playerY;

            /* viewZ = dY*cos(A) + dX*sin(A) -- depth into screen */
            if (_playerA == 0) {
                viewZ = dY;
            } else if (_playerA == 512) {
                viewZ = -dY;
            } else if (_playerA == 256) {
                viewZ = dX;
            } else if (_playerA == 768) {
                viewZ = -dX;
            } else {
                switch (_viewQuarter) {
                    case 0: viewZ = MulS(LOOKUP8(g_cos, _viewAngle), dY) + MulS(LOOKUP8(g_sin, _viewAngle), dX); break;
                    case 1: viewZ = -MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), dY) + MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), dX); break;
                    case 2: viewZ = -MulS(LOOKUP8(g_cos, _viewAngle), dY) - MulS(LOOKUP8(g_sin, _viewAngle), dX); break;
                    case 3: viewZ = MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), dY) - MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), dX); break;
                }
            }

            if (viewZ <= 10) continue; /* behind player or too close */

            /* viewX = dX*cos(A) - dY*sin(A) -- lateral offset */
            if (_playerA == 0) {
                viewX = dX;
            } else if (_playerA == 512) {
                viewX = -dX;
            } else if (_playerA == 256) {
                viewX = -dY;
            } else if (_playerA == 768) {
                viewX = dY;
            } else {
                switch (_viewQuarter) {
                    case 0: viewX = MulS(LOOKUP8(g_cos, _viewAngle), dX) - MulS(LOOKUP8(g_sin, _viewAngle), dY); break;
                    case 1: viewX = -MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), dX) - MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), dY); break;
                    case 2: viewX = -MulS(LOOKUP8(g_cos, _viewAngle), dX) + MulS(LOOKUP8(g_sin, _viewAngle), dY); break;
                    case 3: viewX = MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), dX) + MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), dY); break;
                }
            }

            /* Perspective projection -- 64x64 sprite */
            enemyScale = 800.0f / (float)viewZ;
            scaledW = (s16)(64.0f * enemyScale);
            scaledH = (s16)(64.0f * enemyScale);

            /* Clamp dimensions */
            if (scaledW < 1) scaledW = 1;
            if (scaledW > 384) scaledW = 384;
            if (scaledH < 1) scaledH = 1;
            if (scaledH > 224) scaledH = 224;

            /* Screen position: center horizontally, feet at horizon (y=104) */
            scrX = (s16)(192 + ((s32)viewX * 192) / (s32)viewZ) - scaledW / 2;
            scrY = (s16)(104 - scaledH / 3);

            /* Only render if on screen */
            if (scrX + scaledW <= 0 || scrX >= 384 || scrY + scaledH <= 0 || scrY >= 208) continue;

            /* === WALL OCCLUSION (per-column depth test) === */
            {
                u8 enemySso;
                u16 dummyStep;
                s16 colStart, colEnd, ocCol;
                s16 visLeft, visRight;
                s16 clipL, clipR;
                s16 mxOffset;
                bool anyVisible;

                /* Compute enemy's equivalent wall sso from its depth */
                if (viewZ >= MIN_DIST) {
                    LookupHeight((u16)((viewZ - MIN_DIST) >> 2), &enemySso, &dummyStep);
                } else {
                    enemySso = HORIZON_HEIGHT; /* very close = treat as always in front */
                }

                /* Column range of enemy on screen (clamped to 0..47) */
                colStart = scrX >> 3;
                if (colStart < 0) colStart = 0;
                colEnd = (scrX + scaledW - 1) >> 3;
                if (colEnd > 47) colEnd = 47;
                if (colStart > 47 || colEnd < 0) continue;

                /* Scan for leftmost and rightmost visible (not occluded) columns */
                visLeft = -1;
                visRight = -1;
                anyVisible = false;
                for (ocCol = colStart; ocCol <= colEnd; ocCol++) {
                    if (g_wallSso[ocCol] < enemySso) {
                        /* Wall is farther than enemy at this column -> enemy visible */
                        if (!anyVisible) visLeft = ocCol;
                        visRight = ocCol;
                        anyVisible = true;
                    }
                }

                if (!anyVisible) continue; /* fully behind walls, skip */

                /* Compute pixel clipping amounts */
                clipL = (visLeft << 3) - scrX;
                if (clipL < 0) clipL = 0;
                clipR = (scrX + scaledW) - ((visRight + 1) << 3);
                if (clipR < 0) clipR = 0;

                /* Apply clipping to screen position and width */
                scrX += clipL;
                scaledW -= clipL + clipR;
                if (scaledW < 1) continue;

                /* Compute affine source X offset for left clipping */
                /* Mx is 13.3 format, Dx is 7.9 format */
                /* mxOffset_13.3 = (clipL * Dx_7.9) >> 6 */
                mxOffset = (clipL > 0) ? (s16)(((s32)clipL * (s32)inverse_fixed(enemyScale)) >> 6) : 0;

                /* Write BGMap entries per-frame.
                 * For mirrored directions (5,6,7) we reverse tile column order
                 * and set BGM_HFLIP (0x2000) so left-facing sprites appear
                 * correctly as right-facing. */
                {
                    u16 *bgm = (u16*)BGMap(bgmapIdx);
                    u16 charBase = ZOMBIE_CHAR_START + ei * ENEMY_CHAR_STRIDE;
                    u8 row, c;
                    u8 eDir = getEnemyDirection(ei, _playerX, _playerY, _playerA);
                    if (eDir >= 5) {
                        /* Mirrored: reverse columns + HFLP per tile */
                        for (row = 0; row < ZOMBIE_TILE_H; row++) {
                            for (c = 0; c < ZOMBIE_TILE_W; c++) {
                                bgm[row * 64 + c] = (charBase + row * ZOMBIE_TILE_W + (ZOMBIE_TILE_W - 1 - c)) | 0x2000;
                            }
                        }
                    } else {
                        /* Normal: left-to-right, no flip */
                        for (row = 0; row < ZOMBIE_TILE_H; row++) {
                            for (c = 0; c < ZOMBIE_TILE_W; c++) {
                                bgm[row * 64 + c] = charBase + row * ZOMBIE_TILE_W + c;
                            }
                        }
                    }
                }

                /* Set world position and dimensions (using clipped values) */
                WA[worldNum].gx = scrX;
                WA[worldNum].gy = scrY;
                WA[worldNum].mx = 0;
                WA[worldNum].my = 0;
                WA[worldNum].w = scaledW;
                WA[worldNum].h = scaledH;

                /* Apply affine scaling with source X offset for clipping */
                affine_enemy_scale(worldNum, enemyScale, mxOffset);

                /* ENABLE world LAST (everything is ready now) */
                WAM[worldNum << 4] = WRLD_ON | bgmapIdx | WRLD_AFFINE;
            }
        } /* end for each enemy */

    }

    /* === PICKUP RENDERING === */
    {
        u8 pi, slot;
        s16 dX, dY, viewZ, viewX;
        s16 scrX, scrY, scaledW, scaledH;
        float pickupScale;
        u8 worldNum, bgmapIdx;

        /* Disable all pickup worlds first (prevents ghost rendering) */
        for (slot = 0; slot < MAX_VISIBLE_PICKUPS; slot++) {
            worldNum = 27 - slot;
            bgmapIdx = PICKUP_BGMAP_START + slot;
            WAM[worldNum << 4] = bgmapIdx | WRLD_AFFINE;
        }

        /* Sort pickups by distance (simple: just render closest MAX_VISIBLE_PICKUPS) */
        slot = 0;
        for (pi = 0; pi < MAX_PICKUPS && slot < MAX_VISIBLE_PICKUPS; pi++) {
            Pickup *p = &g_pickups[pi];

            if (!p->active) continue;

            /* Compute delta from player to pickup */
            dX = (s16)p->x - (s16)_playerX;
            dY = (s16)p->y - (s16)_playerY;

            /* viewZ = depth into screen */
            if (_playerA == 0) {
                viewZ = dY;
            } else if (_playerA == 512) {
                viewZ = -dY;
            } else if (_playerA == 256) {
                viewZ = dX;
            } else if (_playerA == 768) {
                viewZ = -dX;
            } else {
                switch (_viewQuarter) {
                    case 0: viewZ = MulS(LOOKUP8(g_cos, _viewAngle), dY) + MulS(LOOKUP8(g_sin, _viewAngle), dX); break;
                    case 1: viewZ = -MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), dY) + MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), dX); break;
                    case 2: viewZ = -MulS(LOOKUP8(g_cos, _viewAngle), dY) - MulS(LOOKUP8(g_sin, _viewAngle), dX); break;
                    case 3: viewZ = MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), dY) - MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), dX); break;
                }
            }

            if (viewZ <= 10) continue; /* behind player or too close */

            /* viewX = lateral offset */
            if (_playerA == 0) {
                viewX = dX;
            } else if (_playerA == 512) {
                viewX = -dX;
            } else if (_playerA == 256) {
                viewX = -dY;
            } else if (_playerA == 768) {
                viewX = dY;
            } else {
                switch (_viewQuarter) {
                    case 0: viewX = MulS(LOOKUP8(g_cos, _viewAngle), dX) - MulS(LOOKUP8(g_sin, _viewAngle), dY); break;
                    case 1: viewX = -MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), dX) - MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), dY); break;
                    case 2: viewX = -MulS(LOOKUP8(g_cos, _viewAngle), dX) + MulS(LOOKUP8(g_sin, _viewAngle), dY); break;
                    case 3: viewX = MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), dX) + MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), dY); break;
                }
            }

            /* Perspective projection -- 32x24 source sprite.
             * Scale constant 400 = half of enemy's 800, since pickups are smaller items.
             */
            pickupScale = 400.0f / (float)viewZ;
            scaledW = (s16)(32.0f * pickupScale);
            scaledH = (s16)(24.0f * pickupScale);

            /* Clamp dimensions */
            if (scaledW < 1) scaledW = 1;
            if (scaledW > 384) scaledW = 384;
            if (scaledH < 1) scaledH = 1;
            if (scaledH > 224) scaledH = 224;

            /* Screen position: center horizontally.
             * Ground plane Y = where enemy feet land at this depth.
             * enemyScale = 2*pickupScale, enemyH = 128*pickupScale,
             * enemyFeetY = 104 + 2*enemyH/3 = 104 + 85.33*pickupScale.
             * Pickup BOTTOM sits at groundY, so top = groundY - scaledH.
             */
            scrX = (s16)(192 + ((s32)viewX * 192) / (s32)viewZ) - scaledW / 2;
            scrY = (s16)(104.0f + 85.33f * pickupScale) - scaledH;

            /* Only render if on screen */
            if (scrX + scaledW <= 0 || scrX >= 384 || scrY + scaledH <= 0 || scrY >= 208) continue;

            /* === WALL OCCLUSION (same as enemy occlusion) === */
            {
                u8 pickupSso;
                u16 dummyStep;
                s16 colStart, colEnd, ocCol;
                s16 visLeft, visRight;
                s16 clipL, clipR;
                s16 mxOffset;
                bool anyVisible;

                /* Compute pickup's equivalent wall sso from its depth */
                if (viewZ >= MIN_DIST) {
                    LookupHeight((u16)((viewZ - MIN_DIST) >> 2), &pickupSso, &dummyStep);
                } else {
                    pickupSso = HORIZON_HEIGHT;
                }

                /* Column range on screen (clamped to 0..47) */
                colStart = scrX >> 3;
                if (colStart < 0) colStart = 0;
                colEnd = (scrX + scaledW - 1) >> 3;
                if (colEnd > 47) colEnd = 47;
                if (colStart > 47 || colEnd < 0) continue;

                /* Scan for visible columns */
                visLeft = -1;
                visRight = -1;
                anyVisible = false;
                for (ocCol = colStart; ocCol <= colEnd; ocCol++) {
                    if (g_wallSso[ocCol] < pickupSso) {
                        if (!anyVisible) visLeft = ocCol;
                        visRight = ocCol;
                        anyVisible = true;
                    }
                }

                if (!anyVisible) continue; /* fully behind walls */

                /* Compute pixel clipping */
                clipL = (visLeft << 3) - scrX;
                if (clipL < 0) clipL = 0;
                clipR = (scrX + scaledW) - ((visRight + 1) << 3);
                if (clipR < 0) clipR = 0;

                scrX += clipL;
                scaledW -= clipL + clipR;
                if (scaledW < 1) continue;

                /* Source X offset for left clipping */
                mxOffset = (clipL > 0) ? (s16)(((s32)clipL * (s32)inverse_fixed(pickupScale)) >> 6) : 0;

                /* Assign this pickup to a render slot */
                worldNum = 27 - slot;
                bgmapIdx = PICKUP_BGMAP_START + slot;

                /* Load the correct pickup sprite tiles into this slot's char memory */
                loadPickupFrame(slot, PICKUP_TILES[p->type]);

                /* Write BGMap tile entries for this slot */
                {
                    u16 *bgm = (u16*)BGMap(bgmapIdx);
                    u16 charBase = PICKUP_CHAR_START + slot * PICKUP_CHAR_STRIDE;
                    u8 row, col;
                    for (row = 0; row < PICKUP_TILE_H; row++) {
                        for (col = 0; col < PICKUP_TILE_W; col++) {
                            bgm[row * 64 + col] = charBase + row * PICKUP_TILE_W + col;
                        }
                    }
                }

                /* Set world position and dimensions (using clipped values) */
                WA[worldNum].gx = scrX;
                WA[worldNum].gy = scrY;
                WA[worldNum].mx = 0;
                WA[worldNum].my = 0;
                WA[worldNum].w = scaledW;
                WA[worldNum].h = scaledH;

                /* Apply affine scaling with clipping offset */
                affine_enemy_scale(worldNum, pickupScale, mxOffset);

                /* ENABLE world */
                WAM[worldNum << 4] = WRLD_ON | bgmapIdx | WRLD_AFFINE;

                slot++;
            } /* end occlusion block */
        } /* end for each pickup */
    }

    /* ================================================================
     * PARTICLE RENDERING (single puffs + shotgun groups)
     * Uses world 24 with BGMap 9 (affine mode)
     * ================================================================ */
    {
        u8 pi, rendered;
        s16 dX, dY, viewZ, viewX;
        s16 scrX, scrY, scaledW, scaledH;
        float pScale;
        u8 srcW, srcH, mapW, mapH;
        const unsigned short *mapPtr;
        u16 mapOffset;

        /* Disable particle world by default */
        WAM[PARTICLE_WORLD << 4] = PARTICLE_BGMAP | WRLD_AFFINE;

        rendered = 0;
        for (pi = 0; pi < MAX_PARTICLES && rendered < MAX_VIS_PARTICLES; pi++) {
            Particle *p = &g_particles[pi];
            if (!p->active) continue;

            /* View-space transform (same as pickup logic) */
            dX = p->x - (s16)_playerX;
            dY = p->y - (s16)_playerY;

            if (_playerA == 0)        { viewZ = dY;  viewX = dX; }
            else if (_playerA == 512) { viewZ = -dY; viewX = -dX; }
            else if (_playerA == 256) { viewZ = dX;  viewX = -dY; }
            else if (_playerA == 768) { viewZ = -dX; viewX = dY; }
            else {
                switch (_viewQuarter) {
                    case 0: viewZ = MulS(LOOKUP8(g_cos, _viewAngle), dY) + MulS(LOOKUP8(g_sin, _viewAngle), dX);
                            viewX = MulS(LOOKUP8(g_cos, _viewAngle), dX) - MulS(LOOKUP8(g_sin, _viewAngle), dY); break;
                    case 1: viewZ = -MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), dY) + MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), dX);
                            viewX = -MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), dX) - MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), dY); break;
                    case 2: viewZ = -MulS(LOOKUP8(g_cos, _viewAngle), dY) - MulS(LOOKUP8(g_sin, _viewAngle), dX);
                            viewX = -MulS(LOOKUP8(g_cos, _viewAngle), dX) + MulS(LOOKUP8(g_sin, _viewAngle), dY); break;
                    case 3: viewZ = MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), dY) - MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), dX);
                            viewX = MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), dX) + MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), dY); break;
                }
            }

            if (viewZ <= 10) continue;

            /* Select sprite size and map data based on type */
            if (p->isGroup) {
                srcW = 32; srcH = 32;
                mapW = GROUP_TILES_W; mapH = GROUP_TILES_H;
                mapOffset = (u16)(p->variant * 4 + p->frame) * GROUP_FRAME_TILES;
                mapPtr = &shotgunGroupMap[mapOffset];
                pScale = 400.0f / (float)viewZ;
            } else {
                /* Variable-size puff frames: A,B = 8x8; C,D = 16x16 */
                switch (p->frame) {
                    case 0: srcW = 8; srcH = 8; mapW = 1; mapH = 1;
                            mapPtr = &puffMap[PUFF_OFFSET0]; break;
                    case 1: srcW = 8; srcH = 8; mapW = 1; mapH = 1;
                            mapPtr = &puffMap[PUFF_OFFSET1]; break;
                    case 2: srcW = 16; srcH = 16; mapW = 2; mapH = 2;
                            mapPtr = &puffMap[PUFF_OFFSET2]; break;
                    default: srcW = 16; srcH = 16; mapW = 2; mapH = 2;
                            mapPtr = &puffMap[PUFF_OFFSET3]; break;
                }
                pScale = 250.0f / (float)viewZ;
            }

            scaledW = (s16)((float)srcW * pScale);
            scaledH = (s16)((float)srcH * pScale);
            if (scaledW < 1) scaledW = 1;
            if (scaledW > 200) scaledW = 200;
            if (scaledH < 1) scaledH = 1;
            if (scaledH > 200) scaledH = 200;

            /* Screen position: center at wall hit point, vertically centered */
            scrX = (s16)(192 + ((s32)viewX * 192) / (s32)viewZ) - scaledW / 2;
            scrY = (s16)(104.0f) - scaledH / 2;

            if (scrX + scaledW <= 0 || scrX >= 384 || scrY + scaledH <= 0 || scrY >= 208)
                continue;

            /* Write BGMap entries (clear 4x4 area first to avoid leftovers) */
            {
                u16 *bgm = (u16*)BGMap(PARTICLE_BGMAP);
                u8 row, col;
                for (row = 0; row < 4; row++) {
                    for (col = 0; col < 4; col++) {
                        bgm[row * 64 + col] = 0;
                    }
                }
                for (row = 0; row < mapH; row++) {
                    for (col = 0; col < mapW; col++) {
                        bgm[row * 64 + col] = mapPtr[row * mapW + col];
                    }
                }
            }

            /* Set world position and dimensions */
            WA[PARTICLE_WORLD].gx = scrX;
            WA[PARTICLE_WORLD].gy = scrY;
            WA[PARTICLE_WORLD].mx = 0;
            WA[PARTICLE_WORLD].my = 0;
            WA[PARTICLE_WORLD].w = scaledW;
            WA[PARTICLE_WORLD].h = scaledH;

            affine_enemy_scale(PARTICLE_WORLD, pScale, 0);

            /* Enable world */
            WAM[PARTICLE_WORLD << 4] = WRLD_ON | PARTICLE_BGMAP | WRLD_AFFINE;
            rendered++;
        }
    }
}
u32 GetARGB(u8 brightness)
{
	return (brightness << 16) + (brightness << 8) + brightness;
}

u32 tilePosition;
void drawTile(u16 *iX, u8 *iStartY, u16 *iStartPos) {

	tilePosition = (*iStartY<<4) + ((*iX<<1)>>3);
	copymem((void*)BGMap(1)+tilePosition, (void*)(vb_doomMap+*iStartPos), 2);
}

/* Write a raw character index directly to the BGMap (for textured walls/gradient).
 * Uses BGM_PAL3 (0xC000) for independent wall palette control via GPLT3. */
u16 g_texTileEntry;
void drawTileChar(u16 *iX, u8 *iStartY, u16 charIdx) {
	tilePosition = (*iStartY<<4) + ((*iX<<1)>>3);
	g_texTileEntry = charIdx | 0xC000;  /* BGM_PAL3 */
	copymem((void*)BGMap(1)+tilePosition, (void*)&g_texTileEntry, 2);
}

/* Custom affine scaler for enemy sprites.
 * Same fixed-point accumulation as affine_fast_scale but writes ALL 8
 * param entries per scanline (zeroing unused ones) to prevent stale data.
 * Uses rounding (+32) when converting YSrc from 23.9 to 13.3 fixed-point.
 */
void affine_enemy_scale(u8 world, float scale, s16 mxOffset) {
	int tmp, scanline, height;
	s16 *param;
	f16 XScl, YScl;
	f32 YSrc;

	tmp = (world << 4);
	param = (s16*)((WAM[tmp + 9] << 1) + 0x00020000);
	XScl = YScl = inverse_fixed(scale);
	height = (int)WAM[tmp + 8];
	YSrc = 0;

	for (scanline = 0; scanline < height; scanline++) {
		int base = scanline << 3;
		param[base]     = mxOffset;             /* Mx: source X start (13.3), offset for clipping */
		param[base + 1] = 0;                    /* Mp: parallax */
		param[base + 2] = (YSrc + 32) >> 6;    /* My: 23.9 to 13.3 with rounding */
		param[base + 3] = XScl;                 /* Dx: X increment (7.9) */
		param[base + 4] = 0;                    /* unused */
		param[base + 5] = 0;                    /* unused */
		param[base + 6] = 0;                    /* unused */
		param[base + 7] = 0;                    /* unused */
		YSrc += YScl;
	}
}

// ITOFIX7_9
void affine_fast_scale_fixed2(u8 world, f16 scale) {
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

/*
void affine_fast_scale(u8 world, float scale) {
	int i,tmp;
	s16 *param;
	f16 XScl,YScl;
	f32 YSrc;

	tmp = (world<<4);
	param = (s16*)((WAM[tmp+9]<<1)+0x00020000);

	XScl = YScl = inverse_fixed(scale);
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
}*/