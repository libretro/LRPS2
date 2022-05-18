/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#pragma once

enum GIF_PATH {
	GIF_PATH_1 = 0,
	GIF_PATH_2,
	GIF_PATH_3
};

// Lower byte contains path minus 1
enum GIF_TRANSFER_TYPE {
	GIF_TRANS_INVALID  = 0x000, // Invalid
	GIF_TRANS_XGKICK   = 0x100, // Path 1
	GIF_TRANS_MTVU     = 0x200, // Path 1
	GIF_TRANS_DIRECT   = 0x301, // Path 2
	GIF_TRANS_DIRECTHL = 0x401, // Path 2
	GIF_TRANS_DMA      = 0x502, // Path 3
	GIF_TRANS_FIFO     = 0x602  // Path 3
};

enum GIF_PATH_STATE {
	GIF_PATH_IDLE    = 0, // Path is idle (hasn't started a GS packet)
	GIF_PATH_PACKED  = 1, // Path is on a PACKED  gif tag
	GIF_PATH_REGLIST = 2, // Path is on a REGLIST gif tag
	GIF_PATH_IMAGE   = 3, // Path is on a IMAGE   gif tag
	GIF_PATH_WAIT	 = 4  // Used only by PATH3 to simulate packet length (Path 3 Masking)
};

enum GIF_REG_COMPLEX
{
	GIF_REG_STQRGBAXYZF2	= 0x00,
	GIF_REG_STQRGBAXYZ2	= 0x01
};

enum GIF_A_D_REG
{
	GIF_A_D_REG_PRIM	= 0x00,
	GIF_A_D_REG_RGBAQ	= 0x01,
	GIF_A_D_REG_ST		= 0x02,
	GIF_A_D_REG_UV		= 0x03,
	GIF_A_D_REG_XYZF2	= 0x04,
	GIF_A_D_REG_XYZ2	= 0x05,
	GIF_A_D_REG_TEX0_1	= 0x06,
	GIF_A_D_REG_TEX0_2	= 0x07,
	GIF_A_D_REG_CLAMP_1	= 0x08,
	GIF_A_D_REG_CLAMP_2	= 0x09,
	GIF_A_D_REG_FOG		= 0x0a,
	GIF_A_D_REG_XYZF3	= 0x0c,
	GIF_A_D_REG_XYZ3	= 0x0d,
	GIF_A_D_REG_NOP		= 0x0f,
	GIF_A_D_REG_TEX1_1	= 0x14,
	GIF_A_D_REG_TEX1_2	= 0x15,
	GIF_A_D_REG_TEX2_1	= 0x16,
	GIF_A_D_REG_TEX2_2	= 0x17,
	GIF_A_D_REG_XYOFFSET_1	= 0x18,
	GIF_A_D_REG_XYOFFSET_2	= 0x19,
	GIF_A_D_REG_PRMODECONT	= 0x1a,
	GIF_A_D_REG_PRMODE	= 0x1b,
	GIF_A_D_REG_TEXCLUT	= 0x1c,
	GIF_A_D_REG_SCANMSK	= 0x22,
	GIF_A_D_REG_MIPTBP1_1	= 0x34,
	GIF_A_D_REG_MIPTBP1_2	= 0x35,
	GIF_A_D_REG_MIPTBP2_1	= 0x36,
	GIF_A_D_REG_MIPTBP2_2	= 0x37,
	GIF_A_D_REG_TEXA	= 0x3b,
	GIF_A_D_REG_FOGCOL	= 0x3d,
	GIF_A_D_REG_TEXFLUSH	= 0x3f,
	GIF_A_D_REG_SCISSOR_1	= 0x40,
	GIF_A_D_REG_SCISSOR_2	= 0x41,
	GIF_A_D_REG_ALPHA_1	= 0x42,
	GIF_A_D_REG_ALPHA_2	= 0x43,
	GIF_A_D_REG_DIMX	= 0x44,
	GIF_A_D_REG_DTHE	= 0x45,
	GIF_A_D_REG_COLCLAMP	= 0x46,
	GIF_A_D_REG_TEST_1	= 0x47,
	GIF_A_D_REG_TEST_2	= 0x48,
	GIF_A_D_REG_PABE	= 0x49,
	GIF_A_D_REG_FBA_1	= 0x4a,
	GIF_A_D_REG_FBA_2	= 0x4b,
	GIF_A_D_REG_FRAME_1	= 0x4c,
	GIF_A_D_REG_FRAME_2	= 0x4d,
	GIF_A_D_REG_ZBUF_1	= 0x4e,
	GIF_A_D_REG_ZBUF_2	= 0x4f,
	GIF_A_D_REG_BITBLTBUF	= 0x50,
	GIF_A_D_REG_TRXPOS	= 0x51,
	GIF_A_D_REG_TRXREG	= 0x52,
	GIF_A_D_REG_TRXDIR	= 0x53,
	GIF_A_D_REG_HWREG	= 0x54,
	GIF_A_D_REG_SIGNAL	= 0x60,
	GIF_A_D_REG_FINISH	= 0x61,
	GIF_A_D_REG_LABEL	= 0x62
};

