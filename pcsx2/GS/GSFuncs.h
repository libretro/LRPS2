#pragma once
#include "Pcsx2Types.h"

#include "Renderers/Common/GSRenderer.h"

extern GSRenderer* s_gs;

int GSinit(void);
int GSopen2(u32 flags, u8 *basemem);
void GSclose(void);
void GSshutdown(void);

#define GSvsync(field) s_gs->VSync(field)
#define GSgifTransfer(mem, size) s_gs->Transfer<3>(mem, size)
#define GSgifTransfer1(mem, addr) s_gs->Transfer<0>(const_cast<u8*>(mem) + addr, (0x4000 - addr) / 16)
#define GSgifTransfer2(mem, size) s_gs->Transfer<1>(const_cast<u8*>(mem), size)
#define GSgifTransfer3(mem, size) s_gs->Transfer<2>(const_cast<u8*>(mem), size)
#define GSgifSoftReset(mask) s_gs->SoftReset(mask)
#define GSInitAndReadFIFO(mem, size) s_gs->InitAndReadFIFO(mem, size)
#define GSsetGameCRC(crc, options) s_gs->SetGameCRC(crc, options)

#define GSreset() s_gs->Reset()
#define GSUpdateOptions() s_gs->UpdateRendererOptions()

int GSfreeze(int mode, void *_data);
