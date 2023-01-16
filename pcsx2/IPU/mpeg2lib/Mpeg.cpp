/*
 * Mpeg.c
 * Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 * Modified by Florin for PCSX2 emu
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

// [Air] Note: many functions in this module are large and only used once, so they
//	have been forced to inline since it won't bloat the program and gets rid of
//	some call overhead.

#include "Common.h"
#include "IPU/IPU.h"
#include "Mpeg.h"

#include "Utilities/MemsetFast.inl"

// Removes bits from the bitstream.  This is done independently of UBITS/SBITS because a
// lot of mpeg streams have to read ahead and rewind bits and re-read them at different
// bit depths or sign'age.
#define DUMPBITS(num) (g_BP.Advance(num))
#define GETWORD()     (g_BP.FillBuffer(16))
#define INTRA 		MACROBLOCK_INTRA
#define QUANT 		MACROBLOCK_QUANT
#define MC 		MACROBLOCK_MOTION_FORWARD
#define CODED 		MACROBLOCK_PATTERN
#define FWD 		MACROBLOCK_MOTION_FORWARD
#define BWD 		MACROBLOCK_MOTION_BACKWARD
#define INTER 		MACROBLOCK_MOTION_FORWARD|MACROBLOCK_MOTION_BACKWARD

struct MBtab {
    u8 modes;
    u8 len;
};

struct MVtab {
    u8 delta;
    u8 len;
};

struct DMVtab {
    s8 dmv;
    u8 len;
};

struct CBPtab {
    u8 cbp;
    u8 len;
};

struct DCtab {
    u8 size;
    u8 len;
};

struct DCTtab {
    u8 run;
    u8 level;
    u8 len;
};

struct MBAtab {
    u8 mba;
    u8 len;
};

struct MBAtabSet
{
	MBAtab mba5[30];
	MBAtab mba11[26*4];
};

struct DCtabSet
{
	DCtab lum0[32];		// Table B-12, dct_dc_size_luminance, codes 00xxx ... 11110
	DCtab lum1[16];		// Table B-12, dct_dc_size_luminance, codes 111110xxx ... 111111111
	DCtab chrom0[32];	// Table B-13, dct_dc_size_chrominance, codes 00xxx ... 11110
	DCtab chrom1[32];	// Table B-13, dct_dc_size_chrominance, codes 111110xxxx ... 1111111111
};

struct DCTtabSet
{
	DCTtab first[12];
	DCTtab next[12];

	DCTtab tab0[60];
	DCTtab tab0a[252];
	DCTtab tab1[8];
	DCTtab tab1a[8];

	DCTtab tab2[16];
	DCTtab tab3[16];
	DCTtab tab4[16];
	DCTtab tab5[16];
	DCTtab tab6[16];
};

static const MBtab MB_I [] = {
    {INTRA|QUANT, 2}, {INTRA, 1}
};

static const __aligned16 MBtab MB_P [] = {
    {INTRA|QUANT, 6}, {CODED|QUANT, 5}, {MC|CODED|QUANT, 5}, {INTRA,    5},
    {MC,          3}, {MC,          3}, {MC,             3}, {MC,       3},
    {CODED,       2}, {CODED,       2}, {CODED,          2}, {CODED,    2},
    {CODED,       2}, {CODED,       2}, {CODED,          2}, {CODED,    2},
    {MC|CODED,    1}, {MC|CODED,    1}, {MC|CODED,       1}, {MC|CODED, 1},
    {MC|CODED,    1}, {MC|CODED,    1}, {MC|CODED,       1}, {MC|CODED, 1},
    {MC|CODED,    1}, {MC|CODED,    1}, {MC|CODED,       1}, {MC|CODED, 1},
    {MC|CODED,    1}, {MC|CODED,    1}, {MC|CODED,       1}, {MC|CODED, 1}
};

static const __aligned16 MBtab MB_B [] = {
    {0,                 0}, {INTRA|QUANT,       6},
    {BWD|CODED|QUANT,   6}, {FWD|CODED|QUANT,   6},
    {INTER|CODED|QUANT, 5}, {INTER|CODED|QUANT, 5},
					{INTRA,       5}, {INTRA,       5},
    {FWD,         4}, {FWD,         4}, {FWD,         4}, {FWD,         4},
    {FWD|CODED,   4}, {FWD|CODED,   4}, {FWD|CODED,   4}, {FWD|CODED,   4},
    {BWD,         3}, {BWD,         3}, {BWD,         3}, {BWD,         3},
    {BWD,         3}, {BWD,         3}, {BWD,         3}, {BWD,         3},
    {BWD|CODED,   3}, {BWD|CODED,   3}, {BWD|CODED,   3}, {BWD|CODED,   3},
    {BWD|CODED,   3}, {BWD|CODED,   3}, {BWD|CODED,   3}, {BWD|CODED,   3},
    {INTER,       2}, {INTER,       2}, {INTER,       2}, {INTER,       2},
    {INTER,       2}, {INTER,       2}, {INTER,       2}, {INTER,       2},
    {INTER,       2}, {INTER,       2}, {INTER,       2}, {INTER,       2},
    {INTER,       2}, {INTER,       2}, {INTER,       2}, {INTER,       2},
    {INTER|CODED, 2}, {INTER|CODED, 2}, {INTER|CODED, 2}, {INTER|CODED, 2},
    {INTER|CODED, 2}, {INTER|CODED, 2}, {INTER|CODED, 2}, {INTER|CODED, 2},
    {INTER|CODED, 2}, {INTER|CODED, 2}, {INTER|CODED, 2}, {INTER|CODED, 2},
    {INTER|CODED, 2}, {INTER|CODED, 2}, {INTER|CODED, 2}, {INTER|CODED, 2}
};

static const MVtab MV_4 [] = {
    { 3, 6}, { 2, 4}, { 1, 3}, { 1, 3}, { 0, 2}, { 0, 2}, { 0, 2}, { 0, 2}
};

static const __aligned16 MVtab MV_10 [] = {
    { 0,10}, { 0,10}, { 0,10}, { 0,10}, { 0,10}, { 0,10}, { 0,10}, { 0,10},
    { 0,10}, { 0,10}, { 0,10}, { 0,10}, {15,10}, {14,10}, {13,10}, {12,10},
    {11,10}, {10,10}, { 9, 9}, { 9, 9}, { 8, 9}, { 8, 9}, { 7, 9}, { 7, 9},
    { 6, 7}, { 6, 7}, { 6, 7}, { 6, 7}, { 6, 7}, { 6, 7}, { 6, 7}, { 6, 7},
    { 5, 7}, { 5, 7}, { 5, 7}, { 5, 7}, { 5, 7}, { 5, 7}, { 5, 7}, { 5, 7},
    { 4, 7}, { 4, 7}, { 4, 7}, { 4, 7}, { 4, 7}, { 4, 7}, { 4, 7}, { 4, 7}
};


static const DMVtab DMV_2 [] = {
    { 0, 1}, { 0, 1}, { 1, 2}, {-1, 2}
};


static const __aligned16 CBPtab CBP_7 [] = {
    {0x22, 7}, {0x12, 7}, {0x0a, 7}, {0x06, 7},
    {0x21, 7}, {0x11, 7}, {0x09, 7}, {0x05, 7},
    {0x3f, 6}, {0x3f, 6}, {0x03, 6}, {0x03, 6},
    {0x24, 6}, {0x24, 6}, {0x18, 6}, {0x18, 6},
    {0x3e, 5}, {0x3e, 5}, {0x3e, 5}, {0x3e, 5},
    {0x02, 5}, {0x02, 5}, {0x02, 5}, {0x02, 5},
    {0x3d, 5}, {0x3d, 5}, {0x3d, 5}, {0x3d, 5},
    {0x01, 5}, {0x01, 5}, {0x01, 5}, {0x01, 5},
    {0x38, 5}, {0x38, 5}, {0x38, 5}, {0x38, 5},
    {0x34, 5}, {0x34, 5}, {0x34, 5}, {0x34, 5},
    {0x2c, 5}, {0x2c, 5}, {0x2c, 5}, {0x2c, 5},
    {0x1c, 5}, {0x1c, 5}, {0x1c, 5}, {0x1c, 5},
    {0x28, 5}, {0x28, 5}, {0x28, 5}, {0x28, 5},
    {0x14, 5}, {0x14, 5}, {0x14, 5}, {0x14, 5},
    {0x30, 5}, {0x30, 5}, {0x30, 5}, {0x30, 5},
    {0x0c, 5}, {0x0c, 5}, {0x0c, 5}, {0x0c, 5},
    {0x20, 4}, {0x20, 4}, {0x20, 4}, {0x20, 4},
    {0x20, 4}, {0x20, 4}, {0x20, 4}, {0x20, 4},
    {0x10, 4}, {0x10, 4}, {0x10, 4}, {0x10, 4},
    {0x10, 4}, {0x10, 4}, {0x10, 4}, {0x10, 4},
    {0x08, 4}, {0x08, 4}, {0x08, 4}, {0x08, 4},
    {0x08, 4}, {0x08, 4}, {0x08, 4}, {0x08, 4},
    {0x04, 4}, {0x04, 4}, {0x04, 4}, {0x04, 4},
    {0x04, 4}, {0x04, 4}, {0x04, 4}, {0x04, 4},
    {0x3c, 3}, {0x3c, 3}, {0x3c, 3}, {0x3c, 3},
    {0x3c, 3}, {0x3c, 3}, {0x3c, 3}, {0x3c, 3},
    {0x3c, 3}, {0x3c, 3}, {0x3c, 3}, {0x3c, 3},
    {0x3c, 3}, {0x3c, 3}, {0x3c, 3}, {0x3c, 3}
};

static const __aligned16 CBPtab CBP_9 [] = {
    {0,    0}, {0x00, 9}, {0x27, 9}, {0x1b, 9},
    {0x3b, 9}, {0x37, 9}, {0x2f, 9}, {0x1f, 9},
    {0x3a, 8}, {0x3a, 8}, {0x36, 8}, {0x36, 8},
    {0x2e, 8}, {0x2e, 8}, {0x1e, 8}, {0x1e, 8},
    {0x39, 8}, {0x39, 8}, {0x35, 8}, {0x35, 8},
    {0x2d, 8}, {0x2d, 8}, {0x1d, 8}, {0x1d, 8},
    {0x26, 8}, {0x26, 8}, {0x1a, 8}, {0x1a, 8},
    {0x25, 8}, {0x25, 8}, {0x19, 8}, {0x19, 8},
    {0x2b, 8}, {0x2b, 8}, {0x17, 8}, {0x17, 8},
    {0x33, 8}, {0x33, 8}, {0x0f, 8}, {0x0f, 8},
    {0x2a, 8}, {0x2a, 8}, {0x16, 8}, {0x16, 8},
    {0x32, 8}, {0x32, 8}, {0x0e, 8}, {0x0e, 8},
    {0x29, 8}, {0x29, 8}, {0x15, 8}, {0x15, 8},
    {0x31, 8}, {0x31, 8}, {0x0d, 8}, {0x0d, 8},
    {0x23, 8}, {0x23, 8}, {0x13, 8}, {0x13, 8},
    {0x0b, 8}, {0x0b, 8}, {0x07, 8}, {0x07, 8}
};

static const __aligned16 MBAtabSet MBA = {
	{	// mba5
				{6, 5}, {5, 5}, {4, 4}, {4, 4}, {3, 4}, {3, 4},
		{2, 3}, {2, 3}, {2, 3}, {2, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3},
		{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
		{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}
	},

	{	// mba11
		{32, 11}, {31, 11}, {30, 11}, {29, 11},
		{28, 11}, {27, 11}, {26, 11}, {25, 11},
		{24, 11}, {23, 11}, {22, 11}, {21, 11},
		{20, 10}, {20, 10}, {19, 10}, {19, 10},
		{18, 10}, {18, 10}, {17, 10}, {17, 10},
		{16, 10}, {16, 10}, {15, 10}, {15, 10},
		{14,  8}, {14,  8}, {14,  8}, {14,  8},
		{14,  8}, {14,  8}, {14,  8}, {14,  8},
		{13,  8}, {13,  8}, {13,  8}, {13,  8},
		{13,  8}, {13,  8}, {13,  8}, {13,  8},
		{12,  8}, {12,  8}, {12,  8}, {12,  8},
		{12,  8}, {12,  8}, {12,  8}, {12,  8},
		{11,  8}, {11,  8}, {11,  8}, {11,  8},
		{11,  8}, {11,  8}, {11,  8}, {11,  8},
		{10,  8}, {10,  8}, {10,  8}, {10,  8},
		{10,  8}, {10,  8}, {10,  8}, {10,  8},
		{ 9,  8}, { 9,  8}, { 9,  8}, { 9,  8},
		{ 9,  8}, { 9,  8}, { 9,  8}, { 9,  8},
		{ 8,  7}, { 8,  7}, { 8,  7}, { 8,  7},
		{ 8,  7}, { 8,  7}, { 8,  7}, { 8,  7},
		{ 8,  7}, { 8,  7}, { 8,  7}, { 8,  7},
		{ 8,  7}, { 8,  7}, { 8,  7}, { 8,  7},
		{ 7,  7}, { 7,  7}, { 7,  7}, { 7,  7},
		{ 7,  7}, { 7,  7}, { 7,  7}, { 7,  7},
		{ 7,  7}, { 7,  7}, { 7,  7}, { 7,  7},
		{ 7,  7}, { 7,  7}, { 7,  7}, { 7,  7}
	}
};

static const __aligned16 DCtabSet DCtable =
{
	// lum0: Table B-12, dct_dc_size_luminance, codes 00xxx ... 11110 */
	{ {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	  {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
	  {0, 3}, {0, 3}, {0, 3}, {0, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3},
	  {4, 3}, {4, 3}, {4, 3}, {4, 3}, {5, 4}, {5, 4}, {6, 5}, {0, 0} },

	/* lum1: Table B-12, dct_dc_size_luminance, codes 111110xxx ... 111111111 */
	{ {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6},
	  {8, 7}, {8, 7}, {8, 7}, {8, 7}, {9, 8}, {9, 8}, {10,9}, {11,9} },

	/* chrom0: Table B-13, dct_dc_size_chrominance, codes 00xxx ... 11110 */
	{ {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
	  {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	  {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
	  {3, 3}, {3, 3}, {3, 3}, {3, 3}, {4, 4}, {4, 4}, {5, 5}, {0, 0} },

	/* chrom1: Table B-13, dct_dc_size_chrominance, codes 111110xxxx ... 1111111111 */
	{ {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
	  {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
	  {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7},
	  {8, 8}, {8, 8}, {8, 8}, {8, 8}, {9, 9}, {9, 9}, {10,10}, {11,10} },
};

static const __aligned16 DCTtabSet DCT =
{
	/* first[12]: Table B-14, DCT coefficients table zero,
	 * codes 0100 ... 1xxx (used for first (DC) coefficient)
	 */
	{ {0,2,4}, {2,1,4}, {1,1,3}, {1,1,3},
	  {0,1,1}, {0,1,1}, {0,1,1}, {0,1,1},
	  {0,1,1}, {0,1,1}, {0,1,1}, {0,1,1} },

	/* next[12]: Table B-14, DCT coefficients table zero,
	 * codes 0100 ... 1xxx (used for all other coefficients)
	 */
	{ {0,2,4},  {2,1,4},  {1,1,3},  {1,1,3},
	  {64,0,2}, {64,0,2}, {64,0,2}, {64,0,2}, /* EOB */
	  {0,1,2},  {0,1,2},  {0,1,2},  {0,1,2} },

	/* tab0[60]: Table B-14, DCT coefficients table zero,
	 * codes 000001xx ... 00111xxx
	 */
	{ {65,0,6}, {65,0,6}, {65,0,6}, {65,0,6}, /* Escape */
	  {2,2,7}, {2,2,7}, {9,1,7}, {9,1,7},
	  {0,4,7}, {0,4,7}, {8,1,7}, {8,1,7},
	  {7,1,6}, {7,1,6}, {7,1,6}, {7,1,6},
	  {6,1,6}, {6,1,6}, {6,1,6}, {6,1,6},
	  {1,2,6}, {1,2,6}, {1,2,6}, {1,2,6},
	  {5,1,6}, {5,1,6}, {5,1,6}, {5,1,6},
	  {13,1,8}, {0,6,8}, {12,1,8}, {11,1,8},
	  {3,2,8}, {1,3,8}, {0,5,8}, {10,1,8},
	  {0,3,5}, {0,3,5}, {0,3,5}, {0,3,5},
	  {0,3,5}, {0,3,5}, {0,3,5}, {0,3,5},
	  {4,1,5}, {4,1,5}, {4,1,5}, {4,1,5},
	  {4,1,5}, {4,1,5}, {4,1,5}, {4,1,5},
	  {3,1,5}, {3,1,5}, {3,1,5}, {3,1,5},
	  {3,1,5}, {3,1,5}, {3,1,5}, {3,1,5} },

	/* tab0a[252]: Table B-15, DCT coefficients table one,
	 * codes 000001xx ... 11111111
	 */
	{ {65,0,6}, {65,0,6}, {65,0,6}, {65,0,6}, /* Escape */
	  {7,1,7}, {7,1,7}, {8,1,7}, {8,1,7},
	  {6,1,7}, {6,1,7}, {2,2,7}, {2,2,7},
	  {0,7,6}, {0,7,6}, {0,7,6}, {0,7,6},
	  {0,6,6}, {0,6,6}, {0,6,6}, {0,6,6},
	  {4,1,6}, {4,1,6}, {4,1,6}, {4,1,6},
	  {5,1,6}, {5,1,6}, {5,1,6}, {5,1,6},
	  {1,5,8}, {11,1,8}, {0,11,8}, {0,10,8},
	  {13,1,8}, {12,1,8}, {3,2,8}, {1,4,8},
	  {2,1,5}, {2,1,5}, {2,1,5}, {2,1,5},
	  {2,1,5}, {2,1,5}, {2,1,5}, {2,1,5},
	  {1,2,5}, {1,2,5}, {1,2,5}, {1,2,5},
	  {1,2,5}, {1,2,5}, {1,2,5}, {1,2,5},
	  {3,1,5}, {3,1,5}, {3,1,5}, {3,1,5},
	  {3,1,5}, {3,1,5}, {3,1,5}, {3,1,5},
	  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	  {1,1,3}, {1,1,3}, {1,1,3}, {1,1,3},
	  {64,0,4}, {64,0,4}, {64,0,4}, {64,0,4}, /* EOB */
	  {64,0,4}, {64,0,4}, {64,0,4}, {64,0,4},
	  {64,0,4}, {64,0,4}, {64,0,4}, {64,0,4},
	  {64,0,4}, {64,0,4}, {64,0,4}, {64,0,4},
	  {0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
	  {0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
	  {0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
	  {0,3,4}, {0,3,4}, {0,3,4}, {0,3,4},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,1,2}, {0,1,2}, {0,1,2}, {0,1,2},
	  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	  {0,2,3}, {0,2,3}, {0,2,3}, {0,2,3},
	  {0,4,5}, {0,4,5}, {0,4,5}, {0,4,5},
	  {0,4,5}, {0,4,5}, {0,4,5}, {0,4,5},
	  {0,5,5}, {0,5,5}, {0,5,5}, {0,5,5},
	  {0,5,5}, {0,5,5}, {0,5,5}, {0,5,5},
	  {9,1,7}, {9,1,7}, {1,3,7}, {1,3,7},
	  {10,1,7}, {10,1,7}, {0,8,7}, {0,8,7},
	  {0,9,7}, {0,9,7}, {0,12,8}, {0,13,8},
	  {2,3,8}, {4,2,8}, {0,14,8}, {0,15,8} },

	/* Table B-14, DCT coefficients table zero,
	 * codes 0000001000 ... 0000001111
	 */
	{ {16,1,10}, {5,2,10}, {0,7,10}, {2,3,10},
	  {1,4,10}, {15,1,10}, {14,1,10}, {4,2,10} },

	/* Table B-15, DCT coefficients table one,
	 * codes 000000100x ... 000000111x
	 */
	{ {5,2,9}, {5,2,9}, {14,1,9}, {14,1,9},
	  {2,4,10}, {16,1,10}, {15,1,9}, {15,1,9} },

	/* Table B-14/15, DCT coefficients table zero / one,
	 * codes 000000010000 ... 000000011111
	 */
	{ {0,11,12}, {8,2,12}, {4,3,12}, {0,10,12},
	  {2,4,12}, {7,2,12}, {21,1,12}, {20,1,12},
	  {0,9,12}, {19,1,12}, {18,1,12}, {1,5,12},
	  {3,3,12}, {0,8,12}, {6,2,12}, {17,1,12} },

	/* Table B-14/15, DCT coefficients table zero / one,
	 * codes 0000000010000 ... 0000000011111
	 */
	{ {10,2,13}, {9,2,13}, {5,3,13}, {3,4,13},
	  {2,5,13}, {1,7,13}, {1,6,13}, {0,15,13},
	  {0,14,13}, {0,13,13}, {0,12,13}, {26,1,13},
	  {25,1,13}, {24,1,13}, {23,1,13}, {22,1,13} },

	/* Table B-14/15, DCT coefficients table zero / one,
	 * codes 00000000010000 ... 00000000011111
	 */
	{ {0,31,14}, {0,30,14}, {0,29,14}, {0,28,14},
	  {0,27,14}, {0,26,14}, {0,25,14}, {0,24,14},
	  {0,23,14}, {0,22,14}, {0,21,14}, {0,20,14},
	  {0,19,14}, {0,18,14}, {0,17,14}, {0,16,14} },

	/* Table B-14/15, DCT coefficients table zero / one,
	 * codes 000000000010000 ... 000000000011111
	 */
	{ {0,40,15}, {0,39,15}, {0,38,15}, {0,37,15},
	  {0,36,15}, {0,35,15}, {0,34,15}, {0,33,15},
	  {0,32,15}, {1,14,15}, {1,13,15}, {1,12,15},
	  {1,11,15}, {1,10,15}, {1,9,15}, {1,8,15} },

	/* Table B-14/15, DCT coefficients table zero / one,
	 * codes 0000000000010000 ... 0000000000011111
	 */
	{ {1,18,16}, {1,17,16}, {1,16,16}, {1,15,16},
	  {6,3,16}, {16,2,16}, {15,2,16}, {14,2,16},
	  {13,2,16}, {12,2,16}, {11,2,16}, {31,1,16},
	  {30,1,16}, {29,1,16}, {28,1,16}, {27,1,16} }

};

const int non_linear_quantizer_scale [] =
{
	0,  1,  2,  3,  4,  5,	6,	7,
	8, 10, 12, 14, 16, 18,  20,  22,
	24, 28, 32, 36, 40, 44,  48,  52,
	56, 64, 72, 80, 88, 96, 104, 112
};

/* Bitstream and buffer needs to be reallocated in order for successful
	reading of the old data. Here the old data stored in the 2nd slot
	of the internal buffer is copied to 1st slot, and the new data read
	into 1st slot is copied to the 2nd slot. Which will later be copied
	back to the 1st slot when 128bits have been read.
*/
const DCTtab * tab;
static int mba_count = 0;

static __ri u32 UBITS(uint bits)
{
	uint readpos8 = g_BP.BP/8;

	uint result = BigEndian(*(u32*)( (u8*)g_BP.internal_qwc + readpos8 ));
	uint bp7 = (g_BP.BP & 7);
	result <<= bp7;
	result >>= (32 - bits);

	return result;
}

static __ri s32 SBITS(uint bits)
{
	// Read an unaligned 32 bit value and then shift the bits up and then back down.

	uint readpos8 = g_BP.BP/8;

	int result = BigEndian(*(s32*)( (s8*)g_BP.internal_qwc + readpos8 ));
	uint bp7 = (g_BP.BP & 7);
	result <<= bp7;
	result >>= (32 - bits);

	return result;
}

static __fi u32 GETBITS(uint num)
{
	uint ret = UBITS(num);
	g_BP.Advance(num);
	return ret;
}

int bitstream_init(void)
{
	return g_BP.FillBuffer(32);
}

int get_macroblock_modes(void)
{
	int macroblock_modes;
	const MBtab * tab;

	switch (decoder.coding_type)
	{
		case I_TYPE:
			macroblock_modes = UBITS(2);

			if (macroblock_modes == 0) return 0;   // error

			tab = MB_I + (macroblock_modes >> 1);
			DUMPBITS(tab->len);
			macroblock_modes = tab->modes;

			if ((!(decoder.frame_pred_frame_dct)) &&
				(decoder.picture_structure == FRAME_PICTURE))
				macroblock_modes |= GETBITS(1) * DCT_TYPE_INTERLACED;
			return macroblock_modes;

		case P_TYPE:
			macroblock_modes = UBITS(6);

			if (macroblock_modes == 0) return 0;   // error

			tab = MB_P + (macroblock_modes >> 1);
			DUMPBITS(tab->len);
			macroblock_modes = tab->modes;

			if (decoder.picture_structure != FRAME_PICTURE)
			{
				if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
					macroblock_modes |= GETBITS(2) * MOTION_TYPE_BASE;
			}
			else if (decoder.frame_pred_frame_dct)
			{
				if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
					macroblock_modes |= MC_FRAME;
			}
			else
			{
				if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
					macroblock_modes |= GETBITS(2) * MOTION_TYPE_BASE;

				if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))
					macroblock_modes |= GETBITS(1) * DCT_TYPE_INTERLACED;

			}
			return macroblock_modes;

		case B_TYPE:
			macroblock_modes = UBITS(6);

			if (macroblock_modes == 0) return 0;   // error

			tab = MB_B + macroblock_modes;
			DUMPBITS(tab->len);
			macroblock_modes = tab->modes;

			if (decoder.picture_structure != FRAME_PICTURE)
			{
				if (!(macroblock_modes & MACROBLOCK_INTRA))
					macroblock_modes |= GETBITS(2) * MOTION_TYPE_BASE;
			}
			else if (decoder.frame_pred_frame_dct)
				macroblock_modes |= MC_FRAME;
			else
			{
				if (macroblock_modes & MACROBLOCK_INTRA) goto intra;

				macroblock_modes |= GETBITS(2) * MOTION_TYPE_BASE;

				if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN))
intra:
					macroblock_modes |= GETBITS(1) * DCT_TYPE_INTERLACED;
			}
			return (macroblock_modes | (tab->len << 16));

		case D_TYPE:
			macroblock_modes = GETBITS(1);
			//I suspect (as this is actually a 2 bit command) that this should be getbits(2)
			//additionally, we arent dumping any bits here when i think we should be, need a game to test. (Refraction)
			if (macroblock_modes == 0) return 0;   // error
			return (MACROBLOCK_INTRA | (1 << 16));

		default:
			break;
	}

	return 0;
}

static __fi int get_quantizer_scale(void)
{
	int quantizer_scale_code = GETBITS(5);

	if (decoder.q_scale_type)
		return non_linear_quantizer_scale [quantizer_scale_code];
	return quantizer_scale_code << 1;
}

static __fi int get_coded_block_pattern(void)
{
	const CBPtab * tab;
	u16 code = UBITS(16);

	if (code >= 0x2000)
		tab = CBP_7 + (UBITS(7) - 16);
	else
		tab = CBP_9 + UBITS(9);

	DUMPBITS(tab->len);
	return tab->cbp;
}

int __fi get_motion_delta(const int f_code)
{
	int delta;
	int sign;
	const MVtab * tab;
	u16 code = UBITS(16);

	if ((code & 0x8000))
	{
		DUMPBITS(1);
		return 0x00010000;
	}
	else if ((code & 0xf000) || ((code & 0xfc00) == 0x0c00))
		tab = MV_4 + UBITS(4);
	else
		tab = MV_10 + UBITS(10);

	delta = tab->delta + 1;
	DUMPBITS(tab->len);

	sign = SBITS(1);
	DUMPBITS(1);

	return (((delta ^ sign) - sign) | (tab->len << 16));
}

int __fi get_dmv(void)
{
	const DMVtab* tab = DMV_2 + UBITS(2);
	DUMPBITS(tab->len);
	return (tab->dmv | (tab->len << 16));
}

int get_macroblock_address_increment(void)
{
	const MBAtab *mba;
	
	u16 code = UBITS(16);

	if (code >= 4096)
		mba = MBA.mba5 + (UBITS(5) - 2);
	else if (code >= 768)
		mba = MBA.mba11 + (UBITS(11) - 24);
	else switch (UBITS(11))
	{
		case 8:		/* macroblock_escape */
			DUMPBITS(11);
			return 0xb0023;

		case 15:	/* macroblock_stuffing (MPEG1 only) */
			if (decoder.mpeg1)
			{
				DUMPBITS(11);
				return 0xb0022;
			}
			// Fall through

		default:
			return 0;//error
	}

	DUMPBITS(mba->len);

	return ((mba->mba + 1) | (mba->len << 16));
}

static __fi int get_luma_dc_dct_diff(void)
{
	int size;
	int dc_diff;
	u16 code    = UBITS(5);

	if (code < 31)
	{
		size = DCtable.lum0[code].size;
		DUMPBITS(DCtable.lum0[code].len);
		// 5 bits max
	}
	else
	{
		code = UBITS(9) - 0x1f0;
		size = DCtable.lum1[code].size;
		DUMPBITS(DCtable.lum1[code].len);
		// 9 bits max
	}
	
	if (size == 0)
		return 0;

	dc_diff = GETBITS(size);

	// 6 for tab0 and 11 for tab1
	if ((dc_diff & (1<<(size-1)))==0)
		dc_diff-= (1<<size) - 1;

	return dc_diff;
}

static __fi int get_chroma_dc_dct_diff(void)
{
	int size;
	int dc_diff;
	u16 code = UBITS(5);

	if (code<31)
	{
		size = DCtable.chrom0[code].size;
		DUMPBITS(DCtable.chrom0[code].len);
	}
	else
	{
		code = UBITS(10) - 0x3e0;
		size = DCtable.chrom1[code].size;
		DUMPBITS(DCtable.chrom1[code].len);
	}

	if (size==0)
		return 0;

	dc_diff = GETBITS(size);

	if ((dc_diff & (1<<(size-1)))==0)
		dc_diff-= (1<<size) - 1;

	return dc_diff;
}

static __fi void SATURATE(int& val)
{
	if ((u32)(val + 2048) > 4095)
		val = (val >> 31) ^ 2047;
}

static bool get_intra_block(void)
{
	const u8 * scan = decoder.scantype ? mpeg2_scan.alt : mpeg2_scan.norm;
	const u8 (&quant_matrix)[64] = decoder.iq;
	int quantizer_scale = decoder.quantizer_scale;
	s16 * dest = decoder.DCTblock;
	u16 code; 

	/* decode AC coefficients */
	for (int i=1 + ipu_cmd.pos[4]; ; i++)
	{
		switch (ipu_cmd.pos[5])
		{
			case 0:
				if (!GETWORD())
				{
					ipu_cmd.pos[4] = i - 1;
					return false;
				}

				code = UBITS(16);

				if (code >= 16384 && (!decoder.intra_vlc_format || decoder.mpeg1))
				{
					tab = &DCT.next[(code >> 12) - 4];
				}
				else if (code >= 1024)
				{
					if (decoder.intra_vlc_format && !decoder.mpeg1)
					{
						tab = &DCT.tab0a[(code >> 8) - 4];
					}
					else
					{
						tab = &DCT.tab0[(code >> 8) - 4];
					}
				}
				else if (code >= 512)
				{
					if (decoder.intra_vlc_format && !decoder.mpeg1)
					{
						tab = &DCT.tab1a[(code >> 6) - 8];
					}
					else
					{
						tab = &DCT.tab1[(code >> 6) - 8];
					}
				}

				// [TODO] Optimization: Following codes can all be done by a single "expedited" lookup
				// that should use a single unrolled DCT table instead of five separate tables used
				// here.  Multiple conditional statements are very slow, while modern CPU data caches
				// have lots of room to spare.

				else if (code >= 256)
				{
					tab = &DCT.tab2[(code >> 4) - 16];
				}
				else if (code >= 128)
				{
					tab = &DCT.tab3[(code >> 3) - 16];
				}
				else if (code >= 64)
				{
					tab = &DCT.tab4[(code >> 2) - 16];
				}
				else if (code >= 32)
				{
					tab = &DCT.tab5[(code >> 1) - 16];
				}
				else if (code >= 16)
				{
					tab = &DCT.tab6[code - 16];
				}
				else
				{
					ipu_cmd.pos[4] = 0;
					return true;
				}

				DUMPBITS(tab->len);

				if (tab->run==64) /* end_of_block */
				{
					ipu_cmd.pos[4] = 0;
					return true;
				}

				i += (tab->run == 65) ? GETBITS(6) : tab->run;
				if (i >= 64)
				{
					ipu_cmd.pos[4] = 0;
					return true;
				}
				// Fall through

			case 1:
				{
					if (!GETWORD())
					{
						ipu_cmd.pos[4] = i - 1;
						ipu_cmd.pos[5] = 1;
						return false;
					}

					uint j = scan[i];
					int val;

					if (tab->run==65) /* escape */
					{
						if(!decoder.mpeg1)
						{
							val = (SBITS(12) * quantizer_scale * quant_matrix[i]) >> 4;
							DUMPBITS(12);
						}
						else
						{
							val = SBITS(8);
							DUMPBITS(8);

							if (!(val & 0x7f))
							{
								val = GETBITS(8) + 2 * val;
							}

							val = (val * quantizer_scale * quant_matrix[i]) >> 4;
							val = (val + ~ (((s32)val) >> 31)) | 1;
						}
					}
					else
					{
						val = (tab->level * quantizer_scale * quant_matrix[i]) >> 4;
						if(decoder.mpeg1)
						{
							/* oddification */
							val = (val - 1) | 1;
						}

						/* if (bitstream_get (1)) val = -val; */
						int bit1 = SBITS(1);
						val = (val ^ bit1) - bit1;
						DUMPBITS(1);
					}

					SATURATE(val);
					dest[j] = val;
					ipu_cmd.pos[5] = 0;
				}
		}
	}

  ipu_cmd.pos[4] = 0;
  return true;
}

static bool get_non_intra_block(int * last)
{
	int i;
	int j;
	int val;
	const u8 * scan = decoder.scantype ? mpeg2_scan.alt : mpeg2_scan.norm;
	const u8 (&quant_matrix)[64] = decoder.niq;
	int quantizer_scale = decoder.quantizer_scale;
	s16 * dest = decoder.DCTblock;
	u16 code;

	/* decode AC coefficients */
	for (i= ipu_cmd.pos[4] ; ; i++)
	{
		switch (ipu_cmd.pos[5])
		{
		case 0:
			if (!GETWORD())
			{
				ipu_cmd.pos[4] = i;
				return false;
			}

			code = UBITS(16);

			if (code >= 16384)
			{
				if (i==0)
					tab = &DCT.first[(code >> 12) - 4];
				else
					tab = &DCT.next[(code >> 12)- 4];
			}
			else if (code >= 1024)
				tab = &DCT.tab0[(code >> 8) - 4];
			else if (code >= 512)
				tab = &DCT.tab1[(code >> 6) - 8];

			// [TODO] Optimization: Following codes can all be done by a single "expedited" lookup
			// that should use a single unrolled DCT table instead of five separate tables used
			// here.  Multiple conditional statements are very slow, while modern CPU data caches
			// have lots of room to spare.

			else if (code >= 256)
				tab = &DCT.tab2[(code >> 4) - 16];
			else if (code >= 128)
				tab = &DCT.tab3[(code >> 3) - 16];
			else if (code >= 64)
				tab = &DCT.tab4[(code >> 2) - 16];
			else if (code >= 32)
				tab = &DCT.tab5[(code >> 1) - 16];
			else if (code >= 16)
				tab = &DCT.tab6[code - 16];
			else
			{
				ipu_cmd.pos[4] = 0;
				return true;
			}

			DUMPBITS(tab->len);

			if (tab->run==64) /* end_of_block */
			{
				*last = i;
				ipu_cmd.pos[4] = 0;
				return true;
			}

			i += (tab->run == 65) ? GETBITS(6) : tab->run;
			if (i >= 64)
			{
				*last = i;
				ipu_cmd.pos[4] = 0;
				return true;
			}
			// Fall through

		case 1:
			if (!GETWORD())
			{
			  ipu_cmd.pos[4] = i;
			  ipu_cmd.pos[5] = 1;
			  return false;
			}

			j = scan[i];

			if (tab->run==65) /* escape */
			{
				if (!decoder.mpeg1)
				{
					val = ((2 * (SBITS(12) + SBITS(1)) + 1) * quantizer_scale * quant_matrix[i]) >> 5;
					DUMPBITS(12);
				}
				else
				{
				  val = SBITS(8);
				  DUMPBITS(8);

				  if (!(val & 0x7f))
				  {
					val = GETBITS(8) + 2 * val;
				  }

				  val = ((2 * (val + (((s32)val) >> 31)) + 1) * quantizer_scale * quant_matrix[i]) / 32;
				  val = (val + ~ (((s32)val) >> 31)) | 1;
				}
			}
			else
			{
				int bit1 = SBITS(1);
				val = ((2 * tab->level + 1) * quantizer_scale * quant_matrix[i]) >> 5;
				val = (val ^ bit1) - bit1;
				DUMPBITS(1);
			}

			SATURATE(val);
			dest[j] = val;
			ipu_cmd.pos[5] = 0;
		}
	}

	ipu_cmd.pos[4] = 0;
	return true;
}

static __fi bool slice_intra_DCT(const int cc, u8 * const dest, const int stride, const bool skip)
{
	if (!skip || ipu_cmd.pos[3])
	{
		ipu_cmd.pos[3] = 0;
		if (!GETWORD())
		{
			ipu_cmd.pos[3] = 1;
			return false;
		}

		/* Get the intra DC coefficient and inverse quantize it */
		if (cc == 0)
			decoder.dc_dct_pred[0] += get_luma_dc_dct_diff();
		else
			decoder.dc_dct_pred[cc] += get_chroma_dc_dct_diff();

		decoder.DCTblock[0] = decoder.dc_dct_pred[cc] << (3 - decoder.intra_dc_precision);
	}

	if (!get_intra_block())
		return false;

	mpeg2_idct_copy(decoder.DCTblock, dest, stride);

	return true;
}

static __fi bool slice_non_intra_DCT(s16 * const dest, const int stride, const bool skip)
{
	int last;

	if (!skip)
		memzero_sse_a(decoder.DCTblock);

	if (!get_non_intra_block(&last))
		return false;

	mpeg2_idct_add(last, decoder.DCTblock, dest, stride);

	return true;
}

static u8 getBits8(u8 *address, bool advance)
{
	if (!g_BP.FillBuffer(8)) return 0;

	const u8* readpos = &g_BP.internal_qwc[0]._u8[g_BP.BP/8];

	if (uint shift = (g_BP.BP & 7))
	{
		uint mask = (0xff >> shift);
		*(u8*)address = (((~mask) & readpos[1]) >> (8 - shift)) | (((mask) & *readpos) << shift);
	}
	else
		*(u8*)address = *(u8*)readpos;

	if (advance) g_BP.Advance(8);

	return 1;
}

__fi bool mpeg2sliceIDEC(void)
{
	u16 code;

	switch (ipu_cmd.pos[0])
	{
	case 0:
		decoder.dc_dct_pred[0] =
		decoder.dc_dct_pred[1] =
		decoder.dc_dct_pred[2] = 128 << decoder.intra_dc_precision;

		ipuRegs.top = 0;
		ipuRegs.ctrl.ECD = 0;
		// Fall through

	case 1:
		ipu_cmd.pos[0] = 1;
		if (!bitstream_init())
			return false;
		// Fall through

	case 2:
		ipu_cmd.pos[0] = 2;
		for (;;)
		{
			// IPU0 isn't ready for data, so let's wait for it to be
			if (!ipu0ch.chcr.STR || ipuRegs.ctrl.OFC || ipu0ch.qwc == 0)
				return false;
			
			macroblock_8& mb8 = decoder.mb8;
			macroblock_rgb16& rgb16 = decoder.rgb16;
			macroblock_rgb32& rgb32 = decoder.rgb32;

			int DCT_offset, DCT_stride;
			const MBAtab * mba;

			switch (ipu_cmd.pos[1])
			{
			case 0:
				decoder.macroblock_modes = get_macroblock_modes();

				if (decoder.macroblock_modes & MACROBLOCK_QUANT) //only IDEC
					decoder.quantizer_scale = get_quantizer_scale();

				decoder.coded_block_pattern = 0x3F;//all 6 blocks
				memzero_sse_a(mb8);
				memzero_sse_a(rgb32);
				// Fall through

			case 1:
				ipu_cmd.pos[1] = 1;

				if (decoder.macroblock_modes & DCT_TYPE_INTERLACED)
				{
					DCT_offset = decoder_stride;
					DCT_stride = decoder_stride * 2;
				}
				else
				{
					DCT_offset = decoder_stride * 8;
					DCT_stride = decoder_stride;
				}

				switch (ipu_cmd.pos[2])
				{
				case 0:
				case 1:
					if (!slice_intra_DCT(0, (u8*)mb8.Y, DCT_stride, ipu_cmd.pos[2] == 1))
					{
						ipu_cmd.pos[2] = 1;
						return false;
					}
					// Fall through

				case 2:
					if (!slice_intra_DCT(0, (u8*)mb8.Y + 8, DCT_stride, ipu_cmd.pos[2] == 2))
					{
						ipu_cmd.pos[2] = 2;
						return false;
					}
					// Fall through

				case 3:
					if (!slice_intra_DCT(0, (u8*)mb8.Y + DCT_offset, DCT_stride, ipu_cmd.pos[2] == 3))
					{
						ipu_cmd.pos[2] = 3;
						return false;
					}
					// Fall through

				case 4:
					if (!slice_intra_DCT(0, (u8*)mb8.Y + DCT_offset + 8, DCT_stride, ipu_cmd.pos[2] == 4))
					{
						ipu_cmd.pos[2] = 4;
						return false;
					}
					// Fall through

				case 5:
					if (!slice_intra_DCT(1, (u8*)mb8.Cb, decoder_stride >> 1, ipu_cmd.pos[2] == 5))
					{
						ipu_cmd.pos[2] = 5;
						return false;
					}
					// Fall through

				case 6:
					if (!slice_intra_DCT(2, (u8*)mb8.Cr, decoder_stride >> 1, ipu_cmd.pos[2] == 6))
					{
						ipu_cmd.pos[2] = 6;
						return false;
					}
					break;

				default:
					break;
				}

				// Send The MacroBlock via DmaIpuFrom
				ipu_csc(mb8, rgb32, decoder.sgn);

				if (decoder.ofm == 0)
					decoder.SetOutputTo(rgb32);
				else
				{
					ipu_dither(rgb32, rgb16, decoder.dte);
					decoder.SetOutputTo(rgb16);
				}
				// Fall through

			case 2:
			{
				uint read = ipu_fifo.out.write((u32*)decoder.GetIpuDataPtr(), decoder.ipu0_data);
				decoder.AdvanceIpuDataBy(read);

				if (decoder.ipu0_data != 0)
				{
					// IPU FIFO filled up -- Will have to finish transferring later.
					ipu_cmd.pos[1] = 2;
					return false;
				}
				mba_count = 0;
			}
			// Fall through

			case 3:
				for (;;)
				{
					if (!GETWORD())
					{
						ipu_cmd.pos[1] = 3;
						return false;
					}

					code = UBITS(16);
					if (code >= 0x1000)
					{
						mba = MBA.mba5 + (UBITS(5) - 2);
						break;
					}
					else if (code >= 0x0300)
					{
						mba = MBA.mba11 + (UBITS(11) - 24);
						break;
					}
					else switch (UBITS(11))
					{
						case 8:		/* macroblock_escape */
							mba_count += 33;
							// Fall through

						case 15:	/* macroblock_stuffing (MPEG1 only) */
							DUMPBITS(11);
							continue;

						default:	/* end of slice/frame, or error? */
						{
							goto finish_idec;	
						}
					}
				}

				DUMPBITS(mba->len);
				mba_count += mba->mba;

				if (mba_count)
				{
					decoder.dc_dct_pred[0] =
					decoder.dc_dct_pred[1] =
					decoder.dc_dct_pred[2] = 128 << decoder.intra_dc_precision;
				}
				// Fall through

			case 4:
				if (!GETWORD())
				{
					ipu_cmd.pos[1] = 4;
					return false;
				}
				break;

			default:
				break;
			}

			ipu_cmd.pos[1] = 0;
			ipu_cmd.pos[2] = 0;
		}
		// Fall through

finish_idec:
		ipuRegs.ctrl.SCD = 0;
		coded_block_pattern = decoder.coded_block_pattern;
		// Fall through

	case 3:
	{
		u8 bit8;
		if (!getBits8((u8*)&bit8, 0))
		{
			ipu_cmd.pos[0] = 3;
			return false;
		}

		if (bit8 == 0)
		{
			g_BP.Align();
			ipuRegs.ctrl.SCD = 1;
		}
	}
	// Fall through

	case 4:
		if (!getBits32((u8*)&ipuRegs.top, 0))
		{
			ipu_cmd.pos[0] = 4;
			return false;
		}

#ifndef MSB_FIRST
		ipuRegs.top = BigEndian(ipuRegs.top);
#endif
		break;

	default:
		break;
	}

	return true;
}

__fi bool mpeg2_slice(void)
{
	int DCT_offset, DCT_stride;

	macroblock_8& mb8 = decoder.mb8;
	macroblock_16& mb16 = decoder.mb16;

	switch (ipu_cmd.pos[0])
	{
	case 0:
		if (decoder.dcr)
		{
			decoder.dc_dct_pred[0] =
			decoder.dc_dct_pred[1] =
			decoder.dc_dct_pred[2] = 128 << decoder.intra_dc_precision;
		}
			
		ipuRegs.ctrl.ECD = 0;
		ipuRegs.top = 0;
		memzero_sse_a(mb8);
		memzero_sse_a(mb16);
		// Fall through 

	case 1:
		if (!bitstream_init())
		{
			ipu_cmd.pos[0] = 1;
			return false;
		}
		// Fall through

	case 2:
		ipu_cmd.pos[0] = 2;

		if (decoder.macroblock_modes & DCT_TYPE_INTERLACED)
		{
			DCT_offset = decoder_stride;
			DCT_stride = decoder_stride * 2;
		}
		else
		{
			DCT_offset = decoder_stride * 8;
			DCT_stride = decoder_stride;
		}

		if (decoder.macroblock_modes & MACROBLOCK_INTRA)
		{
			switch(ipu_cmd.pos[1])
			{
			case 0:
				decoder.coded_block_pattern = 0x3F;
				// Fall through

			case 1:
				if (!slice_intra_DCT(0, (u8*)mb8.Y, DCT_stride, ipu_cmd.pos[1] == 1))
				{
					ipu_cmd.pos[1] = 1;
					return false;
				}
				// Fall through

			case 2:
				if (!slice_intra_DCT(0, (u8*)mb8.Y + 8, DCT_stride, ipu_cmd.pos[1] == 2))
				{
					ipu_cmd.pos[1] = 2;
					return false;
				}
				// Fall through

			case 3:
				if (!slice_intra_DCT(0, (u8*)mb8.Y + DCT_offset, DCT_stride, ipu_cmd.pos[1] == 3))
				{
					ipu_cmd.pos[1] = 3;
					return false;
				}
				// Fall through

			case 4:
				if (!slice_intra_DCT(0, (u8*)mb8.Y + DCT_offset + 8, DCT_stride, ipu_cmd.pos[1] == 4))
				{
					ipu_cmd.pos[1] = 4;
					return false;
				}
				// Fall through

			case 5:
				if (!slice_intra_DCT(1, (u8*)mb8.Cb, decoder_stride >> 1, ipu_cmd.pos[1] == 5))
				{
					ipu_cmd.pos[1] = 5;
					return false;
				}
				// Fall through

			case 6:
				if (!slice_intra_DCT(2, (u8*)mb8.Cr, decoder_stride >> 1, ipu_cmd.pos[1] == 6))
				{
					ipu_cmd.pos[1] = 6;
					return false;
				}
				break;

			default:
				break;
			}

			// Copy macroblock8 to macroblock16 - without sign extension.
			// Manually inlined due to MSVC refusing to inline the SSE-optimized version.
			{
				const u8	*s = (const u8*)&mb8;
				u16			*d = (u16*)&mb16;

				//Y  bias	- 16 * 16
				//Cr bias	- 8 * 8
				//Cb bias	- 8 * 8

				__m128i zeroreg = _mm_setzero_si128();

				for (uint i = 0; i < (256+64+64) / 32; ++i)
				{
					//*d++ = *s++;
					__m128i woot1 = _mm_load_si128((__m128i*)s);
					__m128i woot2 = _mm_load_si128((__m128i*)s+1);
					_mm_store_si128((__m128i*)d,	_mm_unpacklo_epi8(woot1, zeroreg));
					_mm_store_si128((__m128i*)d+1,	_mm_unpackhi_epi8(woot1, zeroreg));
					_mm_store_si128((__m128i*)d+2,	_mm_unpacklo_epi8(woot2, zeroreg));
					_mm_store_si128((__m128i*)d+3,	_mm_unpackhi_epi8(woot2, zeroreg));
					s += 32;
					d += 32;
				}
			}
		}
		else
		{
			if (decoder.macroblock_modes & MACROBLOCK_PATTERN)
			{
				switch(ipu_cmd.pos[1])
				{
				case 0:
					decoder.coded_block_pattern = get_coded_block_pattern();  // max 9bits
					// Fall through

				case 1:
					if (decoder.coded_block_pattern & 0x20)
					{
						if (!slice_non_intra_DCT((s16*)mb16.Y, DCT_stride, ipu_cmd.pos[1] == 1))
						{
							ipu_cmd.pos[1] = 1;
							return false;
						}
					}
					// Fall through

				case 2:
					if (decoder.coded_block_pattern & 0x10)
					{
						if (!slice_non_intra_DCT((s16*)mb16.Y + 8, DCT_stride, ipu_cmd.pos[1] == 2))
						{
							ipu_cmd.pos[1] = 2;
							return false;
						}
					}
					// Fall through
					
				case 3:
					if (decoder.coded_block_pattern & 0x08)
					{
						if (!slice_non_intra_DCT((s16*)mb16.Y + DCT_offset, DCT_stride, ipu_cmd.pos[1] == 3))
						{
							ipu_cmd.pos[1] = 3;
							return false;
						}
					}
					// Fall through
					
				case 4:
					if (decoder.coded_block_pattern & 0x04)
					{
						if (!slice_non_intra_DCT((s16*)mb16.Y + DCT_offset + 8, DCT_stride, ipu_cmd.pos[1] == 4))
						{
							ipu_cmd.pos[1] = 4;
							return false;
						}
					}
					// Fall through
					
				case 5:
					if (decoder.coded_block_pattern & 0x2)
					{
						if (!slice_non_intra_DCT((s16*)mb16.Cb, decoder_stride >> 1, ipu_cmd.pos[1] == 5))
						{
							ipu_cmd.pos[1] = 5;
							return false;
						}
					}
					// Fall through
					
				case 6:
					if (decoder.coded_block_pattern & 0x1)
					{
						if (!slice_non_intra_DCT((s16*)mb16.Cr, decoder_stride >> 1, ipu_cmd.pos[1] == 6))
						{
							ipu_cmd.pos[1] = 6;
							return false;
						}
					}
					break;

				default:
					break;
				}
			}
		}

		// Send The MacroBlock via DmaIpuFrom
		ipuRegs.ctrl.SCD = 0;
		coded_block_pattern = decoder.coded_block_pattern;

		decoder.SetOutputTo(mb16);
		// Fall through

	case 3:
	{
		// IPU0 isn't ready for data, so let's wait for it to be
		if (!ipu0ch.chcr.STR || ipuRegs.ctrl.OFC || ipu0ch.qwc == 0)
		{
			ipu_cmd.pos[0] = 3;
			return false;
		}

		uint read = ipu_fifo.out.write((u32*)decoder.GetIpuDataPtr(), decoder.ipu0_data);
		decoder.AdvanceIpuDataBy(read);

		if (decoder.ipu0_data != 0)
		{
			// IPU FIFO filled up -- Will have to finish transferring later.
			ipu_cmd.pos[0] = 3;
			return false;
		}

		mba_count = 0;
	}
	// Fall through
	
	case 4:
	{
		u8 bit8;
		if (!getBits8((u8*)&bit8, 0))
		{
			ipu_cmd.pos[0] = 4;
			return false;
		}

		if (bit8 == 0)
		{
			g_BP.Align();
			ipuRegs.ctrl.SCD = 1;
		}
	}
	// Fall through
	
	case 5:
		if (!getBits32((u8*)&ipuRegs.top, 0))
		{
			ipu_cmd.pos[0] = 5;
			return false;
		}

#ifndef MSB_FIRST
		ipuRegs.top = BigEndian(ipuRegs.top);
#endif
		break;
	}

	return true;
}
