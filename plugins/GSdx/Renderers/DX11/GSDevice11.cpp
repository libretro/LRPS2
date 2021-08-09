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

#include "stdafx.h"
#include "GSdx.h"
#include "GSDevice11.h"
#include "GSUtil.h"
#include "resource.h"
#include <fstream>
#include <VersionHelpers.h>
#include "options_tools.h"

GSDevice11::GSDevice11()
{
	memset(&m_state, 0, sizeof(m_state));
	memset(&m_vs_cb_cache, 0, sizeof(m_vs_cb_cache));
	memset(&m_gs_cb_cache, 0, sizeof(m_gs_cb_cache));
	memset(&m_ps_cb_cache, 0, sizeof(m_ps_cb_cache));

	FXAA_Compiled = false;
	ExShader_Compiled = false;

	m_state.topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	m_state.bf = -1;

	m_mipmap = theApp.GetConfigI("mipmap");
	m_upscale_multiplier = option_upscale_mult;
	
	const BiFiltering nearest_filter = static_cast<BiFiltering>(theApp.GetConfigI("filter"));
	const int aniso_level = theApp.GetConfigI("MaxAnisotropy");
	if ((nearest_filter != BiFiltering::Nearest && !theApp.GetConfigB("paltex") && aniso_level))
		m_aniso_filter = aniso_level;
	else
		m_aniso_filter = 0;
}

bool GSDevice11::SetFeatureLevel(D3D_FEATURE_LEVEL level, bool compat_mode)
{
	m_shader.level = level;

	switch (level)
	{
	case D3D_FEATURE_LEVEL_10_0:
		m_shader.model = "0x400";
		m_shader.vs = "vs_4_0";
		m_shader.gs = "gs_4_0";
		m_shader.ps = "ps_4_0";
		m_shader.cs = "cs_4_0";
		break;
	case D3D_FEATURE_LEVEL_10_1:
		m_shader.model = "0x401";
		m_shader.vs = "vs_4_1";
		m_shader.gs = "gs_4_1";
		m_shader.ps = "ps_4_1";
		m_shader.cs = "cs_4_1";
		break;
	case D3D_FEATURE_LEVEL_11_0:
		m_shader.model = "0x500";
		m_shader.vs = "vs_5_0";
		m_shader.gs = "gs_5_0";
		m_shader.ps = "ps_5_0";
		m_shader.cs = "cs_5_0";
		break;
	default:
		ASSERT(0);
		return false;
	}

	return true;
}

bool GSDevice11::Create(const std::shared_ptr<GSWnd> &wnd)
{
	bool nvidia_vendor = false;

	if(!__super::Create(wnd))
	{
		return false;
	}

	HRESULT hr = E_FAIL;

	D3D11_BUFFER_DESC bd;
	D3D11_SAMPLER_DESC sd;
	D3D11_DEPTH_STENCIL_DESC dsd;
	D3D11_RASTERIZER_DESC rd;
	D3D11_BLEND_DESC bsd;

	retro_hw_render_interface_d3d11 *d3d11 = nullptr;
	if (!environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, (void **)&d3d11) || !d3d11) {
		printf("Failed to get HW rendering interface!\n");
		return false;
	}

	if (d3d11->interface_version != RETRO_HW_RENDER_INTERFACE_D3D11_VERSION) {
		printf("HW render interface mismatch, expected %u, got %u!\n", RETRO_HW_RENDER_INTERFACE_D3D11_VERSION, d3d11->interface_version);
		return false;
	}

	m_dev = d3d11->device;
	m_ctx = d3d11->context;
	D3D_FEATURE_LEVEL level = d3d11->featureLevel;
	if(!SetFeatureLevel(level, true))
		return false;

	// Set maximum texture size limit based on supported feature level.
	if (level >= D3D_FEATURE_LEVEL_11_0)
		m_d3d_texsize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	else
		m_d3d_texsize = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;

	{	// HACK: check nVIDIA
		// Note: It can cause issues on several games such as SOTC, Fatal Frame, plus it adds border offset.
		bool disable_safe_features = theApp.GetConfigB("UserHacks") && theApp.GetConfigB("UserHacks_Disable_Safe_Features");
		m_hack_topleft_offset = (m_upscale_multiplier != 1 && nvidia_vendor && !disable_safe_features) ? -0.01f : 0.0f;
	}

	// debug
#ifdef _DEBUG
	CComPtr<ID3D11Debug> debug;
	hr = m_dev->QueryInterface<ID3D11Debug>(&debug);

	if (SUCCEEDED(hr))
	{
		CComPtr<ID3D11InfoQueue> info_queue;
		hr = debug->QueryInterface<ID3D11InfoQueue>(&info_queue);

		if (SUCCEEDED(hr))
		{
			int break_on = theApp.GetConfigI("dx_break_on_severity");

			info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, break_on & (1 << 0));
			info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, break_on & (1 << 1));
			info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, break_on & (1 << 2));
			info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_INFO, break_on & (1 << 3));
		}
	}
