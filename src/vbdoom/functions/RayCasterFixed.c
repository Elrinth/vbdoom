#include "RayCasterFixed.h"
#include "RayCasterData.h"
#include "enemy.h"

#define LOOKUP_STORAGE extern
#include "RayCasterTables.h"
/* math.h already included via RayCasterFixed.h; M_PI only used in comments */
#define W (384) // vb screen width
#define Wby2 (W/2)
#define H 208 // reduce by interface height which is 32..
#define Hby2 (H/2)

u16 hfovv = (u16)(0.6 * SCREEN_HEIGHT);
/* vfov = 0.6 * 224 = 134.4, integer approximation 134.
 * Used as divisor: inverse_fixed(vfov/rotatedY) = 512*rotatedY/vfov */
#define VFOV_INT 134

// (v * f) >> 8
u16 MulU(u8 v, u16 f)
{
    const u8  f_h = f >> 8;
    const u8  f_l = f & 0xFF;
    const u16 hm  = v * f_h;
    const u16 lm  = v * f_l;
    return hm + (lm >> 8);
}

s16 MulS(u8 v, s16 f)
{
    const u16 uf = MulU(v, (u16)(ABS(f)));
    if(f < 0)
    {
        return ~uf;
    }
    return uf;
}

s16 MulTan(u8 value, bool inverse, u8 quarter, u8 angle, const u16* lookupTable)
{
    u8 signedValue = value;
    if(inverse)
    {
        if(value == 0)
        {
            if(quarter & 1)
            {
                return -AbsTan(quarter, angle, lookupTable);
            }
            return AbsTan(quarter, angle, lookupTable);
        }
        signedValue = INVERT(value);
    }
    if(signedValue == 0)
    {
        return 0;
    }
    if(quarter & 1)
    {
        return -MulU(signedValue, LOOKUP16(lookupTable, INVERT(angle)));
    }
    return MulU(signedValue, LOOKUP16(lookupTable, angle));
}

s16 AbsTan(u8 quarter, u8 angle, const u16* lookupTable)
{
    if(quarter & 1)
    {
        return LOOKUP16(lookupTable, INVERT(angle));
    }
    return LOOKUP16(lookupTable, angle);
}

/*
u8 Arctan(s16 *dy, s16 *dx) {
  // Handle zero division (favoring right angles)

  if (*dx == 0) {
  	return 0;
  }

  // Pre-calculated constants for table lookup (adjust based on your table size)
  u16 RECIP_MAX_DY =  (1 << (4))  // Reciprocal of max dy (fixed-point)

  // Ensure dx is always positive for lookup (flip sign if needed)
  bool negativeX = *dx < 0;
  *dx = ABS(*dx);

  // Calculate approximate index using multiplication and bit shifting
  int index = (*dy * RECIP_MAX_DY) >> (4);

  // Clamp index to table size
  if (index < 0) {
    index = 0;
  } else if (index >= 256) {
    index = 256 - 1;
  }

  // Lookup arctangent value from the table
  u8 arctan = g_arctan_table[index];

  // Restore sign if dx was negative
  if (negativeX) {
    arctan = _PI - arctan;
  }

  // Linear interpolation for better accuracy (optional)
  // ... (implement linear interpolation using additional pre-calculated values)

  return arctan;
}*/

/* Wall type and tile coords of the last ray hit (set by CalculateDistance, read by TraceFrame) */
u8 g_lastWallType = 1;
u8 g_lastWallTileX = 0;
u8 g_lastWallTileY = 0;

/* Center screen column wall data (saved during TraceFrame for USE activation) */
u8 g_centerWallType = 0;
u8 g_centerWallTileX = 0;
u8 g_centerWallTileY = 0;

bool IsWall(u8 tileX, u8 tileY)
{
    if(tileX > MAP_X - 1 || tileY > MAP_Y - 1)
    {
        return true;
    }
    return g_map[(u16)tileY * MAP_X + (u16)tileX] != 0;
}

u8 GetWallType(u8 tileX, u8 tileY)
{
    if(tileX > MAP_X - 1 || tileY > MAP_Y - 1)
    {
        return 1;
    }
    return g_map[(u16)tileY * MAP_X + (u16)tileX];
}

