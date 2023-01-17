/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "Pcsx2Types.h"

#include <array>

#include "GSFastList.h"
#include "GSTexture.h"
#include "GSVertex.h"
#include "../../GSAlignedClass.h"

enum ShaderConvert
{
	ShaderConvert_COPY = 0,
	ShaderConvert_RGBA8_TO_16_BITS,
	ShaderConvert_DATM_1,
	ShaderConvert_DATM_0,
	ShaderConvert_MOD_256,
	ShaderConvert_SCANLINE = 5,
	ShaderConvert_DIAGONAL_FILTER,
	ShaderConvert_TRANSPARENCY_FILTER,
	ShaderConvert_TRIANGULAR_FILTER,
	ShaderConvert_COMPLEX_FILTER,
	ShaderConvert_FLOAT32_TO_32_BITS = 10,
	ShaderConvert_FLOAT32_TO_RGBA8,
	ShaderConvert_FLOAT16_TO_RGB5A1,
	ShaderConvert_RGBA8_TO_FLOAT32 = 13,
	ShaderConvert_RGBA8_TO_FLOAT24,
	ShaderConvert_RGBA8_TO_FLOAT16,
	ShaderConvert_RGB5A1_TO_FLOAT16,
	ShaderConvert_RGBA_TO_8I = 17,
	ShaderConvert_YUV,
	ShaderConvert_Count
};

enum ChannelFetch
{
	ChannelFetch_NONE  = 0,
	ChannelFetch_RED   = 1,
	ChannelFetch_GREEN = 2,
	ChannelFetch_BLUE  = 3,
	ChannelFetch_ALPHA = 4,
	ChannelFetch_RGB   = 5,
	ChannelFetch_GXBY  = 6,
};

#pragma pack(push, 1)

class MergeConstantBuffer
{
public:
	GSVector4 BGColor;

	MergeConstantBuffer() {memset(this, 0, sizeof(*this));}
};

class InterlaceConstantBuffer
{
public:
	GSVector2 ZrH;
	float hH;
	float _pad[1];

	InterlaceConstantBuffer() {memset(this, 0, sizeof(*this));}
};

class FXAAConstantBuffer
{
public:
	GSVector4 rcpFrame;
	GSVector4 rcpFrameOpt;

	FXAAConstantBuffer() {memset(this, 0, sizeof(*this));}
};

#pragma pack(pop)

enum HWBlendFlags
{
	// A couple of flag to determine the blending behavior
	BLEND_MIX1   = 0x20,  // Mix of hw and sw, do Cs*F or Cs*As in shader
	BLEND_MIX2   = 0x40,  // Mix of hw and sw, do Cs*(As + 1) or Cs*(F + 1) in shader
	BLEND_MIX3   = 0x80,  // Mix of hw and sw, do Cs*(1 - As) or Cs*(1 - F) in shader
	BLEND_A_MAX  = 0x100, // Impossible blending uses coeff bigger than 1
	BLEND_C_CLR  = 0x200, // Clear color blending (use directly the destination color as blending factor)
	BLEND_NO_REC = 0x400, // Doesn't require sampling of the RT as a texture
	BLEND_ACCU   = 0x800, // Allow to use a mix of SW and HW blending to keep the best of the 2 worlds
};

// Determines the HW blend function for DX11/OGL
struct HWBlend { u16 flags, op, src, dst; };

class GSDevice : public GSAlignedClass<32>
{
private:
	FastList<GSTexture*> m_pool;
	static std::array<HWBlend, 3*3*3*3 + 1> m_blendMap;

protected:
	enum : u16
	{
		// HW blend factors
		SRC_COLOR,   INV_SRC_COLOR,    DST_COLOR,  INV_DST_COLOR,
		SRC1_COLOR,  INV_SRC1_COLOR,   SRC_ALPHA,  INV_SRC_ALPHA,
		DST_ALPHA,   INV_DST_ALPHA,    SRC1_ALPHA, INV_SRC1_ALPHA,
		CONST_COLOR, INV_CONST_COLOR,  CONST_ONE,  CONST_ZERO,

		// HW blend operations
		OP_ADD, OP_SUBTRACT, OP_REV_SUBTRACT
	};

	static const int m_NO_BLEND     = 0;
	static const int m_MERGE_BLEND  = m_blendMap.size() - 1;

	GSTexture* m_backbuffer;
	GSTexture* m_merge;
	GSTexture* m_weavebob;
	GSTexture* m_blend;
	GSTexture* m_target_tmp;
	GSTexture* m_current;
	struct {size_t stride, start, count, limit;} m_vertex;
	struct {size_t start, count, limit;} m_index;
	unsigned int m_frame; // for ageing the pool
	bool m_linear_present;

	virtual GSTexture* CreateSurface(int type, int w, int h, int format) = 0;
	virtual GSTexture* FetchSurface(int type, int w, int h, int format);

	virtual void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c) = 0;
	virtual void DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset) = 0;
	virtual void DoFXAA(GSTexture* sTex, GSTexture* dTex) {}
	virtual u16 ConvertBlendEnum(u16 generic) = 0; // Convert blend factors/ops from the generic enum to DX11/OGl specific.

public:
	GSDevice();
	virtual ~GSDevice();

	void Recycle(GSTexture* t);

	virtual bool Create();
	virtual bool Reset(int w, int h);
	virtual void Present(const GSVector4i& r, int shader);
	virtual void Flip() {  }

	virtual void EndScene();

	virtual void ClearRenderTarget(GSTexture* t, const GSVector4& c) {}
	virtual void ClearRenderTarget(GSTexture* t, u32 c) {}
	virtual void ClearDepth(GSTexture* t) {}
	virtual void ClearStencil(GSTexture* t, u8 c) {}

	GSTexture* CreateRenderTarget(int w, int h, int format = 0);
	GSTexture* CreateDepthStencil(int w, int h, int format = 0);
	GSTexture* CreateTexture(int w, int h, int format = 0);
	GSTexture* CreateOffscreen(int w, int h, int format = 0);

	virtual GSTexture* CopyOffscreen(GSTexture* src, const GSVector4& sRect, int w, int h, int format = 0, int ps_shader = 0) {return NULL;}

	virtual void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r) {}
	virtual void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, int shader = 0, bool linear = true) {}
	virtual void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha) {}

	void StretchRect(GSTexture* sTex, GSTexture* dTex, const GSVector4& dRect, int shader = 0, bool linear = true);

	void Merge(GSTexture* sTex[3], GSVector4* sRect, GSVector4* dRect, const GSVector2i& fs, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c);
	void Interlace(const GSVector2i& ds, int field, int mode, float yoffset);
	void FXAA();

	bool ResizeTexture(GSTexture** t, int type, int w, int h);
	bool ResizeTexture(GSTexture** t, int w, int h);
	bool ResizeTarget(GSTexture** t, int w, int h);
	bool ResizeTarget(GSTexture** t);

	void AgePool();
	void PurgePool();

	// Convert the GS blend equations to HW specific blend factors/ops
	// Index is computed as ((((A * 3 + B) * 3) + C) * 3) + D. A, B, C, D taken from ALPHA register.
	HWBlend GetBlend(size_t index);
	u16 GetBlendFlags(size_t index);
};