enum GIF_FLG {
	GIF_FLG_PACKED	= 0,
	GIF_FLG_REGLIST	= 1,
	GIF_FLG_IMAGE	= 2,
	GIF_FLG_IMAGE2	= 3
};

enum GIF_REG {
	GIF_REG_PRIM	= 0x00,
	GIF_REG_RGBA	= 0x01,
	GIF_REG_STQ	= 0x02,
	GIF_REG_UV	= 0x03,
	GIF_REG_XYZF2	= 0x04,
	GIF_REG_XYZ2	= 0x05,
	GIF_REG_TEX0_1	= 0x06,
	GIF_REG_TEX0_2	= 0x07,
	GIF_REG_CLAMP_1	= 0x08,
	GIF_REG_CLAMP_2	= 0x09,
	GIF_REG_FOG	= 0x0a,
	GIF_REG_INVALID	= 0x0b,
	GIF_REG_XYZF3	= 0x0c,
	GIF_REG_XYZ3	= 0x0d,
	GIF_REG_A_D	= 0x0e,
	GIF_REG_NOP	= 0x0f
};

enum gifstate_t {
	GIF_STATE_READY = 0,
	GIF_STATE_EMPTY = 0x10
};

struct gifStruct {
	int  gifstate;
	bool gspath3done;

	u32 gscycles;
	u32 prevcycles;
	u32 mfifocycles;
	u32 gifqwc;
	bool gifmfifoirq;
};

struct GIF_Fifo
{
	unsigned int data[64]; //16 QW FIFO
	unsigned int fifoSize;

	int write_fifo(u32* pMem, int size);
	int read_fifo();

	void init();
};

union tGIF_CTRL
{
	struct {
		u32 RST : 1;
		u32 reserved1 : 2;
		u32 PSE : 1;
		u32 reserved2 : 28;
	};
	u32 _u32;
};

union tGIF_MODE
{
	struct {
		u32 M3R : 1;
		u32 reserved1 : 1;
		u32 IMT : 1;
		u32 reserved2 : 29;
	};
	u32 _u32;
};

union tGIF_STAT
{
	struct {
		u32 M3R : 1;		// GIF_MODE Mask
		u32 M3P : 1;		// VIF PATH3 Mask
		u32 IMT : 1;		// Intermittent Transfer Mode
		u32 PSE : 1;		// Temporary Transfer Stop
		u32 reserved1 : 1;	// ...
		u32 IP3 : 1;		// Interrupted PATH3
		u32 P3Q : 1;		// PATH3 request Queued
		u32 P2Q : 1;		// PATH2 request Queued
		u32 P1Q : 1;		// PATH1 request Queued
		u32 OPH : 1;		// Output Path (Outputting Data)
		u32 APATH : 2;		// Data Transfer Path (In progress)
		u32 DIR : 1;		// Transfer Direction
		u32 reserved2 : 11;	// ...
		u32 FQC : 5;		// QWC in GIF-FIFO
		u32 reserved3 : 3;	// ...
	};
	u32 _u32;
};

union tGIF_TAG0
{
	struct {
		u32 NLOOP : 15;
		u32 EOP : 1;
		u32 TAG : 16;
	};
	u32 _u32;
};

union tGIF_TAG1
{
	struct {
		u32 TAG : 14;
		u32 PRE : 1;
		u32 PRIM : 11;
		u32 FLG : 2;
		u32 NREG : 4;
	};
	u32 _u32;
};

union tGIF_CNT
{
	struct {
		u32 LOOPCNT : 15;
		u32 reserved1 : 1;
		u32 REGCNT : 4;
		u32 VUADDR : 2;
		u32 reserved2 : 10;

	};
	u32 _u32;
};

union tGIF_P3CNT
{
	struct {
		u32 P3CNT : 15;
		u32 reserved1 : 17;
	};
	u32 _u32;
};

union tGIF_P3TAG
{
	struct {
		u32 LOOPCNT : 15;
		u32 EOP : 1;
		u32 reserved1 : 16;
	};
	u32 _u32;
};

struct GIFregisters
{
	tGIF_CTRL 	ctrl;
	u32 padding[3];
	tGIF_MODE 	mode;
	u32 padding1[3];
	tGIF_STAT	stat;
	u32 padding2[7];

	tGIF_TAG0	tag0;
	u32 padding3[3];
	tGIF_TAG1	tag1;
	u32 padding4[3];
	u32			tag2;
	u32 padding5[3];
	u32			tag3;
	u32 padding6[3];

	tGIF_CNT	cnt;
	u32 padding7[3];
	tGIF_P3CNT	p3cnt;
	u32 padding8[3];
	tGIF_P3TAG	p3tag;
	u32 padding9[3];
};