void LookupHeight(u16 distance, u8* height, u16* step) {
    if(distance >= 256)  {
        const u16 ds = distance >> 3;
        if(ds >= 256) {
			*height = LOOKUP8(g_farHeight, 255) - 1;
			*step   = LOOKUP16(g_farStep, 255);
		}
		else {
		  *height = LOOKUP8(g_farHeight, ds);
		  *step   = LOOKUP16(g_farStep, ds);
		}
    }
    else {
        *height = LOOKUP8(g_nearHeight, distance);
        *step   = LOOKUP16(g_nearStep, distance);
    }
}

void CalculateDistance(u16 rayX, u16 rayY, u16 rayA, s16* deltaX, s16* deltaY, u8* textureNo, u8* textureX)
{
    register s8  tileStepX = 0;
    register s8  tileStepY = 0;
    register s16 interceptX = rayX;
    register s16 interceptY = rayY;

    const u8 quarter = rayA >> 8;
    const u8 angle   = rayA & 0xFF;
    const u8 offsetX = rayX & 0xFF;
    const u8 offsetY = rayY & 0xFF;

    u8 tileX = rayX >> 8;
    u8 tileY = rayY >> 8;
    s16 hitX;
    s16 hitY;

    if(angle == 0)
    {
        switch(quarter & 1)
        {
        case 0:
            tileStepX = 0;
            tileStepY = quarter == 0 ? 1 : -1;
            if(tileStepY == 1)
            {
                interceptY -= 256;
            }
            for(;;)
            {
                tileY += tileStepY;
                if(IsWall(tileX, tileY))
                {
                    goto HorizontalHit;
                }
            }
            break;
        case 1:
            tileStepY = 0;
            tileStepX = quarter == 1 ? 1 : -1;
            if(tileStepX == 1)
            {
                interceptX -= 256;
            }
            for(;;)
            {
                tileX += tileStepX;
                if(IsWall(tileX, tileY))
                {
                    goto VerticalHit;
                }
            }
            break;
        }
    }
    else
    {
        s16 stepX = 0;
        s16 stepY = 0;

        switch(quarter)
        {
        case 0:
        case 1:
            tileStepX = 1;
            interceptY += MulTan(offsetX, true, quarter, angle, g_cotan);
            interceptX -= 256;
            stepX = AbsTan(quarter, angle, g_tan);
            break;
        case 2:
        case 3:
            tileStepX = -1;
            interceptY -= MulTan(offsetX, false, quarter, angle, g_cotan);
            stepX = -AbsTan(quarter, angle, g_tan);
            break;
        }

        switch(quarter)
        {
        case 0:
        case 3:
            tileStepY = 1;
            interceptX += MulTan(offsetY, true, quarter, angle, g_tan);
            interceptY -= 256;
            stepY = AbsTan(quarter, angle, g_cotan);
            break;
        case 1:
        case 2:
            tileStepY = -1;
            interceptX -= MulTan(offsetY, false, quarter, angle, g_tan);
            stepY = -AbsTan(quarter, angle, g_cotan);
            break;
        }

        for(;;)
        {
            while((tileStepY == 1 && (interceptY >> 8 < tileY)) || (tileStepY == -1 && (interceptY >> 8 >= tileY)))
            {
                tileX += tileStepX;
                if(IsWall(tileX, tileY))
                {
                    goto VerticalHit;
                }
                interceptY += stepY;
            }
            while((tileStepX == 1 && (interceptX >> 8 < tileX)) || (tileStepX == -1 && (interceptX >> 8 >= tileX)))
            {
                tileY += tileStepY;
                if(IsWall(tileX, tileY))
                {
                    goto HorizontalHit;
                }
                interceptX += stepX;
            }
        }
    }

HorizontalHit:
    hitX       = interceptX + (tileStepX == 1 ? 256 : 0);
    hitY       = (tileY << 8) + (tileStepY == -1 ? 256 : 0);
    *textureNo = 0;
    *textureX  = interceptX & 0xFF;
    g_lastWallType = GetWallType(tileX, tileY);
    g_lastWallTileX = tileX;
    g_lastWallTileY = tileY;
    goto WallHit;

VerticalHit:
    hitX       = (tileX << 8) + (tileStepX == -1 ? 256 : 0);
    hitY       = interceptY + (tileStepY == 1 ? 256 : 0);
    *textureNo = 1;
    *textureX  = interceptY & 0xFF;
    g_lastWallType = GetWallType(tileX, tileY);
    g_lastWallTileX = tileX;
    g_lastWallTileY = tileY;
    goto WallHit;

WallHit:
    *deltaX = hitX - rayX;
    *deltaY = hitY - rayY;
}

