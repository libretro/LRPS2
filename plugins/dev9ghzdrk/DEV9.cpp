/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014 David Quintana [gigaherz]
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#ifdef _WIN32
#include <winsock2.h>
#include <Winioctl.h>
#include <windows.h>
#elif defined(__linux__)
#include <sys/types.h>
#include <sys/mman.h>
#include <err.h>
#include <unistd.h>
#endif


#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#define EXTERN 
#include "DEV9.h"
#undef EXTERN 
#include "Config.h"
#include "smap.h"
#include "ata.h"

#ifdef _WIN32
#pragma warning(disable:4244)

HINSTANCE hInst=NULL;
#endif

//#define HDD_48BIT

#if defined(__i386__) && !defined(_WIN32)

static __inline__ unsigned long long GetTickCount(void)
{
	unsigned long long int x;
	__asm__ volatile ("rdtsc" : "=A" (x));
	return x;
}

#elif defined(__x86_64__) && !defined(_WIN32)

static __inline__ unsigned long long GetTickCount(void)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#endif

u8 eeprom[] = {
	//0x6D, 0x76, 0x63, 0x61, 0x31, 0x30, 0x08, 0x01,
	0x76, 0x6D, 0x61, 0x63, 0x30, 0x31, 0x07, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

u32 *iopPC;

const unsigned char version  = PS2E_DEV9_VERSION;
const unsigned char revision = 0;
const unsigned char build    = 4;    // increase that with each version


#ifndef BUILTIN_DEV9_PLUGIN
static const char *libraryName     = "GiGaHeRz's DEV9 Driver"
#endif
#ifdef _DEBUG
	"(debug)"
#endif
;

#ifdef _WIN32
HANDLE hEeprom;
HANDLE mapping;
#else
int hEeprom;
int mapping;
#endif

#ifndef BUILTIN_DEV9_PLUGIN
EXPORT_C_(u32)
PS2EgetLibType() {
	return PS2E_LT_DEV9;
}

EXPORT_C_(const char*)
PS2EgetLibName() {
	return libraryName;
}

EXPORT_C_(u32)
PS2EgetLibVersion2(u32 type) {
	return (version<<16) | (revision<<8) | build;
}
#endif


// Warning: The below log function is SLOW. Better fix it before attempting to use it.
int Log = 0;

void __Log(char *fmt, ...) {
}

EXPORT_C_(s32)
DEV9init()
{

#ifdef DEV9_LOG_ENABLE
	LogInit();
	DEV9_LOG("DEV9init\n");
#endif
	memset(&dev9, 0, sizeof(dev9));
	DEV9_LOG("DEV9init2\n");

	DEV9_LOG("DEV9init3\n");

	FLASHinit();

#ifdef _WIN32
	hEeprom = CreateFile(
	  "eeprom.dat",
	  GENERIC_READ|GENERIC_WRITE,
	  0,
	  NULL,
	  OPEN_EXISTING,
	  FILE_FLAG_WRITE_THROUGH,
	  NULL
	);

	if(hEeprom==INVALID_HANDLE_VALUE)
	{
		dev9.eeprom=(u16*)eeprom;
	}
	else
	{
		mapping=CreateFileMapping(hEeprom,NULL,PAGE_READWRITE,0,0,NULL);
		if(mapping==INVALID_HANDLE_VALUE)
		{
			CloseHandle(hEeprom);
			dev9.eeprom=(u16*)eeprom;
		}
		else
		{
			dev9.eeprom = (u16*)MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,0);

			if(dev9.eeprom==NULL)
			{
				CloseHandle(mapping);
				CloseHandle(hEeprom);
				dev9.eeprom=(u16*)eeprom;
			}
		}
	}
#else
	hEeprom = open("eeprom.dat", O_RDWR, 0);

	if(-1 == hEeprom)
	{
		dev9.eeprom=(u16*)eeprom;
	}
	else
	{
			dev9.eeprom = (u16*)mmap(NULL, 64, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, hEeprom, 0);

			if(dev9.eeprom==NULL)
			{
				close(hEeprom);
				dev9.eeprom=(u16*)eeprom;
			}
	}
#endif

	int rxbi;

	for(rxbi=0;rxbi<(SMAP_BD_SIZE/8);rxbi++)
	{
		smap_bd_t *pbd = (smap_bd_t *)&dev9.dev9R[SMAP_BD_RX_BASE & 0xffff];
		pbd = &pbd[rxbi];

		pbd->ctrl_stat = SMAP_BD_RX_EMPTY;
		pbd->length = 0;
	}

	DEV9_LOG("DEV9init ok\n");

	return 0;
}

EXPORT_C_(void)
DEV9shutdown() {
	DEV9_LOG("DEV9shutdown\n");
}

