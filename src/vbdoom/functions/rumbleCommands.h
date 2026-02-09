#ifndef _RUMBLE_CMD_H_
#define _RUMBLE_CMD_H_

#define RUMBLE_MAX_EFFECTS_IN_CHAIN     8

#define RUMBLE_CHAIN_EFFECT_0           0x00
#define RUMBLE_CHAIN_EFFECT_1           0x01
#define RUMBLE_CHAIN_EFFECT_2           0x02
#define RUMBLE_CHAIN_EFFECT_3           0x03
#define RUMBLE_CHAIN_EFFECT_4           0x04

#define RUMBLE_FREQ_160HZ               0x00
#define RUMBLE_FREQ_240HZ               0x01
#define RUMBLE_FREQ_320HZ               0x02
#define RUMBLE_FREQ_400HZ               0x03

#define RUMBLE_CMD_STOP                 0x00
#define RUMBLE_CMD_MIN_EFFECT           0x01
#define RUMBLE_CMD_MAX_EFFECT           0x7B
#define RUMBLE_CMD_PLAY                 0x7C
#define RUMBLE_CMD_CHAIN_EFFECT_0       0x80
#define RUMBLE_CMD_CHAIN_EFFECT_1       0x81
#define RUMBLE_CMD_CHAIN_EFFECT_2       0x82
#define RUMBLE_CMD_CHAIN_EFFECT_3       0x83
#define RUMBLE_CMD_CHAIN_EFFECT_4       0x84
#define RUMBLE_CMD_FREQ_160HZ           0x90
#define RUMBLE_CMD_FREQ_240HZ           0x91
#define RUMBLE_CMD_FREQ_320HZ           0x92
#define RUMBLE_CMD_FREQ_400HZ           0x93
#define RUMBLE_CMD_WRITE_EFFECT_CHAIN         0xB0
#define RUMBLE_CMD_WRITE_EFFECT_LOOPS_CHAIN   0xB1

#define RUMBLE_EFFECT_CHAIN_END         0xFF

#endif