/* Cast a ray and return the exact wall hit position in world coordinates */
void CastRayHitPos(u16 rayX, u16 rayY, u16 rayA, s16* outHitX, s16* outHitY)
{
    s16 deltaX, deltaY;
    u8 texNo, texX;
    CalculateDistance(rayX, rayY, rayA & 1023, &deltaX, &deltaY, &texNo, &texX);
    *outHitX = (s16)rayX + deltaX;
    *outHitY = (s16)rayY + deltaY;
}

/* Bresenham tile-walk line-of-sight check.
 * Coordinates are 8.8 fixed-point; only intermediate tiles are tested. */
bool hasLineOfSight(u16 fromX, u16 fromY, u16 toX, u16 toY)
{
    s8 x0 = (s8)(fromX >> 8);
    s8 y0 = (s8)(fromY >> 8);
    s8 x1 = (s8)(toX >> 8);
    s8 y1 = (s8)(toY >> 8);
    s8 dx = x1 - x0;
    s8 dy = y1 - y0;
    s8 sx = (dx > 0) ? 1 : -1;
    s8 sy = (dy > 0) ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    {
        s16 err = (s16)dx - (s16)dy;
        while (x0 != x1 || y0 != y1) {
            s16 e2 = err * 2;
            if (e2 > -(s16)dy) { err -= dy; x0 += sx; }
            if (e2 <  (s16)dx) { err += dx; y0 += sy; }
            /* Reached destination -- don't test the enemy's own tile */
            if (x0 == x1 && y0 == y1) break;
            if (IsWall((u8)x0, (u8)y0)) return false;
        }
    }
    return true;
}

// (playerX, playerY) is 8 box coordinate bits, 8 inside coordinate bits
// (playerA) is full circle as 1024
void Trace(u16 screenX, u8* screenY, u8* textureNo, u8* textureX, u16* textureY, u16* textureStep)
{
    u16 rayAngle = (u16)(_playerA + LOOKUP16(g_deltaAngle, screenX));

    // neutralize artefacts around edges
    switch(rayAngle & 0xFF)
    {
    case 1:
    case 254:
        rayAngle--;
        break;
    case 2:
    case 255:
        rayAngle++;
        break;
    }
    rayAngle &= 1023;

    s16 deltaX;
    s16 deltaY;
    CalculateDistance(_playerX, _playerY, rayAngle, &deltaX, &deltaY, textureNo, textureX);

    // distance = deltaY * cos(playerA) + deltaX * sin(playerA)
    s16 distance = 0;
    if(_playerA == 0)
    {
        distance += deltaY;
    }
    else if(_playerA == 512)
    {
        distance -= deltaY;
    }
    else
        switch(_viewQuarter)
        {
        case 0:
            distance += MulS(LOOKUP8(g_cos, _viewAngle), deltaY);
            break;
        case 1:
            distance -= MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), deltaY);
            break;
        case 2:
            distance -= MulS(LOOKUP8(g_cos, _viewAngle), deltaY);
            break;
        case 3:
            distance += MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), deltaY);
            break;
        }

    if(_playerA == 256)
    {
        distance += deltaX;
    }
    else if(_playerA == 768)
    {
        distance -= deltaX;
    }
    else
        switch(_viewQuarter)
        {
        case 0:
            distance += MulS(LOOKUP8(g_sin, _viewAngle), deltaX);
            break;
        case 1:
            distance += MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), deltaX);
            break;
        case 2:
            distance -= MulS(LOOKUP8(g_sin, _viewAngle), deltaX);
            break;
        case 3:
            distance -= MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), deltaX);
            break;
        }
    if(distance >= MIN_DIST)
    {
        *textureY = 0;
        LookupHeight((distance - MIN_DIST) >> 2, screenY, textureStep);
    }
    else
    {
        *screenY     = SCREEN_HEIGHT >> 1;
        *textureY    = LOOKUP16(g_overflowOffset, distance);
        *textureStep = LOOKUP16(g_overflowStep, distance);
    }
}

void Start(u16 playerX, u16 playerY, s16 playerA)
{
    _viewQuarter = playerA >> 8;
    _viewAngle   = playerA & 0xFF;
    _playerX     = playerX;
    _playerY     = playerY;
    _playerA     = playerA;
}

