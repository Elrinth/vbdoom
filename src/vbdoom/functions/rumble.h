#ifndef _TE_RUMBLE_H_
#define _TE_RUMBLE_H_

#include "rumbleCommands.h"

void rumble_play();
void rumble_stop();
void rumble_playEffect(unsigned char effect);
void rumble_setFrequency(unsigned char frequency);
void rumble_playEffectChain(unsigned char effectChain);
void rumble_storeEffectChain(unsigned char chainNumber, unsigned char chainLength, unsigned char* effectChain);
void rumble_storeEffectChainLoops(unsigned char chainNumber, unsigned char chainLength, unsigned char* effectChainLoops);


void rumble_sendCommandWithValue(unsigned char command, unsigned char value);
void rumble_sendByte(unsigned char byteValue);


#endif