#endif

	// convert

	D3D11_INPUT_ELEMENT_DESC il_convert[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	ShaderMacro sm_model(m_shader.model);

	{
		const char shader_raw[] =
			"#ifdef SHADER_MODEL // make safe to include in resource file to enforce dependency\n"
			"#ifndef PS_SCALE_FACTOR\n"
			"#define PS_SCALE_FACTOR 1\n"
			"#endif\n"
			"struct VS_INPUT\n"
			"{\n"
			"	float4 p : POSITION;\n"
			"	float2 t : TEXCOORD0;\n"
			"	float4 c : COLOR;\n"
			"};\n"
			"struct VS_OUTPUT\n"
			"{\n"
			"	float4 p : SV_Position;\n"
			"	float2 t : TEXCOORD0;\n"
			"	float4 c : COLOR;\n"
			"};\n"
			"Texture2D Texture;\n"
			"SamplerState TextureSampler;\n"
			"float4 sample_c(float2 uv)\n"
			"{\n"
			"	return Texture.Sample(TextureSampler, uv);\n"
			"}\n"
			"struct PS_INPUT\n"
			"{\n"
			"	float4 p : SV_Position;\n"
			"	float2 t : TEXCOORD0;\n"
			"	float4 c : COLOR;\n"
			"};\n"
			"struct PS_OUTPUT\n"
			"{\n"
			"	float4 c : SV_Target0;\n"
			"};\n"
			"VS_OUTPUT vs_main(VS_INPUT input)\n"
			"{\n"
			"	VS_OUTPUT output;\n"
			"\n"
			"	output.p = input.p;\n"
			"	output.t = input.t;\n"
			"	output.c = input.c;\n"
			"\n"
			"	return output;\n"
			"}\n"
			"\n"
			"PS_OUTPUT ps_main0(PS_INPUT input)\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	\n"
			"	output.c = sample_c(input.t);\n"
			"\n"
			"	return output;\n"
			"}\n"
			"PS_OUTPUT ps_main7(PS_INPUT input)\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	float4 c = sample_c(input.t);\n"
			"	c.a = dot(c.rgb, float3(0.299, 0.587, 0.114));\n"
			"	output.c = c;\n"
			"	return output;\n"
			"}\n"
			"float4 ps_crt(PS_INPUT input, int i)\n"
			"{\n"
			"	float4 mask[4] = \n"
			"	{\n"
			"		float4(1, 0, 0, 0), \n"
			"		float4(0, 1, 0, 0), \n"
			"		float4(0, 0, 1, 0), \n"
			"		float4(1, 1, 1, 0)\n"
			"	};\n"
			"	return sample_c(input.t) * saturate(mask[i] + 0.5f);\n"
			"}\n"
			"float4 ps_scanlines(PS_INPUT input, int i)\n"
			"{\n"
			"	float4 mask[2] =\n"
			"	{\n"
			"		float4(1, 1, 1, 0),\n"
			"		float4(0, 0, 0, 0)\n"
			"	};\n"
			"	return sample_c(input.t) * saturate(mask[i] + 0.5f);\n"
			"}\n"
			"uint ps_main1(PS_INPUT input) : SV_Target0\n"
			"{\n"
			"	float4 c = sample_c(input.t);\n"
			"	c.a *= 256.0f / 127; // hm, 0.5 won't give us 1.0 if we just multiply with 2\n"
			"	uint4 i = c * float4(0x001f, 0x03e0, 0x7c00, 0x8000);\n"
			"	return (i.x & 0x001f) | (i.y & 0x03e0) | (i.z & 0x7c00) | (i.w & 0x8000);	\n"
			"}\n"
			"PS_OUTPUT ps_main2(PS_INPUT input)\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	clip(sample_c(input.t).a - 127.5f / 255); // >= 0x80 pass\n"
			"	output.c = 0;\n"
			"	return output;\n"
			"}\n"
			"\n"
			"PS_OUTPUT ps_main3(PS_INPUT input)\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	clip(127.5f / 255 - sample_c(input.t).a); // < 0x80 pass (== 0x80 should not pass)\n"
			"	output.c = 0;\n"
			"	return output;\n"
			"}\n"
			"\n"
			"PS_OUTPUT ps_main4(PS_INPUT input)\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	float4 c = round(sample_c(input.t) * 255);\n"
			"	// We use 2 fmod to avoid negative value.\n"
			"	float4 fmod1 = fmod(c, 256) + 256;\n"
			"	float4 fmod2 = fmod(fmod1, 256);\n"
			"	output.c = fmod2 / 255.0f;\n"
			"	return output;\n"
			"}\n"
			"\n"
			"PS_OUTPUT ps_main5(PS_INPUT input) // scanlines\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	uint4 p = (uint4)input.p;\n"
			"	output.c = ps_scanlines(input, p.y % 2);\n"
			"	return output;\n"
			"}\n"
			"\n"
			"PS_OUTPUT ps_main6(PS_INPUT input) // diagonal\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	uint4 p = (uint4)input.p;\n"
			"	output.c = ps_crt(input, (p.x + (p.y % 3)) % 3);\n"
			"	return output;\n"
			"}\n"
			"\n"
			"PS_OUTPUT ps_main8(PS_INPUT input) // triangular\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	uint4 p = (uint4)input.p;\n"
			"	// output.c = ps_crt(input, ((p.x + (p.y & 1) * 3) >> 1) % 3); \n"
			"	output.c = ps_crt(input, ((p.x + ((p.y >> 1) & 1) * 3) >> 1) % 3);\n"
			"	return output;\n"
			"}\n"
			"static const float PI = 3.14159265359f;\n"
			"PS_OUTPUT ps_main9(PS_INPUT input) // triangular\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	float2 texdim, halfpixel; \n"
			"	Texture.GetDimensions(texdim.x, texdim.y); \n"
			"	if (ddy(input.t.y) * texdim.y > 0.5) \n"
			"		output.c = sample_c(input.t); \n"
			"	else\n"
			"		output.c = (0.9 - 0.4 * cos(2 * PI * input.t.y * texdim.y)) * sample_c(float2(input.t.x, (floor(input.t.y * texdim.y) + 0.5) / texdim.y));\n"
			"\n"
			"	return output;\n"
			"}\n"
			"\n"
			"uint ps_main10(PS_INPUT input) : SV_Target0\n"
			"{\n"
			"	// Convert a FLOAT32 depth texture into a 32 bits UINT texture\n"
			"	return uint(exp2(32.0f) * sample_c(input.t).r);\n"
			"}\n"
			"\n"
			"PS_OUTPUT ps_main11(PS_INPUT input)\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"\n"
			"	// Convert a FLOAT32 depth texture into a RGBA color texture\n"
			"	const float4 bitSh = float4(exp2(24.0f), exp2(16.0f), exp2(8.0f), exp2(0.0f));\n"
			"	const float4 bitMsk = float4(0.0, 1.0 / 256.0, 1.0 / 256.0, 1.0 / 256.0);\n"
			"\n"
			"	float4 res = frac(float4(sample_c(input.t).rrrr) * bitSh);\n"
			"\n"
			"	output.c = (res - res.xxyz * bitMsk) * 256.0f / 255.0f;\n"
			"\n"
			"	return output;\n"
			"}\n"
			"\n"
			"PS_OUTPUT ps_main12(PS_INPUT input)\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	// Convert a FLOAT32 (only 16 lsb) depth into a RGB5A1 color texture\n"
			"	const float4 bitSh = float4(exp2(32.0f), exp2(27.0f), exp2(22.0f), exp2(17.0f));\n"
			"	const uint4 bitMsk = uint4(0x1F, 0x1F, 0x1F, 0x1);\n"
			"	uint4 color = uint4(float4(sample_c(input.t).rrrr) * bitSh) & bitMsk;\n"
			"	output.c = float4(color) / float4(32.0f, 32.0f, 32.0f, 1.0f);\n"
			"	return output;\n"
			"}\n"
			"float ps_main13(PS_INPUT input) : SV_Depth\n"
			"{\n"
			"	// Convert a RRGBA texture into a float depth texture\n"
			"	// FIXME: I'm afraid of the accuracy\n"
			"	const float4 bitSh = float4(exp2(-32.0f), exp2(-24.0f), exp2(-16.0f), exp2(-8.0f)) * (float4)255.0;\n"
			"	return dot(sample_c(input.t), bitSh);\n"
			"}\n"
			"\n"
			"float ps_main14(PS_INPUT input) : SV_Depth\n"
			"{\n"
			"	// Same as above but without the alpha channel (24 bits Z)\n"
			"	// Convert a RRGBA texture into a float depth texture\n"
			"	const float3 bitSh = float3(exp2(-32.0f), exp2(-24.0f), exp2(-16.0f)) * (float3)255.0;\n"
			"	return dot(sample_c(input.t).rgb, bitSh);\n"
			"}\n"
			"\n"
			"float ps_main15(PS_INPUT input) : SV_Depth\n"
			"{\n"
			"	// Same as above but without the A/B channels (16 bits Z)\n"
			"	// Convert a RRGBA texture into a float depth texture\n"
			"	// FIXME: I'm afraid of the accuracy\n"
			"	const float2 bitSh = float2(exp2(-32.0f), exp2(-24.0f)) * (float2)255.0;\n"
			"	return dot(sample_c(input.t).rg, bitSh);\n"
			"}\n"
			"\n"
			"float ps_main16(PS_INPUT input) : SV_Depth\n"
			"{\n"
			"	// Convert a RGB5A1 (saved as RGBA8) color to a 16 bit Z\n"
			"	// FIXME: I'm afraid of the accuracy\n"
			"	const float4 bitSh = float4(exp2(-32.0f), exp2(-27.0f), exp2(-22.0f), exp2(-17.0f));\n"
			"	// Trunc color to drop useless lsb\n"
			"	float4 color = trunc(sample_c(input.t) * (float4)255.0 / float4(8.0f, 8.0f, 8.0f, 128.0f));\n"
			"	return dot(float4(color), bitSh);\n"
			"}\n"
			"\n"
			"PS_OUTPUT ps_main17(PS_INPUT input)\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	// Potential speed optimization. There is a high probability that\n"
			"	// game only want to extract a single channel (blue). It will allow\n"
			"	// to remove most of the conditional operation and yield a +2/3 fps\n"
			"	// boost on MGS3\n"
			"	//\n"
			"	// Hypothesis wrong in Prince of Persia ... Seriously WTF !\n"
			"	//#define ONLY_BLUE;\n"
			"	// Convert a RGBA texture into a 8 bits packed texture\n"
			"	// Input column: 8x2 RGBA pixels\n"
			"	// 0: 8 RGBA\n"
			"	// 1: 8 RGBA\n"
			"	// Output column: 16x4 Index pixels\n"
			"	// 0: 8 R | 8 B\n"
			"	// 1: 8 R | 8 B\n"
			"	// 2: 8 G | 8 A\n"
			"	// 3: 8 G | 8 A\n"
			"	float c;\n"
			"	uint2 sel = uint2(input.p.xy) % uint2(16u, 16u);\n"
			"	int2  tb  = ((int2(input.p.xy) & ~int2(15, 3)) >> 1);\n"
			"	int ty   = tb.y | (int(input.p.y) & 1);\n"
			"	int txN  = tb.x | (int(input.p.x) & 7);\n"
			"	int txH  = tb.x | ((int(input.p.x) + 4) & 7);\n"
			"	txN *= PS_SCALE_FACTOR;\n"
			"	txH *= PS_SCALE_FACTOR;\n"
			"	ty  *= PS_SCALE_FACTOR;\n"
			"	// TODO investigate texture gather\n"
			"	float4 cN = Texture.Load(int3(txN, ty, 0));\n"
			"	float4 cH = Texture.Load(int3(txH, ty, 0));\n"
			"	if ((sel.y & 4u) == 0u)\n"
			"	{\n"
			"#ifdef ONLY_BLUE\n"
			"		c = cN.b;\n"
			"#else\n"
			"		// Column 0 and 2\n"
			"		if ((sel.y & 3u) < 2u)\n"
			"		{\n"
			"			// First 2 lines of the col\n"
			"			if (sel.x < 8u)\n"
			"				c = cN.r;\n"
			"			else\n"
			"				c = cN.b;\n"
			"		}\n"
			"		else\n"
			"		{\n"
			"			if (sel.x < 8u)\n"
			"				c = cH.g;\n"
			"			else\n"
			"				c = cH.a;\n"
			"		}\n"
			"#endif\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"#ifdef ONLY_BLUE\n"
			"		c = cH.b;\n"
			"#else\n"
			"		// Column 1 and 3\n"
			"		if ((sel.y & 3u) < 2u)\n"
			"		{\n"
			"			// First 2 lines of the col\n"
			"			if (sel.x < 8u)\n"
			"				c = cH.r;\n"
			"			else\n"
			"				c = cH.b;\n"
			"		}\n"
			"		else\n"
			"		{\n"
			"			if (sel.x < 8u)\n"
			"				c = cN.g;\n"
			"			else\n"
			"				c = cN.a;\n"
			"		}\n"
			"#endif\n"
			"	}\n"
			"\n"
			"	output.c = (float4)(c); // Divide by something here?\n"
			"	return output;\n"
			"}\n"
			"// DUMMY\n"
			"PS_OUTPUT ps_main18(PS_INPUT input)\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	output.c = input.p;\n"
			"	return output;\n"
			"}\n"
			"\n"
			"PS_OUTPUT ps_main19(PS_INPUT input)\n"
			"{\n"
			"	PS_OUTPUT output;\n"
			"	output.c = input.c * float4(1.0, 1.0, 1.0, sample_c(input.t).r);\n"
			"	return output;\n"
			"}\n"
			"#endif";
		std::vector<char> shader(shader_raw, shader_raw + sizeof(shader_raw)/sizeof(*shader_raw));
		CreateShader(shader, "convert.fx", nullptr, "vs_main", sm_model.GetPtr(), &m_convert.vs, il_convert, countof(il_convert), &m_convert.il);

		ShaderMacro sm_convert(m_shader.model);
		sm_convert.AddMacro("PS_SCALE_FACTOR", std::max(1, m_upscale_multiplier));

		D3D_SHADER_MACRO* sm_convert_ptr = sm_convert.GetPtr();

		for(size_t i = 0; i < countof(m_convert.ps); i++)
		{
			CreateShader(shader, "convert.fx", nullptr, format("ps_main%d", i).c_str(), sm_convert_ptr, & m_convert.ps[i]);
		}
	}

	memset(&dsd, 0, sizeof(dsd));

	hr = m_dev->CreateDepthStencilState(&dsd, &m_convert.dss);

	dsd.DepthEnable = true;
	dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;

	hr = m_dev->CreateDepthStencilState(&dsd, &m_convert.dss_write);

	memset(&bsd, 0, sizeof(bsd));

	bsd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = m_dev->CreateBlendState(&bsd, &m_convert.bs);

	// merge

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(MergeConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_merge.cb);

	{
		const char shader_raw[] = 
			"#ifdef SHADER_MODEL // make safe to include in resource file to enforce dependency\n"
			"\n"
			"Texture2D Texture;\n"
			"SamplerState Sampler;\n"
			"\n"
			"cbuffer cb0\n"
			"{\n"
			"	float4 BGColor;\n"
			"};\n"
			"\n"
			"struct PS_INPUT\n"
			"{\n"
			"	float4 p : SV_Position;\n"
			"	float2 t : TEXCOORD0;\n"
			"};\n"
			"\n"
			"float4 ps_main0(PS_INPUT input) : SV_Target0\n"
			"{\n"
			"	float4 c = Texture.Sample(Sampler, input.t);\n"
			"	c.a *= 2.0f;\n"
			"	return c;\n"
			"}\n"
			"\n"
			"float4 ps_main1(PS_INPUT input) : SV_Target0\n"
			"{\n"
			"	float4 c = Texture.Sample(Sampler, input.t);\n"
			"	c.a = BGColor.a;\n"
			"	return c;\n"
			"}\n"
			"#endif\n"
			;
		std::vector<char> shader(shader_raw, shader_raw + sizeof(shader_raw)/sizeof(*shader_raw));
		for(size_t i = 0; i < countof(m_merge.ps); i++)
		{
			CreateShader(shader, "merge.fx", nullptr, format("ps_main%d", i).c_str(), sm_model.GetPtr(), &m_merge.ps[i]);
		}
	}

	memset(&bsd, 0, sizeof(bsd));

	bsd.RenderTarget[0].BlendEnable = true;
	bsd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bsd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bsd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bsd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bsd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bsd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	bsd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = m_dev->CreateBlendState(&bsd, &m_merge.bs);

	// interlace

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(InterlaceConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_interlace.cb);

	{
		const char shader_raw[] =
			"#ifdef SHADER_MODEL // make safe to include in resource file to enforce dependency\n"
			"Texture2D Texture;\n"
			"SamplerState Sampler;\n"
			"cbuffer cb0\n"
			"{\n"
			"	float2 ZrH;\n"
			"	float hH;\n"
			"};\n"
			"struct PS_INPUT\n"
			"{\n"
			"	float4 p : SV_Position;\n"
			"	float2 t : TEXCOORD0;\n"
			"};\n"
			"float4 ps_main0(PS_INPUT input) : SV_Target0\n"
			"{\n"
			"	clip(frac(input.t.y * hH) - 0.5);\n"
			"	return Texture.Sample(Sampler, input.t);\n"
			"}\n"
			"float4 ps_main1(PS_INPUT input) : SV_Target0\n"
			"{\n"
			"	clip(0.5 - frac(input.t.y * hH));\n"
			"	return Texture.Sample(Sampler, input.t);\n"
			"}\n"
			"float4 ps_main2(PS_INPUT input) : SV_Target0\n"
			"{\n"
			"	float4 c0 = Texture.Sample(Sampler, input.t - ZrH);\n"
			"	float4 c1 = Texture.Sample(Sampler, input.t);\n"
			"	float4 c2 = Texture.Sample(Sampler, input.t + ZrH);\n"
			"	return (c0 + c1 * 2 + c2) / 4;\n"
			"}\n"
			"float4 ps_main3(PS_INPUT input) : SV_Target0\n"
			"{\n"
			"	return Texture.Sample(Sampler, input.t);\n"
			"}\n"
			"#endif\n";
		std::vector<char> shader(shader_raw, shader_raw + sizeof(shader_raw)/sizeof(*shader_raw));
		for(size_t i = 0; i < countof(m_interlace.ps); i++)
		{
			CreateShader(shader, "interlace.fx", nullptr, format("ps_main%d", i).c_str(), sm_model.GetPtr(), &m_interlace.ps[i]);
		}
	}

	// Fxaa

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(FXAAConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_fxaa.cb);

	//

	memset(&rd, 0, sizeof(rd));

	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.FrontCounterClockwise = false;
	rd.DepthBias = false;
	rd.DepthBiasClamp = 0;
	rd.SlopeScaledDepthBias = 0;
	rd.DepthClipEnable = false; // ???
	rd.ScissorEnable = true;
	rd.MultisampleEnable = true;
	rd.AntialiasedLineEnable = false;

	hr = m_dev->CreateRasterizerState(&rd, &m_rs);

	m_ctx->RSSetState(m_rs);

	//

	memset(&sd, 0, sizeof(sd));

	sd.Filter = m_aniso_filter ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.MinLOD = -FLT_MAX;
	sd.MaxLOD = FLT_MAX;
	sd.MaxAnisotropy = m_aniso_filter;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

	hr = m_dev->CreateSamplerState(&sd, &m_convert.ln);

	sd.Filter = m_aniso_filter ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_POINT;

	hr = m_dev->CreateSamplerState(&sd, &m_convert.pt);

	//

	Reset(1, 1);

	//

	CreateTextureFX();

	//

	memset(&dsd, 0, sizeof(dsd));

	dsd.DepthEnable = false;
	dsd.StencilEnable = true;
	dsd.StencilReadMask = 1;
	dsd.StencilWriteMask = 1;
	dsd.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	dsd.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	dsd.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsd.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	dsd.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	dsd.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	dsd.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsd.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

	m_dev->CreateDepthStencilState(&dsd, &m_date.dss);

	D3D11_BLEND_DESC blend;

	memset(&blend, 0, sizeof(blend));

	m_dev->CreateBlendState(&blend, &m_date.bs);
	return true;
}