// Function to calculate the screen X, Y position and scale of an object
void CalculateObjectScreenPosition(u16 objectX, u16 objectY, u16 objectWidth, u16 objectHeight, u8* screenX, u8* screenY, u8* scale, bool* shouldDraw) {
    // Calculate the differences in the x and y coordinates
    s16 deltaX = objectX - _playerX;
    s16 deltaY = objectY - _playerY;

    // Determine the player's view angle
	u8 quarter = _viewAngle >> 8;
	u8 angle = _viewAngle & 0xFF;

	// Calculate cosine and sine values based on the view angle's quarter
	s16 cosValue, sinValue;
	switch (quarter) {
		case 0:
			cosValue = LOOKUP8(g_cos, angle);
			sinValue = LOOKUP8(g_sin, angle);
			break;
		case 1:
			cosValue = -LOOKUP8(g_cos, INVERT(angle));
			sinValue = LOOKUP8(g_sin, INVERT(angle));
			break;
		case 2:
			cosValue = -LOOKUP8(g_cos, angle);
			sinValue = -LOOKUP8(g_sin, angle);
			break;
		case 3:
			cosValue = LOOKUP8(g_cos, INVERT(angle));
			sinValue = -LOOKUP8(g_sin, INVERT(angle));
			break;
	}

    // Calculate the distance to the object using the adjusted cosine and sine values
	s16 distance = MulS(deltaX, cosValue) + MulS(deltaY, sinValue);

	// Early exit if the object is behind the player or too close
	if (distance <= 0) {
		*screenX = 0;
		*screenY = 0;
		*scale = 0;
		*shouldDraw = false;
		return;
	}

	// Calculate the screen X position (using player angle to adjust object position)
	s16 objectScreenX = MulS(deltaX, sinValue) - MulS(deltaY, cosValue);
	*screenX = Wby2 + (Wby2 * objectScreenX / distance);

	// Calculate the scale of the object (inverse of the distance)
	*scale = (u8)(W / distance);

	// Calculate the screen Y position (center of the screen minus half the scaled object height)
	s16 objectScreenY = Hby2 - (objectHeight * *scale / 2);

	// Set the screen Y position
	*screenY = objectScreenY;

	// Adjust screenX and screenY to account for object dimensions
	s16 halfWidth = (objectWidth * *scale) / 2;
	s16 halfHeight = (objectHeight * *scale) / 2;

	*screenX -= halfWidth;

	// Check if the object is within the screen bounds, considering its size
	if (*screenX + objectWidth * *scale < 0 || *screenX >= W || *screenY + objectHeight * *scale < 0 || *screenY >= H) {
		*shouldDraw = false;
	} else {
		*shouldDraw = true;
	}
}

void getObjectScreenPosAndScale(u16 *objX, u16 *objY, bool *isVisible, u16 *resX, u16 *resY, s32 *scale) {
	s16 vx1 = _playerX - *objX;
	s16 vy1 = *objY - _playerY;

	s16 distance = 0;
	s16 tx1 = 0;
	s16 tz1 = 0;
	// rotate around player view

	switch(_viewQuarter)
	{
		case 0:
			//distance += MulS(LOOKUP8(g_cos, _viewAngle), vy1);
			//distance += MulS(LOOKUP8(g_sin, _viewAngle), vx1);
			tx1 = MulS(LOOKUP8(g_sin, _viewAngle), vx1) + MulS(LOOKUP8(g_cos, _viewAngle), vy1);
			tz1 = MulS(LOOKUP8(g_cos, _viewAngle), vx1) + MulS(LOOKUP8(g_sin, _viewAngle), vy1);
			break;
		case 1:
			//distance -= MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), vy1);
			//distance += MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), vx1);
			tx1 = MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), vx1) - MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), vy1);
			tz1 = MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), vx1) + MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), vy1);
			break;
		case 2:
			//distance -= MulS(LOOKUP8(g_cos, _viewAngle), vy1);
			//distance -= MulS(LOOKUP8(g_sin, _viewAngle), vx1);
			tx1 = MulS(LOOKUP8(g_sin, _viewAngle), vx1) - MulS(LOOKUP8(g_cos, _viewAngle), vy1);
			tz1 = MulS(LOOKUP8(g_cos, _viewAngle), vx1) - MulS(LOOKUP8(g_sin, _viewAngle), vy1);
			break;
		case 3:
			//distance += MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), vy1);
			//distance -= MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), vx1);
			tx1 = MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), vx1) + MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), vy1);
			tz1 = MulS(LOOKUP8(g_cos, INVERT(_viewAngle)), vx1) - MulS(LOOKUP8(g_sin, INVERT(_viewAngle)), vy1);
			break;
	}
	//tx1 = MulS(LOOKUP8(g_sin, _viewAngle), vx1) - MulS(LOOKUP8(g_cos, _viewAngle), vy1);
	//tz1 = MulS(LOOKUP8(g_cos, _viewAngle), vx1) + MulS(LOOKUP8(g_sin, _viewAngle), vy1);

	//s16 tx1 = vx1 * playerAngleSin - vy1 * playerAngleCos;
	//s16 tz1 = vx1 * playerAngleCos + vy1 * playerAngleSin;

	// only display if enemy is in Field-Of-View of player.
	if (tz1 <= 0 && tx1 <= 0) {
		*isVisible = false;
		return;
	}

	//interceptX -= MulTan(offsetY, false, quarter, angle, g_tan);

	s16 yscale1 = 208 / tz1; // float yscale1 = vfov / tz1;

	int x1 = Wby2 - (int)(tx1 * 2);
	int y1b = Hby2 - (int)(tz1 * 1);
	//int x1 = Wby2 - (int)(tx1 * xscale1);
	//int y1b = Hby2 - (int)(Yaw(yfloor, tz1) * yscale1);

	*resX = x1;
	*resY = y1b;
	*scale = yscale1;


}


