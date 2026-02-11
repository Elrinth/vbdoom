#include "RayCaster.h"
#include "RayCasterRenderer.h"
#include "RayCasterFixed.h"
#include <constants.h>
#include <mem.h>
#include "enemy.h"
#include "pickup.h"
#include "particle.h"
#include "projectile.h"
#include "door.h"
#include "doomgfx.h"
#include "../assets/images/sprites/pickups/pickup_sprites.h"
#include "../assets/images/sprites/fireball/fireball_sprites.h"
#include "../assets/images/rocket_projectile_sprites.h"
#include "../assets/images/particle_sprites.h"
#include "../assets/images/wall_textures.h"

extern BYTE vb_doomMap[];

/* Trig lookup tables (defined in RayCasterTables.h, compiled in RayCasterFixed.c) */
extern const u8 g_cos[];
extern const u8 g_sin[];

/* Map data (defined in RayCasterData.h, compiled in RayCasterFixed.c) */
extern u8 g_map[];

/* Inlined tile write macros -- eliminates function call + copymem overhead.
 * Each call saved: ~30 cycles (call) + ~30 cycles (copymem for 2 bytes).
 * At ~576 calls/frame, this saves ~34,000 cycles/frame. */
#define BGMAP1_ENTRY(py, px) (*((u16*)(BGMap(1) + ((u32)(py) << 4) + ((u32)((px) << 1) >> 3))))
#define DRAW_TILE(px, py, tilePos)    BGMAP1_ENTRY(py, px) = *((u16*)(vb_doomMap + (tilePos)))
#define DRAW_TILE_CHAR(px, py, ch)    BGMAP1_ENTRY(py, px) = (u16)(ch) | 0xC000u  /* BGM_PAL3 */

s32 diffX = 0;
s32 diffY = 0;

/* Forward declarations */
void drawTile(u16 *iX, u8 *iStartY, u16 *iStartPos);
void affine_enemy_scale(u8 world, f16 invScale, s16 mxOffset, s16 myOffset);

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
u8 g_doorGapY[48];  /* screen Y where door gap starts (0 = no gap at this column) */

/* Cached trig values for the current frame -- computed once in TraceFrame,
 * reused for all enemy/pickup/particle view-space transforms. */
static s16 g_cachedCos, g_cachedSin;
static s16 g_cachedCosInv, g_cachedSinInv; /* for INVERT(_viewAngle) */

/* Shared view-space transform: rotates (dX, dY) from world delta to
 * camera-space (viewZ = depth, viewX = lateral offset).
 * Eliminates 3 copies of ~40 lines each. */
static void worldToView(s16 dX, s16 dY, s16 *viewZ, s16 *viewX) {
    if (_playerA == 0) {
        *viewZ = dY;  *viewX = dX;
    } else if (_playerA == 512) {
        *viewZ = -dY; *viewX = -dX;
    } else if (_playerA == 256) {
        *viewZ = dX;  *viewX = -dY;
    } else if (_playerA == 768) {
        *viewZ = -dX; *viewX = dY;
    } else {
        switch (_viewQuarter) {
            case 0: *viewZ = MulS(g_cachedCos, dY) + MulS(g_cachedSin, dX);
                    *viewX = MulS(g_cachedCos, dX) - MulS(g_cachedSin, dY); break;
            case 1: *viewZ = -MulS(g_cachedCosInv, dY) + MulS(g_cachedSinInv, dX);
                    *viewX = -MulS(g_cachedCosInv, dX) - MulS(g_cachedSinInv, dY); break;
            case 2: *viewZ = -MulS(g_cachedCos, dY) - MulS(g_cachedSin, dX);
                    *viewX = -MulS(g_cachedCos, dX) + MulS(g_cachedSin, dY); break;
            case 3: *viewZ = MulS(g_cachedCosInv, dY) - MulS(g_cachedSinInv, dX);
                    *viewX = MulS(g_cachedCosInv, dX) + MulS(g_cachedSinInv, dY); break;
        }
    }
}

