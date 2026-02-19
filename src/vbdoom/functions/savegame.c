/*
 * Save/load game to battery-backed SRAM.
 * Packed binary format; no malloc.
 */
#include <libgccvb.h>
#include <mem.h>
#include "savegame.h"

#define SAVE_BUF_SIZE  512
#define SAVE_WORDS    (SAVE_BUF_SIZE / 2)

/* Pack u16 as two bytes (little-endian) to avoid unaligned word access on V810 */
static void put16(u8 **p, u16 v) {
	u8 *q = *p;
	q[0] = (u8)(v & 0xFF);
	q[1] = (u8)(v >> 8);
	*p = q + 2;
}

static void packSaveState(const SaveState *s, u8 *buf) {
	u16 i;
	u8 *p = buf;

	put16(&p, (u16)SAVE_MAGIC);
	*p++ = (u8)SAVE_VERSION;
	*p++ = s->level;

	put16(&p, s->playerX);
	put16(&p, s->playerY);
	put16(&p, (u16)s->playerAngle);
	*p++ = s->health;
	put16(&p, s->armour);
	*p++ = s->armorType;
	*p++ = s->hasKeyRed;
	*p++ = s->hasKeyYellow;
	*p++ = s->hasKeyBlue;
	*p++ = s->currentWeapon;

	for (i = 0; i < 6u; i++) {
		*p++ = s->weapons[i].hasWeapon;
		*p++ = s->weapons[i].ammo;
	}

	for (i = 0; i < SAVE_MAX_ENEMIES; i++) {
		put16(&p, s->enemies[i].x);
		put16(&p, s->enemies[i].y);
		put16(&p, (u16)s->enemies[i].angle);
		*p++ = s->enemies[i].state;
		*p++ = s->enemies[i].health;
		*p++ = s->enemies[i].movedir;
		*p++ = s->enemies[i].enemyType;
		*p++ = (u8)s->enemies[i].active;
	}

	for (i = 0; i < SAVE_MAX_PICKUPS; i++) {
		put16(&p, s->pickups[i].x);
		put16(&p, s->pickups[i].y);
		*p++ = s->pickups[i].type;
		*p++ = s->pickups[i].animFrame;
		*p++ = s->pickups[i].animTimer;
		*p++ = s->pickups[i].active;
		*p++ = s->pickups[i].respawnTimer;
	}
}

void saveGameToSRAM(const SaveState *s) {
	static u8 buf[SAVE_BUF_SIZE];
	u16 i;

	setmem(buf, 0, SAVE_BUF_SIZE);
	packSaveState(s, buf);

	for (i = 0; i < SAVE_WORDS; i++) {
		u16 w = (u16)buf[i * 2] | ((u16)buf[i * 2 + 1] << 8);
		SAVERAM[i] = w;
	}
}

static u16 get16(const u8 **p) {
	const u8 *q = *p;
	u16 v = (u16)q[0] | ((u16)q[1] << 8);
	*p = q + 2;
	return v;
}

static void unpackSaveState(const u8 *buf, SaveState *s) {
	u16 i;
	const u8 *p = buf;

	p += 2;  /* skip magic */
	p++;     /* skip version */
	s->level = *p++;

	s->playerX = get16(&p);
	s->playerY = get16(&p);
	s->playerAngle = (s16)get16(&p);
	s->health = *p++;
	s->armour = get16(&p);
	s->armorType = *p++;
	s->hasKeyRed = *p++;
	s->hasKeyYellow = *p++;
	s->hasKeyBlue = *p++;
	s->currentWeapon = *p++;

	for (i = 0; i < 6u; i++) {
		s->weapons[i].hasWeapon = *p++;
		s->weapons[i].ammo = *p++;
	}

	for (i = 0; i < SAVE_MAX_ENEMIES; i++) {
		s->enemies[i].x = get16(&p);
		s->enemies[i].y = get16(&p);
		s->enemies[i].angle = (s16)get16(&p);
		s->enemies[i].state = *p++;
		s->enemies[i].health = *p++;
		s->enemies[i].movedir = *p++;
		s->enemies[i].enemyType = *p++;
		s->enemies[i].active = (bool)*p++;
	}

	for (i = 0; i < SAVE_MAX_PICKUPS; i++) {
		s->pickups[i].x = get16(&p);
		s->pickups[i].y = get16(&p);
		s->pickups[i].type = *p++;
		s->pickups[i].animFrame = *p++;
		s->pickups[i].animTimer = *p++;
		s->pickups[i].active = *p++;
		s->pickups[i].respawnTimer = *p++;
	}
}

bool loadGameFromSRAM(SaveState *s) {
	static u8 buf[SAVE_BUF_SIZE];
	u16 i;
	u16 magic;

	for (i = 0; i < SAVE_WORDS; i++) {
		u16 w = SAVERAM[i];
		buf[i * 2] = (u8)(w & 0xFF);
		buf[i * 2 + 1] = (u8)(w >> 8);
	}
	magic = (u16)buf[0] | ((u16)buf[1] << 8);
	if (magic != SAVE_MAGIC || buf[2] != SAVE_VERSION)
		return false;
	unpackSaveState(buf, s);
	return true;
}

static SaveState s_loadedSave;
static bool s_hasLoadedSave;

bool tryLoadGameFromSRAM(void) {
	if (loadGameFromSRAM(&s_loadedSave)) {
		s_hasLoadedSave = true;
		return true;
	}
	return false;
}

const SaveState *getLoadedSave(void) {
	return s_hasLoadedSave ? &s_loadedSave : 0;
}

void clearLoadedSave(void) {
	s_hasLoadedSave = false;
}