void TraceEnemy(u16 *enemyX, u16 *enemyY, u8* screenY, u8* textureNo, u8* textureX, u16* textureY, u16* textureStep, u16 *enemyScreenX)
{
    // Calculate angle between player and enemy
    u8 playerAngle = _playerA & 0xFF;
    //u16 angle = AbsTan(_viewQuarter, _viewAngle, g_tan) - AbsTan(_viewQuarter, atan2(enemyY - _playerY, enemyX - _playerX) * 1024 / (2 * M_PI), g_tan);
    u16 enemyAngle = AbsTan(_viewQuarter, playerAngle, g_tan) - AbsTan(_viewQuarter, AbsTan(_viewQuarter, playerAngle, g_tan), g_tan);
	//uint16_t playerAbsTan = AbsTan(playerQuarter, playerAngle, g_tan);
    //uint16_t enemyAbsTan = AbsTan(playerQuarter, AbsTan(playerQuarter, static_cast<uint8_t>((atan2(enemyY - _playerY, enemyX - _playerX) * 1024) / (2 * M_PI)), g_tan), g_tan);

    // Calculate the angle between the player and the enemy
    //uint16_t enemyAngle = playerAbsTan - enemyAbsTan;

    // Ensure the enemy angle is within [0, 1024)
    enemyAngle &= 1023;

    // Calculate distance between player and enemy
    u16 deltaX = *enemyX - _playerX;
	u16 deltaY = *enemyY - _playerY;
	u16 distanceSquared = MulS(deltaX, deltaX) + MulS(deltaY, deltaY);
	u16 distance = g_sqrtLookupTable[distanceSquared];
	s16 enemyDisplacementX = MulS(distance, LOOKUP8(g_cos, enemyAngle));

    // Neutralize angle artifacts around edges
    switch(enemyAngle & 0xFF)
    {
    case 1:
    case 254:
        enemyAngle--;
        break;
    case 2:
    case 255:
        enemyAngle++;
        break;
    }
    enemyAngle &= 1023;

    s16 rayDeltaX;
    s16 rayDeltaY;

    // Calculate ray's delta based on angle and distance
    CalculateDistance(_playerX, _playerY, enemyAngle, &rayDeltaX, &rayDeltaY, textureNo, textureX);

    // Calculate distance from player to enemy
    s16 distToEnemy = (distance - MIN_DIST) >> 2;

    // If distance is greater than the minimum distance, determine screen placement
    if(distance >= MIN_DIST)
    {
        *textureY = 0;
        LookupHeight(distToEnemy, screenY, textureStep);
    }
    else
    {
        // Handle overflow condition
        *screenY     = SCREEN_HEIGHT >> 1;
        *textureY    = LOOKUP16(g_overflowOffset, distToEnemy);
        *textureStep = LOOKUP16(g_overflowStep, distToEnemy);
    }
    //*enemyScreenX = MulS(distance, LOOKUP8(g_cos, enemyAngle));
    switch(_viewQuarter)
	{
	case 0:
		*enemyScreenX = MulS(distance, LOOKUP8(g_cos, enemyAngle));
		break;
	case 1:
		*enemyScreenX = MulS(distance, LOOKUP8(g_cos, INVERT(enemyAngle)));
		break;
	case 2:
		*enemyScreenX = -MulS(distance, LOOKUP8(g_cos, INVERT(enemyAngle)));
		break;
	case 3:
		*enemyScreenX = -MulS(distance, LOOKUP8(g_cos, INVERT(enemyAngle)));
		break;
	}
	*enemyScreenX = SCREEN_WIDTH >> 1 + (*enemyScreenX);

}


