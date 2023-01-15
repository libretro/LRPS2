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

#include "Pcsx2Types.h"

#include "GSDevice11.h"
#include "../../GSTables.h"

/* TFX shader */
const char tfx_shader_raw[] =
			"#ifdef SHADER_MODEL // make safe to include in resource file to enforce dependency\n"
			"\n"
			"#define FMT_32 0\n"
			"#define FMT_24 1\n"
			"#define FMT_16 2\n"
			"\n"
			"#ifndef VS_TME\n"
			"#define VS_TME 1\n"
			"#define VS_FST 1\n"
			"#endif\n"
			"\n"
			"#ifndef GS_IIP\n"
			"#define GS_IIP 0\n"
			"#define GS_PRIM 3\n"
			"#define GS_POINT 0\n"
			"#define GS_LINE 0\n"
			"#endif\n"
			"\n"
			"#ifndef PS_FST\n"
			"#define PS_FST 0\n"
			"#define PS_WMS 0\n"
			"#define PS_WMT 0\n"
			"#define PS_FMT FMT_32\n"
			"#define PS_AEM 0\n"
			"#define PS_TFX 0\n"
			"#define PS_TCC 1\n"
			"#define PS_ATST 0\n"
			"#define PS_FOG 0\n"
			"#define PS_CLR1 0\n"
			"#define PS_FBA 0\n"
			"#define PS_FBMASK 0\n"
			"#define PS_LTF 1\n"
			"#define PS_TCOFFSETHACK 0\n"
			"#define PS_POINT_SAMPLER 0\n"
			"#define PS_SHUFFLE 0\n"
			"#define PS_READ_BA 0\n"
			"#define PS_DFMT 0\n"
			"#define PS_DEPTH_FMT 0\n"
			"#define PS_PAL_FMT 0\n"
			"#define PS_CHANNEL_FETCH 0\n"
			"#define PS_TALES_OF_ABYSS_HLE 0\n"
			"#define PS_URBAN_CHAOS_HLE 0\n"
			"#define PS_INVALID_TEX0 0\n"
			"#define PS_SCALE_FACTOR 1\n"
			"#define PS_HDR 0\n"
			"#define PS_COLCLIP 0\n"
			"#define PS_BLEND_A 0\n"
			"#define PS_BLEND_B 0\n"
			"#define PS_BLEND_C 0\n"
			"#define PS_BLEND_D 0\n"
			"#define PS_PABE 0\n"
			"#define PS_DITHER 0\n"
			"#define PS_ZCLAMP 0\n"
			"#endif\n"
			"\n"
			"#define SW_BLEND (PS_BLEND_A || PS_BLEND_B || PS_BLEND_D)\n"
			"#define PS_AEM_FMT (PS_FMT & 3)\n"
			"\n"
			"struct VS_INPUT\n"
			"{\n"
			"	float2 st : TEXCOORD0;\n"
			"	uint4 c : COLOR0;\n"
			"	float q : TEXCOORD1;\n"
			"	uint2 p : POSITION0;\n"
			"	uint z : POSITION1;\n"
			"	uint2 uv : TEXCOORD2;\n"
			"	float4 f : COLOR1;\n"
			"};\n"
			"\n"
			"struct VS_OUTPUT\n"
			"{\n"
			"	float4 p : SV_Position;\n"
			"	float4 t : TEXCOORD0;\n"
			"	float4 ti : TEXCOORD2;\n"
			"	float4 c : COLOR0;\n"
			"};\n"
			"\n"
			"struct PS_INPUT\n"
			"{\n"
			"	float4 p : SV_Position;\n"
			"	float4 t : TEXCOORD0;\n"
			"	float4 ti : TEXCOORD2;\n"
			"	float4 c : COLOR0;\n"
			"};\n"
			"\n"
			"struct PS_OUTPUT\n"
			"{\n"
			"	float4 c0 : SV_Target0;\n"
			"	float4 c1 : SV_Target1;\n"
			"#if PS_ZCLAMP\n"
			"	float depth : SV_Depth;\n"
			"#endif\n"
			"};\n"
			"\n"
			"Texture2D<float4> Texture : register(t0);\n"
			"Texture2D<float4> Palette : register(t1);\n"
			"Texture2D<float4> RtSampler : register(t3);\n"
			"Texture2D<float4> RawTexture : register(t4);\n"
			"SamplerState TextureSampler : register(s0);\n"
			"SamplerState PaletteSampler : register(s1);\n"
			"\n"
			"cbuffer cb0\n"
			"{\n"
			"	float4 VertexScale;\n"
			"	float4 VertexOffset;\n"
			"	float4 Texture_Scale_Offset;\n"
			"	uint MaxDepth;\n"
			"	uint3 pad_cb0;\n"
			"};\n"
			"\n"
			"cbuffer cb1\n"
			"{\n"
			"	float3 FogColor;\n"
			"	float AREF;\n"
			"	float4 HalfTexel;\n"
			"	float4 WH;\n"
			"	float4 MinMax;\n"
			"	float2 MinF;\n"
			"	float2 TA;\n"
			"	uint4 MskFix;\n"
			"	int4 ChannelShuffle;\n"
			"	uint4 FbMask;\n"
			"	float4 TC_OffsetHack;\n"
			"	float Af;\n"
			"	float MaxDepthPS;\n"
			"	float2 pad_cb1;\n"
			"	float4x4 DitherMatrix;\n"
			"};\n"
			"\n"
			"cbuffer cb2\n"
			"{\n"
			"	float2 PointSize;\n"
			"};\n"
			"\n"
			"float4 sample_c(float2 uv)\n"
			"{\n"
			"	if (PS_POINT_SAMPLER)\n"
			"	{\n"
			"		// Weird issue with ATI/AMD cards,\n"
			"		// it looks like they add 127/128 of a texel to sampling coordinates\n"
			"		// occasionally causing point sampling to erroneously round up.\n"
			"		// I'm manually adjusting coordinates to the centre of texels here,\n"
			"		// though the centre is just paranoia, the top left corner works fine.\n"
			"		// As of 2018 this issue is still present.\n"
			"		uv = (trunc(uv * WH.zw) + float2(0.5, 0.5)) / WH.zw;\n"
			"	}\n"
			"	return Texture.Sample(TextureSampler, uv);\n"
			"}\n"
			"\n"
			"float4 sample_p(float u)\n"
			"{\n"
			"	return Palette.Sample(PaletteSampler, u);\n"
			"}\n"
			"\n"
			"float4 clamp_wrap_uv(float4 uv)\n"
			"{\n"
			"	float4 tex_size;\n"
			"\n"
			"	if (PS_INVALID_TEX0 == 1)\n"
			"		tex_size = WH.zwzw;\n"
			"	else\n"
			"		tex_size = WH.xyxy;\n"
			"\n"
			"	if(PS_WMS == PS_WMT)\n"
			"	{\n"
			"		if(PS_WMS == 2)\n"
			"		{\n"
			"			uv = clamp(uv, MinMax.xyxy, MinMax.zwzw);\n"
			"		}\n"
			"		else if(PS_WMS == 3)\n"
			"		{\n"
			"			#if PS_FST == 0\n"
			"			// wrap negative uv coords to avoid an off by one error that shifted\n"
			"			// textures. Fixes Xenosaga's hair issue.\n"
			"			uv = frac(uv);\n"
			"			#endif\n"
			"			uv = (float4)(((uint4)(uv * tex_size) & MskFix.xyxy) | MskFix.zwzw) / tex_size;\n"
			"		}\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		if(PS_WMS == 2)\n"
			"		{\n"
			"			uv.xz = clamp(uv.xz, MinMax.xx, MinMax.zz);\n"
			"		}\n"
			"		else if(PS_WMS == 3)\n"
			"		{\n"
			"			#if PS_FST == 0\n"
			"			uv.xz = frac(uv.xz);\n"
			"			#endif\n"
			"			uv.xz = (float2)(((uint2)(uv.xz * tex_size.xx) & MskFix.xx) | MskFix.zz) / tex_size.xx;\n"
			"		}\n"
			"		if(PS_WMT == 2)\n"
			"		{\n"
			"			uv.yw = clamp(uv.yw, MinMax.yy, MinMax.ww);\n"
			"		}\n"
			"		else if(PS_WMT == 3)\n"
			"		{\n"
			"			#if PS_FST == 0\n"
			"			uv.yw = frac(uv.yw);\n"
			"			#endif\n"
			"			uv.yw = (float2)(((uint2)(uv.yw * tex_size.yy) & MskFix.yy) | MskFix.ww) / tex_size.yy;\n"
			"		}\n"
			"	}\n"
			"\n"
			"	return uv;\n"
			"}\n"
			"\n"
			"float4x4 sample_4c(float4 uv)\n"
			"{\n"
			"	float4x4 c;\n"
			"\n"
			"	c[0] = sample_c(uv.xy);\n"
			"	c[1] = sample_c(uv.zy);\n"
			"	c[2] = sample_c(uv.xw);\n"
			"	c[3] = sample_c(uv.zw);\n"
			"\n"
			"	return c;\n"
			"}\n"
			"\n"
			"float4 sample_4_index(float4 uv)\n"
			"{\n"
			"	float4 c;\n"
			"\n"
			"	c.x = sample_c(uv.xy).a;\n"
			"	c.y = sample_c(uv.zy).a;\n"
			"	c.z = sample_c(uv.xw).a;\n"
			"	c.w = sample_c(uv.zw).a;\n"
			"\n"
			"	// Denormalize value\n"
			"	uint4 i = uint4(c * 255.0f + 0.5f);\n"
			"\n"
			"	if (PS_PAL_FMT == 1)\n"
			"	{\n"
			"		// 4HL\n"
			"		c = float4(i & 0xFu) / 255.0f;\n"
			"	}\n"
			"	else if (PS_PAL_FMT == 2)\n"
			"	{\n"
			"		// 4HH\n"
			"		c = float4(i >> 4u) / 255.0f;\n"
			"	}\n"
			"\n"
			"	// Most of texture will hit this code so keep normalized float value\n"
			"	// 8 bits\n"
			"	return c * 255./256 + 0.5/256;\n"
			"}\n"
			"\n"
			"float4x4 sample_4p(float4 u)\n"
			"{\n"
			"	float4x4 c;\n"
			"\n"
			"	c[0] = sample_p(u.x);\n"
			"	c[1] = sample_p(u.y);\n"
			"	c[2] = sample_p(u.z);\n"
			"	c[3] = sample_p(u.w);\n"
			"\n"
			"	return c;\n"
			"}\n"
			"\n"
			"int fetch_raw_depth(int2 xy)\n"
			"{\n"
			"	float4 col = RawTexture.Load(int3(xy, 0));\n"
			"	return (int)(col.r * exp2(32.0f));\n"
			"}\n"
			"\n"
			"float4 fetch_raw_color(int2 xy)\n"
			"{\n"
			"	return RawTexture.Load(int3(xy, 0));\n"
			"}\n"
			"\n"
			"float4 fetch_c(int2 uv)\n"
			"{\n"
			"	return Texture.Load(int3(uv, 0));\n"
			"}\n"
			"\n"
			"//////////////////////////////////////////////////////////////////////\n"
			"// Depth sampling\n"
			"//////////////////////////////////////////////////////////////////////\n"
			"\n"
			"int2 clamp_wrap_uv_depth(int2 uv)\n"
			"{\n"
			"	int4 mask = (int4)MskFix << 4;\n"
			"	if (PS_WMS == PS_WMT)\n"
			"	{\n"
			"		if (PS_WMS == 2)\n"
			"		{\n"
			"			uv = clamp(uv, mask.xy, mask.zw);\n"
			"		}\n"
			"		else if (PS_WMS == 3)\n"
			"		{\n"
			"			uv = (uv & mask.xy) | mask.zw;\n"
			"		}\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		if (PS_WMS == 2)\n"
			"		{\n"
			"			uv.x = clamp(uv.x, mask.x, mask.z);\n"
			"		}\n"
			"		else if (PS_WMS == 3)\n"
			"		{\n"
			"			uv.x = (uv.x & mask.x) | mask.z;\n"
			"		}\n"
			"		if (PS_WMT == 2)\n"
			"		{\n"
			"			uv.y = clamp(uv.y, mask.y, mask.w);\n"
			"		}\n"
			"		else if (PS_WMT == 3)\n"
			"		{\n"
			"			uv.y = (uv.y & mask.y) | mask.w;\n"
			"		}\n"
			"	}\n"
			"	return uv;\n"
			"}\n"
			"\n"
			"float4 sample_depth(float2 st, float2 pos)\n"
			"{\n"
			"	float2 uv_f = (float2)clamp_wrap_uv_depth(int2(st)) * (float2)PS_SCALE_FACTOR * (float2)(1.0f / 16.0f);\n"
			"	int2 uv = (int2)uv_f;\n"
			"\n"
			"	float4 t = (float4)(0.0f);\n"
			"\n"
			"	if (PS_TALES_OF_ABYSS_HLE == 1)\n"
			"	{\n"
			"		// Warning: UV can't be used in channel effect\n"
			"		int depth = fetch_raw_depth(pos);\n"
			"\n"
			"		// Convert msb based on the palette\n"
			"		t = Palette.Load(int3((depth >> 8) & 0xFF, 0, 0)) * 255.0f;\n"
			"	}\n"
			"	else if (PS_URBAN_CHAOS_HLE == 1)\n"
			"	{\n"
			"		// Depth buffer is read as a RGB5A1 texture. The game try to extract the green channel.\n"
			"		// So it will do a first channel trick to extract lsb, value is right-shifted.\n"
			"		// Then a new channel trick to extract msb which will shifted to the left.\n"
			"		// OpenGL uses a FLOAT32 format for the depth so it requires a couple of conversion.\n"
			"		// To be faster both steps (msb&lsb) are done in a single pass.\n"
			"\n"
			"		// Warning: UV can't be used in channel effect\n"
			"		int depth = fetch_raw_depth(pos);\n"
			"\n"
			"		// Convert lsb based on the palette\n"
			"		t = Palette.Load(int3(depth & 0xFF, 0, 0)) * 255.0f;\n"
			"\n"
			"		// Msb is easier\n"
			"		float green = (float)((depth >> 8) & 0xFF) * 36.0f;\n"
			"		green = min(green, 255.0f);\n"
			"		t.g += green;\n"
			"	}\n"
			"	else if (PS_DEPTH_FMT == 1)\n"
			"	{\n"
			"		// Based on ps_main11 of convert\n"
			"\n"
			"		// Convert a FLOAT32 depth texture into a RGBA color texture\n"
			"		const float4 bitSh = float4(exp2(24.0f), exp2(16.0f), exp2(8.0f), exp2(0.0f));\n"
			"		const float4 bitMsk = float4(0.0, 1.0f / 256.0f, 1.0f / 256.0f, 1.0f / 256.0f);\n"
			"\n"
			"		float4 res = frac((float4)fetch_c(uv).r * bitSh);\n"
			"\n"
			"		t = (res - res.xxyz * bitMsk) * 256.0f;\n"
			"	}\n"
			"	else if (PS_DEPTH_FMT == 2)\n"
			"	{\n"
			"		// Based on ps_main12 of convert\n"
			"\n"
			"		// Convert a FLOAT32 (only 16 lsb) depth into a RGB5A1 color texture\n"
			"		const float4 bitSh = float4(exp2(32.0f), exp2(27.0f), exp2(22.0f), exp2(17.0f));\n"
			"		const uint4 bitMsk = uint4(0x1F, 0x1F, 0x1F, 0x1);\n"
			"		uint4 color = (uint4)((float4)fetch_c(uv).r * bitSh) & bitMsk;\n"
			"\n"
			"		t = (float4)color * float4(8.0f, 8.0f, 8.0f, 128.0f);\n"
			"	}\n"
			"	else if (PS_DEPTH_FMT == 3)\n"
			"	{\n"
			"		// Convert a RGBA/RGB5A1 color texture into a RGBA/RGB5A1 color texture\n"
			"		t = fetch_c(uv) * 255.0f;\n"
			"	}\n"
			"\n"
			"	if (PS_AEM_FMT == FMT_24)\n"
			"	{\n"
			"		t.a = ((PS_AEM == 0) || any(bool3(t.rgb))) ? 255.0f * TA.x : 0.0f;\n"
			"	}\n"
			"	else if (PS_AEM_FMT == FMT_16)\n"
			"	{\n"
			"		t.a = t.a >= 128.0f ? 255.0f * TA.y : ((PS_AEM == 0) || any(bool3(t.rgb))) ? 255.0f * TA.x : 0.0f;\n"
			"	}\n"
			"\n"
			"	return t;\n"
			"}\n"
			"\n"
			"//////////////////////////////////////////////////////////////////////\n"
			"// Fetch a Single Channel\n"
			"//////////////////////////////////////////////////////////////////////\n"
			"\n"
			"float4 fetch_red(int2 xy)\n"
			"{\n"
			"	float4 rt;\n"
			"\n"
			"	if ((PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2))\n"
			"	{\n"
			"		int depth = (fetch_raw_depth(xy)) & 0xFF;\n"
			"		rt = (float4)(depth) / 255.0f;\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		rt = fetch_raw_color(xy);\n"
			"	}\n"
			"\n"
			"	return sample_p(rt.r) * 255.0f;\n"
			"}\n"
			"\n"
			"float4 fetch_green(int2 xy)\n"
			"{\n"
			"	float4 rt;\n"
			"\n"
			"	if ((PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2))\n"
			"	{\n"
			"		int depth = (fetch_raw_depth(xy) >> 8) & 0xFF;\n"
			"		rt = (float4)(depth) / 255.0f;\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		rt = fetch_raw_color(xy);\n"
			"	}\n"
			"\n"
			"	return sample_p(rt.g) * 255.0f;\n"
			"}\n"
			"\n"
			"float4 fetch_blue(int2 xy)\n"
			"{\n"
			"	float4 rt;\n"
			"       if ((PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2))\n"
			"       {\n"
			"               int depth = (fetch_raw_depth(xy) >> 16) & 0xFF;\n"
			"               rt = (float4)(depth) / 255.0f;\n"
			"       }\n"
			"       else\n"
			"       {\n"
			"                rt = fetch_raw_color(xy);\n"
			"       }\n"
			"	return sample_p(rt.b) * 255.0f;\n"
			"}\n"
			"\n"
			"float4 fetch_alpha(int2 xy)\n"
			"{\n"
			"	float4 rt = fetch_raw_color(xy);\n"
			"	return sample_p(rt.a) * 255.0f;\n"
			"}\n"
			"\n"
			"float4 fetch_rgb(int2 xy)\n"
			"{\n"
			"	float4 rt = fetch_raw_color(xy);\n"
			"	float4 c = float4(sample_p(rt.r).r, sample_p(rt.g).g, sample_p(rt.b).b, 1.0);\n"
			"	return c * 255.0f;\n"
			"}\n"
			"\n"
			"float4 fetch_gXbY(int2 xy)\n"
			"{\n"
			"	if ((PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2))\n"
			"	{\n"
			"		int depth = fetch_raw_depth(xy);\n"
			"		int bg = (depth >> (8 + ChannelShuffle.w)) & 0xFF;\n"
			"		return (float4)(bg);\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		int4 rt = (int4)(fetch_raw_color(xy) * 255.0);\n"
			"		int green = (rt.g >> ChannelShuffle.w) & ChannelShuffle.z;\n"
			"		int blue = (rt.b << ChannelShuffle.y) & ChannelShuffle.x;\n"
			"		return (float4)(green | blue);\n"
			"	}\n"
			"}\n"
			"\n"
			"float4 sample_color(float2 st)\n"
			"{\n"
			"	#if PS_TCOFFSETHACK\n"
			"	st += TC_OffsetHack.xy;\n"
			"	#endif\n"
			"\n"
			"	float4 t;\n"
			"	float4x4 c;\n"
			"	float2 dd;\n"
			"\n"
			"	if (PS_LTF == 0 && PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_WMS < 2 && PS_WMT < 2)\n"
			"	{\n"
			"		c[0] = sample_c(st);\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		float4 uv;\n"
			"\n"
			"		if(PS_LTF)\n"
			"		{\n"
			"			uv = st.xyxy + HalfTexel;\n"
			"			dd = frac(uv.xy * WH.zw);\n"
			"\n"
			"			if(PS_FST == 0)\n"
			"			{\n"
			"				dd = clamp(dd, (float2)0.0f, (float2)0.9999999f);\n"
			"			}\n"
			"		}\n"
			"		else\n"
			"		{\n"
			"			uv = st.xyxy;\n"
			"		}\n"
			"\n"
			"		uv = clamp_wrap_uv(uv);\n"
			"\n"
			"#if PS_PAL_FMT != 0\n"
			"			c = sample_4p(sample_4_index(uv));\n"
			"#else\n"
			"			c = sample_4c(uv);\n"
			"#endif\n"
			"	}\n"
			"\n"
			"	[unroll]\n"
			"	for (uint i = 0; i < 4; i++)\n"
			"	{\n"
			"		if(PS_AEM_FMT == FMT_24)\n"
			"		{\n"
			"			c[i].a = !PS_AEM || any(c[i].rgb) ? TA.x : 0;\n"
			"		}\n"
			"		else if(PS_AEM_FMT == FMT_16)\n"
			"		{\n"
			"			c[i].a = c[i].a >= 0.5 ? TA.y : !PS_AEM || any(c[i].rgb) ? TA.x : 0;\n"
			"		}\n"
			"	}\n"
			"\n"
			"	if(PS_LTF)\n"
			"	{\n"
			"		t = lerp(lerp(c[0], c[1], dd.x), lerp(c[2], c[3], dd.x), dd.y);\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		t = c[0];\n"
			"	}\n"
			"\n"
			"	return trunc(t * 255.0f + 0.05f);\n"
			"}\n"
			"\n"
			"float4 tfx(float4 T, float4 C)\n"
			"{\n"
			"	float4 C_out;\n"
			"	float4 FxT = trunc(trunc(C) * T / 128.0f);\n"
			"\n"
			"#if (PS_TFX == 0)\n"
			"	C_out = FxT;\n"
			"#elif (PS_TFX == 1)\n"
			"	C_out = T;\n"
			"#elif (PS_TFX == 2)\n"
			"	C_out.rgb = FxT.rgb + C.a;\n"
			"	C_out.a = T.a + C.a;\n"
			"#elif (PS_TFX == 3)\n"
			"	C_out.rgb = FxT.rgb + C.a;\n"
			"	C_out.a = T.a;\n"
			"#else\n"
			"	C_out = C;\n"
			"#endif\n"
			"\n"
			"#if (PS_TCC == 0)\n"
			"	C_out.a = C.a;\n"
			"#endif\n"
			"\n"
			"#if (PS_TFX == 0) || (PS_TFX == 2) || (PS_TFX == 3)\n"
			"	// Clamp only when it is useful\n"
			"	C_out = min(C_out, 255.0f);\n"
			"#endif\n"
			"\n"
			"	return C_out;\n"
			"}\n"
			"\n"
			"void atst(float4 C)\n"
			"{\n"
			"	float a = C.a;\n"
			"\n"
			"	// Do nothing for PS_ATST 0\n"
			"	if(PS_ATST == 1)\n"
			"	{\n"
			"		if (a > AREF) discard;\n"
			"	}\n"
			"	else if(PS_ATST == 2)\n"
			"	{\n"
			"		if (a < AREF) discard;\n"
			"	}\n"
			"	else if(PS_ATST == 3)\n"
			"	{\n"
			"		 if (abs(a - AREF) > 0.5f) discard;\n"
			"	}\n"
			"	else if(PS_ATST == 4)\n"
			"	{\n"
			"		if (abs(a - AREF) < 0.5f) discard;\n"
			"	}\n"
			"}\n"
			"\n"
			"float4 fog(float4 c, float f)\n"
			"{\n"
			"	if(PS_FOG)\n"
			"	{\n"
			"		c.rgb = trunc(lerp(FogColor, c.rgb, f));\n"
			"	}\n"
			"\n"
			"	return c;\n"
			"}\n"
			"\n"
			"float4 ps_color(PS_INPUT input)\n"
			"{\n"
			"#if PS_FST == 0 && PS_INVALID_TEX0 == 1\n"
			"	// Re-normalize coordinate from invalid GS to corrected texture size\n"
			"	float2 st = (input.t.xy * WH.xy) / (input.t.w * WH.zw);\n"
			"	// no st_int yet\n"
			"#elif PS_FST == 0\n"
			"	float2 st = input.t.xy / input.t.w;\n"
			"	float2 st_int = input.ti.zw / input.t.w;\n"
			"#else\n"
			"	float2 st = input.ti.xy;\n"
			"	float2 st_int = input.ti.zw;\n"
			"#endif\n"
			"\n"
			"#if PS_CHANNEL_FETCH == 1\n"
			"	float4 T = fetch_red(int2(input.p.xy));\n"
			"#elif PS_CHANNEL_FETCH == 2\n"
			"	float4 T = fetch_green(int2(input.p.xy));\n"
			"#elif PS_CHANNEL_FETCH == 3\n"
			"	float4 T = fetch_blue(int2(input.p.xy));\n"
			"#elif PS_CHANNEL_FETCH == 4\n"
			"	float4 T = fetch_alpha(int2(input.p.xy));\n"
			"#elif PS_CHANNEL_FETCH == 5\n"
			"	float4 T = fetch_rgb(int2(input.p.xy));\n"
			"#elif PS_CHANNEL_FETCH == 6\n"
			"	float4 T = fetch_gXbY(int2(input.p.xy));\n"
			"#elif PS_DEPTH_FMT > 0\n"
			"	float4 T = sample_depth(st_int, input.p.xy);\n"
			"#else\n"
			"	float4 T = sample_color(st);\n"
			"#endif\n"
			"\n"
			"	float4 C = tfx(T, input.c);\n"
			"\n"
			"	atst(C);\n"
			"\n"
			"	C = fog(C, input.t.z);\n"
			"\n"
			"	if(PS_CLR1) // needed for Cd * (As/Ad/F + 1) blending modes\n"
			"	{\n"
			"		C.rgb = (float3)255.0f;\n"
			"	}\n"
			"\n"
			"	return C;\n"
			"}\n"
			"\n"
			"void ps_fbmask(inout float4 C, float2 pos_xy)\n"
			"{\n"
			"	if (PS_FBMASK)\n"
			"	{\n"
			"		float4 RT = trunc(RtSampler.Load(int3(pos_xy, 0)) * 255.0f + 0.1f);\n"
			"		C = (float4)(((uint4)C & ~FbMask) | ((uint4)RT & FbMask));\n"
			"	}\n"
			"}\n"
			"\n"
			"void ps_dither(inout float3 C, float2 pos_xy)\n"
			"{\n"
			"	if (PS_DITHER)\n"
			"	{\n"
			"		int2 fpos;\n"
			"\n"
			"		if (PS_DITHER == 2)\n"
			"			fpos = int2(pos_xy);\n"
			"		else\n"
			"			fpos = int2(pos_xy / (float)PS_SCALE_FACTOR);\n"
			"\n"
			"		C += DitherMatrix[fpos.y & 3][fpos.x & 3];\n"
			"	}\n"
			"}\n"
			"\n"
			"void ps_blend(inout float4 Color, float As, float2 pos_xy)\n"
			"{\n"
			"	if (SW_BLEND)\n"
			"	{\n"
			"		float4 RT = trunc(RtSampler.Load(int3(pos_xy, 0)) * 255.0f + 0.1f);\n"
			"\n"
			"		float Ad = (PS_DFMT == FMT_24) ? 1.0f : RT.a / 128.0f;\n"
			"\n"
			"		float3 Cd = RT.rgb;\n"
			"		float3 Cs = Color.rgb;\n"
			"		float3 Cv;\n"
			"\n"
			"		float3 A = (PS_BLEND_A == 0) ? Cs : ((PS_BLEND_A == 1) ? Cd : (float3)0.0f);\n"
			"		float3 B = (PS_BLEND_B == 0) ? Cs : ((PS_BLEND_B == 1) ? Cd : (float3)0.0f);\n"
			"		float3 C = (PS_BLEND_C == 0) ? As : ((PS_BLEND_C == 1) ? Ad : Af);\n"
			"		float3 D = (PS_BLEND_D == 0) ? Cs : ((PS_BLEND_D == 1) ? Cd : (float3)0.0f);\n"
			"\n"
			"		Cv = (PS_BLEND_A == PS_BLEND_B) ? D : trunc(((A - B) * C) + D);\n"
			"\n"
			"      // PABE\n"
			"		if (PS_PABE)\n"
			"			Cv = (Color.a >= 128.0f) ? Cv : Color.rgb;\n"
			"\n"
			"		// Dithering\n"
			"		ps_dither(Cv, pos_xy);\n"
			"\n"
			"		// Standard Clamp\n"
			"		if (PS_COLCLIP == 0 && PS_HDR == 0)\n"
			"			Cv = clamp(Cv, (float3)0.0f, (float3)255.0f);\n"
			"\n"
			"		// In 16 bits format, only 5 bits of color are used. It impacts shadows computation of Castlevania\n"
			"		if (PS_DFMT == FMT_16)\n"
			"			Cv = (float3)((int3)Cv & (int3)0xF8);\n"
			"		else if (PS_COLCLIP == 1 && PS_HDR == 0)\n"
			"			Cv = (float3)((int3)Cv & (int3)0xFF);\n"
			"\n"
			"		Color.rgb = Cv;\n"
			"	}\n"
			"}\n"
			"\n"
			"PS_OUTPUT ps_main(PS_INPUT input)\n"
			"{\n"
			"	float4 C = ps_color(input);\n"
			"\n"
			"	PS_OUTPUT output;\n"
			"\n"
			"	if (PS_SHUFFLE)\n"
			"	{\n"
			"		uint4 denorm_c = uint4(C);\n"
			"		uint2 denorm_TA = uint2(float2(TA.xy) * 255.0f + 0.5f);\n"
			"\n"
			"		// Mask will take care of the correct destination\n"
			"		if (PS_READ_BA)\n"
			"			C.rb = C.bb;\n"
			"		else\n"
			"			C.rb = C.rr;\n"
			"\n"
			"		if (PS_READ_BA)\n"
			"		{\n"
			"			if (denorm_c.a & 0x80u)\n"
			"				C.ga = (float2)(float((denorm_c.a & 0x7Fu) | (denorm_TA.y & 0x80u)));\n"
			"			else\n"
			"				C.ga = (float2)(float((denorm_c.a & 0x7Fu) | (denorm_TA.x & 0x80u)));\n"
			"		}\n"
			"		else\n"
			"		{\n"
			"			if (denorm_c.g & 0x80u)\n"
			"				C.ga = (float2)(float((denorm_c.g & 0x7Fu) | (denorm_TA.y & 0x80u)));\n"
			"			else\n"
			"				C.ga = (float2)(float((denorm_c.g & 0x7Fu) | (denorm_TA.x & 0x80u)));\n"
			"		}\n"
			"	}\n"
			"\n"
			"	// Must be done before alpha correction\n"
			"	float alpha_blend = C.a / 128.0f;\n"
			"\n"
			"	// Alpha correction\n"
			"	if (PS_DFMT == FMT_16)\n"
			"	{\n"
			"		float A_one = 128.0f; // alpha output will be 0x80\n"
			"		C.a = PS_FBA ? A_one : step(A_one, C.a) * A_one;\n"
			"	}\n"
			"	else if ((PS_DFMT == FMT_32) && PS_FBA)\n"
			"	{\n"
			"		float A_one = 128.0f;\n"
			"		if (C.a < A_one) C.a += A_one;\n"
			"	}\n"
			"\n"
			"	if (!SW_BLEND)\n"
			"		ps_dither(C.rgb, input.p.xy);\n"
			"\n"
			"	ps_blend(C, alpha_blend, input.p.xy);\n"
			"\n"
			"	ps_fbmask(C, input.p.xy);\n"
			"\n"
			"	// When dithering the bottom 3 bits become meaningless and cause lines in the picture\n"
			"	// so we need to limit the color depth on dithered items\n"
			"	// SW_BLEND already deals with this so no need to do in those cases\n"
			"	if (!SW_BLEND && PS_DITHER && PS_DFMT == FMT_16 && !PS_COLCLIP)\n"
			"	{\n"
			"		C.rgb = clamp(C.rgb, (float3)0.0f, (float3)255.0f);\n"
			"		C.rgb = (uint3)((uint3)C.rgb & (uint3)0xF8);\n"
			"	}\n"
			"\n"
			"	output.c0 = C / 255.0f;\n"
			"	output.c1 = (float4)(alpha_blend);\n"
			"\n"
			"#if PS_ZCLAMP\n"
			"	output.depth = min(input.p.z, MaxDepthPS);\n"
			"#endif\n"
			"\n"
			"	return output;\n"
			"}\n"
			"\n"
			"//////////////////////////////////////////////////////////////////////\n"
			"// Vertex Shader\n"
			"//////////////////////////////////////////////////////////////////////\n"
			"\n"
			"VS_OUTPUT vs_main(VS_INPUT input)\n"
			"{\n"
			"	// Clamp to max depth, gs doesn't wrap\n"
			"	input.z = min(input.z, MaxDepth);\n"
			"\n"
			"	VS_OUTPUT output;\n"
			"\n"
			"	// pos -= 0.05 (1/320 pixel) helps avoiding rounding problems (integral part of pos is usually 5 digits, 0.05 is about as low as we can go)\n"
			"	// example: ceil(afterseveralvertextransformations(y = 133)) => 134 => line 133 stays empty\n"
			"	// input granularity is 1/16 pixel, anything smaller than that won't step drawing up/left by one pixel\n"
			"	// example: 133.0625 (133 + 1/16) should start from line 134, ceil(133.0625 - 0.05) still above 133\n"
			"\n"
			"	float4 p = float4(input.p, input.z, 0) - float4(0.05f, 0.05f, 0, 0);\n"
			"\n"
			"	output.p = p * VertexScale - VertexOffset;\n"
			"\n"
			"	if(VS_TME)\n"
			"	{\n"
			"		float2 uv = input.uv - Texture_Scale_Offset.zw;\n"
			"		float2 st = input.st - Texture_Scale_Offset.zw;\n"
			"\n"
			"		// Integer nomalized\n"
			"		output.ti.xy = uv * Texture_Scale_Offset.xy;\n"
			"\n"
			"		if (VS_FST)\n"
			"		{\n"
			"			// Integer integral\n"
			"			output.ti.zw = uv;\n"
			"		}\n"
			"		else\n"
			"		{\n"
			"			// float for post-processing in some games\n"
			"			output.ti.zw = st / Texture_Scale_Offset.xy;\n"
			"		}\n"
			"		// Float coords\n"
			"		output.t.xy = st;\n"
			"		output.t.w = input.q;\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		output.t.xy = 0;\n"
			"		output.t.w = 1.0f;\n"
			"		output.ti = 0;\n"
			"	}\n"
			"\n"
			"	output.c = input.c;\n"
			"	output.t.z = input.f.r;\n"
			"\n"
			"	return output;\n"
			"}\n"
			"\n"
			"//////////////////////////////////////////////////////////////////////\n"
			"// Geometry Shader\n"
			"//////////////////////////////////////////////////////////////////////\n"
			"\n"
			"#if GS_PRIM == 0 && GS_POINT == 0\n"
			"\n"
			"[maxvertexcount(1)]\n"
			"void gs_main(point VS_OUTPUT input[1], inout PointStream<VS_OUTPUT> stream)\n"
			"{\n"
			"	stream.Append(input[0]);\n"
			"}\n"
			"\n"
			"#elif GS_PRIM == 0 && GS_POINT == 1\n"
			"\n"
			"[maxvertexcount(6)]\n"
			"void gs_main(point VS_OUTPUT input[1], inout TriangleStream<VS_OUTPUT> stream)\n"
			"{\n"
			"	// Transform a point to a NxN sprite\n"
			"	VS_OUTPUT Point = input[0];\n"
			"\n"
			"	// Get new position\n"
			"	float4 lt_p = input[0].p;\n"
			"	float4 rb_p = input[0].p + float4(PointSize.x, PointSize.y, 0.0f, 0.0f);\n"
			"	float4 lb_p = rb_p;\n"
			"	float4 rt_p = rb_p;\n"
			"	lb_p.x = lt_p.x;\n"
			"	rt_p.y = lt_p.y;\n"
			"\n"
			"	// Triangle 1\n"
			"	Point.p = lt_p;\n"
			"	stream.Append(Point);\n"
			"\n"
			"	Point.p = lb_p;\n"
			"	stream.Append(Point);\n"
			"\n"
			"	Point.p = rt_p;\n"
			"	stream.Append(Point);\n"
			"\n"
			"	// Triangle 2\n"
			"	Point.p = lb_p;\n"
			"	stream.Append(Point);\n"
			"\n"
			"	Point.p = rt_p;\n"
			"	stream.Append(Point);\n"
			"\n"
			"	Point.p = rb_p;\n"
			"	stream.Append(Point);\n"
			"}\n"
			"\n"
			"#elif GS_PRIM == 1 && GS_LINE == 0\n"
			"\n"
			"[maxvertexcount(2)]\n"
			"void gs_main(line VS_OUTPUT input[2], inout LineStream<VS_OUTPUT> stream)\n"
			"{\n"
			"#if GS_IIP == 0\n"
			"	input[0].c = input[1].c;\n"
			"#endif\n"
			"\n"
			"	stream.Append(input[0]);\n"
			"	stream.Append(input[1]);\n"
			"}\n"
			"\n"
			"#elif GS_PRIM == 1 && GS_LINE == 1\n"
			"\n"
			"[maxvertexcount(6)]\n"
			"void gs_main(line VS_OUTPUT input[2], inout TriangleStream<VS_OUTPUT> stream)\n"
			"{\n"
			"	// Transform a line to a thick line-sprite\n"
			"	VS_OUTPUT left = input[0];\n"
			"	VS_OUTPUT right = input[1];\n"
			"	float2 lt_p = input[0].p.xy;\n"
			"	float2 rt_p = input[1].p.xy;\n"
			"\n"
			"	// Potentially there is faster math\n"
			"	float2 line_vector = normalize(rt_p.xy - lt_p.xy);\n"
			"	float2 line_normal = float2(line_vector.y, -line_vector.x);\n"
			"	float2 line_width = (line_normal * PointSize) / 2;\n"
			"\n"
			"	lt_p -= line_width;\n"
			"	rt_p -= line_width;\n"
			"	float2 lb_p = input[0].p.xy + line_width;\n"
			"	float2 rb_p = input[1].p.xy + line_width;\n"
			"\n"
			"	#if GS_IIP == 0\n"
			"	left.c = right.c;\n"
			"	#endif\n"
			"\n"
			"	// Triangle 1\n"
			"	left.p.xy = lt_p;\n"
			"	stream.Append(left);\n"
			"\n"
			"	left.p.xy = lb_p;\n"
			"	stream.Append(left);\n"
			"\n"
			"	right.p.xy = rt_p;\n"
			"	stream.Append(right);\n"
			"	stream.RestartStrip();\n"
			"\n"
			"	// Triangle 2\n"
			"	left.p.xy = lb_p;\n"
			"	stream.Append(left);\n"
			"\n"
			"	right.p.xy = rt_p;\n"
			"	stream.Append(right);\n"
			"\n"
			"	right.p.xy = rb_p;\n"
			"	stream.Append(right);\n"
			"	stream.RestartStrip();\n"
			"}\n"
			"\n"
			"#elif GS_PRIM == 2\n"
			"\n"
			"[maxvertexcount(3)]\n"
			"void gs_main(triangle VS_OUTPUT input[3], inout TriangleStream<VS_OUTPUT> stream)\n"
			"{\n"
			"	#if GS_IIP == 0\n"
			"	input[0].c = input[2].c;\n"
			"	input[1].c = input[2].c;\n"
			"	#endif\n"
			"\n"
			"	stream.Append(input[0]);\n"
			"	stream.Append(input[1]);\n"
			"	stream.Append(input[2]);\n"
			"}\n"
			"\n"
			"#elif GS_PRIM == 3\n"
			"\n"
			"[maxvertexcount(4)]\n"
			"void gs_main(line VS_OUTPUT input[2], inout TriangleStream<VS_OUTPUT> stream)\n"
			"{\n"
			"	VS_OUTPUT lt = input[0];\n"
			"	VS_OUTPUT rb = input[1];\n"
			"\n"
			"	// flat depth\n"
			"	lt.p.z = rb.p.z;\n"
			"	// flat fog and texture perspective\n"
			"	lt.t.zw = rb.t.zw;\n"
			"\n"
			"	// flat color\n"
			"	lt.c = rb.c;\n"
			"\n"
			"	// Swap texture and position coordinate\n"
			"	VS_OUTPUT lb = rb;\n"
			"	lb.p.x = lt.p.x;\n"
			"	lb.t.x = lt.t.x;\n"
			"	lb.ti.x = lt.ti.x;\n"
			"	lb.ti.z = lt.ti.z;\n"
			"\n"
			"	VS_OUTPUT rt = rb;\n"
			"	rt.p.y = lt.p.y;\n"
			"	rt.t.y = lt.t.y;\n"
			"	rt.ti.y = lt.ti.y;\n"
			"	rt.ti.w = lt.ti.w;\n"
			"\n"
			"	stream.Append(lt);\n"
			"	stream.Append(lb);\n"
			"	stream.Append(rt);\n"
			"	stream.Append(rb);\n"
			"}\n"
			"\n"
			"#endif\n"
			"#endif\n"
			;