void TraceFrame(u16 *playerX, u16 *playerY, s16 *playerA)
{
	Start(*playerX, *playerY, *playerA);

	/* Cache trig lookups once per frame for all sprite transforms */
	g_cachedCos = LOOKUP8(g_cos, _viewAngle);
	g_cachedSin = LOOKUP8(g_sin, _viewAngle);
	g_cachedCosInv = LOOKUP8(g_cos, INVERT(_viewAngle));
	g_cachedSinInv = LOOKUP8(g_sin, INVERT(_viewAngle));
	/*
    Start((u16)(playerX * 256.0),
               (u16)(playerY * 256.0),
               (s16)(playerA / (2.0 * M_PI) * 1024.0));*/

    for(x = 0; x < SCREEN_WIDTH; x+=8)
    {
		/* --- See-through door locals --- */
		u8 isDoorPartial = 0;
		u8 door_sso_s, door_tc_s, door_tn_s;
		u8 door_tileX_s, door_tileY_s, door_openAmt;
		u16 door_tso_s, door_tst_s;

    	// reset
		drawnTop = false;
		drawnBottom = false;
		g_doorGapY[x >> 3] = 0;  /* clear door gap for this column */

        Trace(x, &sso, &tn, &tc, &tso, &tst);
		/* Save center column wall data for USE activation (before re-trace) */
		if (x == 192) {
			g_centerWallType = g_lastWallType;
			g_centerWallTileX = g_lastWallTileX;
			g_centerWallTileY = g_lastWallTileY;
		}

		/* See-through door: detect partially-open door and re-trace for background */
		if (g_lastWallType == WALL_TYPE_DOOR) {
			u8 dOpen = getDoorOpenAmount(g_lastWallTileX, g_lastWallTileY);
			if (dOpen > 0) {
				isDoorPartial = 1;
				door_sso_s = sso;
				door_tc_s = tc;
				door_tn_s = tn;
				door_tso_s = tso;
				door_tst_s = tst;
				door_tileX_s = g_lastWallTileX;
				door_tileY_s = g_lastWallTileY;
				door_openAmt = dOpen;
				/* Temporarily clear door tile and re-trace for background wall */
				g_map[(u16)door_tileY_s * MAP_X + (u16)door_tileX_s] = 0;
				Trace(x, &sso, &tn, &tc, &tso, &tst);
				g_map[(u16)door_tileY_s * MAP_X + (u16)door_tileX_s] = WALL_TYPE_DOOR;
			}
		}

		/* sso/tn/tc/tso/tst now hold background wall data (if door) or original data */
		ssoX2 = sso<<1;
		tv = tn == 1?0:1;
		if (sso < 20) tv = 1;  /* far walls always dark */

        ws = HORIZON_HEIGHT - sso;
        if(ws < 0) {
		  ws  = 0;
		  sso = HORIZON_HEIGHT;
		}
		/* For enemy occlusion at door columns, use background sso so enemies
		 * behind the door can pass the depth test and be visible through the gap.
		 * The vertical door-gap clipping in the enemy renderer handles the rest. */
		g_wallSso[x >> 3] = sso;
        wsPercent8 = ws & 7;
		y = 0;
		curY = 0;
		clampedY = 0;
		tile = 0;
		drawnTop = false;
		drawnBottom = false;

		/* Compute wall type early so ceiling transitions can use it */
		wt = g_lastWallType;
		if (wt < 1 || wt > WALL_TEX_COUNT) wt = 1;
		if (wt == WALL_TYPE_DOOR) tv = 0;  /* doors always use bright face */

		/* Door height reduction: only for non-see-through cases.
		 * When isDoorPartial, the background wall is rendered at full height;
		 * the door itself is drawn as an overdraw pass below. */
		if (!isDoorPartial && wt == WALL_TYPE_DOOR) {
			u8 doorOpen = getDoorOpenAmount(g_lastWallTileX, g_lastWallTileY);
			if (doorOpen > 0) {
				u16 reduction = ((u16)ssoX2 * doorOpen) / DOOR_OPEN_MAX;
				if (reduction >= ssoX2) {
					ssoX2 = 0;
					sso = 0;
				} else {
					ssoX2 -= (u16)reduction;
					sso = (u8)(ssoX2 >> 1);
				}
				wsPercent8 = ws & 7;
			}
		}

		wallTexBase = WALL_TEX_CHAR_START + (u16)(wt - 1) * WALL_TEX_PER_TYPE
		            + (u16)tv * WALL_TEX_LIT_BLOCK + (tc >> 5);

        for(y = 0; y < ws; y+=8) /* draw ceiling */
        {
            if (y+7 >= ws && wsPercent8 > 0) {
            	/* Textured ceiling/wall transition tile */
            	drawnTop = true;
				transChar = TRANS_TEX_CHAR_START + (u16)(wt - 1) * 14
				          + (u16)tv * 7 + (7 - wsPercent8);
				DRAW_TILE_CHAR(x, curY, transChar);
            } else {
				DRAW_TILE(x, curY, 0);
			}
			curY+=8;
        }
		vanligTile = 0;
		texAccum = (u32)tso;

		/* Compute wall draw length: subtract the wall pixels already drawn
		 * by the ceiling transition tile to avoid bottom overshoot.
		 * This also fixes bottomFill resolution: ssoX2 is always even (= 2*sso)
		 * so ssoX2 & 7 only produces 0/2/4/6. After subtracting the ceiling
		 * overlap, wallDrawLen has full 1-pixel resolution. */
		{
			u16 wallDrawLen = ssoX2;
			if (drawnTop) {
				u16 ceilOverlap = (u16)(8 - wsPercent8);
				if (wallDrawLen > ceilOverlap)
					wallDrawLen -= ceilOverlap;
				else
					wallDrawLen = 0;
			}
			bottomFill = wallDrawLen & 7;

			for(y = 0; y < wallDrawLen; y+=8) /* draw walls */
			{
				/* Compute row from textureY accumulator */
				texRow = (u8)((texAccum >> 13) & 7);
				wallTexChar = wallTexBase + (u16)texRow * WALL_TEX_COLS;

				if (y == 0 && ws != 0 && drawnTop == false) {
					/* Top partial: wsPercent8 is 0 here, wall starts on tile boundary */
					DRAW_TILE_CHAR(x, curY, wallTexChar);
				} else if (y + 7 >= wallDrawLen && bottomFill > 0) {
					/* Bottom partial: textured transition tile with V-flip */
					transChar = TRANS_TEX_CHAR_START + (u16)(wt - 1) * 14
					          + (u16)tv * 7 + (bottomFill - 1);
					DRAW_TILE_CHAR(x, curY, transChar | 0x1000);  /* V-flip */
					drawnBottom = true;
				} else {
					/* Full textured wall tile */
					DRAW_TILE_CHAR(x, curY, wallTexChar);
				}
				texAccum += (u32)tst << 3;  /* advance textureY by tst * 8 scanlines */
				curY+=8;
			}
		}
        /*
        // draw roof
        			drawDoomStageTile(xPos, 0, cyaRoof, 2482);
        			// draw floor
        			drawDoomStageTile(xPos, cybFloor, H-1, 2480);
        			// draw wall
        			drawDoomStageTile(xPos, cyaRoof, cybFloor, 2484); //2472);
        			*/



        {	/* draw floor -- fill all remaining rows to screen bottom */
			bool floorFirst = true;
			while (curY < SCREEN_HEIGHT) {
				if (floorFirst && drawnBottom == false && bottomFill > 0) {
					/* Floor/wall transition: textured V-flip */
					transChar = TRANS_TEX_CHAR_START + (u16)(wt - 1) * 14
							  + (u16)tv * 7 + (bottomFill - 1);
					DRAW_TILE_CHAR(x, curY, transChar | 0x1000);  /* V-flip */
				} else {
					DRAW_TILE(x, curY, 0);
				}
				floorFirst = false;
				curY+=8;
			}
		}

		/* === Door overdraw: draw door ceiling + shortened wall on top of background === */
		if (isDoorPartial) {
			u16 door_ssoX2_v;
			s16 door_ws_s16;
			u8 door_ws_v, door_wsP8;
			u16 door_reduction;
			u16 door_wTexBase;
			u16 door_wallDrawLen;
			u8 door_bFill;

			/* Compute door geometry from saved trace data */
			door_ssoX2_v = (u16)door_sso_s << 1;
			door_ws_s16 = HORIZON_HEIGHT - (s16)door_sso_s;
			if (door_ws_s16 < 0) {
				door_ws_v = 0;
				door_ssoX2_v = (u16)HORIZON_HEIGHT << 1;
			} else {
				door_ws_v = (u8)door_ws_s16;
			}
			door_wsP8 = door_ws_v & 7;

			/* Apply door height reduction */
			door_reduction = ((u16)door_ssoX2_v * (u16)door_openAmt) / DOOR_OPEN_MAX;
			if (door_reduction >= door_ssoX2_v)
				door_ssoX2_v = 0;
			else
				door_ssoX2_v -= door_reduction;

			/* Door texture base (doors always use bright face: tv=0) */
			door_wTexBase = WALL_TEX_CHAR_START
			              + (u16)(WALL_TYPE_DOOR - 1) * WALL_TEX_PER_TYPE
			              + (door_tc_s >> 5);

			/* Reset curY and overdraw ceiling + shortened wall */
			curY = 0;
			drawnTop = false;

			/* Draw door ceiling tiles */
			for (y = 0; y < door_ws_v; y += 8) {
				if (y + 7 >= door_ws_v && door_wsP8 > 0) {
					drawnTop = true;
					transChar = TRANS_TEX_CHAR_START
					          + (u16)(WALL_TYPE_DOOR - 1) * 14
					          + (7 - door_wsP8);
					DRAW_TILE_CHAR(x, curY, transChar);
				} else {
					DRAW_TILE(x, curY, 0);
				}
				curY += 8;
			}

			/* Draw shortened door wall tiles */
			door_wallDrawLen = door_ssoX2_v;
			if (drawnTop) {
				u16 cOverlap = (u16)(8 - door_wsP8);
				if (door_wallDrawLen > cOverlap)
					door_wallDrawLen -= cOverlap;
				else
					door_wallDrawLen = 0;
			}
			door_bFill = door_wallDrawLen & 7;
			texAccum = (u32)door_tso_s;

			for (y = 0; y < door_wallDrawLen; y += 8) {
				texRow = (u8)((texAccum >> 13) & 7);
				wallTexChar = door_wTexBase + (u16)texRow * WALL_TEX_COLS;

				if (y == 0 && door_ws_v != 0 && drawnTop == false) {
					DRAW_TILE_CHAR(x, curY, wallTexChar);
				} else if (y + 7 >= door_wallDrawLen && door_bFill > 0) {
					transChar = TRANS_TEX_CHAR_START
					          + (u16)(WALL_TYPE_DOOR - 1) * 14
					          + (door_bFill - 1);
					DRAW_TILE_CHAR(x, curY, transChar | 0x1000);  /* V-flip */
				} else {
					DRAW_TILE_CHAR(x, curY, wallTexChar);
				}
				texAccum += (u32)door_tst_s << 3;
				curY += 8;
			}
			/* Record where the door gap starts for enemy/pickup vertical clipping */
			g_doorGapY[x >> 3] = curY;
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
        f16 invScale;   /* 7.9 fixed-point reciprocal for affine */
        u8 worldNum, bgmapIdx;
        s16 myOffset;   /* source Y offset for vertical door-gap clipping */
        /* Debug: uncomment to show enemy 0 viewZ/scrX/scrY on HUD:
         * u8 ones, tens, hundreds, thousands;
         * u16 startPos, drawPos;
         * s16 dbgViewZ = 0, dbgScrX = 0, dbgScrY = 0; */

        for (ei = 0; ei < MAX_VISIBLE_ENEMIES; ei++) {
            u8 realIdx = g_visibleEnemies[ei];
            EnemyState *e;

            /* 1 world per visible slot, 1 BGMap per visible slot */
            worldNum = 30 - ei;                    /* slot 0 = world 30, slot 1 = world 29 */
            bgmapIdx = ENEMY_BGMAP_START + ei;     /* slot 0 = BGMap 3, slot 1 = BGMap 4 */

            scrX = 0;
            scrY = 0;
            viewZ = 0;
            viewX = 0;

            /* STEP 0: Disable world immediately (prevents ghost rendering) */
            WAM[worldNum << 4] = bgmapIdx | WRLD_AFFINE;

            if (realIdx == 255) continue;
            e = &g_enemies[realIdx];
            if (!e->active) continue;

            /* Compute delta from player to enemy */
            dX = (s16)e->x - (s16)_playerX;
            dY = (s16)e->y - (s16)_playerY;

            /* View-space transform (shared function, cached trig) */
            worldToView(dX, dY, &viewZ, &viewX);

            if (viewZ <= 10) continue; /* behind player or too close */

            /* Perspective projection -- 64x64 sprite (integer math)
             * enemyScale = 800/viewZ, so scaledW = 64*800/viewZ = 51200/viewZ */
            scaledW = (s16)(51200 / (s32)viewZ);
            scaledH = scaledW;

            /* Clamp dimensions */
            if (scaledW < 1) scaledW = 1;
            if (scaledW > 384) scaledW = 384;
            if (scaledH < 1) scaledH = 1;
            if (scaledH > 224) scaledH = 224;

            /* Screen position: center horizontally, feet at horizon (y=104) */
            scrX = (s16)(192 + ((s32)viewX * 192) / (s32)viewZ) - (scaledW >> 1);
            scrY = 104 - (s16)(((s32)scaledH * 85) >> 8);  /* 85/256 ~ 1/3 */

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

                /* Compute affine source X offset for left clipping
                 * inverse_fixed(800/viewZ) = 512*viewZ/800 = viewZ*16/25 */
                invScale = (f16)(((s32)viewZ << 4) / 25);
                mxOffset = (clipL > 0) ? (s16)(((s32)clipL * (s32)invScale) >> 6) : 0;
                myOffset = 0;

                /* Vertical door-gap clipping: if enemy is only visible through
                 * door gap columns, clip its top to the gap Y */
                {
                    bool allDoorGap = true;
                    u8 maxGapY = 0;
                    for (ocCol = visLeft; ocCol <= visRight; ocCol++) {
                        if (g_wallSso[ocCol] < enemySso) {  /* visible at this col */
                            if (g_doorGapY[ocCol] == 0) {
                                allDoorGap = false;
                                break;
                            }
                            if (g_doorGapY[ocCol] > maxGapY)
                                maxGapY = g_doorGapY[ocCol];
                        }
                    }
                    if (allDoorGap && maxGapY > 0 && maxGapY > scrY) {
                        s16 clipTop = maxGapY - scrY;
                        scrY += clipTop;
                        scaledH -= clipTop;
                        myOffset = (s16)(((s32)clipTop * (s32)invScale) >> 6);
                        if (scaledH < 1) continue;
                    }
                }

                /* Write BGMap entries per-frame.
                 * For mirrored directions (5,6,7) we reverse tile column order
                 * and set BGM_HFLIP (0x2000) so left-facing sprites appear
                 * correctly as right-facing. */
                {
                    u16 *bgm = (u16*)BGMap(bgmapIdx);
                    u16 charBase = ZOMBIE_CHAR_START + ei * ENEMY_CHAR_STRIDE;
                    u8 row, c;
                    u8 eDir = getEnemyDirection(realIdx, _playerX, _playerY, _playerA);
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

                /* Apply affine scaling with source X/Y offset for clipping */
                affine_enemy_scale(worldNum, invScale, mxOffset, myOffset);

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
        f16 invScale;   /* 7.9 fixed-point reciprocal for affine */
        u8 worldNum, bgmapIdx;
        s16 myOffset;   /* source Y offset for vertical door-gap clipping */

        /* Disable all pickup worlds first (prevents ghost rendering) */
        for (slot = 0; slot < MAX_VISIBLE_PICKUPS; slot++) {
            worldNum = 25 - slot;
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

            /* View-space transform (shared function, cached trig) */
            worldToView(dX, dY, &viewZ, &viewX);

            if (viewZ <= 10) continue; /* behind player or too close */

            /* Perspective projection -- 32x24 source sprite.
             * Weapon pickups (shotgun, rocket) use 2x scale for visibility. */
            {
                u16 scaleNumer = 400;
                if (p->type == PICKUP_WEAPON_SHOTGUN || p->type == PICKUP_WEAPON_ROCKET)
                    scaleNumer = 800;
                scaledW = (s16)((u32)32 * scaleNumer / (s32)viewZ);
                scaledH = (s16)((u32)24 * scaleNumer / (s32)viewZ);
            }

            /* Clamp dimensions */
            if (scaledW < 1) scaledW = 1;
            if (scaledW > 384) scaledW = 384;
            if (scaledH < 1) scaledH = 1;
            if (scaledH > 224) scaledH = 224;

            /* Screen position: center horizontally.
             * groundY = 104 + 34133/viewZ — same for all pickups (floor doesn't move with sprite size).
             * Pickup BOTTOM sits at groundY, so top = groundY - scaledH.
             */
            scrX = (s16)(192 + ((s32)viewX * 192) / (s32)viewZ) - (scaledW >> 1);
            scrY = (s16)(104 + (s16)(34133 / (s32)viewZ)) - scaledH;

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

                /* Source X offset for left clipping.
                 * invScale = 512*viewZ/scaleNumer: 400->viewZ*32/25, 800->viewZ*16/25 */
                {
                    u16 scaleNumer = 400;
                    if (p->type == PICKUP_WEAPON_SHOTGUN || p->type == PICKUP_WEAPON_ROCKET)
                        scaleNumer = 800;
                    invScale = (f16)(((s32)viewZ << 9) / (s32)scaleNumer);
                }
                mxOffset = (clipL > 0) ? (s16)(((s32)clipL * (s32)invScale) >> 6) : 0;
                myOffset = 0;

                /* Vertical door-gap clipping: if pickup is only visible through
                 * door gap columns, clip its top to the gap Y */
                {
                    bool allDoorGap = true;
                    u8 maxGapY = 0;
                    for (ocCol = visLeft; ocCol <= visRight; ocCol++) {
                        if (g_wallSso[ocCol] < pickupSso) {  /* visible at this col */
                            if (g_doorGapY[ocCol] == 0) {
                                allDoorGap = false;
                                break;
                            }
                            if (g_doorGapY[ocCol] > maxGapY)
                                maxGapY = g_doorGapY[ocCol];
                        }
                    }
                    if (allDoorGap && maxGapY > 0 && maxGapY > scrY) {
                        s16 clipTop = maxGapY - scrY;
                        scrY += clipTop;
                        scaledH -= clipTop;
                        myOffset = (s16)(((s32)clipTop * (s32)invScale) >> 6);
                        if (scaledH < 1) continue;
                    }
                }

                /* Assign this pickup to a render slot */
                worldNum = 25 - slot;
                bgmapIdx = PICKUP_BGMAP_START + slot;

                /* Load the correct pickup sprite tiles into this slot's char memory.
                 * For animated pickups (helmet, armor), use the current animation frame. */
                {
                    const unsigned int *pTileData;
                    if (p->type == PICKUP_HELMET) {
                        pTileData = PICKUP_HELMET_FRAME_TABLE[getPickupAnimFrame(p)];
                    } else if (p->type == PICKUP_ARMOR) {
                        pTileData = PICKUP_ARMOR_FRAME_TABLE[getPickupAnimFrame(p)];
                    } else {
                        pTileData = PICKUP_TILES[p->type];
                    }
                    loadPickupFrame(slot, pTileData);
                }

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
                affine_enemy_scale(worldNum, invScale, mxOffset, myOffset);

                /* ENABLE world */
                WAM[worldNum << 4] = WRLD_ON | bgmapIdx | WRLD_AFFINE;

                slot++;
            } /* end occlusion block */
        } /* end for each pickup */
    }

    /* ================================================================
     * PARTICLE / FIREBALL RENDERING
     * Uses world 23 with BGMap 0 (affine mode) -- 1 render slot
     * Priority: fireballs > bullet puffs / shotgun groups
     * ================================================================ */
    {
        u8 pi, rendered;
        s16 dX, dY, viewZ, viewX;
        s16 scrX, scrY, scaledW, scaledH;
        f16 invScale;   /* 7.9 fixed-point reciprocal for affine */
        u16 scaleNumer; /* numerator for scale: 400 (group) or 250 (puff) */
        u8 srcW, srcH, mapW, mapH;
        const unsigned short *mapPtr;
        u16 mapOffset;

        /* Disable particle world by default */
        WAM[PARTICLE_WORLD << 4] = PARTICLE_BGMAP | WRLD_AFFINE;

        rendered = 0;

        /* --- PROJECTILE RENDERING (fireballs + rockets, priority over puffs) --- */
        for (pi = 0; pi < MAX_PROJECTILES && rendered < MAX_VIS_PARTICLES; pi++) {
            Projectile *proj = &g_projectiles[pi];
            const unsigned int *fbTileData;
            u16 fbCharBase;
            u8 tileW, tileH;

            if (proj->state == PROJ_DEAD) continue;

            /* View-space transform */
            dX = (s16)proj->x - (s16)_playerX;
            dY = (s16)proj->y - (s16)_playerY;
            worldToView(dX, dY, &viewZ, &viewX);

            if (viewZ <= 10) continue;

            /* Select tile data based on projectile type and state */
            tileW = 4; tileH = 4; /* default: fireball 32x32 */
            if (proj->state == PROJ_FLYING) {
                if (proj->type == PROJ_TYPE_ROCKET) {
                    /* Rocket: directional sprite (4x3 tiles = 32x24) */
                    u8 dir8;
                    s16 relAngle = proj->angle - _playerA;
                    if (relAngle < 0) relAngle += 1024;
                    dir8 = (u8)(((relAngle + 64) >> 7) & 7);
                    fbTileData = ROCKET_PROJ_FRAMES[ROCKET_PROJ_DIR_FRAME[dir8]];
                    tileW = ROCKET_PROJ_TILE_W; tileH = ROCKET_PROJ_TILE_H;
                } else {
                    fbTileData = FIREBALL_FLIGHT_FRAMES[proj->animFrame & 1];
                }
            } else {
                /* Both fireball and rocket use same explosion frames */
                u8 ef = proj->animFrame;
                if (ef >= FIREBALL_EXPLODE_COUNT) ef = FIREBALL_EXPLODE_COUNT - 1;
                fbTileData = FIREBALL_EXPLODE_FRAMES[ef];
            }

            /* Load tiles into char memory (after particle tiles) */
            fbCharBase = PARTICLE_CHAR_START + PARTICLE_TILE_COUNT;
            {
                u32 addr = 0x00078000 + (u32)fbCharBase * 16;
                u16 bytes = (u16)tileW * tileH * 16;
                copymem((void*)addr, (void*)fbTileData, bytes);
            }

            srcW = tileW * 8; srcH = tileH * 8;
            scaleNumer = 400;

            scaledW = (s16)(((s32)srcW * scaleNumer) / (s32)viewZ);
            scaledH = (s16)(((s32)srcH * scaleNumer) / (s32)viewZ);
            if (scaledW < 1) scaledW = 1;
            if (scaledW > 200) scaledW = 200;
            if (scaledH < 1) scaledH = 1;
            if (scaledH > 200) scaledH = 200;

            scrX = (s16)(192 + ((s32)viewX * 192) / (s32)viewZ) - (scaledW >> 1);
            scrY = 104 - (scaledH >> 1);

            if (scrX + scaledW <= 0 || scrX >= 384 || scrY + scaledH <= 0 || scrY >= 208)
                continue;

            /* Write BGMap entries */
            {
                u16 *bgm = (u16*)BGMap(PARTICLE_BGMAP);
                u8 row, col;
                for (row = 0; row < tileH; row++) {
                    for (col = 0; col < tileW; col++) {
                        bgm[row * 64 + col] = fbCharBase + row * tileW + col;
                    }
                }
            }

            WA[PARTICLE_WORLD].gx = scrX;
            WA[PARTICLE_WORLD].gy = scrY;
            WA[PARTICLE_WORLD].mx = 0;
            WA[PARTICLE_WORLD].my = 0;
            WA[PARTICLE_WORLD].w = scaledW;
            WA[PARTICLE_WORLD].h = scaledH;

            invScale = (f16)(((s32)viewZ << 9) / (s32)scaleNumer);
            affine_enemy_scale(PARTICLE_WORLD, invScale, 0, 0);

            WAM[PARTICLE_WORLD << 4] = WRLD_ON | PARTICLE_BGMAP | WRLD_AFFINE;
            rendered++;
        }

        /* --- PARTICLE PUFF RENDERING (only if no fireball was rendered) --- */
        for (pi = 0; pi < MAX_PARTICLES && rendered < MAX_VIS_PARTICLES; pi++) {
            Particle *p = &g_particles[pi];
            if (!p->active) continue;

            /* View-space transform (shared function, cached trig) */
            dX = p->x - (s16)_playerX;
            dY = p->y - (s16)_playerY;
            worldToView(dX, dY, &viewZ, &viewX);

            if (viewZ <= 10) continue;

            /* Select sprite size and map data based on type */
            if (p->isGroup) {
                srcW = 32; srcH = 32;
                mapW = GROUP_TILES_W; mapH = GROUP_TILES_H;
                mapOffset = (u16)(p->variant * 4 + p->frame) * GROUP_FRAME_TILES;
                mapPtr = &shotgunGroupMap[mapOffset];
                scaleNumer = 400;
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
                scaleNumer = 250;
            }

            /* scaledW = srcW * scaleNumer / viewZ (all integer) */
            scaledW = (s16)(((s32)srcW * scaleNumer) / (s32)viewZ);
            scaledH = (s16)(((s32)srcH * scaleNumer) / (s32)viewZ);
            if (scaledW < 1) scaledW = 1;
            if (scaledW > 200) scaledW = 200;
            if (scaledH < 1) scaledH = 1;
            if (scaledH > 200) scaledH = 200;

            /* Screen position: center at wall hit point, vertically centered */
            scrX = (s16)(192 + ((s32)viewX * 192) / (s32)viewZ) - (scaledW >> 1);
            scrY = 104 - (scaledH >> 1);

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

            /* inverse_fixed(scaleNumer/viewZ) = 512*viewZ/scaleNumer */
            invScale = (f16)(((s32)viewZ << 9) / (s32)scaleNumer);
            affine_enemy_scale(PARTICLE_WORLD, invScale, 0, 0);

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

/* Keep function versions for any external callers (e.g., doomstage.c) */
u32 tilePosition;
void drawTile(u16 *iX, u8 *iStartY, u16 *iStartPos) {
	BGMAP1_ENTRY(*iStartY, *iX) = *((u16*)(vb_doomMap + *iStartPos));
}
void drawTileChar(u16 *iX, u8 *iStartY, u16 charIdx) {
	BGMAP1_ENTRY(*iStartY, *iX) = charIdx | 0xC000u;
}

/* Custom affine scaler for enemy sprites.
 * Same fixed-point accumulation as affine_fast_scale but writes ALL 8
 * param entries per scanline (zeroing unused ones) to prevent stale data.
 * Uses rounding (+32) when converting YSrc from 23.9 to 13.3 fixed-point.
 */
void affine_enemy_scale(u8 world, f16 invScale, s16 mxOffset, s16 myOffset) {
	int tmp, scanline, height;
	s16 *param;
	f16 XScl, YScl;
	f32 YSrc;

	tmp = (world << 4);
	param = (s16*)((WAM[tmp + 9] << 1) + 0x00020000);
	XScl = YScl = invScale;  /* already 7.9 fixed-point reciprocal */
	height = (int)WAM[tmp + 8];
	YSrc = (f32)((s32)myOffset << 6);  /* convert 13.3 to accumulator format */

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