// Calculate the screen position and scale of the enemy relative to the player's viewpoint
void TopDownEnemyPosition(u16 enemyX, u16 enemyY, s16* screenX, u16* screenY, f16* scale, s8* scaleInt, bool *withinView) {
    // Calculate relative position of the enemy from the player
    s16 relX = enemyX - _playerX;
    s16 relY = _playerY - enemyY;

    uint16_t  tyDiff = abs(relX);
	uint16_t  txDiff = abs(relY);
	 uint16_t  total  = tyDiff + txDiff;

    // Calculate the angle between the player's view direction and the enemy
    u16 angle = _viewAngle & 0xFF;

    if (_viewQuarter == 0) {
		angle = angle; //INVERT(angle); ööä
    } else if (_viewQuarter == 1) {
        angle = INVERT(angle);
    } else if (_viewQuarter == 2) {
        angle = _viewAngle;
    } else if (_viewQuarter == 3) {
        angle = INVERT(angle);
    }

    u8 cosA = LOOKUP8(g_cos, angle);
    u8 sinA = LOOKUP8(g_sin, angle);
    s16 rotatedX = 0;
    s16 rotatedY = 0;

    s16 relXSinA = MulS(sinA, relX);
    s16 relXCosA = MulS(cosA, relX);

    s16 relYSinA = MulS(sinA, relY);
	s16 relYCosA = MulS(cosA, relY);

    //tx1 = MulS(LOOKUP8(g_sin, _viewAngle), vx1) + MulS(LOOKUP8(g_cos, _viewAngle), vy1);
    //tz1 = MulS(LOOKUP8(g_cos, _viewAngle), vx1) + MulS(LOOKUP8(g_sin, _viewAngle), vy1);
	if (angle == 0) {
		relYSinA = relY;
    } else if (angle == 512) {
		relYSinA = -relY;
	}
	if (angle == 256) {
		relXCosA = relX;
	} else if (angle == 768) {
		relXCosA = -relX;
	}
    switch (_viewQuarter) {
        case 0:  // Quadrant 1 (standard trigonometric order)
          /*rotatedX = MulS(cosA, relX) - MulS(sinA, relY);*/
          /*rotatedY = MulS(sinA, relX) + MulS(cosA, relY);*/
          rotatedX = relXSinA + relYCosA;
		  rotatedY = relXCosA + relYSinA;
          break;
        case 1:  // Quadrant 2 (adjust Y for flipped view)
          rotatedX = relXCosA + relYSinA;
          rotatedY = relXSinA + relYCosA;
          break;
        case 2:  // Quadrant 3 (adjust X and Y for flipped view and reversed order)
          rotatedX = -relXCosA - relYSinA;
          rotatedY = -relXSinA + relYCosA;
          break;
        case 3:  // Quadrant 4 (adjust X for flipped view)
          rotatedX = relXCosA - relYSinA;
          rotatedY = relXSinA + relYCosA;
          break;
      }

      /*tx1 = vx1 * psin - vy1 * pcos;
	  tz1 = vx1 * pcos + vy1 * psin;
	  if (tz1 <= 0) continue;*/

    // Rotate the relative position of the enemy based on the player's view angle
    /*
     s16 rotatedX = -(MulS(relX, LOOKUP8(g_sin, angle)) - MulS(relY, LOOKUP8( g_cos, angle)));
	 s16 rotatedY = MulS(relX, LOOKUP8(g_sin, angle)) + MulS(relY, LOOKUP8(g_cos, angle));
    if (_viewQuarter == 0) { // up right
		rotatedX = (MulS(relX, LOOKUP8(g_sin, INVERT(angle))) - MulS(relY, LOOKUP8(g_cos, angle)));
		rotatedY = MulS(relX, LOOKUP8(g_sin, angle)) + MulS(relY, LOOKUP8(g_cos, angle));
		//rotatedX *= 4;
    } else if (_viewQuarter == 1) { // down right
		rotatedX = MulS(relX, LOOKUP8(g_cos, angle)) + MulS(relY, LOOKUP8(g_sin, angle));
		rotatedY = MulS(relX, LOOKUP8(g_sin, angle)) + MulS(relY, LOOKUP8(g_cos, angle));
    } else if (_viewQuarter == 2) {

	} else if (_viewQuarter == 3) {
		rotatedX = -MulS(relX, LOOKUP8(g_cos, angle)) - MulS(relY, LOOKUP8(g_sin, angle));
		rotatedY = MulS(relX, LOOKUP8(g_sin, angle)) + MulS(relY, LOOKUP8(g_cos, angle));
		//rotatedX *= 4;
	}*/

	//*withinView = (rotatedX >= 0  && rotatedX < SCREEN_WIDTH);

	u8 sqrtRes = ((rotatedX >> 8) * (rotatedX >> 8) + (rotatedY >> 8) * (rotatedY >> 8)) & 15; // max 256
	// 0 to 16 indexes
	u16 distance = (g_sqrtLookupTable[sqrtRes]);
	*scaleInt = sqrtRes-4;
	//*scale = g_fixedscales[distance];

	/* inverse_fixed(vfov/rotatedY) = 512*rotatedY/vfov (all integer) */
	*scale = (f16)(((s32)rotatedY << 9) / VFOV_INT);
	*screenX = Wby2 + (s16)((s32)rotatedX * (s32)hfovv / (s32)rotatedY);

    // Scale the position of the enemy to fit within the screen
    //*screenX = Wby2 + rotatedX; // Assuming Wby2 is half the screen width
    //*screenY = Hby2 - (rotatedY%100); // Assuming Hby2 is half the screen height

    // Translate object position to screen space (adjust for SCREEN_WIDTH)
      //int maxAbsRotatedX = abs(MulS(rotatedX, _viewQuarter));
      //*screenX = Hby2 + (_viewQuarter + (maxAbsRotatedX * _viewAngle) / SCREEN_WIDTH) * ((rotatedX >= 0) ? 1 : -1);
      //int maxAbsRotatedY = abs(MulS(rotatedY, _viewQuarter));
      //*screenY = (_viewQuarter / 2 + (maxAbsRotatedY * _viewAngle) / SCREEN_WIDTH) * ((rotatedY >= 0) ? 1 : -1);

    // Calculate the scale of the enemy based on its distance from the player

    //*scale = LOOKUP8(g_enemyScaleLookupTable, distance); // You may need to define this lookup table

    // Calculate the angle between the player's view direction and the direction towards the enemy
	u16 enemyAngle = AbsTan(_viewQuarter, angle, g_tan);
	u16 angleDiff = abs(enemyAngle - 512); // 512 corresponds to the direction opposite to the player's view

	// Check if the enemy is within the player's view
	*withinView = angleDiff <= (_viewAngle >>1);
/*
	if(total < 1000)
	{
		*scale = g_fixedscales[15];
	}
	else if(total >= 1001 && total <= 2000)
	{
		*scale = g_fixedscales[14];
	}
	else if (total >= 2001 && total <= 4000)
	{
		*scale = g_fixedscales[14];
	}
	else if(total >= 4001 && total <= 5000)
	{
		*scale = g_fixedscales[13];
	}
	else if(total >= 5001 && total <= 6000)
	{
		*scale = g_fixedscales[12];
	}
	else if(total >= 6001 && total <= 7000)
	{
		*scale = g_fixedscales[11];
	}
	else if(total >= 8001 && total <= 9000)
	{
		*scale = g_fixedscales[10];
	}
	else if(total >= 4001 && total <= 5000)
	{
		*scale = g_fixedscales[9];
	}
	else if(total >= 5001 && total <= 6000)
	{
		*scale = g_fixedscales[8];
	}
	else if(total >= 6001 && total <= 7000)
	{
		*scale = g_fixedscales[7];
	}
	else if(total >= 7001 && total <= 8000)
	{
		*scale = g_fixedscales[6];
	}
	else if(total >= 8001 && total <= 9000)
	{
		*scale = g_fixedscales[5];
	}
	else if(total >= 9001 && total <= 10000)
	{
		*scale = g_fixedscales[4];
	}
	else if(total >= 10001 && total <= 11000)
	{
		*scale = g_fixedscales[3];
	}
	else if(total >= 11001 && total <= 12000)
	{
		*scale = g_fixedscales[2];
	}
	else if(total >= 12001 && total <= 13000)
	{
		*scale = g_fixedscales[1];
	}
	else
	{
		*scale = g_fixedscales[0];// minsta
	}*/
}