bool GSDevice11::Reset(int w, int h)
{
	if(!__super::Reset(w, h))
		return false;
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = w;
	desc.Height = h;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA srsc = {};
	srsc.pSysMem  = malloc(w*h*4);
	srsc.SysMemPitch = w * 4;
	srsc.SysMemSlicePitch = w * h * 4;
	memset((void*)srsc.pSysMem, 0xFF, w*h*3);


	CComPtr<ID3D11Texture2D> texture;
	if (FAILED(m_dev->CreateTexture2D(&desc, nullptr, &texture)))
		return false;
	free((void*)srsc.pSysMem);

	m_backbuffer = new GSTexture11(texture);

	return true;
}

void GSDevice11::Flip()
{
//	if(!m_current)
//		return;

	ID3D11RenderTargetView *nullView = nullptr;
	m_ctx->OMSetRenderTargets(1, &nullView, nullptr);

	ID3D11ShaderResourceView* srv = *(GSTexture11*)m_backbuffer;
	m_ctx->PSSetShaderResources(0, 1, &srv);

	extern retro_video_refresh_t video_cb;
	video_cb(RETRO_HW_FRAME_BUFFER_VALID, m_backbuffer->GetWidth(), m_backbuffer->GetHeight(), 0);

	/* restore State */
	uint32 stride = m_state.vb_stride;
	uint32 offset = 0;
	m_ctx->IASetVertexBuffers(0, 1, &m_state.vb, &stride, &offset);
	m_ctx->IASetIndexBuffer(m_state.ib, DXGI_FORMAT_R32_UINT, 0);
	m_ctx->IASetInputLayout(m_state.layout);
	m_ctx->IASetPrimitiveTopology(m_state.topology);
	m_ctx->VSSetShader(m_state.vs, NULL, 0);
	m_ctx->VSSetConstantBuffers(0, 1, &m_state.vs_cb);
	m_ctx->GSSetShader(m_state.gs, NULL, 0);
	m_ctx->GSSetConstantBuffers(0, 1, &m_state.gs_cb);
	m_ctx->PSSetShader(m_state.ps, NULL, 0);
	m_ctx->PSSetConstantBuffers(0, 1, &m_state.ps_cb);
	m_ctx->PSSetShaderResources(0, m_state.ps_sr_views.size(), m_state.ps_sr_views.data());
	m_ctx->PSSetSamplers(0, countof(m_state.ps_ss), m_state.ps_ss);
	m_ctx->OMSetDepthStencilState(m_state.dss, m_state.sref);
	float BlendFactor[] = {m_state.bf, m_state.bf, m_state.bf, 0};
	m_ctx->OMSetBlendState(m_state.bs, BlendFactor, 0xffffffff);
#if 0
	m_ctx->OMSetRenderTargets(1, &m_state.rt_view, m_state.dsv);
	D3D11_VIEWPORT vp = {m_hack_topleft_offset, m_hack_topleft_offset,
						 (float)m_state.viewport.x, (float)m_state.viewport.y, 0.0f, 1.0f};
	m_ctx->RSSetViewports(1, &vp);
	m_ctx->RSSetScissorRects(1, m_state.scissor);
#endif
}

void GSDevice11::BeforeDraw()
{
	// DX can't read from the FB
	// So let's copy it and send that to the shader instead

	auto bits = m_state.ps_sr_bitfield;
	m_state.ps_sr_bitfield = 0;

	unsigned long i;
	while (_BitScanForward(&i, bits))
	{
		GSTexture11* tex = m_state.ps_sr_texture[i];

		if (tex->Equal(m_state.rt_texture) || tex->Equal(m_state.rt_ds))
		{
#ifdef _DEBUG
			log_cb(RETRO_LOG_WARN, "FB read detected on slot %i, copying...\n", i);
#endif
			GSTexture* cp = nullptr;

			CloneTexture(tex, &cp);

			PSSetShaderResource(i, cp);
		}

		bits ^= 1u << i;
	}

	PSUpdateShaderState();
}

void GSDevice11::AfterDraw()
{
	unsigned long i;
	while (_BitScanForward(&i, m_state.ps_sr_bitfield))
	{
#ifdef _DEBUG
		log_cb(RETRO_LOG_WARN, "Cleaning up copied texture on slot %i\n", i);
#endif
		Recycle(m_state.ps_sr_texture[i]);
		PSSetShaderResource(i, NULL);
	}
}

void GSDevice11::DrawPrimitive()
{
	BeforeDraw();

	m_ctx->Draw(m_vertex.count, m_vertex.start);

	AfterDraw();
}

void GSDevice11::DrawIndexedPrimitive()
{
	BeforeDraw();

	m_ctx->DrawIndexed(m_index.count, m_index.start, m_vertex.start);

	AfterDraw();
}

void GSDevice11::DrawIndexedPrimitive(int offset, int count)
{
	ASSERT(offset + count <= (int)m_index.count);

	BeforeDraw();

	m_ctx->DrawIndexed(count, m_index.start + offset, m_vertex.start);

	AfterDraw();
}

void GSDevice11::ClearRenderTarget(GSTexture* t, const GSVector4& c)
{
	if (!t) return;
	m_ctx->ClearRenderTargetView(*(GSTexture11*)t, c.v);
}

void GSDevice11::ClearRenderTarget(GSTexture* t, uint32 c)
{
	if (!t) return;
	GSVector4 color = GSVector4::rgba32(c) * (1.0f / 255);

	m_ctx->ClearRenderTargetView(*(GSTexture11*)t, color.v);
}

void GSDevice11::ClearDepth(GSTexture* t)
{
	if (!t) return;
	m_ctx->ClearDepthStencilView(*(GSTexture11*)t, D3D11_CLEAR_DEPTH, 0.0f, 0);
}

void GSDevice11::ClearStencil(GSTexture* t, uint8 c)
{
	if (!t) return;
	m_ctx->ClearDepthStencilView(*(GSTexture11*)t, D3D11_CLEAR_STENCIL, 0, c);
}

