#ifndef __SNDPLAY_
#define __SNDPLAY_
#include <stdbool.h>
#include <types.h>

/* Initialize sound system: load music waveform, configure PCM channels */
void mp_init(void);

/* ---- Background Music ---- */

/* Song IDs for musicLoadSong() */
#define SONG_TITLE  0
#define SONG_E1M1   1
#define SONG_E1M2   2
#define SONG_E1M3   3
#define SONG_E1M4   4
#define SONG_E1M5   5
#define SONG_E1M6   6

/* Master music volume (0-4).  Derived from settings.music (0-9) via
 * musicVolFromSetting().  Set to 0 to disable music entirely.
 * Reduced range balances waveform synthesis against PCM SFX. */
extern volatile u8 g_musicVolume;

/* Raw settings.music value (0-9), kept in sync with g_musicVolume.
 * Used by gameLoop to reconstruct pause-menu settings. */
extern u8 g_musicSetting;

/* Raw settings.rumble value (0-9).  0 = off, >0 = rumble pak enabled.
 * Used by gameLoop for damage/secret rumble and pause-menu reconstruction. */
extern u8 g_rumbleSetting;

/* Convert settings.music (0-9) to g_musicVolume.
 * Uses a reduced non-linear curve (max 4) to balance waveform
 * synthesis channels against the quieter PCM SFX modulation. */
u8 musicVolFromSetting(u8 setting);

/* Load a song by ID. Does NOT start playback. */
void musicLoadSong(u8 songId);

/* Start (or restart) the currently loaded song. */
void musicStart(void);

/* Stop playback and silence the music channel. */
void musicStop(void);

/* Returns 1 if the music sequencer is currently playing. */
u8 isMusicPlaying(void);

/* Called each frame to advance background music sequencer.
 * Pass isPlayMusic = true to enable music playback. */
void updateMusic(bool isPlayMusic);

/* ---- PCM Sound Effects ---- */

/* Play a PCM sound effect on the player channel (ch4 SWEEP).
 * soundId: one of SFX_PUNCH, SFX_PISTOL, SFX_SHOTGUN, SFX_SHOTGUN_COCK, SFX_ITEM_UP
 * New sounds interrupt the current one.
 * Ch4 is dedicated to PCM -- no music is affected. */
void playPlayerSFX(u8 soundId);

/* Play a PCM sound effect on the enemy channel (ch2 WAVE3).
 * soundId: one of SFX_POSSESSED_SIGHT1..3, SFX_POSSESSED_DEATH1..3, etc.
 * distance: raw distance byte (0-255, 0=closest). Volume is attenuated.
 * Ch2 is dedicated to PCM -- no music is affected. */
void playEnemySFX(u8 soundId, u8 distance);

#endif