EXPORT_C_(s32)
DEV9open(void *pDsp)
{
	DEV9_LOG("DEV9open\n");
	LoadConf();
#ifdef __LIBRETRO__
  SaveConf();
#endif
	DEV9_LOG("open r+: %s\n", config.Hdd);
	config.HddSize = 8*1024;
	
	iopPC = (u32*)pDsp;
	
#ifdef ENABLE_ATA
	ata_init();
#endif
	return _DEV9open();
}

EXPORT_C_(void)
DEV9close()
{
	DEV9_LOG("DEV9close\n");
#ifdef ENABLE_ATA
	ata_term();
#endif
	_DEV9close();
}

EXPORT_C_(int)
_DEV9irqHandler(void)
{
	//dev9Ru16(SPD_R_INTR_STAT)|= dev9.irqcause;
	DEV9_LOG("_DEV9irqHandler %x, %x\n", dev9.irqcause, dev9Ru16(SPD_R_INTR_MASK));
	if (dev9.irqcause & dev9Ru16(SPD_R_INTR_MASK)) 
		return 1;
	return 0;
}

EXPORT_C_(DEV9handler)
DEV9irqHandler(void) {
	return (DEV9handler)_DEV9irqHandler;
}

void _DEV9irq(int cause, int cycles)
{
	DEV9_LOG("_DEV9irq %x, %x\n", cause, dev9Ru16(SPD_R_INTR_MASK));

	dev9.irqcause|= cause;

	if(cycles<1)
		DEV9irq(1);
	else
		DEV9irq(cycles);
}


EXPORT_C_(u8)
 DEV9read8(u32 addr) {
	if (!config.ethEnable & !config.hddEnable)
		return 0;

	u8 hard;
	if (addr>=ATA_DEV9_HDD_BASE && addr<ATA_DEV9_HDD_END)
	{
#ifdef ENABLE_ATA
		return ata_read<1>(addr);
#else
		return 0;
#endif
	}
	if (addr>=SMAP_REGBASE && addr<FLASH_REGBASE)
	{
		//smap
		return smap_read8(addr);
	}
	
	switch (addr) 
	{
		case SPD_R_PIO_DATA:

			/*if(dev9.eeprom_dir!=1)
			{
				hard=0;
				break;
			}*/

			if(dev9.eeprom_state==EEPROM_TDATA)
			{
				if(dev9.eeprom_command==2) //read
				{
					if(dev9.eeprom_bit==0xFF)
						hard=0;
					else
						hard=((dev9.eeprom[dev9.eeprom_address]<<dev9.eeprom_bit)&0x8000)>>11;
					dev9.eeprom_bit++;
					if(dev9.eeprom_bit==16)
					{
						dev9.eeprom_address++;
						dev9.eeprom_bit=0;
					}
				}
				else hard=0;
			}
			else hard=0;
			return hard;

		case DEV9_R_REV:
			hard = 0x32; // expansion bay
			break;

		default:
			if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE))) {
				return (u8)FLASHread32(addr, 1);
			}

			hard = dev9Ru8(addr); 
			DEV9_LOG("*Unknown 8bit read at address %lx value %x\n", addr, hard);
			return hard;
	}
	
	DEV9_LOG("*Known 8bit read at address %lx value %x\n", addr, hard);
	return hard;
}

EXPORT_C_(u16)
DEV9read16(u32 addr)
{
	if (!config.ethEnable & !config.hddEnable)
		return 0;

	u16 hard;
	if (addr>=ATA_DEV9_HDD_BASE && addr<ATA_DEV9_HDD_END)
	{
#ifdef ENABLE_ATA
		return ata_read<2>(addr);
#else
		return 0;
#endif
	}
	if (addr>=SMAP_REGBASE && addr<FLASH_REGBASE)
	{
		//smap
		return smap_read16(addr);
	}

	switch (addr) 
	{
		case SPD_R_INTR_STAT:
			return dev9.irqcause;

		case DEV9_R_REV:
			hard = 0x0030; // expansion bay
			break;

		case SPD_R_REV_1:
			hard = 0x0011;
			DEV9_LOG("STD_R_REV_1 16bit read %x\n", hard);
			return hard;

		case SPD_R_REV_3:
			// bit 0: smap
			// bit 1: hdd
			// bit 5: flash
			hard = 0;
			/*if (config.hddEnable) {
				hard|= 0x2;
			}*/
			if (config.ethEnable) {
				hard|= 0x1;
			}
			hard|= 0x20;//flash
			break;

		case SPD_R_0e:
			hard = 0x0002;
			break;
		default:
			if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE))) {
				return (u16)FLASHread32(addr, 2);
			}

			hard = dev9Ru16(addr); 
			DEV9_LOG("*Unknown 16bit read at address %lx value %x\n", addr, hard);
			return hard;
	}
	
	DEV9_LOG("*Known 16bit read at address %lx value %x\n", addr, hard);
	return hard;
}

