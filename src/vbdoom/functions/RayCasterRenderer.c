#define _USE_MATH_DEFINES

#include "RayCaster.h"
#include "RayCasterRenderer.h"
#include "RayCasterFixed.h"
#include <constants.h>
#include <math.h>
#include "enemy.h"
#include "pickup.h"
#include "doomgfx.h"
#include "../assets/images/sprites/pickups/pickup_sprites.h"

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
        for(y = 0; y < ws; y+=8) // draw roof
        {
            //*lb = GetARGB(96 + (HORIZON_HEIGHT - y));
            //lb += SCREEN_WIDTH;
            //clampedY = clamp(curY, 0, H);
            //drawTile(x, curY, curY+8, 2478);
            tile = 0;
            if (y+7 >= ws) {
            	drawnTop = true;
				tile = 2466;
				if (tv == 0)
					tile = 2462;
				if (wsPercent8 == 1) {
					tile = tile-96*5;
				} else if (wsPercent8 == 2) {
					tile = tile-96*4;
				} else if (wsPercent8 == 3) {
					tile = tile-96*3;
				} else if (wsPercent8 == 4) {
					tile = tile-96*2;
				} else if (wsPercent8 == 5) {
					tile = tile-96;
				} else if (wsPercent8 == 6) {
					tile = tile;
				} else if (wsPercent8 == 7) {
					tile = tile+96;
				}
            }
            drawTile(&x, &curY, &tile);
			curY+=8;
        }
		vanligTile = 0;
        for(y = 0; y < ssoX2; y+=8) // draw walls
        {
        	// whole colored tile
            tile = 2480;
            if (tv == 0)
            	tile = 2478;
			vanligTile = tile;
            // part tile:
            if (y == 0 && ws != 0 && drawnTop == false) {
            	tile = 2466;
            	if (tv == 0)
            		tile = 2462;
				if (wsPercent8 == 0) {
					tile = tile-96*5;
				} else if (wsPercent8 == 1) {
					tile = tile-96*4;
				} else if (wsPercent8 == 2) {
					tile = tile-96*3;
				} else if (wsPercent8 == 3) {
					tile = tile-96*2;
				} else if (wsPercent8 == 4) {
					tile = tile-96;
				} else if (wsPercent8 == 5) {
					tile = tile;
				} else if (wsPercent8 == 6) {
					tile = tile+96;
				} else if (wsPercent8 == 7) {
					tile = tile+96*2;
				}
            } else if (y + 7 >= ssoX2) {

            	tile = 2468;
            	if (tv < 1)
					tile = 2464;
				if (wsPercent8 == 7) {
					drawnBottom = true;
					tile = tile+96*2;
				} else if (wsPercent8 == 6) {
					tile = vanligTile;
				} else if (wsPercent8 == 5) {
					tile = vanligTile;
				} else if (wsPercent8 == 3) {
					drawnBottom = true;
					tile = tile-96*1;
				} else if (wsPercent8 == 2) {
					drawnBottom = true;
					tile = tile-96*0;
				} else if (wsPercent8 == 1) {
					drawnBottom = true;
					tile = tile+96*1;
				} else if (wsPercent8 == 0) {
					drawnBottom = true;
					tile = tile-96*5;
				}
            }

            drawTile(&x, &curY, &tile);
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



        for(y = 0; y < ws; y+=8) // draw floor
        {
        	tile = 0;
        	vanligTile = tile;
        	if (ws != 0 && y == 0 && drawnBottom == false) {
				tile = 2468;
				if (tv < 1)
					tile = 2464;
				if (wsPercent8 == 0) {
					tile = tile+96;
				} else if (wsPercent8 == 4) {
					tile = tile-96*2;
				} else if (wsPercent8 == 5) {
					tile = tile-96*3;
				} else if (wsPercent8 == 6) {
					tile = tile-96*5;
				}
			}
            //*lb = GetARGB(96 + (HORIZON_HEIGHT - (ws - y)));
            //lb += SCREEN_WIDTH;
            drawTile(&x, &curY, &tile);
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
        u8 ones, tens, hundreds, thousands;
        u16 startPos, drawPos;

        /* Save enemy 0 debug values */
        s16 dbgViewZ = 0, dbgScrX = 0, dbgScrY = 0;

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

            /* Save debug values for enemy 0 */
            if (ei == 0) {
                dbgViewZ = viewZ;
                dbgScrX = scrX;
                dbgScrY = scrY;
            }

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

        /* Debug display: show enemy 0 viewZ, scrX, scrY on UI */
        startPos = 96*18;

        /* Row 1: viewZ (depth to enemy 0) */
        ones = (u8)(ABS(dbgViewZ) % 10);
        tens = (u8)((ABS(dbgViewZ) / 10) % 10);
        hundreds = (u8)((ABS(dbgViewZ) / 100) % 10);
        thousands = (u8)((ABS(dbgViewZ) / 1000) % 10);
        drawPos = 142;
        if (thousands > 0) {
            copymem((void*)BGMap(13)+drawPos+2, (void*)(vb_doomMap+startPos+(thousands*2)), 2);
        } else {
            copymem((void*)BGMap(13)+drawPos+2, (void*)(vb_doomMap+startPos+20), 2);
        }
        if (thousands > 0 || hundreds > 0) {
            copymem((void*)BGMap(13)+drawPos+4, (void*)(vb_doomMap+startPos+(hundreds*2)), 2);
        } else {
            copymem((void*)BGMap(13)+drawPos+4, (void*)(vb_doomMap+startPos+20), 2);
        }
        if (thousands > 0 || hundreds > 0 || tens > 0) {
            copymem((void*)BGMap(13)+drawPos+6, (void*)(vb_doomMap+startPos+(tens*2)), 2);
        } else {
            copymem((void*)BGMap(13)+drawPos+6, (void*)(vb_doomMap+startPos+20), 2);
        }
        copymem((void*)BGMap(13)+drawPos+8, (void*)(vb_doomMap+startPos+(ones*2)), 2);

        /* Row 2: scrX (enemy 0 screen X position) */
        ones = (u8)(ABS(dbgScrX) % 10);
        tens = (u8)((ABS(dbgScrX) / 10) % 10);
        hundreds = (u8)((ABS(dbgScrX) / 100) % 10);
        thousands = (u8)((ABS(dbgScrX) / 1000) % 10);
        drawPos = 152;
        if (thousands > 0) {
            copymem((void*)BGMap(13)+drawPos+2, (void*)(vb_doomMap+startPos+(thousands*2)), 2);
        } else {
            copymem((void*)BGMap(13)+drawPos+2, (void*)(vb_doomMap+startPos+20), 2);
        }
        if (thousands > 0 || hundreds > 0) {
            copymem((void*)BGMap(13)+drawPos+4, (void*)(vb_doomMap+startPos+(hundreds*2)), 2);
        } else {
            copymem((void*)BGMap(13)+drawPos+4, (void*)(vb_doomMap+startPos+20), 2);
        }
        if (thousands > 0 || hundreds > 0 || tens > 0) {
            copymem((void*)BGMap(13)+drawPos+6, (void*)(vb_doomMap+startPos+(tens*2)), 2);
        } else {
            copymem((void*)BGMap(13)+drawPos+6, (void*)(vb_doomMap+startPos+20), 2);
        }
        copymem((void*)BGMap(13)+drawPos+8, (void*)(vb_doomMap+startPos+(ones*2)), 2);

        /* Row 3: scrY (enemy 0 screen Y position) */
        ones = (u8)(ABS(dbgScrY) % 10);
        tens = (u8)((ABS(dbgScrY) / 10) % 10);
        hundreds = (u8)((ABS(dbgScrY) / 100) % 10);
        thousands = (u8)((ABS(dbgScrY) / 1000) % 10);
        drawPos = 192;
        if (thousands > 0) {
            copymem((void*)BGMap(13)+drawPos+2, (void*)(vb_doomMap+startPos+(thousands*2)), 2);
        } else {
            copymem((void*)BGMap(13)+drawPos+2, (void*)(vb_doomMap+startPos+20), 2);
        }
        if (thousands > 0 || hundreds > 0) {
            copymem((void*)BGMap(13)+drawPos+4, (void*)(vb_doomMap+startPos+(hundreds*2)), 2);
        } else {
            copymem((void*)BGMap(13)+drawPos+4, (void*)(vb_doomMap+startPos+20), 2);
        }
        if (thousands > 0 || hundreds > 0 || tens > 0) {
            copymem((void*)BGMap(13)+drawPos+6, (void*)(vb_doomMap+startPos+(tens*2)), 2);
        } else {
            copymem((void*)BGMap(13)+drawPos+6, (void*)(vb_doomMap+startPos+20), 2);
        }
        copymem((void*)BGMap(13)+drawPos+8, (void*)(vb_doomMap+startPos+(ones*2)), 2);
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