#include "rumble.h"

#define	RUMBLE_CCR	0x00
#define	RUMBLE_CCSR	0x04
#define	RUMBLE_CDTR	0x08

volatile unsigned char* RUMBLE_REGS = (unsigned char*)0x02000000;

void rumble_sendByte(unsigned char byteValue) {
    while (RUMBLE_REGS[RUMBLE_CCR] & 0x02);
    RUMBLE_REGS[RUMBLE_CDTR] = byteValue;
    RUMBLE_REGS[RUMBLE_CCSR]  &= (~0x02);
    RUMBLE_REGS[RUMBLE_CCR] = 0x84;
}

void rumble_sendCommandWithValue(unsigned char command, unsigned char value) {
    rumble_sendByte(command);
    rumble_sendByte(value);
}

void rumble_sendEffect(unsigned char effect) {
    if(effect >= RUMBLE_CMD_MIN_EFFECT && effect <= RUMBLE_CMD_MAX_EFFECT) {
        rumble_sendByte(effect);
    }
}

void rumble_playEffect(unsigned char effect) {
    rumble_sendEffect(effect);
}

void rumble_storeEffectChain(unsigned char chainNumber, unsigned char chainLength, unsigned char* effectChain) {
    unsigned char i = 0;
    rumble_sendByte(RUMBLE_CMD_WRITE_EFFECT_CHAIN);
    rumble_sendByte(chainNumber);
    for(i=0; i < chainLength && i < RUMBLE_MAX_EFFECTS_IN_CHAIN; i++) {
        rumble_sendEffect(effectChain[i]);
    }
    rumble_sendByte(RUMBLE_EFFECT_CHAIN_END);
}

void rumble_storeEffectChainLoops(unsigned char chainNumber, unsigned char chainLength, unsigned char* effectChainLoops) {
    unsigned char i = 0;
    rumble_sendByte(RUMBLE_CMD_WRITE_EFFECT_LOOPS_CHAIN);
    rumble_sendByte(chainNumber);
    for(i=0; i < chainLength && i < RUMBLE_MAX_EFFECTS_IN_CHAIN; i++) {
        rumble_sendByte(effectChainLoops[i]);
    }
    rumble_sendByte(RUMBLE_EFFECT_CHAIN_END);
}

void rumble_playEffectChain(unsigned char effectChain) {
    unsigned char command = effectChain;
    if(command >= RUMBLE_CHAIN_EFFECT_0 && command <= RUMBLE_CHAIN_EFFECT_4) {
        command +=  RUMBLE_CMD_CHAIN_EFFECT_0;
    }
    if(command >= RUMBLE_CMD_CHAIN_EFFECT_0 && command <= RUMBLE_CMD_CHAIN_EFFECT_4) {
        rumble_sendByte(command);
    }
}

void rumble_setFrequency(unsigned char frequency) {
    unsigned char command = frequency;
    if(command >= RUMBLE_FREQ_160HZ && command <= RUMBLE_FREQ_400HZ) {
        command +=  RUMBLE_CMD_FREQ_160HZ;
    }
    if(command >= RUMBLE_CMD_FREQ_160HZ && command <= RUMBLE_CMD_FREQ_400HZ) {
        rumble_sendByte(command);
    }
}

void rumble_play() {
    rumble_sendByte(RUMBLE_CMD_PLAY);
}

void rumble_stop() {
    rumble_sendByte(RUMBLE_CMD_STOP);
}