EXPORT_C_(u32)
DEV9read32(u32 addr)
{
	if (!config.ethEnable & !config.hddEnable)
		return 0;

	u32 hard;
	if (addr>=ATA_DEV9_HDD_BASE && addr<ATA_DEV9_HDD_END)
	{
#ifdef ENABLE_ATA
		return ata_read<4>(addr);
#else
		return 0;
#endif
	}
	if (addr>=SMAP_REGBASE && addr<FLASH_REGBASE)
	{
		//smap
		return smap_read32(addr);
	}
//	switch (addr) {

//		default:
			if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE))) {
				return (u32)FLASHread32(addr, 4);
			}

			hard = dev9Ru32(addr); 
			DEV9_LOG("*Unknown 32bit read at address %lx value %x\n", addr, hard);
			return hard;
//	}

//	DEV9_LOG("*Known 32bit read at address %lx: %lx\n", addr, hard);
//	return hard;
}

EXPORT_C_(void)
DEV9write8(u32 addr,  u8 value)
{
	if (!config.ethEnable & !config.hddEnable)
		return;

	if (addr>=ATA_DEV9_HDD_BASE && addr<ATA_DEV9_HDD_END)
	{
#ifdef ENABLE_ATA
		ata_write<1>(addr,value);
#endif
		return;
	}
	if (addr>=SMAP_REGBASE && addr<FLASH_REGBASE)
	{
		//smap
		smap_write8(addr,value);
		return;
	}
	switch (addr) {
		case 0x10000020:
			dev9.irqcause = 0xff;
			break;
		case SPD_R_INTR_STAT:
			emu_printf("SPD_R_INTR_STAT	, WTFH ?\n");
			dev9.irqcause=value;
			return;
		case SPD_R_INTR_MASK:
			emu_printf("SPD_R_INTR_MASK8	, WTFH ?\n");
			break;

		case SPD_R_PIO_DIR:
			//DEV9_LOG("SPD_R_PIO_DIR 8bit write %x\n", value);

			if((value&0xc0)!=0xc0)
				return;

			if((value&0x30)==0x20)
			{
				dev9.eeprom_state=0;
			}
			dev9.eeprom_dir=(value>>4)&3;
			
			return;

		case SPD_R_PIO_DATA:
			//DEV9_LOG("SPD_R_PIO_DATA 8bit write %x\n", value);

			if((value&0xc0)!=0xc0)
				return;

			switch(dev9.eeprom_state)
			{
				case EEPROM_READY:
					dev9.eeprom_command=0;
					dev9.eeprom_state++;
					break;
				case EEPROM_OPCD0:
					dev9.eeprom_command = (value>>4)&2;
					dev9.eeprom_state++;
					dev9.eeprom_bit=0xFF;
					break;
				case EEPROM_OPCD1:
					dev9.eeprom_command |= (value>>5)&1;
					dev9.eeprom_state++;
					break;
				case EEPROM_ADDR0:
				case EEPROM_ADDR1:
				case EEPROM_ADDR2:
				case EEPROM_ADDR3:
				case EEPROM_ADDR4:
				case EEPROM_ADDR5:
					dev9.eeprom_address =
						(dev9.eeprom_address&(63^(1<<(dev9.eeprom_state-EEPROM_ADDR0))))|
						((value>>(dev9.eeprom_state-EEPROM_ADDR0))&(0x20>>(dev9.eeprom_state-EEPROM_ADDR0)));
					dev9.eeprom_state++;
					break;
				case EEPROM_TDATA:
					{
						if(dev9.eeprom_command==1) //write
						{
							dev9.eeprom[dev9.eeprom_address] =
								(dev9.eeprom[dev9.eeprom_address]&(63^(1<<dev9.eeprom_bit)))|
								((value>>dev9.eeprom_bit)&(0x8000>>dev9.eeprom_bit));
							dev9.eeprom_bit++;
							if(dev9.eeprom_bit==16)
							{
								dev9.eeprom_address++;
								dev9.eeprom_bit=0;
							}
						}
					}
					break;
			}

			return;

		default:
			if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE))) {
				FLASHwrite32(addr, (u32)value, 1);
				return;
			}

			dev9Ru8(addr) = value;
			DEV9_LOG("*Unknown 8bit write at address %lx value %x\n", addr, value);
			return;
	}
	dev9Ru8(addr) = value;
	DEV9_LOG("*Known 8bit write at address %lx value %x\n", addr, value);
}

