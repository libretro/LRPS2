#pragma once
#include "Pcsx2Types.h"

int GSinit(void);
int GSopen2(u32 flags, u8 *basemem);
void GSclose(void);
void GSshutdown(void);

void GSvsync(int field);
void GSgifTransfer(const u8* mem, u32 size);
void GSgifTransfer1(u8* mem, u32 addr);
void GSgifTransfer2(u8* mem, u32 size);
void GSgifTransfer3(u8* mem, u32 size);
void GSgifSoftReset(u32 mask);
void GSInitAndReadFIFO(u8* mem, u32 size);

void GSsetGameCRC(u32 crc, int options);

void GSreset(void);
int GSfreeze(int mode, void *_data);