GSTexture* GSDevice11::CreateSurface(int type, int w, int h, int format)
{
	HRESULT hr;

	D3D11_TEXTURE2D_DESC desc;

	memset(&desc, 0, sizeof(desc));

	// Texture limit for D3D10/11 min 1, max 8192 D3D10, max 16384 D3D11.
	desc.Width = std::max(1, std::min(w, m_d3d_texsize));
	desc.Height = std::max(1, std::min(h, m_d3d_texsize));
	desc.Format = (DXGI_FORMAT)format;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;

	// mipmap = m_mipmap > 1 || m_filter != TriFiltering::None;
	bool mipmap = m_mipmap > 1;
	int layers = mipmap && format == DXGI_FORMAT_R8G8B8A8_UNORM ? (int)log2(std::max(w,h)) : 1;

	switch(type)
	{
	case GSTexture::RenderTarget:
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		break;
	case GSTexture::DepthStencil:
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		break;
	case GSTexture::Texture:
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.MipLevels = layers;
		break;
	case GSTexture::Offscreen:
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags |= D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		break;
	}

	GSTexture11* t = NULL;

	CComPtr<ID3D11Texture2D> texture;

	hr = m_dev->CreateTexture2D(&desc, NULL, &texture);

	if(SUCCEEDED(hr))
	{
		t = new GSTexture11(texture);

		switch(type)
		{
		case GSTexture::RenderTarget:
			ClearRenderTarget(t, 0);
			break;
		case GSTexture::DepthStencil:
			ClearDepth(t);
			break;
		}
	}
	else
	{
		throw std::bad_alloc();
	}

	return t;
}

GSTexture* GSDevice11::FetchSurface(int type, int w, int h, int format)
{
	if (format == 0)
		format = (type == GSTexture::DepthStencil || type == GSTexture::SparseDepthStencil) ? DXGI_FORMAT_R32G8X24_TYPELESS : DXGI_FORMAT_R8G8B8A8_UNORM;

	return __super::FetchSurface(type, w, h, format);
}

GSTexture* GSDevice11::CopyOffscreen(GSTexture* src, const GSVector4& sRect, int w, int h, int format, int ps_shader)
{
	GSTexture* dst = NULL;

	if(format == 0)
	{
		format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	ASSERT(format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_R16_UINT || format == DXGI_FORMAT_R32_UINT);

	if(GSTexture* rt = CreateRenderTarget(w, h, format))
	{
		GSVector4 dRect(0, 0, w, h);

		StretchRect(src, sRect, rt, dRect, m_convert.ps[ps_shader], NULL);

		dst = CreateOffscreen(w, h, format);

		if(dst)
		{
			m_ctx->CopyResource(*(GSTexture11*)dst, *(GSTexture11*)rt);
		}

		Recycle(rt);
	}

	return dst;
}

void GSDevice11::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r)
{
	if (!sTex || !dTex)
	{
		ASSERT(0);
		return;
	}

	D3D11_BOX box = { (UINT)r.left, (UINT)r.top, 0U, (UINT)r.right, (UINT)r.bottom, 1U };

	// DX api isn't happy if we pass a box for depth copy
	// It complains that depth/multisample must be a full copy
	// and asks us to use a NULL for the box
	bool depth = (sTex->GetType() == GSTexture::DepthStencil);
	auto pBox = depth ? nullptr : &box;

	m_ctx->CopySubresourceRegion(*(GSTexture11*)dTex, 0, 0, 0, 0, *(GSTexture11*)sTex, 0, pBox);
}

void GSDevice11::CloneTexture(GSTexture* src, GSTexture** dest)
{
	if (!src || !(src->GetType() == GSTexture::DepthStencil || src->GetType() == GSTexture::RenderTarget))
	{
		ASSERT(0);
		return;
	}

	int w = src->GetWidth();
	int h = src->GetHeight();

	if (src->GetType() == GSTexture::DepthStencil)
		*dest = CreateDepthStencil(w, h, src->GetFormat());
	else
		*dest = CreateRenderTarget(w, h, src->GetFormat());

	CopyRect(src, *dest, GSVector4i(0, 0, w, h));
}

void GSDevice11::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, int shader, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, m_convert.ps[shader], NULL, linear);
}

void GSDevice11::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ID3D11PixelShader* ps, ID3D11Buffer* ps_cb, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, ps, ps_cb, m_convert.bs, linear);
}

void GSDevice11::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha)
{
	D3D11_BLEND_DESC bd = {};
	CComPtr<ID3D11BlendState> bs;

	uint8 write_mask = 0;

	if (red)   write_mask |= D3D11_COLOR_WRITE_ENABLE_RED;
	if (green) write_mask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
	if (blue)  write_mask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
	if (alpha) write_mask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;

	bd.RenderTarget[0].RenderTargetWriteMask = write_mask;

	m_dev->CreateBlendState(&bd, &bs);

	StretchRect(sTex, sRect, dTex, dRect, m_convert.ps[ShaderConvert_COPY], nullptr, bs, false);
}

void GSDevice11::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ID3D11PixelShader* ps, ID3D11Buffer* ps_cb, ID3D11BlendState* bs , bool linear)
{
	if(!sTex || !dTex)
	{
		ASSERT(0);
		return;
	}

	bool draw_in_depth = (ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT32] || ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT24] ||
		ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT16] || ps == m_convert.ps[ShaderConvert_RGB5A1_TO_FLOAT16]);

	BeginScene();

	GSVector2i ds = dTex->GetSize();

	// om

	
	if (draw_in_depth)
		OMSetDepthStencilState(m_convert.dss_write, 0);
	else
		OMSetDepthStencilState(m_convert.dss, 0);

	OMSetBlendState(bs, 0);

	if (draw_in_depth)
		OMSetRenderTargets(NULL, dTex);
	else
		OMSetRenderTargets(dTex, NULL);

	// ia

	float left = dRect.x * 2 / ds.x - 1.0f;
	float top = 1.0f - dRect.y * 2 / ds.y;
	float right = dRect.z * 2 / ds.x - 1.0f;
	float bottom = 1.0f - dRect.w * 2 / ds.y;

	GSVertexPT1 vertices[] =
	{
		{GSVector4(left, top, 0.5f, 1.0f), GSVector2(sRect.x, sRect.y)},
		{GSVector4(right, top, 0.5f, 1.0f), GSVector2(sRect.z, sRect.y)},
		{GSVector4(left, bottom, 0.5f, 1.0f), GSVector2(sRect.x, sRect.w)},
		{GSVector4(right, bottom, 0.5f, 1.0f), GSVector2(sRect.z, sRect.w)},
	};



	IASetVertexBuffer(vertices, sizeof(vertices[0]), countof(vertices));
	IASetInputLayout(m_convert.il);
	IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// vs

	VSSetShader(m_convert.vs, NULL);


	// gs

	GSSetShader(NULL, NULL);


	// ps

	PSSetShaderResources(sTex, NULL);
	PSSetSamplerState(linear ? m_convert.ln : m_convert.pt, NULL);
	PSSetShader(ps, ps_cb);

	//

	DrawPrimitive();

	//

	EndScene();

	PSSetShaderResources(NULL, NULL);
}

void GSDevice11::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c)
{
	bool slbg = PMODE.SLBG;
	bool mmod = PMODE.MMOD;

	ClearRenderTarget(dTex, c);

	if(sTex[1] && !slbg)
	{
		StretchRect(sTex[1], sRect[1], dTex, dRect[1], m_merge.ps[0], NULL, true);
	}

	if(sTex[0])
	{
		m_ctx->UpdateSubresource(m_merge.cb, 0, NULL, &c, 0, 0);

		StretchRect(sTex[0], sRect[0], dTex, dRect[0], m_merge.ps[mmod ? 1 : 0], m_merge.cb, m_merge.bs, true);
	}
}

void GSDevice11::DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset)
{
	GSVector4 s = GSVector4(dTex->GetSize());

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0.0f, yoffset, s.x, s.y + yoffset);

	InterlaceConstantBuffer cb;

	cb.ZrH = GSVector2(0, 1.0f / s.y);
	cb.hH = s.y / 2;

	m_ctx->UpdateSubresource(m_interlace.cb, 0, NULL, &cb, 0, 0);

	StretchRect(sTex, sRect, dTex, dRect, m_interlace.ps[shader], m_interlace.cb, linear);
}