bool GSDevice11::CreateTextureFX()
{
	HRESULT hr;

	D3D11_BUFFER_DESC bd;

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(VSConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_vs_cb);

	if(FAILED(hr)) return false;

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(GSConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_gs_cb);

	if (FAILED(hr)) return false;

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(PSConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_ps_cb);

	if(FAILED(hr)) return false;

	D3D11_SAMPLER_DESC sd;

	memset(&sd, 0, sizeof(sd));

	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.MinLOD = -FLT_MAX;
	sd.MaxLOD = FLT_MAX;
	sd.MaxAnisotropy = D3D11_MIN_MAXANISOTROPY;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

	hr = m_dev->CreateSamplerState(&sd, &m_palette_ss);

	if(FAILED(hr)) return false;

	// create layout

	VSSelector sel;
	VSConstantBuffer cb;

	SetupVS(sel, &cb);

	GSConstantBuffer gcb;

	SetupGS(GSSelector(1), &gcb);

	//

	return true;
}

void GSDevice11::SetupVS(VSSelector sel, const VSConstantBuffer* cb)
{
	auto i = std::as_const(m_vs).find(sel);

	if(i == m_vs.end())
	{
		ShaderMacro sm(m_shader.model);

		sm.AddMacro("VS_TME", sel.tme);
		sm.AddMacro("VS_FST", sel.fst);

		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"POSITION", 0, DXGI_FORMAT_R16G16_UINT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"POSITION", 1, DXGI_FORMAT_R32_UINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 2, DXGI_FORMAT_R16G16_UINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};

		GSVertexShader11 vs;

		std::vector<char> shader(tfx_shader_raw, tfx_shader_raw + sizeof(tfx_shader_raw)/sizeof(*tfx_shader_raw));
		CreateShader(shader, "tfx.fx", nullptr, "vs_main", sm.GetPtr(), &vs.vs, layout, ARRAY_SIZE(layout), &vs.il);

		m_vs[sel] = vs;

		i = m_vs.find(sel);
	}

	if(m_vs_cb_cache.Update(cb))
	{
		ID3D11DeviceContext* ctx = m_ctx;

		ctx->UpdateSubresource(m_vs_cb, 0, NULL, cb, 0, 0);
	}

	VSSetShader(i->second.vs, m_vs_cb);

	IASetInputLayout(i->second.il);
}