EXPORT_C_(void)
DEV9write16(u32 addr, u16 value)
{
	if (!config.ethEnable & !config.hddEnable)
		return;

	if (addr>=ATA_DEV9_HDD_BASE && addr<ATA_DEV9_HDD_END)
	{
#ifdef ENABLE_ATA
		ata_write<2>(addr,value);
#endif
		return;
	}
	if (addr>=SMAP_REGBASE && addr<FLASH_REGBASE)
	{
		//smap
		smap_write16(addr,value);
		return;
	}
	switch (addr) 
	{
		case SPD_R_INTR_MASK:
			if ((dev9Ru16(SPD_R_INTR_MASK)!=value) && ((dev9Ru16(SPD_R_INTR_MASK)|value) & dev9.irqcause))
			{
				DEV9_LOG("SPD_R_INTR_MASK16=0x%X	, checking for masked/unmasked interrupts\n",value);
				DEV9irq(1);
			}
			break;
		
		default:

			if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE))) {
				FLASHwrite32(addr, (u32)value, 2);
				return;
			}

			dev9Ru16(addr) = value;
			DEV9_LOG("*Unknown 16bit write at address %lx value %x\n", addr, value);
			return;
	}
	dev9Ru16(addr) = value;
	DEV9_LOG("*Known 16bit write at address %lx value %x\n", addr, value);
}

EXPORT_C_(void)
DEV9write32(u32 addr, u32 value)
{
	if (!config.ethEnable & !config.hddEnable)
		return;

	if (addr>=ATA_DEV9_HDD_BASE && addr<ATA_DEV9_HDD_END)
	{
#ifdef ENABLE_ATA
		ata_write<4>(addr,value);
#endif
		return;
	}
	if (addr>=SMAP_REGBASE && addr<FLASH_REGBASE)
	{
		//smap
		smap_write32(addr,value);
		return;
	}
	switch (addr) 
		{
		case SPD_R_INTR_MASK:
			emu_printf("SPD_R_INTR_MASK	, WTFH ?\n");
			break;
		default:
			if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE))) {
				FLASHwrite32(addr, (u32)value, 4);
				return;
			}

			dev9Ru32(addr) = value;
			DEV9_LOG("*Unknown 32bit write at address %lx write %x\n", addr, value);
			return;
	}
	dev9Ru32(addr) = value;
	DEV9_LOG("*Known 32bit write at address %lx value %lx\n", addr, value);
}

EXPORT_C_(void)
DEV9readDMA8Mem(u32 *pMem, int size)
{
	if (!config.ethEnable & !config.hddEnable)
		return;

	DEV9_LOG("*DEV9readDMA8Mem: size %x\n", size);
	emu_printf("rDMA\n");
	
	smap_readDMA8Mem(pMem,size);
#ifdef ENABLE_ATA
	ata_readDMA8Mem(pMem,size);
#endif
}

EXPORT_C_(void)
DEV9writeDMA8Mem(u32* pMem, int size)
{
	if (!config.ethEnable & !config.hddEnable)
		return;

	DEV9_LOG("*DEV9writeDMA8Mem: size %x\n", size);
	emu_printf("wDMA\n");
	
	smap_writeDMA8Mem(pMem,size);
#ifdef ENABLE_ATA
	ata_writeDMA8Mem(pMem,size);
#endif
}


//plugin interface
EXPORT_C_(void)
DEV9irqCallback(void (*callback)(int cycles)) {
	DEV9irq = callback;
}

EXPORT_C_(void)
DEV9async(u32 cycles)
{
	smap_async(cycles);
}

// extended funcs

EXPORT_C_(s32)
 DEV9test() {
	return 0;
}

EXPORT_C_(void)
DEV9setSettingsDir(const char* dir)
{
}

EXPORT_C_(void)
DEV9setLogDir(const char* dir)
{
}

EXPORT_C_(void)
DEV9about()
{
}

EXPORT_C_(s32)
DEV9freeze(int mode, freezeData *data)
{
    // This should store or retrieve any information, for if emulation
    // gets suspended, or for savestates.
    switch (mode) {
        case FREEZE_LOAD:
            // Load previously saved data.
            break;
        case FREEZE_SAVE:
            // Save data.
            break;
        case FREEZE_SIZE:
            // return the size of the data.
            break;
    }
    return 0;
}

EXPORT_C_(void)
DEV9keyEvent(keyEvent *ev)
{
}

int emu_printf(const char *fmt, ...)
{
	va_list vl;
	int ret;
	va_start(vl,fmt);
	ret = vfprintf(stderr,fmt,vl);
	va_end(vl);
	fflush(stderr);
	return ret;
}