// This shouldn't be necessary, we have some bug corrupting memory
// and for some reason isolating this code makes the plugin not crash
void GSDevice11::InitFXAA()
{
	if (!FXAA_Compiled)
	{
		{
			const char shader_raw[] =
				"#if defined(SHADER_MODEL) || defined(FXAA_GLSL_130)\n"
				"#ifndef FXAA_GLSL_130\n"
				"    #define FXAA_GLSL_130 0\n"
				"#endif\n"
				"#define UHQ_FXAA 1          //High Quality Fast Approximate Anti Aliasing. Adapted for GSdx from Timothy Lottes FXAA 3.11.\n"
				"#define FxaaSubpixMax 0.0   //[0.00 to 1.00] Amount of subpixel aliasing removal. 0.00: Edge only antialiasing (no blurring)\n"
				"#define FxaaEarlyExit 1     //[0 or 1] Use Fxaa early exit pathing. When disabled, the entire scene is antialiased(FSAA). 0 is off, 1 is on.\n"
				"#if (FXAA_GLSL_130 == 1)\n"
				"in SHADER\n"
				"{\n"
				"    vec4 p;\n"
				"    vec2 t;\n"
				"    vec4 c;\n"
				"} PSin;\n"
				"layout(location = 0) out vec4 SV_Target0;\n"
				"layout(std140, binding = 14) uniform cb14\n"
				"{\n"
				"    vec2 _xyFrame;\n"
				"    vec4 _rcpFrame;\n"
				"};\n"
				"#elif (SHADER_MODEL >= 0x400)\n"
				"Texture2D Texture : register(t0);\n"
				"SamplerState TextureSampler : register(s0);\n"
				"cbuffer cb0\n"
				"{\n"
				"	float4 _rcpFrame : register(c0);\n"
				"};\n"
				"struct VS_INPUT\n"
				"{\n"
				"	float4 p : POSITION;\n"
				"	float2 t : TEXCOORD0;\n"
				"};\n"
				"\n"
				"struct VS_OUTPUT\n"
				"{\n"
				"	float4 p : SV_Position;\n"
				"	float2 t : TEXCOORD0;\n"
				"};\n"
				"\n"
				"struct PS_OUTPUT\n"
				"{\n"
				"	float4 c : SV_Target0;\n"
				"};\n"
				"\n"
				"#endif\n"
				"\n"
				"/*------------------------------------------------------------------------------\n"
				"                             [FXAA CODE SECTION]\n"
				"------------------------------------------------------------------------------*/\n"
				"\n"
				"#if (SHADER_MODEL >= 0x500)\n"
				"#define FXAA_HLSL_5 1\n"
				"#define FXAA_GATHER4_ALPHA 1\n"
				"\n"
				"#elif (SHADER_MODEL >= 0x400)\n"
				"#define FXAA_HLSL_4 1\n"
				"#define FXAA_GATHER4_ALPHA 0\n"
				"\n"
				"#elif (FXAA_GLSL_130 == 1)\n"
				"#define FXAA_GATHER4_ALPHA 1\n"
				"#endif\n"
				"\n"
				"#if (FXAA_HLSL_5 == 1)\n"
				"struct FxaaTex { SamplerState smpl; Texture2D tex; };\n"
				"#define FxaaTexTop(t, p) t.tex.SampleLevel(t.smpl, p, 0.0)\n"
				"#define FxaaTexOff(t, p, o, r) t.tex.SampleLevel(t.smpl, p, 0.0, o)\n"
				"#define FxaaTexAlpha4(t, p) t.tex.GatherAlpha(t.smpl, p)\n"
				"#define FxaaTexOffAlpha4(t, p, o) t.tex.GatherAlpha(t.smpl, p, o)\n"
				"#define FxaaDiscard clip(-1)\n"
				"#define FxaaSat(x) saturate(x)\n"
				"\n"
				"#elif (FXAA_HLSL_4 == 1)\n"
				"struct FxaaTex { SamplerState smpl; Texture2D tex; };\n"
				"#define FxaaTexTop(t, p) t.tex.SampleLevel(t.smpl, p, 0.0)\n"
				"#define FxaaTexOff(t, p, o, r) t.tex.SampleLevel(t.smpl, p, 0.0, o)\n"
				"#define FxaaDiscard clip(-1)\n"
				"#define FxaaSat(x) saturate(x)\n"
				"\n"
				"#elif (FXAA_GLSL_130 == 1)\n"
				"#define int2 ivec2\n"
				"#define float2 vec2\n"
				"#define float3 vec3\n"
				"#define float4 vec4\n"
				"#define FxaaDiscard discard\n"
				"#define FxaaSat(x) clamp(x, 0.0, 1.0)\n"
				"#define FxaaTex sampler2D\n"
				"#define FxaaTexTop(t, p) textureLod(t, p, 0.0)\n"
				"#define FxaaTexOff(t, p, o, r) textureLodOffset(t, p, 0.0, o)\n"
				"\n"
				"#if (FXAA_GATHER4_ALPHA == 1)\n"
				"// use #extension GL_ARB_gpu_shader5 : enable\n"
				"#define FxaaTexAlpha4(t, p) textureGather(t, p, 3)\n"
				"#define FxaaTexOffAlpha4(t, p, o) textureGatherOffset(t, p, o, 3)\n"
				"#endif\n"
				"\n"
				"#endif\n"
				"\n"
				"#define FxaaEdgeThreshold 0.063\n"
				"#define FxaaEdgeThresholdMin 0.00\n"
				"#define FXAA_QUALITY__P0 1.0\n"
				"#define FXAA_QUALITY__P1 1.5\n"
				"#define FXAA_QUALITY__P2 2.0\n"
				"#define FXAA_QUALITY__P3 2.0\n"
				"#define FXAA_QUALITY__P4 2.0\n"
				"#define FXAA_QUALITY__P5 2.0\n"
				"#define FXAA_QUALITY__P6 2.0\n"
				"#define FXAA_QUALITY__P7 2.0\n"
				"#define FXAA_QUALITY__P8 2.0\n"
				"#define FXAA_QUALITY__P9 2.0\n"
				"#define FXAA_QUALITY__P10 4.0\n"
				"#define FXAA_QUALITY__P11 8.0\n"
				"#define FXAA_QUALITY__P12 8.0\n"
				"\n"
				"/*------------------------------------------------------------------------------\n"
				"                        [GAMMA PREPASS CODE SECTION]\n"
				"------------------------------------------------------------------------------*/\n"
				"float RGBLuminance(float3 color)\n"
				"{\n"
				"	const float3 lumCoeff = float3(0.2126729, 0.7151522, 0.0721750);\n"
				"	return dot(color.rgb, lumCoeff);\n"
				"}\n"
				"\n"
				"#if (FXAA_GLSL_130 == 0)\n"
				"#define PixelSize float2(_rcpFrame.x, _rcpFrame.y)\n"
				"#endif\n"
				"\n"
				"\n"
				"float3 RGBGammaToLinear(float3 color, float gamma)\n"
				"{\n"
				"	color = FxaaSat(color);\n"
				"	color.r = (color.r <= 0.0404482362771082) ?\n"
				"	color.r / 12.92 : pow((color.r + 0.055) / 1.055, gamma);\n"
				"	color.g = (color.g <= 0.0404482362771082) ?\n"
				"	color.g / 12.92 : pow((color.g + 0.055) / 1.055, gamma);\n"
				"	color.b = (color.b <= 0.0404482362771082) ?\n"
				"	color.b / 12.92 : pow((color.b + 0.055) / 1.055, gamma);\n"
				"\n"
				"	return color;\n"
				"}\n"
				"\n"
				"float3 LinearToRGBGamma(float3 color, float gamma)\n"
				"{\n"
				"	color = FxaaSat(color);\n"
				"	color.r = (color.r <= 0.00313066844250063) ?\n"
				"	color.r * 12.92 : 1.055 * pow(color.r, 1.0 / gamma) - 0.055;\n"
				"	color.g = (color.g <= 0.00313066844250063) ?\n"
				"	color.g * 12.92 : 1.055 * pow(color.g, 1.0 / gamma) - 0.055;\n"
				"	color.b = (color.b <= 0.00313066844250063) ?\n"
				"	color.b * 12.92 : 1.055 * pow(color.b, 1.0 / gamma) - 0.055;\n"
				"\n"
				"	return color;\n"
				"}\n"
				"\n"
				"float4 PreGammaPass(float4 color, float2 uv0)\n"
				"{\n"
				"	#if (SHADER_MODEL >= 0x400)\n"
				"		color = Texture.Sample(TextureSampler, uv0);\n"
				"	#elif (FXAA_GLSL_130 == 1)\n"
				"		color = texture(TextureSampler, uv0);\n"
				"	#endif\n"
				"\n"
				"	const float GammaConst = 2.233;\n"
				"	color.rgb = RGBGammaToLinear(color.rgb, GammaConst);\n"
				"	color.rgb = LinearToRGBGamma(color.rgb, GammaConst);\n"
				"	color.a = RGBLuminance(color.rgb);\n"
				"\n"
				"	return color;\n"
				"}\n"
				"\n"
				"\n"
				"/*------------------------------------------------------------------------------\n"
				"                        [FXAA CODE SECTION]\n"
				"------------------------------------------------------------------------------*/\n"
				"\n"
				"float FxaaLuma(float4 rgba)\n"
				"{ \n"
				"	rgba.w = RGBLuminance(rgba.xyz);\n"
				"	return rgba.w; \n"
				"}\n"
				"\n"
				"float4 FxaaPixelShader(float2 pos, FxaaTex tex, float2 fxaaRcpFrame, float fxaaSubpix, float fxaaEdgeThreshold, float fxaaEdgeThresholdMin)\n"
				"{\n"
				"	float2 posM;\n"
				"	posM.x = pos.x;\n"
				"	posM.y = pos.y;\n"
				"\n"
				"	#if (FXAA_GATHER4_ALPHA == 1)\n"
				"	float4 rgbyM = FxaaTexTop(tex, posM);\n"
				"	float4 luma4A = FxaaTexAlpha4(tex, posM);\n"
				"	float4 luma4B = FxaaTexOffAlpha4(tex, posM, int2(-1, -1));\n"
				"	rgbyM.w = RGBLuminance(rgbyM.xyz);\n"
				"\n"
				"	#define lumaM rgbyM.w\n"
				"	#define lumaE luma4A.z\n"
				"	#define lumaS luma4A.x\n"
				"	#define lumaSE luma4A.y\n"
				"	#define lumaNW luma4B.w\n"
				"	#define lumaN luma4B.z\n"
				"	#define lumaW luma4B.x\n"
				"\n"
				"	#else\n"
				"	float4 rgbyM = FxaaTexTop(tex, posM);\n"
				"	rgbyM.w = RGBLuminance(rgbyM.xyz);\n"
				"	#define lumaM rgbyM.w\n"
				"\n"
				"	float lumaS = FxaaLuma(FxaaTexOff(tex, posM, int2( 0, 1), fxaaRcpFrame.xy));\n"
				"	float lumaE = FxaaLuma(FxaaTexOff(tex, posM, int2( 1, 0), fxaaRcpFrame.xy));\n"
				"	float lumaN = FxaaLuma(FxaaTexOff(tex, posM, int2( 0,-1), fxaaRcpFrame.xy));\n"
				"	float lumaW = FxaaLuma(FxaaTexOff(tex, posM, int2(-1, 0), fxaaRcpFrame.xy));\n"
				"	#endif\n"
				"\n"
				"	float maxSM = max(lumaS, lumaM);\n"
				"	float minSM = min(lumaS, lumaM);\n"
				"	float maxESM = max(lumaE, maxSM);\n"
				"	float minESM = min(lumaE, minSM);\n"
				"	float maxWN = max(lumaN, lumaW);\n"
				"	float minWN = min(lumaN, lumaW);\n"
				"\n"
				"	float rangeMax = max(maxWN, maxESM);\n"
				"	float rangeMin = min(minWN, minESM);\n"
				"	float range = rangeMax - rangeMin;\n"
				"	float rangeMaxScaled = rangeMax * fxaaEdgeThreshold;\n"
				"	float rangeMaxClamped = max(fxaaEdgeThresholdMin, rangeMaxScaled);\n"
				"\n"
				"	bool earlyExit = range < rangeMaxClamped;\n"
				"	#if (FxaaEarlyExit == 1)\n"
				"	if(earlyExit) { return rgbyM; }\n"
				"	#endif\n"
				"\n"
				"	#if (FXAA_GATHER4_ALPHA == 0)\n"
				"	float lumaNW = FxaaLuma(FxaaTexOff(tex, posM, int2(-1,-1), fxaaRcpFrame.xy));\n"
				"	float lumaSE = FxaaLuma(FxaaTexOff(tex, posM, int2( 1, 1), fxaaRcpFrame.xy));\n"
				"	float lumaNE = FxaaLuma(FxaaTexOff(tex, posM, int2( 1,-1), fxaaRcpFrame.xy));\n"
				"	float lumaSW = FxaaLuma(FxaaTexOff(tex, posM, int2(-1, 1), fxaaRcpFrame.xy));\n"
				"	#else\n"
				"	float lumaNE = FxaaLuma(FxaaTexOff(tex, posM, int2( 1,-1), fxaaRcpFrame.xy));\n"
				"	float lumaSW = FxaaLuma(FxaaTexOff(tex, posM, int2(-1, 1), fxaaRcpFrame.xy));\n"
				"	#endif\n"
				"\n"
				"	float lumaNS = lumaN + lumaS;\n"
				"	float lumaWE = lumaW + lumaE;\n"
				"	float subpixRcpRange = 1.0/range;\n"
				"	float subpixNSWE = lumaNS + lumaWE;\n"
				"	float edgeHorz1 = (-2.0 * lumaM) + lumaNS;\n"
				"	float edgeVert1 = (-2.0 * lumaM) + lumaWE;\n"
				"	float lumaNESE = lumaNE + lumaSE;\n"
				"	float lumaNWNE = lumaNW + lumaNE;\n"
				"	float edgeHorz2 = (-2.0 * lumaE) + lumaNESE;\n"
				"	float edgeVert2 = (-2.0 * lumaN) + lumaNWNE;\n"
				"\n"
				"	float lumaNWSW = lumaNW + lumaSW;\n"
				"	float lumaSWSE = lumaSW + lumaSE;\n"
				"	float edgeHorz4 = (abs(edgeHorz1) * 2.0) + abs(edgeHorz2);\n"
				"	float edgeVert4 = (abs(edgeVert1) * 2.0) + abs(edgeVert2);\n"
				"	float edgeHorz3 = (-2.0 * lumaW) + lumaNWSW;\n"
				"	float edgeVert3 = (-2.0 * lumaS) + lumaSWSE;\n"
				"	float edgeHorz = abs(edgeHorz3) + edgeHorz4;\n"
				"	float edgeVert = abs(edgeVert3) + edgeVert4;\n"
				"\n"
				"	float subpixNWSWNESE = lumaNWSW + lumaNESE;\n"
				"	float lengthSign = fxaaRcpFrame.x;\n"
				"	bool horzSpan = edgeHorz >= edgeVert;\n"
				"	float subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;\n"
				"	if(!horzSpan) lumaN = lumaW;\n"
				"	if(!horzSpan) lumaS = lumaE;\n"
				"	if(horzSpan) lengthSign = fxaaRcpFrame.y;\n"
				"	float subpixB = (subpixA * (1.0/12.0)) - lumaM;\n"
				"\n"
				"	float gradientN = lumaN - lumaM;\n"
				"	float gradientS = lumaS - lumaM;\n"
				"	float lumaNN = lumaN + lumaM;\n"
				"	float lumaSS = lumaS + lumaM;\n"
				"	bool pairN = abs(gradientN) >= abs(gradientS);\n"
				"	float gradient = max(abs(gradientN), abs(gradientS));\n"
				"	if(pairN) lengthSign = -lengthSign;\n"
				"	float subpixC = FxaaSat(abs(subpixB) * subpixRcpRange);\n"
				"\n"
				"	float2 posB;\n"
				"	posB.x = posM.x;\n"
				"	posB.y = posM.y;\n"
				"	float2 offNP;\n"
				"	offNP.x = (!horzSpan) ? 0.0 : fxaaRcpFrame.x;\n"
				"	offNP.y = ( horzSpan) ? 0.0 : fxaaRcpFrame.y;\n"
				"	if(!horzSpan) posB.x += lengthSign * 0.5;\n"
				"	if( horzSpan) posB.y += lengthSign * 0.5;\n"
				"\n"
				"	float2 posN;\n"
				"	posN.x = posB.x - offNP.x * FXAA_QUALITY__P0;\n"
				"	posN.y = posB.y - offNP.y * FXAA_QUALITY__P0;\n"
				"	float2 posP;\n"
				"	posP.x = posB.x + offNP.x * FXAA_QUALITY__P0;\n"
				"	posP.y = posB.y + offNP.y * FXAA_QUALITY__P0;\n"
				"	float subpixD = ((-2.0)*subpixC) + 3.0;\n"
				"	float lumaEndN = FxaaLuma(FxaaTexTop(tex, posN));\n"
				"	float subpixE = subpixC * subpixC;\n"
				"	float lumaEndP = FxaaLuma(FxaaTexTop(tex, posP));\n"
				"\n"
				"	if(!pairN) lumaNN = lumaSS;\n"
				"	float gradientScaled = gradient * 1.0/4.0;\n"
				"	float lumaMM = lumaM - lumaNN * 0.5;\n"
				"	float subpixF = subpixD * subpixE;\n"
				"	bool lumaMLTZero = lumaMM < 0.0;\n"
				"	lumaEndN -= lumaNN * 0.5;\n"
				"	lumaEndP -= lumaNN * 0.5;\n"
				"	bool doneN = abs(lumaEndN) >= gradientScaled;\n"
				"	bool doneP = abs(lumaEndP) >= gradientScaled;\n"
				"	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P1;\n"
				"	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P1;\n"
				"	bool doneNP = (!doneN) || (!doneP);\n"
				"	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P1;\n"
				"	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P1;\n"
				"\n"
				"	if(doneNP) {\n"
				"	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));\n"
				"	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));\n"
				"	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;\n"
				"	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;\n"
				"	doneN = abs(lumaEndN) >= gradientScaled;\n"
				"	doneP = abs(lumaEndP) >= gradientScaled;\n"
				"	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P2;\n"
				"	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P2;\n"
				"	doneNP = (!doneN) || (!doneP);\n"
				"	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P2;\n"
				"	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P2;\n"
				"\n"
				"	if(doneNP) {\n"
				"	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));\n"
				"	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));\n"
				"	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;\n"
				"	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;\n"
				"	doneN = abs(lumaEndN) >= gradientScaled;\n"
				"	doneP = abs(lumaEndP) >= gradientScaled;\n"
				"	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P3;\n"
				"	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P3;\n"
				"	doneNP = (!doneN) || (!doneP);\n"
				"	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P3;\n"
				"	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P3;\n"
				"\n"
				"	if(doneNP) {\n"
				"	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));\n"
				"	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));\n"
				"	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;\n"
				"	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;\n"
				"	doneN = abs(lumaEndN) >= gradientScaled;\n"
				"	doneP = abs(lumaEndP) >= gradientScaled;\n"
				"	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P4;\n"
				"	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P4;\n"
				"	doneNP = (!doneN) || (!doneP);\n"
				"	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P4;\n"
				"	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P4;\n"
				"\n"
				"	if(doneNP) {\n"
				"	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));\n"
				"	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));\n"
				"	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;\n"
				"	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;\n"
				"	doneN = abs(lumaEndN) >= gradientScaled;\n"
				"	doneP = abs(lumaEndP) >= gradientScaled;\n"
				"	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P5;\n"
				"	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P5;\n"
				"	doneNP = (!doneN) || (!doneP);\n"
				"	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P5;\n"
				"	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P5;\n"
				"\n"
				"	if(doneNP) {\n"
				"	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));\n"
				"	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));\n"
				"	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;\n"
				"	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;\n"
				"	doneN = abs(lumaEndN) >= gradientScaled;\n"
				"	doneP = abs(lumaEndP) >= gradientScaled;\n"
				"	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P6;\n"
				"	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P6;\n"
				"	doneNP = (!doneN) || (!doneP);\n"
				"	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P6;\n"
				"	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P6;\n"
				"\n"
				"	if(doneNP) {\n"
				"	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));\n"
				"	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));\n"
				"	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;\n"
				"	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;\n"
				"	doneN = abs(lumaEndN) >= gradientScaled;\n"
				"	doneP = abs(lumaEndP) >= gradientScaled;\n"
				"	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P7;\n"
				"	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P7;\n"
				"	doneNP = (!doneN) || (!doneP);\n"
				"	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P7;\n"
				"	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P7;\n"
				"\n"
				"	if(doneNP) {\n"
				"	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));\n"
				"	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));\n"
				"	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;\n"
				"	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;\n"
				"	doneN = abs(lumaEndN) >= gradientScaled;\n"
				"	doneP = abs(lumaEndP) >= gradientScaled;\n"
				"	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P8;\n"
				"	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P8;\n"
				"	doneNP = (!doneN) || (!doneP);\n"
				"	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P8;\n"
				"	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P8;\n"
				"\n"
				"	if(doneNP) {\n"
				"	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));\n"
				"	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));\n"
				"	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;\n"
				"	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;\n"
				"	doneN = abs(lumaEndN) >= gradientScaled;\n"
				"	doneP = abs(lumaEndP) >= gradientScaled;\n"
				"	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P9;\n"
				"	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P9;\n"
				"	doneNP = (!doneN) || (!doneP);\n"
				"	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P9;\n"
				"	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P9;\n"
				"\n"
				"	if(doneNP) {\n"
				"	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));\n"
				"	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));\n"
				"	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;\n"
				"	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;\n"
				"	doneN = abs(lumaEndN) >= gradientScaled;\n"
				"	doneP = abs(lumaEndP) >= gradientScaled;\n"
				"	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P10;\n"
				"	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P10;\n"
				"	doneNP = (!doneN) || (!doneP);\n"
				"	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P10;\n"
				"	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P10;\n"
				"\n"
				"	if(doneNP) {\n"
				"	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));\n"
				"	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));\n"
				"	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;\n"
				"	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;\n"
				"	doneN = abs(lumaEndN) >= gradientScaled;\n"
				"	doneP = abs(lumaEndP) >= gradientScaled;\n"
				"	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P11;\n"
				"	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P11;\n"
				"	doneNP = (!doneN) || (!doneP);\n"
				"	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P11;\n"
				"	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P11;\n"
				"\n"
				"	if(doneNP) {\n"
				"	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));\n"
				"	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));\n"
				"	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;\n"
				"	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;\n"
				"	doneN = abs(lumaEndN) >= gradientScaled;\n"
				"	doneP = abs(lumaEndP) >= gradientScaled;\n"
				"	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P12;\n"
				"	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P12;\n"
				"	doneNP = (!doneN) || (!doneP);\n"
				"	if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P12;\n"
				"	if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P12;\n"
				"	}}}}}}}}}}}\n"
				"\n"
				"	float dstN = posM.x - posN.x;\n"
				"	float dstP = posP.x - posM.x;\n"
				"	if(!horzSpan) dstN = posM.y - posN.y;\n"
				"	if(!horzSpan) dstP = posP.y - posM.y;\n"
				"\n"
				"	bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;\n"
				"	float spanLength = (dstP + dstN);\n"
				"	bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;\n"
				"	float spanLengthRcp = 1.0/spanLength;\n"
				"\n"
				"	bool directionN = dstN < dstP;\n"
				"	float dst = min(dstN, dstP);\n"
				"	bool goodSpan = directionN ? goodSpanN : goodSpanP;\n"
				"	float subpixG = subpixF * subpixF;\n"
				"	float pixelOffset = (dst * (-spanLengthRcp)) + 0.5;\n"
				"	float subpixH = subpixG * fxaaSubpix;\n"
				"\n"
				"	float pixelOffsetGood = goodSpan ? pixelOffset : 0.0;\n"
				"	float pixelOffsetSubpix = max(pixelOffsetGood, subpixH);\n"
				"	if(!horzSpan) posM.x += pixelOffsetSubpix * lengthSign;\n"
				"	if( horzSpan) posM.y += pixelOffsetSubpix * lengthSign;\n"
				"\n"
				"	return float4(FxaaTexTop(tex, posM).xyz, lumaM);\n"
				"}\n"
				"\n"
				"#if (FXAA_GLSL_130 == 1)\n"
				"float4 FxaaPass(float4 FxaaColor, float2 uv0)\n"
				"#elif (SHADER_MODEL >= 0x400)\n"
				"float4 FxaaPass(float4 FxaaColor : COLOR0, float2 uv0 : TEXCOORD0)\n"
				"#endif\n"
				"{\n"
				"\n"
				"	#if (SHADER_MODEL >= 0x400)\n"
				"	FxaaTex tex;\n"
				"	tex.tex = Texture;\n"
				"	tex.smpl = TextureSampler;\n"
				"\n"
				"	Texture.GetDimensions(PixelSize.x, PixelSize.y);\n"
				"	FxaaColor = FxaaPixelShader(uv0, tex, 1.0/PixelSize.xy, FxaaSubpixMax, FxaaEdgeThreshold, FxaaEdgeThresholdMin);\n"
				"\n"
				"	#elif (FXAA_GLSL_130 == 1)\n"
				"	vec2 PixelSize = textureSize(TextureSampler, 0);\n"
				"	FxaaColor = FxaaPixelShader(uv0, TextureSampler, 1.0/PixelSize.xy, FxaaSubpixMax, FxaaEdgeThreshold, FxaaEdgeThresholdMin);\n"
				"	#endif\n"
				"\n"
				"	return FxaaColor;\n"
				"}\n"
				"\n"
				"/*------------------------------------------------------------------------------\n"
				"                      [MAIN() & COMBINE PASS CODE SECTION]\n"
				"------------------------------------------------------------------------------*/\n"
				"#if (FXAA_GLSL_130 == 1)\n"
				"\n"
				"void ps_main()\n"
				"{\n"
				"	vec4 color = texture(TextureSampler, PSin.t);\n"
				"	color      = PreGammaPass(color, PSin.t);\n"
				"	color      = FxaaPass(color, PSin.t);\n"
				"\n"
				"	SV_Target0 = color;\n"
				"}\n"
				"\n"
				"#elif (SHADER_MODEL >= 0x400)\n"
				"PS_OUTPUT ps_main(VS_OUTPUT input)\n"
				"{\n"
				"	PS_OUTPUT output;\n"
				"\n"
				"	float4 color = Texture.Sample(TextureSampler, input.t);\n"
				"\n"
				"	color = PreGammaPass(color, input.t);\n"
				"	color = FxaaPass(color, input.t);\n"
				"\n"
				"	output.c = color;\n"
				"	\n"
				"	return output;\n"
				"}\n"
				"\n"
				"#endif\n"
				"\n"
				"#endif\n"
				;
			std::vector<char> shader(shader_raw, shader_raw + sizeof(shader_raw)/sizeof(*shader_raw));
			ShaderMacro sm(m_shader.model);
			CreateShader(shader, "fxaa.fx", nullptr, "ps_main", sm.GetPtr(), &m_fxaa.ps);
		}
		FXAA_Compiled = true;
	}
}