void GSDevice11::SetupGS(GSSelector sel, const GSConstantBuffer* cb)
{
	CComPtr<ID3D11GeometryShader> gs;

	const bool unscale_pt_ln = (sel.point == 1 || sel.line == 1);
	// Geometry shader is disabled if sprite conversion is done on the cpu (sel.cpu_sprite).
	if ((sel.prim > 0 && sel.cpu_sprite == 0 && (sel.iip == 0 || sel.prim == 3)) || unscale_pt_ln)
	{
		const auto i = std::as_const(m_gs).find(sel);

		if (i != m_gs.end())
		{
			gs = i->second;
		}
		else
		{
			ShaderMacro sm(m_shader.model);

			sm.AddMacro("GS_IIP", sel.iip);
			sm.AddMacro("GS_PRIM", sel.prim);
			sm.AddMacro("GS_POINT", sel.point);
			sm.AddMacro("GS_LINE", sel.line);

			std::vector<char> shader(tfx_shader_raw, tfx_shader_raw + sizeof(tfx_shader_raw)/sizeof(*tfx_shader_raw));
			CreateShader(shader, "tfx.fx", nullptr, "gs_main", sm.GetPtr(), &gs);

			m_gs[sel] = gs;
		}
	}


	if (m_gs_cb_cache.Update(cb))
	{
		ID3D11DeviceContext* ctx = m_ctx;

		ctx->UpdateSubresource(m_gs_cb, 0, NULL, cb, 0, 0);
	}

	GSSetShader(gs, m_gs_cb);
}