void fPlayerJaw(s32 iValue) {
}
void fPlayerMoveForward(u16 *ifPlayerX, u16 *ifPlayerY, s16 ifPlayerAng, s16 iSpeed) {

	// 0 values won't allow movement, so increase atleast 1 for these...
	if (ifPlayerAng == 0 || ifPlayerAng == 256 || ifPlayerAng == 384 || ifPlayerAng == 512 || ifPlayerAng == 640 || ifPlayerAng == 768 || ifPlayerAng == 896) {
		ifPlayerAng++;
	}

	u8 vq = (ifPlayerAng & 1023) >> 8;
	u8 angle   = ifPlayerAng & 0xFF;

	if (vq == 0) { // up + right
		angle = ifPlayerAng & 0xFF;
	} else if (vq == 1) { // down + right
		angle = INVERT(ifPlayerAng & 0xFF);
	} else if (vq == 2) { // down + left
		angle = INVERT(ifPlayerAng & 0xFF);
	} else if (vq == 3) { // up + left
		angle = ifPlayerAng & 0xFF;
	}
	s16 valueChangeX = MulS(LOOKUP8(g_cos, angle), iSpeed);
	s16 valueChangeY = MulS(LOOKUP8(g_sin, angle), iSpeed);

	s16 nextX = 0;
	s16 nextY = 0;

	if (vq == 0) { // up + right
		nextX = *ifPlayerX + valueChangeY;
		nextY = *ifPlayerY + valueChangeX;
	} else if (vq == 1) { // down + right
		nextX = *ifPlayerX + valueChangeY;
		nextY = *ifPlayerY - valueChangeX;
	} else if (vq == 2) { // down + left
		nextX = *ifPlayerX - valueChangeX;
		nextY = *ifPlayerY - valueChangeY;
	} else if (vq == 3) { // up + left
		nextX = *ifPlayerX - valueChangeX;
		nextY = *ifPlayerY + valueChangeY;
	}
	if (nextX < 0)
		nextX = 0;
	if (nextY < 0)
		nextY = 0;

	u8 prevTileX = *ifPlayerX >> 8;
	u8 prevTileY = *ifPlayerY >> 8;
	u8 tileX = nextX >> 8;
	u8 tileY = nextY >> 8;
	// make sure the player stays within the level
	if(tileX > MAP_X - 1)
		nextX = ((MAP_X)<<8)-1;
	if (tileY > MAP_Y - 1)
		nextY = ((MAP_Y)<<8)-1;

	tileX = nextX >> 8;
	tileY = nextY >> 8;


	{
		bool wallBlocked = IsWall(tileX, tileY) || IsWall(prevTileX, tileY) || IsWall(tileX, prevTileY);
		bool entityBlocked = collidesWithAnyEnemy((u16)nextX, (u16)nextY, PLAYER_RADIUS, 255);

		if (wallBlocked || entityBlocked) {
			/* Try sliding along X axis only */
			bool xOk = !IsWall(tileX, prevTileY) &&
			           !collidesWithAnyEnemy((u16)nextX, *ifPlayerY, PLAYER_RADIUS, 255);
			bool yOk = !IsWall(prevTileX, tileY) &&
			           !collidesWithAnyEnemy(*ifPlayerX, (u16)nextY, PLAYER_RADIUS, 255);
			if (xOk) {
				*ifPlayerX = nextX;
			} else if (yOk) {
				*ifPlayerY = nextY;
			}
			/* else: fully blocked, deny move */
		} else {
			/* allow move */
			*ifPlayerX = nextX;
			*ifPlayerY = nextY;
		}
	}
}

void fPlayerStrafe(u16 *ifPlayerX, u16 *ifPlayerY, s16 ifPlayerAng, s16 iSpeed) {
	s16 strafeAngle = ifPlayerAng+256;
	if (iSpeed < 0)
		strafeAngle = ifPlayerAng+310; // dunno what this value should be...
	fPlayerMoveForward(ifPlayerX, ifPlayerY, strafeAngle, iSpeed);
}