void GSDevice11::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
	GSVector2i s = dTex->GetSize();

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0, 0, s.x, s.y);

	FXAAConstantBuffer cb;

	InitFXAA();

	cb.rcpFrame = GSVector4(1.0f / s.x, 1.0f / s.y, 0.0f, 0.0f);
	cb.rcpFrameOpt = GSVector4::zero();

	m_ctx->UpdateSubresource(m_fxaa.cb, 0, NULL, &cb, 0, 0);

	StretchRect(sTex, sRect, dTex, dRect, m_fxaa.ps, m_fxaa.cb, true);

	//sTex->Save("c:\\temp1\\1.bmp");
	//dTex->Save("c:\\temp1\\2.bmp");
}

void GSDevice11::SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm)
{
	// sfex3 (after the capcom logo), vf4 (first menu fading in), ffxii shadows, rumble roses shadows, persona4 shadows

	BeginScene();

	ClearStencil(ds, 0);

	// om

	OMSetDepthStencilState(m_date.dss, 1);
	OMSetBlendState(m_date.bs, 0);
	OMSetRenderTargets(NULL, ds);

	// ia

	IASetVertexBuffer(vertices, sizeof(vertices[0]), 4);
	IASetInputLayout(m_convert.il);
	IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// vs

	VSSetShader(m_convert.vs, NULL);

	// gs

	GSSetShader(NULL, NULL);

	// ps
	PSSetShaderResources(rt, NULL);
	PSSetSamplerState(m_convert.pt, NULL);
	PSSetShader(m_convert.ps[datm ? ShaderConvert_DATM_1 : ShaderConvert_DATM_0], NULL);

	//

	DrawPrimitive();

	//

	EndScene();
}