void GSDevice11::SetupPS(PSSelector sel, const PSConstantBuffer* cb, PSSamplerSelector ssel)
{
	auto i = std::as_const(m_ps).find(sel);

	if(i == m_ps.end())
	{
		ShaderMacro sm(m_shader.model);

		sm.AddMacro("PS_SCALE_FACTOR", m_upscale_multiplier);
		sm.AddMacro("PS_FST", sel.fst);
		sm.AddMacro("PS_WMS", sel.wms);
		sm.AddMacro("PS_WMT", sel.wmt);
		sm.AddMacro("PS_FMT", sel.fmt);
		sm.AddMacro("PS_AEM", sel.aem);
		sm.AddMacro("PS_TFX", sel.tfx);
		sm.AddMacro("PS_TCC", sel.tcc);
		sm.AddMacro("PS_ATST", sel.atst);
		sm.AddMacro("PS_FOG", sel.fog);
		sm.AddMacro("PS_CLR1", sel.clr1);
		sm.AddMacro("PS_FBA", sel.fba);
		sm.AddMacro("PS_FBMASK", sel.fbmask);
		sm.AddMacro("PS_LTF", sel.ltf);
		sm.AddMacro("PS_TCOFFSETHACK", sel.tcoffsethack);
		sm.AddMacro("PS_POINT_SAMPLER", sel.point_sampler);
		sm.AddMacro("PS_SHUFFLE", sel.shuffle);
		sm.AddMacro("PS_READ_BA", sel.read_ba);
		sm.AddMacro("PS_CHANNEL_FETCH", sel.channel);
		sm.AddMacro("PS_TALES_OF_ABYSS_HLE", sel.tales_of_abyss_hle);
		sm.AddMacro("PS_URBAN_CHAOS_HLE", sel.urban_chaos_hle);
		sm.AddMacro("PS_DFMT", sel.dfmt);
		sm.AddMacro("PS_DEPTH_FMT", sel.depth_fmt);
		sm.AddMacro("PS_PAL_FMT", sel.fmt >> 2);
		sm.AddMacro("PS_INVALID_TEX0", sel.invalid_tex0);
		sm.AddMacro("PS_HDR", sel.hdr);
		sm.AddMacro("PS_COLCLIP", sel.colclip);
		sm.AddMacro("PS_BLEND_A", sel.blend_a);
		sm.AddMacro("PS_BLEND_B", sel.blend_b);
		sm.AddMacro("PS_BLEND_C", sel.blend_c);
		sm.AddMacro("PS_BLEND_D", sel.blend_d);
      sm.AddMacro("PS_PABE", sel.pabe);
		sm.AddMacro("PS_DITHER", sel.dither);
		sm.AddMacro("PS_ZCLAMP", sel.zclamp);

		CComPtr<ID3D11PixelShader> ps;

		std::vector<char> shader(tfx_shader_raw, tfx_shader_raw + sizeof(tfx_shader_raw)/sizeof(*tfx_shader_raw));
		CreateShader(shader, "tfx.fx", nullptr, "ps_main", sm.GetPtr(), &ps);

		m_ps[sel] = ps;

		i = m_ps.find(sel);
	}

	if(m_ps_cb_cache.Update(cb))
	{
		ID3D11DeviceContext* ctx = m_ctx;

		ctx->UpdateSubresource(m_ps_cb, 0, NULL, cb, 0, 0);
	}

	CComPtr<ID3D11SamplerState> ss0, ss1;

	if(sel.tfx != 4)
	{
		if(!(sel.fmt < 3 && sel.wms < 3 && sel.wmt < 3))
		{
			ssel.ltf = 0;
		}

		auto i = std::as_const(m_ps_ss).find(ssel);

		if(i != m_ps_ss.end())
		{
			ss0 = i->second;
		}
		else
		{
			D3D11_SAMPLER_DESC sd;
			enum D3D11_FILTER af = m_aniso_filter ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			sd.Filter = ssel.ltf ? af : D3D11_FILTER_MIN_MAG_MIP_POINT;

			sd.AddressU = ssel.tau ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.AddressV = ssel.tav ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.MipLODBias = 0;
			sd.MinLOD = -FLT_MAX;
			sd.MaxLOD = FLT_MAX;
			sd.MaxAnisotropy = m_aniso_filter;
			sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
			sd.BorderColor[0] = sd.BorderColor[1] = sd.BorderColor[2] = sd.BorderColor[3] = 0.0f;

			m_dev->CreateSamplerState(&sd, &ss0);

			m_ps_ss[ssel] = ss0;
		}

		if(sel.fmt >= 3)
		{
			ss1 = m_palette_ss;
		}
	}

	PSSetSamplerState(ss0, ss1);

	PSSetShader(i->second, m_ps_cb);
}

