#ifndef _FUNCTIONS_RAYCASTERFIXED_H
#define _FUNCTIONS_RAYCASTERFIXED_H

#include <math.h>
#include <types.h>
#include <stdbool.h>
// Define the maximum distance for which we want to precalculate square roots
/*
#define MAX_DISTANCE (u16)MIN_DIST;

// Define the size of the lookup table
#define TBL_SIZE (u16)MAX_DISTANCE + 1;

// Define the lookup table for square roots
u16 sqrtLookupTable[] MAX_DISTANCE + 1];*/

void Start(u16 playerX, u16 playerY, s16 playerA);
void Trace(u16 screenX, u8* screenY, u8* textureNo, u8* textureX, u16* textureY, u16* textureStep);

void TraceEnemy(u16 *enemyX, u16 *enemyY, u8* screenY, u8* textureNo, u8* textureX, u16* textureY, u16* textureStep, u16 *enemyScreenX);
void TopDownEnemyPosition(u16 enemyX, u16 enemyY, s16* screenX, u16* screenY, f16* scale, s8* scaleInt, bool *withinView);

u16 _playerX;
u16 _playerY;
s16  _playerA;
u8  _viewQuarter;
u8  _viewAngle;

void CalculateObjectScreenPosition(u16 objectX, u16 objectY, u16 objectWidth, u16 objectHeight, u8* screenX, u8* screenY, u8* scale, bool* shouldDraw);
void     LookupHeight(u16 distance, u8* height, u16* step);
bool     IsWall(u8 tileX, u8 tileY);
u8       GetWallType(u8 tileX, u8 tileY);

/* Wall type and tile coords of the last ray hit (set by CalculateDistance, read by TraceFrame) */
extern u8 g_lastWallType;
extern u8 g_lastWallTileX;
extern u8 g_lastWallTileY;

/* Center screen column wall data (saved during TraceFrame for USE activation) */
extern u8 g_centerWallType;
extern u8 g_centerWallTileX;
extern u8 g_centerWallTileY;
s16  MulTan(u8 value, bool inverse, u8 quarter, u8 angle, const u16* lookupTable);
s16  AbsTan(u8 quarter, u8 angle, const u16* lookupTable);
//u8 Arctan(s16 *y, s16 *x);
u16 MulU(u8 v, u16 f);
s16  MulS(u8 v, s16 f);

void getObjectScreenPosAndScale(u16 *objX, u16 *objY, bool *isVisible, u16 *resX, u16 *resY, s32 *scale);



void fPlayerJaw(s32 iValue);
void fPlayerMoveForward(u16 *ifPlayerX, u16 *ifPlayerY, s16 ifPlayerAng, s16 iSpeed);
void fPlayerStrafe(u16 *ifPlayerX, u16 *ifPlayerY, s16 ifPlayerAng, s16 iSpeed);

/* Cast a ray from (rayX,rayY) along angle rayA and return exact wall hit position */
void CastRayHitPos(u16 rayX, u16 rayY, u16 rayA, s16* outHitX, s16* outHitY);

/* Bresenham tile-walk LOS: returns true if no wall between two 8.8 positions */
bool hasLineOfSight(u16 fromX, u16 fromY, u16 toX, u16 toY);

#endif