void GSDevice11::IASetVertexBuffer(const void* vertex, size_t stride, size_t count)
{
	void* ptr = NULL;

	if(IAMapVertexBuffer(&ptr, stride, count))
	{
		GSVector4i::storent(ptr, vertex, count * stride);

		IAUnmapVertexBuffer();
	}
}

bool GSDevice11::IAMapVertexBuffer(void** vertex, size_t stride, size_t count)
{
	ASSERT(m_vertex.count == 0);

	if(count * stride > m_vertex.limit * m_vertex.stride)
	{
		m_vb_old = m_vb;
		m_vb = NULL;

		m_vertex.start = 0;
		m_vertex.limit = std::max<int>(count * 3 / 2, 11000);
	}

	if(m_vb == NULL)
	{
		D3D11_BUFFER_DESC bd;

		memset(&bd, 0, sizeof(bd));

		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = m_vertex.limit * stride;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT hr;

		hr = m_dev->CreateBuffer(&bd, NULL, &m_vb);

		if(FAILED(hr)) return false;
	}

	D3D11_MAP type = D3D11_MAP_WRITE_NO_OVERWRITE;

	if(m_vertex.start + count > m_vertex.limit || stride != m_vertex.stride)
	{
		m_vertex.start = 0;

		type = D3D11_MAP_WRITE_DISCARD;
	}

	D3D11_MAPPED_SUBRESOURCE m;

	if(FAILED(m_ctx->Map(m_vb, 0, type, 0, &m)))
	{
		return false;
	}

	*vertex = (uint8*)m.pData + m_vertex.start * stride;

	m_vertex.count = count;
	m_vertex.stride = stride;

	return true;
}

void GSDevice11::IAUnmapVertexBuffer()
{
	m_ctx->Unmap(m_vb, 0);

	IASetVertexBuffer(m_vb, m_vertex.stride);
}

void GSDevice11::IASetVertexBuffer(ID3D11Buffer* vb, size_t stride)
{
	if(m_state.vb != vb || m_state.vb_stride != stride)
	{
		m_state.vb = vb;
		m_state.vb_stride = stride;

		uint32 stride2 = stride;
		uint32 offset = 0;

		m_ctx->IASetVertexBuffers(0, 1, &vb, &stride2, &offset);
	}
}

void GSDevice11::IASetIndexBuffer(const void* index, size_t count)
{
	ASSERT(m_index.count == 0);

	if(count > m_index.limit)
	{
		m_ib_old = m_ib;
		m_ib = NULL;

		m_index.start = 0;
		m_index.limit = std::max<int>(count * 3 / 2, 11000);
	}

	if(m_ib == NULL)
	{
		D3D11_BUFFER_DESC bd;

		memset(&bd, 0, sizeof(bd));

		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = m_index.limit * sizeof(uint32);
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT hr;

		hr = m_dev->CreateBuffer(&bd, NULL, &m_ib);

		if(FAILED(hr)) return;
	}

	D3D11_MAP type = D3D11_MAP_WRITE_NO_OVERWRITE;

	if(m_index.start + count > m_index.limit)
	{
		m_index.start = 0;

		type = D3D11_MAP_WRITE_DISCARD;
	}

	D3D11_MAPPED_SUBRESOURCE m;

	if(SUCCEEDED(m_ctx->Map(m_ib, 0, type, 0, &m)))
	{
		memcpy((uint8*)m.pData + m_index.start * sizeof(uint32), index, count * sizeof(uint32));

		m_ctx->Unmap(m_ib, 0);
	}

	m_index.count = count;

	IASetIndexBuffer(m_ib);
}

void GSDevice11::IASetIndexBuffer(ID3D11Buffer* ib)
{
	if(m_state.ib != ib)
	{
		m_state.ib = ib;

		m_ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
	}
}

void GSDevice11::IASetInputLayout(ID3D11InputLayout* layout)
{
	if(m_state.layout != layout)
	{
		m_state.layout = layout;

		m_ctx->IASetInputLayout(layout);
	}
}

void GSDevice11::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology)
{
	if(m_state.topology != topology)
	{
		m_state.topology = topology;

		m_ctx->IASetPrimitiveTopology(topology);
	}
}