void GSDevice11::SetupOM(OMDepthStencilSelector dssel, OMBlendSelector bsel, u8 afix)
{
	auto i = std::as_const(m_om_dss).find(dssel);

	if(i == m_om_dss.end())
	{
		D3D11_DEPTH_STENCIL_DESC dsd;

		memset(&dsd, 0, sizeof(dsd));

		if(dssel.date)
		{
			dsd.StencilEnable = true;
			dsd.StencilReadMask = 1;
			dsd.StencilWriteMask = 1;
			dsd.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
			dsd.FrontFace.StencilPassOp = dssel.date_one ? D3D11_STENCIL_OP_ZERO : D3D11_STENCIL_OP_KEEP;
			dsd.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			dsd.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
			dsd.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;
			dsd.BackFace.StencilPassOp = dssel.date_one ? D3D11_STENCIL_OP_ZERO : D3D11_STENCIL_OP_KEEP;
			dsd.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			dsd.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		}

		if(dssel.ztst != ZTST_ALWAYS || dssel.zwe)
		{
			static const D3D11_COMPARISON_FUNC ztst[] =
			{
				D3D11_COMPARISON_NEVER,
				D3D11_COMPARISON_ALWAYS,
				D3D11_COMPARISON_GREATER_EQUAL,
				D3D11_COMPARISON_GREATER
			};

			dsd.DepthEnable = true;
			dsd.DepthWriteMask = dssel.zwe ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
			dsd.DepthFunc = ztst[dssel.ztst];
		}

		CComPtr<ID3D11DepthStencilState> dss;

		m_dev->CreateDepthStencilState(&dsd, &dss);

		m_om_dss[dssel] = dss;

		i = m_om_dss.find(dssel);
	}

	OMSetDepthStencilState(i->second, 1);

	auto j = std::as_const(m_om_bs).find(bsel);

	if(j == m_om_bs.end())
	{
		D3D11_BLEND_DESC bd;

		memset(&bd, 0, sizeof(bd));

		bd.RenderTarget[0].BlendEnable = bsel.abe;

		if(bsel.abe)
		{
			HWBlend blend = GetBlend(bsel.blend_index);
			bd.RenderTarget[0].BlendOp = (D3D11_BLEND_OP)blend.op;
			bd.RenderTarget[0].SrcBlend = (D3D11_BLEND)blend.src;
			bd.RenderTarget[0].DestBlend = (D3D11_BLEND)blend.dst;
			bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;

			if (bsel.accu_blend)
			{
				bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
				bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
			}
			else if (bsel.blend_mix)
				bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		}

		if(bsel.wr) bd.RenderTarget[0].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_RED;
		if(bsel.wg) bd.RenderTarget[0].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
		if(bsel.wb) bd.RenderTarget[0].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
		if(bsel.wa) bd.RenderTarget[0].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;

		CComPtr<ID3D11BlendState> bs;

		m_dev->CreateBlendState(&bd, &bs);

		m_om_bs[bsel] = bs;

		j = m_om_bs.find(bsel);
	}

	OMSetBlendState(j->second, (float)(int)afix / 0x80);
}