void GSDevice11::VSSetShader(ID3D11VertexShader* vs, ID3D11Buffer* vs_cb)
{
	if(m_state.vs != vs)
	{
		m_state.vs = vs;

		m_ctx->VSSetShader(vs, NULL, 0);
	}

	if(m_state.vs_cb != vs_cb)
	{
		m_state.vs_cb = vs_cb;

		m_ctx->VSSetConstantBuffers(0, 1, &vs_cb);
	}
}

void GSDevice11::GSSetShader(ID3D11GeometryShader* gs, ID3D11Buffer* gs_cb)
{
	if(m_state.gs != gs)
	{
		m_state.gs = gs;

		m_ctx->GSSetShader(gs, NULL, 0);
	}

	if (m_state.gs_cb != gs_cb)
	{
		m_state.gs_cb = gs_cb;

		m_ctx->GSSetConstantBuffers(0, 1, &gs_cb);
	}
}

void GSDevice11::PSSetShaderResources(GSTexture* sr0, GSTexture* sr1)
{
	PSSetShaderResource(0, sr0);
	PSSetShaderResource(1, sr1);

	for(size_t i = 2; i < m_state.ps_sr_views.size(); i++)
	{
		PSSetShaderResource(i, NULL);
	}
}

void GSDevice11::PSSetShaderResource(int i, GSTexture* sr)
{
	ID3D11ShaderResourceView* srv = NULL;

	if(sr) srv = *(GSTexture11*)sr;

	PSSetShaderResourceView(i, srv, sr);
}

void GSDevice11::PSSetShaderResourceView(int i, ID3D11ShaderResourceView* srv, GSTexture* sr)
{
	ASSERT(i < (int)m_state.ps_sr_views.size());

	if(m_state.ps_sr_views[i] != srv)
	{
		m_state.ps_sr_views[i] = srv;
		m_state.ps_sr_texture[i] = (GSTexture11*)sr;
		srv ? m_state.ps_sr_bitfield |= 1u << i : m_state.ps_sr_bitfield &= ~(1u << i);
	}
}

void GSDevice11::PSSetSamplerState(ID3D11SamplerState* ss0, ID3D11SamplerState* ss1)
{
	if(m_state.ps_ss[0] != ss0 || m_state.ps_ss[1] != ss1)
	{
		m_state.ps_ss[0] = ss0;
		m_state.ps_ss[1] = ss1;
	}
}

void GSDevice11::PSSetShader(ID3D11PixelShader* ps, ID3D11Buffer* ps_cb)
{
	if(m_state.ps != ps)
	{
		m_state.ps = ps;

		m_ctx->PSSetShader(ps, NULL, 0);
	}

	if(m_state.ps_cb != ps_cb)
	{
		m_state.ps_cb = ps_cb;

		m_ctx->PSSetConstantBuffers(0, 1, &ps_cb);
	}
}

void GSDevice11::PSUpdateShaderState()
{
	m_ctx->PSSetShaderResources(0, m_state.ps_sr_views.size(), m_state.ps_sr_views.data());
	m_ctx->PSSetSamplers(0, countof(m_state.ps_ss), m_state.ps_ss);
}

void GSDevice11::OMSetDepthStencilState(ID3D11DepthStencilState* dss, uint8 sref)
{
	if(m_state.dss != dss || m_state.sref != sref)
	{
		m_state.dss = dss;
		m_state.sref = sref;

		m_ctx->OMSetDepthStencilState(dss, sref);
	}
}

void GSDevice11::OMSetBlendState(ID3D11BlendState* bs, float bf)
{
	if(m_state.bs != bs || m_state.bf != bf)
	{
		m_state.bs = bs;
		m_state.bf = bf;

		float BlendFactor[] = {bf, bf, bf, 0};

		m_ctx->OMSetBlendState(bs, BlendFactor, 0xffffffff);
	}
}

void GSDevice11::OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor)
{
	ID3D11RenderTargetView* rtv = NULL;
	ID3D11DepthStencilView* dsv = NULL;

	if (!rt && !ds)
		throw GSDXRecoverableError();

	if(rt) rtv = *(GSTexture11*)rt;
	if(ds) dsv = *(GSTexture11*)ds;

	if(m_state.rt_view != rtv || m_state.dsv != dsv)
	{
		m_state.rt_view = rtv;
		m_state.rt_texture = static_cast<GSTexture11*>(rt);
		m_state.dsv = dsv;
		m_state.rt_ds = static_cast<GSTexture11*>(ds);

		m_ctx->OMSetRenderTargets(1, &rtv, dsv);
	}

	GSVector2i size = rt ? rt->GetSize() : ds->GetSize();
	if(m_state.viewport != size)
	{
		m_state.viewport = size;

		D3D11_VIEWPORT vp;
		memset(&vp, 0, sizeof(vp));

		vp.TopLeftX = m_hack_topleft_offset;
		vp.TopLeftY = m_hack_topleft_offset;
		vp.Width = (float)size.x;
		vp.Height = (float)size.y;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;

		m_ctx->RSSetViewports(1, &vp);
	}

	GSVector4i r = scissor ? *scissor : GSVector4i(size).zwxy();

	if(!m_state.scissor.eq(r))
	{
		m_state.scissor = r;

		m_ctx->RSSetScissorRects(1, r);
	}
}

GSDevice11::ShaderMacro::ShaderMacro(std::string& smodel)
{
	mlist.emplace_back("SHADER_MODEL", smodel);
}

void GSDevice11::ShaderMacro::AddMacro(const char* n, int d)
{
	mlist.emplace_back(n, std::to_string(d));
}

D3D_SHADER_MACRO* GSDevice11::ShaderMacro::GetPtr(void)
{
	mout.clear();

	for (auto& i : mlist)
		mout.emplace_back(i.name.c_str(), i.def.c_str());

	mout.emplace_back(nullptr, nullptr);
	return (D3D_SHADER_MACRO*)mout.data();
}

void GSDevice11::CreateShader(std::vector<char> source, const char* fn, ID3DInclude *include, const char* entry, D3D_SHADER_MACRO* macro, ID3D11VertexShader** vs, D3D11_INPUT_ELEMENT_DESC* layout, int count, ID3D11InputLayout** il)
{
	HRESULT hr;

	CComPtr<ID3DBlob> shader;

	CompileShader(source, fn, include, entry, macro, &shader, m_shader.vs);

	hr = m_dev->CreateVertexShader((void*)shader->GetBufferPointer(), shader->GetBufferSize(), NULL, vs);

	if(FAILED(hr))
	{
		throw GSDXRecoverableError();
	}

	hr = m_dev->CreateInputLayout(layout, count, shader->GetBufferPointer(), shader->GetBufferSize(), il);

	if(FAILED(hr))
	{
		throw GSDXRecoverableError();
	}
}

void GSDevice11::CreateShader(std::vector<char> source, const char* fn, ID3DInclude *include, const char* entry, D3D_SHADER_MACRO* macro, ID3D11GeometryShader** gs)
{
	HRESULT hr;

	CComPtr<ID3DBlob> shader;

	CompileShader(source, fn, include, entry, macro, &shader, m_shader.gs);

	hr = m_dev->CreateGeometryShader((void*)shader->GetBufferPointer(), shader->GetBufferSize(), NULL, gs);

	if(FAILED(hr))
	{
		throw GSDXRecoverableError();
	}
}

void GSDevice11::CreateShader(std::vector<char> source, const char* fn, ID3DInclude *include, const char* entry, D3D_SHADER_MACRO* macro, ID3D11PixelShader** ps)
{
	HRESULT hr;

	CComPtr<ID3DBlob> shader;

	CompileShader(source, fn, include, entry, macro, &shader, m_shader.ps);

	hr = m_dev->CreatePixelShader((void*)shader->GetBufferPointer(), shader->GetBufferSize(), NULL, ps);

	if(FAILED(hr))
	{
		throw GSDXRecoverableError();
	}
}

void GSDevice11::CompileShader(std::vector<char> source, const char* fn, ID3DInclude *include, const char* entry, D3D_SHADER_MACRO* macro, ID3DBlob** shader, std::string shader_model)
{
	CComPtr<ID3DBlob> error;

	UINT flags = 0;

#ifdef _DEBUG
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_AVOID_FLOW_CONTROL;
#endif

	const HRESULT hr = D3DCompile(
		source.data(), source.size(), fn, macro,
		include, entry, shader_model.c_str(),
		flags, 0, shader, &error
	);

	if (error)
		fprintf(stderr, "%s\n", (const char*)error->GetBufferPointer());

	if (FAILED(hr))
		throw GSDXRecoverableError();
}

uint16 GSDevice11::ConvertBlendEnum(uint16 generic)
{
	switch (generic)
	{
	case SRC_COLOR       : return D3D11_BLEND_SRC_COLOR;
	case INV_SRC_COLOR   : return D3D11_BLEND_INV_SRC_COLOR;
	case DST_COLOR       : return D3D11_BLEND_DEST_COLOR;
	case INV_DST_COLOR   : return D3D11_BLEND_INV_DEST_COLOR;
	case SRC1_COLOR      : return D3D11_BLEND_SRC1_COLOR;
	case INV_SRC1_COLOR  : return D3D11_BLEND_INV_SRC1_COLOR;
	case SRC_ALPHA       : return D3D11_BLEND_SRC_ALPHA;
	case INV_SRC_ALPHA   : return D3D11_BLEND_INV_SRC_ALPHA;
	case DST_ALPHA       : return D3D11_BLEND_DEST_ALPHA;
	case INV_DST_ALPHA   : return D3D11_BLEND_INV_DEST_ALPHA;
	case SRC1_ALPHA      : return D3D11_BLEND_SRC1_ALPHA;
	case INV_SRC1_ALPHA  : return D3D11_BLEND_INV_SRC1_ALPHA;
	case CONST_COLOR     : return D3D11_BLEND_BLEND_FACTOR;
	case INV_CONST_COLOR : return D3D11_BLEND_INV_BLEND_FACTOR;
	case CONST_ONE       : return D3D11_BLEND_ONE;
	case CONST_ZERO      : return D3D11_BLEND_ZERO;
	case OP_ADD          : return D3D11_BLEND_OP_ADD;
	case OP_SUBTRACT     : return D3D11_BLEND_OP_SUBTRACT;
	case OP_REV_SUBTRACT : return D3D11_BLEND_OP_REV_SUBTRACT;
	default              : ASSERT(0); return 0;
	}
}
