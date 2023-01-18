/*
 *	Copyright (C) 2011-2016 PCSX2 Dev Team
 *	Copyright (C) 2007-2009 Gabest
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

#include "../../GSState.h"
#include "GSDeviceOGL.h"
#include "GLState.h"
#include "../../GSUtil.h"
#include "options_tools.h"
#include "../Common/fxaa_shader.h"
#include "../Common/GSRenderer.h"
#include <libretro.h>

extern retro_video_refresh_t video_cb;

/* Merge shader */
static const char merge_glsl_shader_raw[] =
"//#version 420 // Keep it for editor detection\n"
"\n"
"in SHADER\n"
"{\n"
"    vec4 p;\n"
"    vec2 t;\n"
"    vec4 c;\n"
"} PSin;\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"\n"
"layout(std140, binding = 10) uniform cb10\n"
"{\n"
"    vec4 BGColor;\n"
"};\n"
"\n"
"layout(location = 0) out vec4 SV_Target0;\n"
"\n"
"void ps_main0()\n"
"{\n"
"    vec4 c = texture(TextureSampler, PSin.t);\n"
"    // Note: clamping will be done by fixed unit\n"
"    c.a *= 2.0f;\n"
"    SV_Target0 = c;\n"
"}\n"
"\n"
"void ps_main1()\n"
"{\n"
"    vec4 c = texture(TextureSampler, PSin.t);\n"
"    c.a = BGColor.a;\n"
"    SV_Target0 = c;\n"
"}\n"
"\n"
"#endif\n"
;

/* Interlace shader */

static const char interlace_glsl_shader_raw[] =
"//#version 420 // Keep it for editor detection\n"
"\n"
"in SHADER\n"
"{\n"
"    vec4 p;\n"
"    vec2 t;\n"
"    vec4 c;\n"
"} PSin;\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"\n"
"layout(std140, binding = 11) uniform cb11\n"
"{\n"
"    vec2 ZrH;\n"
"    float hH;\n"
"};\n"
"\n"
"layout(location = 0) out vec4 SV_Target0;\n"
"\n"
"// TODO ensure that clip (discard) is < 0 and not <= 0 ???\n"
"void ps_main0()\n"
"{\n"
"    if (fract(PSin.t.y * hH) - 0.5 < 0.0)\n"
"        discard;\n"
"    // I'm not sure it impact us but be safe to lookup texture before conditional if\n"
"    // see: http://www.opengl.org/wiki/GLSL_Sampler#Non-uniform_flow_control\n"
"    vec4 c = texture(TextureSampler, PSin.t);\n"
"\n"
"    SV_Target0 = c;\n"
"}\n"
"\n"
"void ps_main1()\n"
"{\n"
"    if (0.5 - fract(PSin.t.y * hH) < 0.0)\n"
"        discard;\n"
"    // I'm not sure it impact us but be safe to lookup texture before conditional if\n"
"    // see: http://www.opengl.org/wiki/GLSL_Sampler#Non-uniform_flow_control\n"
"    vec4 c = texture(TextureSampler, PSin.t);\n"
"\n"
"    SV_Target0 = c;\n"
"}\n"
"\n"
"void ps_main2()\n"
"{\n"
"    vec4 c0 = texture(TextureSampler, PSin.t - ZrH);\n"
"    vec4 c1 = texture(TextureSampler, PSin.t);\n"
"    vec4 c2 = texture(TextureSampler, PSin.t + ZrH);\n"
"\n"
"    SV_Target0 = (c0 + c1 * 2.0f + c2) / 4.0f;\n"
"}\n"
"\n"
"void ps_main3()\n"
"{\n"
"    SV_Target0 = texture(TextureSampler, PSin.t);\n"
"}\n"
"\n"
"#endif\n"
;

/* Convert shader */
static const char convert_glsl_shader_raw[] =
"//#version 420 // Keep it for editor detection\n"
"\n"
"\n"
"#ifdef VERTEX_SHADER\n"
"\n"
"layout(location = 0) in vec2 POSITION;\n"
"layout(location = 1) in vec2 TEXCOORD0;\n"
"layout(location = 7) in vec4 COLOR;\n"
"\n"
"// FIXME set the interpolation (don't know what dx do)\n"
"// flat means that there is no interpolation. The value given to the fragment shader is based on the provoking vertex conventions.\n"
"//\n"
"// noperspective means that there will be linear interpolation in window-space. This is usually not what you want, but it can have its uses.\n"
"//\n"
"// smooth, the default, means to do perspective-correct interpolation.\n"
"//\n"
"// The centroid qualifier only matters when multisampling. If this qualifier is not present, then the value is interpolated to the pixel's center, anywhere in the pixel, or to one of the pixel's samples. This sample may lie outside of the actual primitive being rendered, since a primitive can cover only part of a pixel's area. The centroid qualifier is used to prevent this; the interpolation point must fall within both the pixel's area and the primitive's area.\n"
"out SHADER\n"
"{\n"
"    vec4 p;\n"
"    vec2 t;\n"
"    vec4 c;\n"
"} VSout;\n"
"\n"
"void vs_main()\n"
"{\n"
"    VSout.p = vec4(POSITION, 0.5f, 1.0f);\n"
"    VSout.t = TEXCOORD0;\n"
"    VSout.c = COLOR;\n"
"    gl_Position = vec4(POSITION, 0.5f, 1.0f); // NOTE I don't know if it is possible to merge POSITION_OUT and gl_Position\n"
"}\n"
"\n"
"#endif\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"\n"
"in SHADER\n"
"{\n"
"    vec4 p;\n"
"    vec2 t;\n"
"    vec4 c;\n"
"} PSin;\n"
"\n"
"// Give a different name so I remember there is a special case!\n"
"#if defined(ps_main1) || defined(ps_main10)\n"
"layout(location = 0) out uint SV_Target1;\n"
"#else\n"
"layout(location = 0) out vec4 SV_Target0;\n"
"#endif\n"
"\n"
"vec4 sample_c()\n"
"{\n"
"    return texture(TextureSampler, PSin.t);\n"
"}\n"
"\n"
"vec4 ps_crt(uint i)\n"
"{\n"
"    vec4 mask[4] = vec4[4]\n"
"        (\n"
"         vec4(1, 0, 0, 0),\n"
"         vec4(0, 1, 0, 0),\n"
"         vec4(0, 0, 1, 0),\n"
"         vec4(1, 1, 1, 0)\n"
"        );\n"
"    return sample_c() * clamp((mask[i] + 0.5f), 0.0f, 1.0f);\n"
"}\n"
"\n"
"#ifdef ps_main0\n"
"void ps_main0()\n"
"{\n"
"    SV_Target0 = sample_c();\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main1\n"
"void ps_main1()\n"
"{\n"
"    // Input Color is RGBA8\n"
"\n"
"    // We want to output a pixel on the PSMCT16* format\n"
"    // A1-BGR5\n"
"\n"
"#if 0\n"
"    // Note: dot is a good idea from pseudo. However we must be careful about float accuraccy.\n"
"    // Here a global idea example:\n"
"    //\n"
"    // SV_Target1 = dot(round(sample_c() * vec4(31.f, 31.f, 31.f, 1.f)), vec4(1.f, 32.f, 1024.f, 32768.f));\n"
"    //\n"
"\n"
"    // For me this code is more accurate but it will require some tests\n"
"\n"
"    vec4 c = sample_c() * 255.0f + 0.5f; // Denormalize value to avoid float precision issue\n"
"\n"
"    // shift Red: -3\n"
"    // shift Green: -3 + 5\n"
"    // shift Blue: -3 + 10\n"
"    // shift Alpha: -7 + 15\n"
"    highp uvec4 i = uvec4(c * vec4(1/8.0f, 4.0f, 128.0f, 256.0f)); // Shift value\n"
"\n"
"    // bit field operation requires GL4 HW. Could be nice to merge it with step/mix below\n"
"    SV_Target1 = (i.r & uint(0x001f)) | (i.g & uint(0x03e0)) | (i.b & uint(0x7c00)) | (i.a & uint(0x8000));\n"
"\n"
"#else\n"
"    // Old code which is likely wrong.\n"
"\n"
"    vec4 c = sample_c();\n"
"\n"
"    c.a *= 256.0f / 127.0f; // hm, 0.5 won't give us 1.0 if we just multiply with 2\n"
"\n"
"    highp uvec4 i = uvec4(c * vec4(uint(0x001f), uint(0x03e0), uint(0x7c00), uint(0x8000)));\n"
"\n"
"    // bit field operation requires GL4 HW.\n"
"    SV_Target1 = (i.x & uint(0x001f)) | (i.y & uint(0x03e0)) | (i.z & uint(0x7c00)) | (i.w & uint(0x8000));\n"
"#endif\n"
"\n"
"\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main10\n"
"void ps_main10()\n"
"{\n"
"    // Convert a GL_FLOAT32 depth texture into a 32 bits UINT texture\n"
"    SV_Target1 = uint(exp2(32.0f) * sample_c().r);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main11\n"
"void ps_main11()\n"
"{\n"
"    // Convert a GL_FLOAT32 depth texture into a RGBA color texture\n"
"    const vec4 bitSh = vec4(exp2(24.0f), exp2(16.0f), exp2(8.0f), exp2(0.0f));\n"
"    const vec4 bitMsk = vec4(0.0, 1.0/256.0, 1.0/256.0, 1.0/256.0);\n"
"\n"
"    vec4 res = fract(vec4(sample_c().r) * bitSh);\n"
"\n"
"    SV_Target0 = (res - res.xxyz * bitMsk) * 256.0f/255.0f;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main12\n"
"void ps_main12()\n"
"{\n"
"    // Convert a GL_FLOAT32 (only 16 lsb) depth into a RGB5A1 color texture\n"
"    const vec4 bitSh = vec4(exp2(32.0f), exp2(27.0f), exp2(22.0f), exp2(17.0f));\n"
"    const uvec4 bitMsk = uvec4(0x1F, 0x1F, 0x1F, 0x1);\n"
"    uvec4 color = uvec4(vec4(sample_c().r) * bitSh) & bitMsk;\n"
"\n"
"    SV_Target0 = vec4(color) / vec4(32.0f, 32.0f, 32.0f, 1.0f);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main13\n"
"void ps_main13()\n"
"{\n"
"    // Convert a RRGBA texture into a float depth texture\n"
"    // FIXME: I'm afraid of the accuracy\n"
"    const vec4 bitSh = vec4(exp2(-32.0f), exp2(-24.0f), exp2(-16.0f), exp2(-8.0f)) * vec4(255.0);\n"
"    gl_FragDepth = dot(sample_c(), bitSh);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main14\n"
"void ps_main14()\n"
"{\n"
"    // Same as above but without the alpha channel (24 bits Z)\n"
"\n"
"    // Convert a RRGBA texture into a float depth texture\n"
"    // FIXME: I'm afraid of the accuracy\n"
"    const vec3 bitSh = vec3(exp2(-32.0f), exp2(-24.0f), exp2(-16.0f)) * vec3(255.0);\n"
"    gl_FragDepth = dot(sample_c().rgb, bitSh);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main15\n"
"void ps_main15()\n"
"{\n"
"    // Same as above but without the A/B channels (16 bits Z)\n"
"\n"
"    // Convert a RRGBA texture into a float depth texture\n"
"    // FIXME: I'm afraid of the accuracy\n"
"    const vec2 bitSh = vec2(exp2(-32.0f), exp2(-24.0f)) * vec2(255.0);\n"
"    gl_FragDepth = dot(sample_c().rg, bitSh);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main16\n"
"void ps_main16()\n"
"{\n"
"    // Convert a RGB5A1 (saved as RGBA8) color to a 16 bit Z\n"
"    // FIXME: I'm afraid of the accuracy\n"
"    const vec4 bitSh = vec4(exp2(-32.0f), exp2(-27.0f), exp2(-22.0f), exp2(-17.0f));\n"
"    // Trunc color to drop useless lsb\n"
"    vec4 color = trunc(sample_c() * vec4(255.0f) / vec4(8.0f, 8.0f, 8.0f, 128.0f));\n"
"    gl_FragDepth = dot(vec4(color), bitSh);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main17\n"
"void ps_main17()\n"
"{\n"
"\n"
"    // Potential speed optimization. There is a high probability that\n"
"    // game only want to extract a single channel (blue). It will allow\n"
"    // to remove most of the conditional operation and yield a +2/3 fps\n"
"    // boost on MGS3\n"
"    //\n"
"    // Hypothesis wrong in Prince of Persia ... Seriously WTF !\n"
"    //#define ONLY_BLUE;\n"
"\n"
"    // Convert a RGBA texture into a 8 bits packed texture\n"
"    // Input column: 8x2 RGBA pixels\n"
"    // 0: 8 RGBA\n"
"    // 1: 8 RGBA\n"
"    // Output column: 16x4 Index pixels\n"
"    // 0: 8 R | 8 B\n"
"    // 1: 8 R | 8 B\n"
"    // 2: 8 G | 8 A\n"
"    // 3: 8 G | 8 A\n"
"    float c;\n"
"\n"
"    uvec2 sel = uvec2(gl_FragCoord.xy) % uvec2(16u, 16u);\n"
"    ivec2 tb  = ((ivec2(gl_FragCoord.xy) & ~ivec2(15, 3)) >> 1);\n"
"\n"
"    int ty   = tb.y | (int(gl_FragCoord.y) & 1);\n"
"    int txN  = tb.x | (int(gl_FragCoord.x) & 7);\n"
"    int txH  = tb.x | ((int(gl_FragCoord.x) + 4) & 7);\n"
"\n"
"    txN *= ScalingFactor.x;\n"
"    txH *= ScalingFactor.x;\n"
"    ty  *= ScalingFactor.y;\n"
"\n"
"    // TODO investigate texture gather\n"
"    vec4 cN = texelFetch(TextureSampler, ivec2(txN, ty), 0);\n"
"    vec4 cH = texelFetch(TextureSampler, ivec2(txH, ty), 0);\n"
"\n"
"\n"
"    if ((sel.y & 4u) == 0u) {\n"
"        // Column 0 and 2\n"
"#ifdef ONLY_BLUE\n"
"        c = cN.b;\n"
"#else\n"
"        if ((sel.y & 3u) < 2u) {\n"
"            // first 2 lines of the col\n"
"            if (sel.x < 8u)\n"
"                c = cN.r;\n"
"            else\n"
"                c = cN.b;\n"
"        } else {\n"
"            if (sel.x < 8u)\n"
"                c = cH.g;\n"
"            else\n"
"                c = cH.a;\n"
"        }\n"
"#endif\n"
"    } else {\n"
"#ifdef ONLY_BLUE\n"
"        c = cH.b;\n"
"#else\n"
"        // Column 1 and 3\n"
"        if ((sel.y & 3u) < 2u) {\n"
"            // first 2 lines of the col\n"
"            if (sel.x < 8u)\n"
"                c = cH.r;\n"
"            else\n"
"                c = cH.b;\n"
"        } else {\n"
"            if (sel.x < 8u)\n"
"                c = cN.g;\n"
"            else\n"
"                c = cN.a;\n"
"        }\n"
"#endif\n"
"    }\n"
"\n"
"\n"
"    SV_Target0 = vec4(c);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main19\n"
"void ps_main19()\n"
"{\n"
"    SV_Target0 = PSin.c * vec4(1.0, 1.0, 1.0, sample_c().r);\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main7\n"
"void ps_main7()\n"
"{\n"
"    vec4 c = sample_c();\n"
"\n"
"    c.a = dot(c.rgb, vec3(0.299, 0.587, 0.114));\n"
"\n"
"    SV_Target0 = c;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main5\n"
"vec4 ps_scanlines(uint i)\n"
"{\n"
"    vec4 mask[2] =\n"
"    {\n"
"        vec4(1, 1, 1, 0),\n"
"        vec4(0, 0, 0, 0)\n"
"    };\n"
"\n"
"    return sample_c() * clamp((mask[i] + 0.5f), 0.0f, 1.0f);\n"
"}\n"
"\n"
"void ps_main5() // scanlines\n"
"{\n"
"    highp uvec4 p = uvec4(gl_FragCoord);\n"
"\n"
"    vec4 c = ps_scanlines(p.y % 2u);\n"
"\n"
"    SV_Target0 = c;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main6\n"
"void ps_main6() // diagonal\n"
"{\n"
"    highp uvec4 p = uvec4(gl_FragCoord);\n"
"\n"
"    vec4 c = ps_crt((p.x + (p.y % 3u)) % 3u);\n"
"\n"
"    SV_Target0 = c;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main8\n"
"void ps_main8() // triangular\n"
"{\n"
"    highp uvec4 p = uvec4(gl_FragCoord);\n"
"\n"
"    vec4 c = ps_crt(((p.x + ((p.y >> 1u) & 1u) * 3u) >> 1u) % 3u);\n"
"\n"
"    SV_Target0 = c;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main9\n"
"void ps_main9()\n"
"{\n"
"\n"
"    const float PI = 3.14159265359f;\n"
"\n"
"    vec2 texdim = vec2(textureSize(TextureSampler, 0));\n"
"\n"
"    vec4 c;\n"
"    if (dFdy(PSin.t.y) * PSin.t.y > 0.5f) {\n"
"        c = sample_c();\n"
"    } else {\n"
"        float factor = (0.9f - 0.4f * cos(2.0f * PI * PSin.t.y * texdim.y));\n"
"        c =  factor * texture(TextureSampler, vec2(PSin.t.x, (floor(PSin.t.y * texdim.y) + 0.5f) / texdim.y));\n"
"    }\n"
"\n"
"    SV_Target0 = c;\n"
"}\n"
"#endif\n"
"\n"
"// Used for DATE (stencil)\n"
"// DATM == 1\n"
"#ifdef ps_main2\n"
"void ps_main2()\n"
"{\n"
"    if(sample_c().a < (127.5f / 255.0f)) // >= 0x80 pass\n"
"        discard;\n"
"}\n"
"#endif\n"
"\n"
"// Used for DATE (stencil)\n"
"// DATM == 0\n"
"#ifdef ps_main3\n"
"void ps_main3()\n"
"{\n"
"    if((127.5f / 255.0f) < sample_c().a) // < 0x80 pass (== 0x80 should not pass)\n"
"        discard;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main4\n"
"void ps_main4()\n"
"{\n"
"    SV_Target0 = mod(round(sample_c() * 255.0f), 256.0f) / 255.0f;\n"
"}\n"
"#endif\n"
"\n"
"#ifdef ps_main18\n"
"void ps_main18()\n"
"{\n"
"    vec4 i = sample_c();\n"
"    vec4 o;\n"
"\n"
"    mat3 rgb2yuv; // Value from GS manual\n"
"    rgb2yuv[0] = vec3(0.587, -0.311, -0.419);\n"
"    rgb2yuv[1] = vec3(0.114, 0.500, -0.081);\n"
"    rgb2yuv[2] = vec3(0.299, -0.169, 0.500);\n"
"\n"
"    vec3 yuv = rgb2yuv * i.gbr;\n"
"\n"
"    float Y = float(0xDB)/255.0f * yuv.x + float(0x10)/255.0f;\n"
"    float Cr = float(0xE0)/255.0f * yuv.y + float(0x80)/255.0f;\n"
"    float Cb = float(0xE0)/255.0f * yuv.z + float(0x80)/255.0f;\n"
"\n"
"    switch(EMODA) {\n"
"        case 0:\n"
"            o.a = i.a;\n"
"            break;\n"
"        case 1:\n"
"            o.a = Y;\n"
"            break;\n"
"        case 2:\n"
"            o.a = Y/2.0f;\n"
"            break;\n"
"        case 3:\n"
"            o.a = 0.0f;\n"
"            break;\n"
"    }\n"
"\n"
"    switch(EMODC) {\n"
"        case 0:\n"
"            o.rgb = i.rgb;\n"
"            break;\n"
"        case 1:\n"
"            o.rgb = vec3(Y);\n"
"            break;\n"
"        case 2:\n"
"            o.rgb = vec3(Y, Cb, Cr);\n"
"            break;\n"
"        case 3:\n"
"            o.rgb = vec3(i.a);\n"
"            break;\n"
"    }\n"
"\n"
"    SV_Target0 = o;\n"
"}\n"
"#endif\n"
"\n"
"#endif\n"
;

/* TFX FS shader */
static const char tfx_fs_glsl_shader_raw[] =
"//#version 420 // Keep it for text editor detection\n"
"\n"
"// Require for bit operation\n"
"//#extension GL_ARB_gpu_shader5 : enable\n"
"\n"
"#define FMT_32 0\n"
"#define FMT_24 1\n"
"#define FMT_16 2\n"
"\n"
"#define PS_PAL_FMT (PS_TEX_FMT >> 2)\n"
"#define PS_AEM_FMT (PS_TEX_FMT & 3)\n"
"\n"
"\n"
"#define SW_BLEND (PS_BLEND_A || PS_BLEND_B || PS_BLEND_D)\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"\n"
"#if !defined(BROKEN_DRIVER) && defined(GL_ARB_enhanced_layouts) && GL_ARB_enhanced_layouts\n"
"layout(location = 0)\n"
"#endif\n"
"in SHADER\n"
"{\n"
"    vec4 t_float;\n"
"    vec4 t_int;\n"
"    vec4 c;\n"
"    flat vec4 fc;\n"
"} PSin;\n"
"\n"
"// Same buffer but 2 colors for dual source blending\n"
"layout(location = 0, index = 0) out vec4 SV_Target0;\n"
"layout(location = 0, index = 1) out vec4 SV_Target1;\n"
"\n"
"layout(binding = 1) uniform sampler2D PaletteSampler;\n"
"layout(binding = 3) uniform sampler2D RtSampler; // note 2 already use by the image below\n"
"layout(binding = 4) uniform sampler2D RawTextureSampler;\n"
"\n"
"#ifndef DISABLE_GL42_image\n"
"#if PS_DATE > 0\n"
"// Performance note: images mustn't be declared if they are unused. Otherwise it will\n"
"// require extra shader validation.\n"
"\n"
"// FIXME how to declare memory access\n"
"layout(r32i, binding = 2) uniform iimage2D img_prim_min;\n"
"// WARNING:\n"
"// You can't enable it if you discard the fragment. The depth is still\n"
"// updated (shadow in Shin Megami Tensei Nocturne)\n"
"//\n"
"// early_fragment_tests must still be enabled in the first pass of the 2 passes algo\n"
"// First pass search the first primitive that will write the bad alpha value. Value\n"
"// won't be written if the fragment fails the depth test.\n"
"//\n"
"// In theory the best solution will be do\n"
"// 1/ copy the depth buffer\n"
"// 2/ do the full depth (current depth writes are disabled)\n"
"// 3/ restore the depth buffer for 2nd pass\n"
"// Of course, it is likely too costly.\n"
"#if PS_DATE == 1 || PS_DATE == 2\n"
"layout(early_fragment_tests) in;\n"
"#endif\n"
"\n"
"// I don't remember why I set this parameter but it is surely useless\n"
"//layout(pixel_center_integer) in vec4 gl_FragCoord;\n"
"#endif\n"
"#else\n"
"// use basic stencil\n"
"#endif\n"
"\n"
"vec4 sample_c(vec2 uv)\n"
"{\n"
"#if PS_TEX_IS_FB == 1\n"
"    return texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0);\n"
"#else\n"
"\n"
"#if PS_POINT_SAMPLER\n"
"    // Weird issue with ATI/AMD cards,\n"
"    // it looks like they add 127/128 of a texel to sampling coordinates\n"
"    // occasionally causing point sampling to erroneously round up.\n"
"    // I'm manually adjusting coordinates to the centre of texels here,\n"
"    // though the centre is just paranoia, the top left corner works fine.\n"
"    // As of 2018 this issue is still present.\n"
"    uv = (trunc(uv * WH.zw) + vec2(0.5, 0.5)) / WH.zw;\n"
"#endif\n"
"\n"
"#if PS_AUTOMATIC_LOD == 1\n"
"    return texture(TextureSampler, uv);\n"
"#elif PS_MANUAL_LOD == 1\n"
"    // FIXME add LOD: K - ( LOG2(Q) * (1 << L))\n"
"    float K = MinMax.x;\n"
"    float L = MinMax.y;\n"
"    float bias = MinMax.z;\n"
"    float max_lod = MinMax.w;\n"
"\n"
"    float gs_lod = K - log2(abs(PSin.t_float.w)) * L;\n"
"    // FIXME max useful ?\n"
"    //float lod = max(min(gs_lod, max_lod) - bias, 0.0f);\n"
"    float lod = min(gs_lod, max_lod) - bias;\n"
"\n"
"    return textureLod(TextureSampler, uv, lod);\n"
"#else\n"
"    return textureLod(TextureSampler, uv, 0); // No lod\n"
"#endif\n"
"\n"
"#endif\n"
"}\n"
"\n"
"vec4 sample_p(float idx)\n"
"{\n"
"    return texture(PaletteSampler, vec2(idx, 0.0f));\n"
"}\n"
"\n"
"vec4 clamp_wrap_uv(vec4 uv)\n"
"{\n"
"    vec4 uv_out = uv;\n"
"#if PS_INVALID_TEX0 == 1\n"
"    vec4 tex_size = WH.zwzw;\n"
"#else\n"
"    vec4 tex_size = WH.xyxy;\n"
"#endif\n"
"\n"
"#if PS_WMS == PS_WMT\n"
"\n"
"#if PS_WMS == 2\n"
"    uv_out = clamp(uv, MinMax.xyxy, MinMax.zwzw);\n"
"#elif PS_WMS == 3\n"
"    #if PS_FST == 0\n"
"    // wrap negative uv coords to avoid an off by one error that shifted\n"
"    // textures. Fixes Xenosaga's hair issue.\n"
"    uv = fract(uv);\n"
"    #endif\n"
"    uv_out = vec4((uvec4(uv * tex_size) & MskFix.xyxy) | MskFix.zwzw) / tex_size;\n"
"#endif\n"
"\n"
"#else // PS_WMS != PS_WMT\n"
"\n"
"#if PS_WMS == 2\n"
"    uv_out.xz = clamp(uv.xz, MinMax.xx, MinMax.zz);\n"
"\n"
"#elif PS_WMS == 3\n"
"    #if PS_FST == 0\n"
"    uv.xz = fract(uv.xz);\n"
"    #endif\n"
"    uv_out.xz = vec2((uvec2(uv.xz * tex_size.xx) & MskFix.xx) | MskFix.zz) / tex_size.xx;\n"
"\n"
"#endif\n"
"\n"
"#if PS_WMT == 2\n"
"    uv_out.yw = clamp(uv.yw, MinMax.yy, MinMax.ww);\n"
"\n"
"#elif PS_WMT == 3\n"
"    #if PS_FST == 0\n"
"    uv.yw = fract(uv.yw);\n"
"    #endif\n"
"    uv_out.yw = vec2((uvec2(uv.yw * tex_size.yy) & MskFix.yy) | MskFix.ww) / tex_size.yy;\n"
"#endif\n"
"\n"
"#endif\n"
"\n"
"    return uv_out;\n"
"}\n"
"\n"
"mat4 sample_4c(vec4 uv)\n"
"{\n"
"    mat4 c;\n"
"\n"
"    // Note: texture gather can't be used because of special clamping/wrapping\n"
"    // Also it doesn't support lod\n"
"    c[0] = sample_c(uv.xy);\n"
"    c[1] = sample_c(uv.zy);\n"
"    c[2] = sample_c(uv.xw);\n"
"    c[3] = sample_c(uv.zw);\n"
"\n"
"    return c;\n"
"}\n"
"\n"
"vec4 sample_4_index(vec4 uv)\n"
"{\n"
"    vec4 c;\n"
"\n"
"    // Either GS will send a texture that contains a single channel\n"
"    // in this case the red channel is remapped as alpha channel\n"
"    //\n"
"    // Or we have an old RT (ie RGBA8) that contains index (4/8) in the alpha channel\n"
"\n"
"    // Note: texture gather can't be used because of special clamping/wrapping\n"
"    // Also it doesn't support lod\n"
"    c.x = sample_c(uv.xy).a;\n"
"    c.y = sample_c(uv.zy).a;\n"
"    c.z = sample_c(uv.xw).a;\n"
"    c.w = sample_c(uv.zw).a;\n"
"\n"
"    uvec4 i = uvec4(c * 255.0f + 0.5f); // Denormalize value\n"
"\n"
"#if PS_PAL_FMT == 1\n"
"    // 4HL\n"
"    return vec4(i & 0xFu) / 255.0f;\n"
"\n"
"#elif PS_PAL_FMT == 2\n"
"    // 4HH\n"
"    return vec4(i >> 4u) / 255.0f;\n"
"\n"
"#else\n"
"    // Most of texture will hit this code so keep normalized float value\n"
"\n"
"    // 8 bits\n"
"    return c;\n"
"#endif\n"
"\n"
"}\n"
"\n"
"mat4 sample_4p(vec4 u)\n"
"{\n"
"    mat4 c;\n"
"\n"
"    c[0] = sample_p(u.x);\n"
"    c[1] = sample_p(u.y);\n"
"    c[2] = sample_p(u.z);\n"
"    c[3] = sample_p(u.w);\n"
"\n"
"    return c;\n"
"}\n"
"\n"
"int fetch_raw_depth()\n"
"{\n"
"    return int(texelFetch(RawTextureSampler, ivec2(gl_FragCoord.xy), 0).r * exp2(32.0f));\n"
"}\n"
"\n"
"vec4 fetch_raw_color()\n"
"{\n"
"    return texelFetch(RawTextureSampler, ivec2(gl_FragCoord.xy), 0);\n"
"}\n"
"\n"
"vec4 fetch_c(ivec2 uv)\n"
"{\n"
"    return texelFetch(TextureSampler, ivec2(uv), 0);\n"
"}\n"
"\n"
"//////////////////////////////////////////////////////////////////////\n"
"// Depth sampling\n"
"//////////////////////////////////////////////////////////////////////\n"
"ivec2 clamp_wrap_uv_depth(ivec2 uv)\n"
"{\n"
"    ivec2 uv_out = uv;\n"
"\n"
"    // Keep the full precision\n"
"    // It allow to multiply the ScalingFactor before the 1/16 coeff\n"
"    ivec4 mask = ivec4(MskFix) << 4;\n"
"\n"
"#if PS_WMS == PS_WMT\n"
"\n"
"#if PS_WMS == 2\n"
"    uv_out = clamp(uv, mask.xy, mask.zw);\n"
"#elif PS_WMS == 3\n"
"    uv_out = (uv & mask.xy) | mask.zw;\n"
"#endif\n"
"\n"
"#else // PS_WMS != PS_WMT\n"
"\n"
"#if PS_WMS == 2\n"
"    uv_out.x = clamp(uv.x, mask.x, mask.z);\n"
"#elif PS_WMS == 3\n"
"    uv_out.x = (uv.x & mask.x) | mask.z;\n"
"#endif\n"
"\n"
"#if PS_WMT == 2\n"
"    uv_out.y = clamp(uv.y, mask.y, mask.w);\n"
"#elif PS_WMT == 3\n"
"    uv_out.y = (uv.y & mask.y) | mask.w;\n"
"#endif\n"
"\n"
"#endif\n"
"\n"
"    return uv_out;\n"
"}\n"
"\n"
"vec4 sample_depth(vec2 st)\n"
"{\n"
"    vec2 uv_f = vec2(clamp_wrap_uv_depth(ivec2(st))) * vec2(ScalingFactor.xy) * vec2(1.0f/16.0f);\n"
"    ivec2 uv = ivec2(uv_f);\n"
"\n"
"    vec4 t = vec4(0.0f);\n"
"#if PS_TALES_OF_ABYSS_HLE == 1\n"
"    // Warning: UV can't be used in channel effect\n"
"    int depth = fetch_raw_depth();\n"
"\n"
"    // Convert msb based on the palette\n"
"    t = texelFetch(PaletteSampler, ivec2((depth >> 8) & 0xFF, 0), 0) * 255.0f;\n"
"\n"
"#elif PS_URBAN_CHAOS_HLE == 1\n"
"    // Depth buffer is read as a RGB5A1 texture. The game try to extract the green channel.\n"
"    // So it will do a first channel trick to extract lsb, value is right-shifted.\n"
"    // Then a new channel trick to extract msb which will shifted to the left.\n"
"    // OpenGL uses a FLOAT32 format for the depth so it requires a couple of conversion.\n"
"    // To be faster both steps (msb&lsb) are done in a single pass.\n"
"\n"
"    // Warning: UV can't be used in channel effect\n"
"    int depth = fetch_raw_depth();\n"
"\n"
"    // Convert lsb based on the palette\n"
"    t = texelFetch(PaletteSampler, ivec2((depth & 0xFF), 0), 0) * 255.0f;\n"
"\n"
"    // Msb is easier\n"
"    float green = float((depth >> 8) & 0xFF) * 36.0f;\n"
"    green = min(green, 255.0f);\n"
"\n"
"    t.g += green;\n"
"\n"
"\n"
"#elif PS_DEPTH_FMT == 1\n"
"    // Based on ps_main11 of convert\n"
"\n"
"    // Convert a GL_FLOAT32 depth texture into a RGBA color texture\n"
"    const vec4 bitSh = vec4(exp2(24.0f), exp2(16.0f), exp2(8.0f), exp2(0.0f));\n"
"    const vec4 bitMsk = vec4(0.0, 1.0/256.0, 1.0/256.0, 1.0/256.0);\n"
"\n"
"    vec4 res = fract(vec4(fetch_c(uv).r) * bitSh);\n"
"\n"
"    t = (res - res.xxyz * bitMsk) * 256.0f;\n"
"\n"
"#elif PS_DEPTH_FMT == 2\n"
"    // Based on ps_main12 of convert\n"
"\n"
"    // Convert a GL_FLOAT32 (only 16 lsb) depth into a RGB5A1 color texture\n"
"    const vec4 bitSh = vec4(exp2(32.0f), exp2(27.0f), exp2(22.0f), exp2(17.0f));\n"
"    const uvec4 bitMsk = uvec4(0x1F, 0x1F, 0x1F, 0x1);\n"
"    uvec4 color = uvec4(vec4(fetch_c(uv).r) * bitSh) & bitMsk;\n"
"\n"
"    t = vec4(color) * vec4(8.0f, 8.0f, 8.0f, 128.0f);\n"
"\n"
"#elif PS_DEPTH_FMT == 3\n"
"    // Convert a RGBA/RGB5A1 color texture into a RGBA/RGB5A1 color texture\n"
"    t = fetch_c(uv) * 255.0f;\n"
"\n"
"#endif\n"
"\n"
"\n"
"    // warning t ranges from 0 to 255\n"
"#if (PS_AEM_FMT == FMT_24)\n"
"    t.a = ( (PS_AEM == 0) || any(bvec3(t.rgb))  ) ? 255.0f * TA.x : 0.0f;\n"
"#elif (PS_AEM_FMT == FMT_16)\n"
"    t.a = t.a >= 128.0f ? 255.0f * TA.y : ( (PS_AEM == 0) || any(bvec3(t.rgb)) ) ? 255.0f * TA.x : 0.0f;\n"
"#endif\n"
"\n"
"\n"
"    return t;\n"
"}\n"
"\n"
"//////////////////////////////////////////////////////////////////////\n"
"// Fetch a Single Channel\n"
"//////////////////////////////////////////////////////////////////////\n"
"vec4 fetch_red()\n"
"{\n"
"#if PS_DEPTH_FMT == 1 || PS_DEPTH_FMT == 2\n"
"    int depth = (fetch_raw_depth()) & 0xFF;\n"
"    vec4 rt = vec4(depth) / 255.0f;\n"
"#else\n"
"    vec4 rt = fetch_raw_color();\n"
"#endif\n"
"    return sample_p(rt.r) * 255.0f;\n"
"}\n"
"\n"
"vec4 fetch_green()\n"
"{\n"
"#if PS_DEPTH_FMT == 1 || PS_DEPTH_FMT == 2\n"
"    int depth = (fetch_raw_depth() >> 8) & 0xFF;\n"
"    vec4 rt = vec4(depth) / 255.0f;\n"
"#else\n"
"    vec4 rt = fetch_raw_color();\n"
"#endif\n"
"    return sample_p(rt.g) * 255.0f;\n"
"}\n"
"\n"
"vec4 fetch_blue()\n"
"{\n"
"#if PS_DEPTH_FMT == 1 || PS_DEPTH_FMT == 2\n"
"    int depth = (fetch_raw_depth() >> 16) & 0xFF;\n"
"    vec4 rt = vec4(depth) / 255.0f;\n"
"#else\n"
"    vec4 rt = fetch_raw_color();\n"
"#endif\n"
"    return sample_p(rt.b) * 255.0f;\n"
"}\n"
"\n"
"vec4 fetch_alpha()\n"
"{\n"
"    vec4 rt = fetch_raw_color();\n"
"    return sample_p(rt.a) * 255.0f;\n"
"}\n"
"\n"
"vec4 fetch_rgb()\n"
"{\n"
"    vec4 rt = fetch_raw_color();\n"
"    vec4 c = vec4(sample_p(rt.r).r, sample_p(rt.g).g, sample_p(rt.b).b, 1.0f);\n"
"    return c * 255.0f;\n"
"}\n"
"\n"
"vec4 fetch_gXbY()\n"
"{\n"
"#if PS_DEPTH_FMT == 1 || PS_DEPTH_FMT == 2\n"
"    int depth = fetch_raw_depth();\n"
"    int bg = (depth >> (8 + ChannelShuffle.w)) & 0xFF;\n"
"    return vec4(bg);\n"
"#else\n"
"    ivec4 rt = ivec4(fetch_raw_color() * 255.0f);\n"
"    int green = (rt.g >> ChannelShuffle.w) & ChannelShuffle.z;\n"
"    int blue  = (rt.b << ChannelShuffle.y) & ChannelShuffle.x;\n"
"    return vec4(green | blue);\n"
"#endif\n"
"}\n"
"\n"
"//////////////////////////////////////////////////////////////////////\n"
"\n"
"vec4 sample_color(vec2 st)\n"
"{\n"
"#if (PS_TCOFFSETHACK == 1)\n"
"    st += TC_OffsetHack.xy;\n"
"#endif\n"
"\n"
"    vec4 t;\n"
"    mat4 c;\n"
"    vec2 dd;\n"
"\n"
"    // FIXME I'm not sure this condition is useful (I think code will be optimized)\n"
"#if (PS_LTF == 0 && PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_WMS < 2 && PS_WMT < 2)\n"
"    // No software LTF and pure 32 bits RGBA texure without special texture wrapping\n"
"    c[0] = sample_c(st);\n"
"\n"
"#else\n"
"    vec4 uv;\n"
"\n"
"    if(PS_LTF != 0)\n"
"    {\n"
"        uv = st.xyxy + HalfTexel;\n"
"        dd = fract(uv.xy * WH.zw);\n"
"#if (PS_FST == 0)\n"
"        // Background in Shin Megami Tensei Lucifers\n"
"        // I suspect that uv isn't a standard number, so fract is outside of the [0;1] range\n"
"        // Note: it is free on GPU but let's do it only for float coordinate\n"
"        dd = clamp(dd, vec2(0.0f), vec2(1.0f));\n"
"#endif\n"
"    }\n"
"    else\n"
"    {\n"
"        uv = st.xyxy;\n"
"    }\n"
"\n"
"    uv = clamp_wrap_uv(uv);\n"
"\n"
"#if PS_PAL_FMT != 0\n"
"    c = sample_4p(sample_4_index(uv));\n"
"#else\n"
"    c = sample_4c(uv);\n"
"#endif\n"
"\n"
"\n"
"#endif\n"
"\n"
"    // PERF note: using dot product reduces by 1 the number of instruction\n"
"    // but I'm not sure it is equivalent neither faster.\n"
"    for (int i = 0; i < 4; i++)\n"
"    {\n"
"        //float sum = dot(c[i].rgb, vec3(1.0f));\n"
"#if (PS_AEM_FMT == FMT_24)\n"
"            c[i].a = ( (PS_AEM == 0) || any(bvec3(c[i].rgb))  ) ? TA.x : 0.0f;\n"
"            //c[i].a = ( (PS_AEM == 0) || (sum > 0.0f) ) ? TA.x : 0.0f;\n"
"#elif (PS_AEM_FMT == FMT_16)\n"
"            c[i].a = c[i].a >= 0.5 ? TA.y : ( (PS_AEM == 0) || any(bvec3(c[i].rgb)) ) ? TA.x : 0.0f;\n"
"            //c[i].a = c[i].a >= 0.5 ? TA.y : ( (PS_AEM == 0) || (sum > 0.0f) ) ? TA.x : 0.0f;\n"
"#endif\n"
"    }\n"
"\n"
"#if(PS_LTF != 0)\n"
"    t = mix(mix(c[0], c[1], dd.x), mix(c[2], c[3], dd.x), dd.y);\n"
"#else\n"
"    t = c[0];\n"
"#endif\n"
"\n"
"    // The 0.05f helps to fix the overbloom of sotc\n"
"    // I think the issue is related to the rounding of texture coodinate. The linear (from fixed unit)\n"
"    // interpolation could be slightly below the correct one.\n"
"    return trunc(t * 255.0f + 0.05f);\n"
"}\n"
"\n"
"vec4 tfx(vec4 T, vec4 C)\n"
"{\n"
"    vec4 C_out;\n"
"    vec4 FxT = trunc(trunc(C) * T / 128.0f);\n"
"\n"
"#if (PS_TFX == 0)\n"
"    C_out = FxT;\n"
"#elif (PS_TFX == 1)\n"
"    C_out = T;\n"
"#elif (PS_TFX == 2)\n"
"    C_out.rgb = FxT.rgb + C.a;\n"
"    C_out.a = T.a + C.a;\n"
"#elif (PS_TFX == 3)\n"
"    C_out.rgb = FxT.rgb + C.a;\n"
"    C_out.a = T.a;\n"
"#else\n"
"    C_out = C;\n"
"#endif\n"
"\n"
"#if (PS_TCC == 0)\n"
"    C_out.a = C.a;\n"
"#endif\n"
"\n"
"#if (PS_TFX == 0) || (PS_TFX == 2) || (PS_TFX == 3)\n"
"    // Clamp only when it is useful\n"
"    C_out = min(C_out, 255.0f);\n"
"#endif\n"
"\n"
"    return C_out;\n"
"}\n"
"\n"
"void atst(vec4 C)\n"
"{\n"
"    float a = C.a;\n"
"\n"
"    // Do nothing for PS_ATST 0\n"
"#if (PS_ATST == 1)\n"
"    if (a > AREF) discard;\n"
"#elif (PS_ATST == 2)\n"
"    if (a < AREF) discard;\n"
"#elif (PS_ATST == 3)\n"
"    if (abs(a - AREF) > 0.5f) discard;\n"
"#elif (PS_ATST == 4)\n"
"    if (abs(a - AREF) < 0.5f) discard;\n"
"#endif\n"
"}\n"
"\n"
"void fog(inout vec4 C, float f)\n"
"{\n"
"#if PS_FOG != 0\n"
"    C.rgb = trunc(mix(FogColor, C.rgb, f));\n"
"#endif\n"
"}\n"
"\n"
"vec4 ps_color()\n"
"{\n"
"    //FIXME: maybe we can set gl_Position.w = q in VS\n"
"#if (PS_FST == 0) && (PS_INVALID_TEX0 == 1)\n"
"    // Re-normalize coordinate from invalid GS to corrected texture size\n"
"    vec2 st = (PSin.t_float.xy * WH.xy) / (vec2(PSin.t_float.w) * WH.zw);\n"
"    // no st_int yet\n"
"#elif (PS_FST == 0)\n"
"    vec2 st = PSin.t_float.xy / vec2(PSin.t_float.w);\n"
"    vec2 st_int = PSin.t_int.zw / vec2(PSin.t_float.w);\n"
"#else\n"
"    // Note xy are normalized coordinate\n"
"    vec2 st = PSin.t_int.xy;\n"
"    vec2 st_int = PSin.t_int.zw;\n"
"#endif\n"
"\n"
"#if PS_CHANNEL_FETCH == 1\n"
"    vec4 T = fetch_red();\n"
"#elif PS_CHANNEL_FETCH == 2\n"
"    vec4 T = fetch_green();\n"
"#elif PS_CHANNEL_FETCH == 3\n"
"    vec4 T = fetch_blue();\n"
"#elif PS_CHANNEL_FETCH == 4\n"
"    vec4 T = fetch_alpha();\n"
"#elif PS_CHANNEL_FETCH == 5\n"
"    vec4 T = fetch_rgb();\n"
"#elif PS_CHANNEL_FETCH == 6\n"
"    vec4 T = fetch_gXbY();\n"
"#elif PS_DEPTH_FMT > 0\n"
"    // Integral coordinate\n"
"    vec4 T = sample_depth(st_int);\n"
"#else\n"
"    vec4 T = sample_color(st);\n"
"#endif\n"
"\n"
"#if PS_IIP == 1\n"
"    vec4 C = tfx(T, PSin.c);\n"
"#else\n"
"    vec4 C = tfx(T, PSin.fc);\n"
"#endif\n"
"\n"
"    atst(C);\n"
"\n"
"    fog(C, PSin.t_float.z);\n"
"\n"
"#if (PS_CLR1 != 0) // needed for Cd * (As/Ad/F + 1) blending modes\n"
"    C.rgb = vec3(255.0f);\n"
"#endif\n"
"\n"
"    return C;\n"
"}\n"
"\n"
"void ps_fbmask(inout vec4 C)\n"
"{\n"
"    // FIXME do I need special case for 16 bits\n"
"#if PS_FBMASK\n"
"    vec4 RT = trunc(texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0) * 255.0f + 0.1f);\n"
"    C = vec4((uvec4(C) & ~FbMask) | (uvec4(RT) & FbMask));\n"
"#endif\n"
"}\n"
"\n"
"void ps_dither(inout vec4 C)\n"
"{\n"
"#if PS_DITHER\n"
"    #if PS_DITHER == 2\n"
"    ivec2 fpos = ivec2(gl_FragCoord.xy);\n"
"    #else\n"
"    ivec2 fpos = ivec2(gl_FragCoord.xy / ScalingFactor.x);\n"
"    #endif\n"
"    C.rgb += DitherMatrix[fpos.y&3][fpos.x&3];\n"
"#endif\n"
"}\n"
"\n"
"void ps_blend(inout vec4 Color, float As)\n"
"{\n"
"#if SW_BLEND\n"
"    vec4 RT = trunc(texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0) * 255.0f + 0.1f);\n"
"    vec4 Color_pabe = Color;\n"
"\n"
"#if PS_DFMT == FMT_24\n"
"    float Ad = 1.0f;\n"
"#else\n"
"    // FIXME FMT_16 case\n"
"    // FIXME Ad or Ad * 2?\n"
"    float Ad = RT.a / 128.0f;\n"
"#endif\n"
"\n"
"    // Let the compiler do its jobs !\n"
"    vec3 Cd = RT.rgb;\n"
"    vec3 Cs = Color.rgb;\n"
"\n"
"#if PS_BLEND_A == 0\n"
"    vec3 A = Cs;\n"
"#elif PS_BLEND_A == 1\n"
"    vec3 A = Cd;\n"
"#else\n"
"    vec3 A = vec3(0.0f);\n"
"#endif\n"
"\n"
"#if PS_BLEND_B == 0\n"
"    vec3 B = Cs;\n"
"#elif PS_BLEND_B == 1\n"
"    vec3 B = Cd;\n"
"#else\n"
"    vec3 B = vec3(0.0f);\n"
"#endif\n"
"\n"
"#if PS_BLEND_C == 0\n"
"    float C = As;\n"
"#elif PS_BLEND_C == 1\n"
"    float C = Ad;\n"
"#else\n"
"    float C = Af;\n"
"#endif\n"
"\n"
"#if PS_BLEND_D == 0\n"
"    vec3 D = Cs;\n"
"#elif PS_BLEND_D == 1\n"
"    vec3 D = Cd;\n"
"#else\n"
"    vec3 D = vec3(0.0f);\n"
"#endif\n"
"\n"
"#if PS_BLEND_A == PS_BLEND_B\n"
"    Color.rgb = D;\n"
"#else\n"
"    Color.rgb = trunc((A - B) * C + D);\n"
"#endif\n"
"\n"
"    // PABE\n"
"#if PS_PABE\n"
"    Color.rgb = (Color_pabe.a >= 128.0f) ? Color.rgb : Color_pabe.rgb;\n"
"#endif\n"
"\n"
"    // Dithering\n"
"    ps_dither(Color);\n"
"\n"
"    // Correct the Color value based on the output format\n"
"#if PS_COLCLIP == 0 && PS_HDR == 0\n"
"    // Standard Clamp\n"
"    Color.rgb = clamp(Color.rgb, vec3(0.0f), vec3(255.0f));\n"
"#endif\n"
"\n"
"    // FIXME rouding of negative float?\n"
"    // compiler uses trunc but it might need floor\n"
"\n"
"    // Warning: normally blending equation is mult(A, B) = A * B >> 7. GPU have the full accuracy\n"
"    // GS: Color = 1, Alpha = 255 => output 1\n"
"    // GPU: Color = 1/255, Alpha = 255/255 * 255/128 => output 1.9921875\n"
"#if PS_DFMT == FMT_16\n"
"    // In 16 bits format, only 5 bits of colors are used. It impacts shadows computation of Castlevania\n"
"\n"
"    Color.rgb = vec3(ivec3(Color.rgb) & ivec3(0xF8));\n"
"#elif PS_COLCLIP == 1 && PS_HDR == 0\n"
"    Color.rgb = vec3(ivec3(Color.rgb) & ivec3(0xFF));\n"
"#endif\n"
"\n"
"#endif\n"
"}\n"
"\n"
"void ps_main()\n"
"{\n"
"#if ((PS_DATE & 3) == 1 || (PS_DATE & 3) == 2)\n"
"\n"
"#if PS_WRITE_RG == 1\n"
"    // Pseudo 16 bits access.\n"
"    float rt_a = texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0).g;\n"
"#else\n"
"    float rt_a = texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0).a;\n"
"#endif\n"
"\n"
"#if (PS_DATE & 3) == 1\n"
"    // DATM == 0: Pixel with alpha equal to 1 will failed\n"
"    bool bad = (127.5f / 255.0f) < rt_a;\n"
"#elif (PS_DATE & 3) == 2\n"
"    // DATM == 1: Pixel with alpha equal to 0 will failed\n"
"    bool bad = rt_a < (127.5f / 255.0f);\n"
"#endif\n"
"\n"
"    if (bad) {\n"
"#if PS_DATE >= 5 || defined(DISABLE_GL42_image)\n"
"        discard;\n"
"#else\n"
"        imageStore(img_prim_min, ivec2(gl_FragCoord.xy), ivec4(-1));\n"
"        return;\n"
"#endif\n"
"    }\n"
"\n"
"#endif\n"
"\n"
"#if PS_DATE == 3 && !defined(DISABLE_GL42_image)\n"
"    int stencil_ceil = imageLoad(img_prim_min, ivec2(gl_FragCoord.xy)).r;\n"
"    // Note gl_PrimitiveID == stencil_ceil will be the primitive that will update\n"
"    // the bad alpha value so we must keep it.\n"
"\n"
"    if (gl_PrimitiveID > stencil_ceil) {\n"
"        discard;\n"
"    }\n"
"#endif\n"
"\n"
"    vec4 C = ps_color();\n"
"\n"
"#if PS_SHUFFLE\n"
"    uvec4 denorm_c = uvec4(C);\n"
"    uvec2 denorm_TA = uvec2(vec2(TA.xy) * 255.0f + 0.5f);\n"
"\n"
"    // Write RB part. Mask will take care of the correct destination\n"
"#if PS_READ_BA\n"
"    C.rb = C.bb;\n"
"#else\n"
"    C.rb = C.rr;\n"
"#endif\n"
"\n"
"    // FIXME precompute my_TA & 0x80\n"
"\n"
"    // Write GA part. Mask will take care of the correct destination\n"
"    // Note: GLSL 4.50/GL_EXT_shader_integer_mix support a mix instruction to select a component\n"
"    // However Nvidia emulate it with an if (at least on kepler arch) ...\n"
"#if PS_READ_BA\n"
"    // bit field operation requires GL4 HW. Could be nice to merge it with step/mix below\n"
"    // uint my_ta = (bool(bitfieldExtract(denorm_c.a, 7, 1))) ? denorm_TA.y : denorm_TA.x;\n"
"    // denorm_c.a = bitfieldInsert(denorm_c.a, bitfieldExtract(my_ta, 7, 1), 7, 1);\n"
"    // c.ga = vec2(float(denorm_c.a));\n"
"\n"
"    if (bool(denorm_c.a & 0x80u))\n"
"        C.ga = vec2(float((denorm_c.a & 0x7Fu) | (denorm_TA.y & 0x80u)));\n"
"    else\n"
"        C.ga = vec2(float((denorm_c.a & 0x7Fu) | (denorm_TA.x & 0x80u)));\n"
"\n"
"#else\n"
"    if (bool(denorm_c.g & 0x80u))\n"
"        C.ga = vec2(float((denorm_c.g & 0x7Fu) | (denorm_TA.y & 0x80u)));\n"
"    else\n"
"        C.ga = vec2(float((denorm_c.g & 0x7Fu) | (denorm_TA.x & 0x80u)));\n"
"\n"
"    // Nice idea but step/mix requires 4 instructions\n"
"    // set / trunc / I2F / Mad\n"
"    //\n"
"    // float sel = step(128.0f, c.g);\n"
"    // vec2 c_shuffle = vec2((denorm_c.gg & 0x7Fu) | (denorm_TA & 0x80u));\n"
"    // c.ga = mix(c_shuffle.xx, c_shuffle.yy, sel);\n"
"#endif\n"
"\n"
"#endif\n"
"\n"
"    // Must be done before alpha correction\n"
"    float alpha_blend = C.a / 128.0f;\n"
"\n"
"    // Correct the ALPHA value based on the output format\n"
"#if (PS_DFMT == FMT_16)\n"
"    float A_one = 128.0f; // alpha output will be 0x80\n"
"    C.a = (PS_FBA != 0) ? A_one : step(128.0f, C.a) * A_one;\n"
"#elif (PS_DFMT == FMT_32) && (PS_FBA != 0)\n"
"    if(C.a < 128.0f) C.a += 128.0f;\n"
"#endif\n"
"\n"
"    // Get first primitive that will write a failling alpha value\n"
"#if PS_DATE == 1 && !defined(DISABLE_GL42_image)\n"
"    // DATM == 0\n"
"    // Pixel with alpha equal to 1 will failed (128-255)\n"
"    if (C.a > 127.5f) {\n"
"        imageAtomicMin(img_prim_min, ivec2(gl_FragCoord.xy), gl_PrimitiveID);\n"
"    }\n"
"    return;\n"
"#elif PS_DATE == 2 && !defined(DISABLE_GL42_image)\n"
"    // DATM == 1\n"
"    // Pixel with alpha equal to 0 will failed (0-127)\n"
"    if (C.a < 127.5f) {\n"
"        imageAtomicMin(img_prim_min, ivec2(gl_FragCoord.xy), gl_PrimitiveID);\n"
"    }\n"
"    return;\n"
"#endif\n"
"\n"
"#if !SW_BLEND\n"
"    ps_dither(C);\n"
"#endif\n"
"\n"
"    ps_blend(C, alpha_blend);\n"
"\n"
"    ps_fbmask(C);\n"
"\n"
"// When dithering the bottom 3 bits become meaningless and cause lines in the picture\n"
"// so we need to limit the color depth on dithered items\n"
"// SW_BLEND already deals with this so no need to do in those cases\n"
"#if !SW_BLEND && PS_DITHER && PS_DFMT == FMT_16 && PS_COLCLIP == 0\n"
"    C.rgb = clamp(C.rgb, vec3(0.0f), vec3(255.0f));\n"
"    C.rgb = uvec3(uvec3(C.rgb) & uvec3(0xF8));\n"
"#endif\n"
"\n"
"// #if PS_HDR == 1\n"
"    // Use negative value to avoid overflow of the texture (in accumulation mode)\n"
"    // Note: code were initially done for an Half-Float texture. Due to overflow\n"
"    // the texture was upgraded to a full float. Maybe this code is useless now!\n"
"    // Good testcase is castlevania\n"
"    // if (any(greaterThan(C.rgb, vec3(128.0f)))) {\n"
"        // C.rgb = (C.rgb - 256.0f);\n"
"    // }\n"
"// #endif\n"
"    SV_Target0 = C / 255.0f;\n"
"    SV_Target1 = vec4(alpha_blend);\n"
"\n"
"#if PS_ZCLAMP\n"
"	gl_FragDepth = min(gl_FragCoord.z, MaxDepthPS);\n"
"#endif \n"
"}\n"
"\n"
"#endif\n"
;

/* TFX VGS shader */
static const char tfx_vgs_glsl_shader_raw[] =
"//#version 420 // Keep it for text editor detection\n"
"\n"
"#ifdef VERTEX_SHADER\n"
"layout(location = 0) in vec2  i_st;\n"
"layout(location = 2) in vec4  i_c;\n"
"layout(location = 3) in float i_q;\n"
"layout(location = 4) in uvec2 i_p;\n"
"layout(location = 5) in uint  i_z;\n"
"layout(location = 6) in uvec2 i_uv;\n"
"layout(location = 7) in vec4  i_f;\n"
"\n"
"#if !defined(BROKEN_DRIVER) && defined(GL_ARB_enhanced_layouts) && GL_ARB_enhanced_layouts\n"
"layout(location = 0)\n"
"#endif\n"
"out SHADER\n"
"{\n"
"    vec4 t_float;\n"
"    vec4 t_int;\n"
"    vec4 c;\n"
"    flat vec4 fc;\n"
"} VSout;\n"
"\n"
"const float exp_min32 = exp2(-32.0f);\n"
"\n"
"void texture_coord()\n"
"{\n"
"    vec2 uv = vec2(i_uv) - TextureOffset.xy;\n"
"    vec2 st = i_st - TextureOffset.xy;\n"
"\n"
"    // Float coordinate\n"
"    VSout.t_float.xy = st;\n"
"    VSout.t_float.w  = i_q;\n"
"\n"
"    // Integer coordinate => normalized\n"
"    VSout.t_int.xy = uv * TextureScale;\n"
"#if VS_INT_FST == 1\n"
"    // Some games uses float coordinate for post-processing effect\n"
"    VSout.t_int.zw = st / TextureScale;\n"
"#else\n"
"    // Integer coordinate => integral\n"
"    VSout.t_int.zw = uv;\n"
"#endif\n"
"}\n"
"\n"
"void vs_main()\n"
"{\n"
"    // Clamp to max depth, gs doesn't wrap\n"
"    highp uint z = min(i_z, MaxDepth);\n"
"\n"
"    // pos -= 0.05 (1/320 pixel) helps avoiding rounding problems (integral part of pos is usually 5 digits, 0.05 is about as low as we can go)\n"
"    // example: ceil(afterseveralvertextransformations(y = 133)) => 134 => line 133 stays empty\n"
"    // input granularity is 1/16 pixel, anything smaller than that won't step drawing up/left by one pixel\n"
"    // example: 133.0625 (133 + 1/16) should start from line 134, ceil(133.0625 - 0.05) still above 133\n"
"    vec4 p;\n"
"\n"
"    p.xy = vec2(i_p) - vec2(0.05f, 0.05f);\n"
"    p.xy = p.xy * VertexScale - VertexOffset;\n"
"    p.w = 1.0f;\n"
"    p.z = float(z) * exp_min32;\n"
"\n"
"    gl_Position = p;\n"
"\n"
"    texture_coord();\n"
"\n"
"    VSout.c = i_c;\n"
"    VSout.fc = i_c;\n"
"    VSout.t_float.z = i_f.x; // pack for with texture\n"
"}\n"
"\n"
"#endif\n"
"\n"
"#ifdef GEOMETRY_SHADER\n"
"\n"
"#if !defined(BROKEN_DRIVER) && defined(GL_ARB_enhanced_layouts) && GL_ARB_enhanced_layouts\n"
"layout(location = 0)\n"
"#endif\n"
"in SHADER\n"
"{\n"
"    vec4 t_float;\n"
"    vec4 t_int;\n"
"    vec4 c;\n"
"    flat vec4 fc;\n"
"} GSin[];\n"
"\n"
"#if !defined(BROKEN_DRIVER) && defined(GL_ARB_enhanced_layouts) && GL_ARB_enhanced_layouts\n"
"layout(location = 0)\n"
"#endif\n"
"out SHADER\n"
"{\n"
"    vec4 t_float;\n"
"    vec4 t_int;\n"
"    vec4 c;\n"
"    flat vec4 fc;\n"
"} GSout;\n"
"\n"
"struct vertex\n"
"{\n"
"    vec4 t_float;\n"
"    vec4 t_int;\n"
"    vec4 c;\n"
"};\n"
"\n"
"void out_vertex(in vec4 position, in vertex v)\n"
"{\n"
"    GSout.t_float  = v.t_float;\n"
"    GSout.t_int    = v.t_int;\n"
"    GSout.c        = v.c;\n"
"    // Flat output\n"
"#if GS_POINT == 1\n"
"    GSout.fc       = GSin[0].fc;\n"
"#else\n"
"    GSout.fc       = GSin[1].fc;\n"
"#endif\n"
"    gl_Position = position;\n"
"    gl_PrimitiveID = gl_PrimitiveIDIn;\n"
"    EmitVertex();\n"
"}\n"
"\n"
"#if GS_POINT == 1\n"
"layout(points) in;\n"
"#else\n"
"layout(lines) in;\n"
"#endif\n"
"layout(triangle_strip, max_vertices = 4) out;\n"
"\n"
"#if GS_POINT == 1\n"
"\n"
"void gs_main()\n"
"{\n"
"    // Transform a point to a NxN sprite\n"
"    vertex point = vertex(GSin[0].t_float, GSin[0].t_int, GSin[0].c);\n"
"\n"
"    // Get new position\n"
"    vec4 lt_p = gl_in[0].gl_Position;\n"
"    vec4 rb_p = gl_in[0].gl_Position + vec4(PointSize.x, PointSize.y, 0.0f, 0.0f);\n"
"    vec4 lb_p = rb_p;\n"
"    vec4 rt_p = rb_p;\n"
"    lb_p.x    = lt_p.x;\n"
"    rt_p.y    = lt_p.y;\n"
"\n"
"    out_vertex(lt_p, point);\n"
"\n"
"    out_vertex(lb_p, point);\n"
"\n"
"    out_vertex(rt_p, point);\n"
"\n"
"    out_vertex(rb_p, point);\n"
"\n"
"    EndPrimitive();\n"
"}\n"
"\n"
"#elif GS_LINE == 1\n"
"\n"
"void gs_main()\n"
"{\n"
"    // Transform a line to a thick line-sprite\n"
"    vertex left  = vertex(GSin[0].t_float, GSin[0].t_int, GSin[0].c);\n"
"    vertex right = vertex(GSin[1].t_float, GSin[1].t_int, GSin[1].c);\n"
"    vec4 lt_p = gl_in[0].gl_Position;\n"
"    vec4 rt_p = gl_in[1].gl_Position;\n"
"\n"
"    // Potentially there is faster math\n"
"    vec2 line_vector = normalize(rt_p.xy - lt_p.xy);\n"
"    vec2 line_normal = vec2(line_vector.y, -line_vector.x);\n"
"    vec2 line_width  = (line_normal * PointSize) / 2;\n"
"\n"
"    lt_p.xy -= line_width;\n"
"    rt_p.xy -= line_width;\n"
"    vec4 lb_p = gl_in[0].gl_Position + vec4(line_width, 0.0f, 0.0f);\n"
"    vec4 rb_p = gl_in[1].gl_Position + vec4(line_width, 0.0f, 0.0f);\n"
"\n"
"    out_vertex(lt_p, left);\n"
"\n"
"    out_vertex(lb_p, left);\n"
"\n"
"    out_vertex(rt_p, right);\n"
"\n"
"    out_vertex(rb_p, right);\n"
"\n"
"    EndPrimitive();\n"
"}\n"
"\n"
"#else\n"
"\n"
"void gs_main()\n"
"{\n"
"    // left top     => GSin[0];\n"
"    // right bottom => GSin[1];\n"
"    vertex rb = vertex(GSin[1].t_float, GSin[1].t_int, GSin[1].c);\n"
"    vertex lt = vertex(GSin[0].t_float, GSin[0].t_int, GSin[0].c);\n"
"\n"
"    vec4 rb_p = gl_in[1].gl_Position;\n"
"    vec4 lb_p = rb_p;\n"
"    vec4 rt_p = rb_p;\n"
"    vec4 lt_p = gl_in[0].gl_Position;\n"
"\n"
"    // flat depth\n"
"    lt_p.z = rb_p.z;\n"
"    // flat fog and texture perspective\n"
"    lt.t_float.zw = rb.t_float.zw;\n"
"    // flat color\n"
"    lt.c = rb.c;\n"
"\n"
"    // Swap texture and position coordinate\n"
"    vertex lb    = rb;\n"
"    lb.t_float.x = lt.t_float.x;\n"
"    lb.t_int.x   = lt.t_int.x;\n"
"    lb.t_int.z   = lt.t_int.z;\n"
"    lb_p.x       = lt_p.x;\n"
"\n"
"    vertex rt    = rb;\n"
"    rt_p.y       = lt_p.y;\n"
"    rt.t_float.y = lt.t_float.y;\n"
"    rt.t_int.y   = lt.t_int.y;\n"
"    rt.t_int.w   = lt.t_int.w;\n"
"\n"
"    out_vertex(lt_p, lt);\n"
"\n"
"    out_vertex(lb_p, lb);\n"
"\n"
"    out_vertex(rt_p, rt);\n"
"\n"
"    out_vertex(rb_p, rb);\n"
"\n"
"    EndPrimitive();\n"
"}\n"
"\n"
"#endif\n"
"\n"
"#endif\n"
;

static const u32 g_merge_cb_index      = 10;
static const u32 g_interlace_cb_index  = 11;
static const u32 g_convert_index       = 15;
static const u32 g_vs_cb_index         = 20;
static const u32 g_ps_cb_index         = 21;

int   GSDeviceOGL::m_shader_inst = 0;
int   GSDeviceOGL::m_shader_reg  = 0;

GSDeviceOGL::GSDeviceOGL()
	: m_force_texture_clear(0)
	, m_fbo(0)
	, m_fbo_read(0)
	, m_va(NULL)
	, m_apitrace(0)
	, m_palette_ss(0)
	, m_vs_cb(NULL)
	, m_ps_cb(NULL)
	, m_shader(NULL)
	, m_shader_tfx_fs(tfx_fs_glsl_shader_raw, tfx_fs_glsl_shader_raw + sizeof(tfx_fs_glsl_shader_raw)/sizeof(*tfx_fs_glsl_shader_raw))
	, m_shader_tfx_vgs(tfx_vgs_glsl_shader_raw, tfx_vgs_glsl_shader_raw + sizeof(tfx_vgs_glsl_shader_raw)/sizeof(*tfx_vgs_glsl_shader_raw))
{
	memset(&m_merge_obj, 0, sizeof(m_merge_obj));
	memset(&m_interlace, 0, sizeof(m_interlace));
	memset(&m_convert, 0, sizeof(m_convert));
	memset(&m_fxaa, 0, sizeof(m_fxaa));
	memset(&m_date, 0, sizeof(m_date));
	memset(&m_om_dss, 0, sizeof(m_om_dss));
	memset(&m_profiler, 0 , sizeof(m_profiler));
	GLState::Clear();

	m_mipmap = option_value(INT_PCSX2_OPT_MIPMAPPING, KeyOptionInt::return_type);
	m_filter = static_cast<TriFiltering>(theApp.GetConfigI("UserHacks_TriFilter"));
}

GSDeviceOGL::~GSDeviceOGL()
{
	// If the create function wasn't called nothing to do.
	if (m_shader == NULL)
		return;

	// Clean vertex buffer state
	delete m_va;

	// Clean m_merge_obj
	delete m_merge_obj.cb;

	// Clean m_interlace
	delete m_interlace.cb;

	// Clean m_convert
	delete m_convert.dss;
	delete m_convert.dss_write;
	delete m_convert.cb;

	// Clean m_fxaa
	delete m_fxaa.cb;

	// Clean m_date
	delete m_date.dss;

	// Clean various opengl allocation
	glDeleteFramebuffers(1, &m_fbo);
	glDeleteFramebuffers(1, &m_fbo_read);

	// Delete HW FX
	delete m_vs_cb;
	delete m_ps_cb;
	glDeleteSamplers(1, &m_palette_ss);

	m_ps.clear();

	glDeleteSamplers(ARRAY_SIZE(m_ps_ss), m_ps_ss);

	for (u32 key = 0; key < ARRAY_SIZE(m_om_dss); key++)
		delete m_om_dss[key];

	PboPool::Destroy();

	// Must be done after the destruction of all shader/program objects
	delete m_shader;
	m_shader = NULL;
}

GSTexture* GSDeviceOGL::CreateSurface(int type, int w, int h, int fmt)
{
	// A wrapper to call GSTextureOGL, with the different kind of parameter
	GSTextureOGL* t = new GSTextureOGL(type, w, h, fmt, m_fbo_read, m_mipmap > 1 || m_filter != TriFiltering::None);

	// NOTE: I'm not sure RenderTarget always need to be cleared. It could be costly for big upscale.
	// FIXME: it will be more logical to do it in FetchSurface. This code is only called at first creation
	//  of the texture. However we could reuse a deleted texture.
	if (m_force_texture_clear == 0) {
		switch(type)
		{
			case GSTexture::RenderTarget:
				ClearRenderTarget(t, 0);
				break;
			case GSTexture::DepthStencil:
				ClearDepth(t);
				// No need to clear the stencil now.
				break;
		}
	}

	return t;
}

GSTexture* GSDeviceOGL::FetchSurface(int type, int w, int h, int format)
{
	if (format == 0)
		format = (type == GSTexture::DepthStencil) ? GL_DEPTH32F_STENCIL8 : GL_RGBA8;

	GSTexture* t = GSDevice::FetchSurface(type, w, h, format);


	if (m_force_texture_clear) {
		GSVector4 red(1.0f, 0.0f, 0.0f, 1.0f);
		switch(type)
		{
			case GSTexture::RenderTarget:
				ClearRenderTarget(t, 0);
				break;
			case GSTexture::DepthStencil:
				ClearDepth(t);
				// No need to clear the stencil now.
				break;
			case GSTexture::Texture:
				if (m_force_texture_clear > 1)
					static_cast<GSTextureOGL*>(t)->Clear((void*)&red);
				else if (m_force_texture_clear)
					static_cast<GSTextureOGL*>(t)->Clear(NULL);

				break;
		}
	}

	return t;
}

// forward declaration
extern GSRenderer* s_gs;

bool GSDeviceOGL::Create()
{
	m_force_texture_clear = theApp.GetConfigI("force_texture_clear");

	// WARNING it must be done after the control setup (at least on MESA)
	// ****************************************************************
	// Various object
	// ****************************************************************
	{
		m_shader = new GSShaderOGL();

		glGenFramebuffers(1, &m_fbo);
		// Always write to the first buffer
		OMSetFBO(m_fbo);
		GLenum target[1] = {GL_COLOR_ATTACHMENT0};
		glDrawBuffers(1, target);
		OMSetFBO(GL_DEFAULT_FRAMEBUFFER);

		glGenFramebuffers(1, &m_fbo_read);
		// Always read from the first buffer
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, GL_DEFAULT_FRAMEBUFFER);
	}

	// ****************************************************************
	// Vertex buffer state
	// ****************************************************************
	{
		std::vector<GSInputLayoutOGL> il_convert = {
			{0, 2 , GL_FLOAT          , GL_FALSE , sizeof(GSVertexPT1) , (const GLvoid*)(0) }  ,
			{1, 2 , GL_FLOAT          , GL_FALSE , sizeof(GSVertexPT1) , (const GLvoid*)(16) } ,
			{2, 4 , GL_UNSIGNED_BYTE  , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(8) }  ,
			{3, 1 , GL_FLOAT          , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(12) } ,
			{4, 2 , GL_UNSIGNED_SHORT , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(16) } ,
			{5, 1 , GL_UNSIGNED_INT   , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(20) } ,
			{6, 2 , GL_UNSIGNED_SHORT , GL_FALSE , sizeof(GSVertex)    , (const GLvoid*)(24) } ,
			{7, 4 , GL_UNSIGNED_BYTE  , GL_TRUE  , sizeof(GSVertex)    , (const GLvoid*)(28) } , // Only 1 byte is useful but hardware unit only support 4B
		};
		m_va = new GSVertexBufferStateOGL(il_convert);
	}

	// ****************************************************************
	// Pre Generate the different sampler object
	// ****************************************************************
	{
		for (u32 key = 0; key < ARRAY_SIZE(m_ps_ss); key++)
			m_ps_ss[key] = CreateSampler(PSSamplerSelector(key));
	}

	// ****************************************************************
	// convert
	// ****************************************************************
	GLuint vs = 0;
	GLuint ps = 0;
	{
		m_convert.cb = new GSUniformBufferOGL("Misc UBO", g_convert_index, sizeof(MiscConstantBuffer));
		// Upload once and forget about it.
		// Use value of 1 when upscale multiplier is 0 for ScalingFactor,
		// this is to avoid doing math with 0 in shader. It helps custom res be less broken.
		m_misc_cb_cache.ScalingFactor = GSVector4i(std::max(1, option_upscale_mult));
		m_convert.cb->cache_upload(&m_misc_cb_cache);

		std::vector<char> convert_shader(convert_glsl_shader_raw, convert_glsl_shader_raw + sizeof(convert_glsl_shader_raw)/sizeof(*convert_glsl_shader_raw));

		vs = m_shader->Compile("convert.glsl", "vs_main", GL_VERTEX_SHADER, convert_shader.data());

		m_convert.vs = vs;
		for(size_t i = 0; i < ARRAY_SIZE(m_convert.ps); i++) {
			char str[32];
			snprintf(str, sizeof(str), "ps_main%d", (int)i);
			ps = m_shader->Compile("convert.glsl", str, GL_FRAGMENT_SHADER, convert_shader.data());
			std::string pretty_name = "Convert pipe " + std::to_string(i);
			m_convert.ps[i] = m_shader->LinkPipeline(pretty_name, vs, 0, ps);
		}

		PSSamplerSelector point;
		m_convert.pt = GetSamplerID(point);

		PSSamplerSelector bilinear;
		bilinear.biln = true;
		m_convert.ln = GetSamplerID(bilinear);

		m_convert.dss = new GSDepthStencilOGL();
		m_convert.dss_write = new GSDepthStencilOGL();
		m_convert.dss_write->EnableDepth();
		m_convert.dss_write->SetDepth(GL_ALWAYS, true);
	}

	// ****************************************************************
	// merge
	// ****************************************************************
	{
		m_merge_obj.cb = new GSUniformBufferOGL("Merge UBO", g_merge_cb_index, sizeof(MergeConstantBuffer));

		std::vector<char> merge_shader(merge_glsl_shader_raw, merge_glsl_shader_raw + sizeof(merge_glsl_shader_raw)/sizeof(*merge_glsl_shader_raw));

		for(size_t i = 0; i < ARRAY_SIZE(m_merge_obj.ps); i++) {
			char str[32];
			snprintf(str, sizeof(str), "ps_main%d", (int)i);
			ps = m_shader->Compile("merge.glsl", str, GL_FRAGMENT_SHADER, merge_shader.data());
			std::string pretty_name = "Merge pipe " + std::to_string(i);
			m_merge_obj.ps[i] = m_shader->LinkPipeline(pretty_name, vs, 0, ps);
		}
	}

	// ****************************************************************
	// interlace
	// ****************************************************************
	{
		m_interlace.cb = new GSUniformBufferOGL("Interlace UBO", g_interlace_cb_index, sizeof(InterlaceConstantBuffer));

		std::vector<char> interlace_shader(interlace_glsl_shader_raw, interlace_glsl_shader_raw + sizeof(interlace_glsl_shader_raw)/sizeof(*interlace_glsl_shader_raw));

		for(size_t i = 0; i < ARRAY_SIZE(m_interlace.ps); i++) {
			char str[32];
			snprintf(str, sizeof(str), "ps_main%d", (int)i);
			ps = m_shader->Compile("interlace.glsl", str, GL_FRAGMENT_SHADER, interlace_shader.data());
			std::string pretty_name = "Interlace pipe " + std::to_string(i);
			m_interlace.ps[i] = m_shader->LinkPipeline(pretty_name, vs, 0, ps);
		}
	}

	// ****************************************************************
	// rasterization configuration
	// ****************************************************************
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_CULL_FACE);
		glEnable(GL_SCISSOR_TEST);
		glDisable(GL_MULTISAMPLE);
		glDisable(GL_DITHER); // Honestly I don't know!
	}

	// ****************************************************************
	// DATE
	// ****************************************************************
	{
		m_date.dss = new GSDepthStencilOGL();
		m_date.dss->EnableStencil();
		m_date.dss->SetStencil(GL_ALWAYS, GL_REPLACE);
	}

	// ****************************************************************
	// Use DX coordinate convention
	// ****************************************************************

	// VS gl_position.z => [-1,-1]
	// FS depth => [0, 1]
	// because of -1 we loose lot of precision for small GS value
	// This extension allow FS depth to range from -1 to 1. So
	// gl_position.z could range from [0, 1]
	// Change depth convention
	if (GLExtension::Has("GL_ARB_clip_control"))
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

	// ****************************************************************
	// HW renderer shader
	// ****************************************************************
	CreateTextureFX();

	// ****************************************************************
	// Pbo Pool allocation
	// ****************************************************************
	{
		// Mesa seems to use it to compute the row length. In our case, we are
		// tightly packed so don't bother with this parameter and set it to the
		// minimum alignment (1 byte)
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		PboPool::Init();
	}

	// ****************************************************************
	// Finish window setup and backbuffer
	// ****************************************************************
	if(!GSDevice::Create())
		return false;

	GSVector4i rect = GSClientRect();
	Reset(rect.z, rect.w);

	return true;
}

void GSDeviceOGL::CreateTextureFX()
{
	m_vs_cb = new GSUniformBufferOGL("HW VS UBO", g_vs_cb_index, sizeof(VSConstantBuffer));
	m_ps_cb = new GSUniformBufferOGL("HW PS UBO", g_ps_cb_index, sizeof(PSConstantBuffer));

	// warning 1 sampler by image unit. So you cannot reuse m_ps_ss...
	m_palette_ss = CreateSampler(PSSamplerSelector(0));
	glBindSampler(1, m_palette_ss);

	// Pre compile the (remaining) Geometry & Vertex Shader
	// One-Hot encoding
	memset(m_gs, 0, sizeof(m_gs));
	m_gs[1] = CompileGS(GSSelector(1));
	m_gs[2] = CompileGS(GSSelector(2));
	m_gs[4] = CompileGS(GSSelector(4));

	for (u32 key = 0; key < ARRAY_SIZE(m_vs); key++)
		m_vs[key] = CompileVS(VSSelector(key));

	// Enable all bits for stencil operations. Technically 1 bit is
	// enough but buffer is polluted with noise. Clear will be limited
	// to the mask.
	glStencilMask(0xFF);
	for (u32 key = 0; key < ARRAY_SIZE(m_om_dss); key++)
		m_om_dss[key] = CreateDepthStencil(OMDepthStencilSelector(key));

	// Help to debug FS in apitrace
	m_apitrace = CompilePS(PSSelector());
}

bool GSDeviceOGL::Reset(int w, int h)
{
	if(!GSDevice::Reset(w, h))
		return false;

	// Opengl allocate the backbuffer with the window. The render is done in the backbuffer when
	// there isn't any FBO. Only a dummy texture is created to easily detect when the rendering is done
	// in the backbuffer
	m_backbuffer = new GSTextureOGL(GSTextureOGL::Backbuffer, w, h, 0, m_fbo_read, false);

	return true;
}

void GSDeviceOGL::Flip()
{
	video_cb(RETRO_HW_FRAME_BUFFER_VALID, s_gs->GetInternalResolution().x, s_gs->GetInternalResolution().y, 0);
}

void GSDeviceOGL::DrawPrimitive()
{
	m_va->DrawPrimitive();
}

void GSDeviceOGL::DrawPrimitive(int offset, int count)
{
	m_va->DrawPrimitive(offset, count);
}

void GSDeviceOGL::DrawIndexedPrimitive()
{
	m_va->DrawIndexedPrimitive();
}

void GSDeviceOGL::DrawIndexedPrimitive(int offset, int count)
{
	m_va->DrawIndexedPrimitive(offset, count);
}

void GSDeviceOGL::ClearRenderTarget(GSTexture* t, const GSVector4& c)
{
	if (!t) return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);
	if (T->HasBeenCleaned() && !T->IsBackbuffer())
		return;

	// Performance note: potentially T->Clear() could be used. Main purpose of
	// Clear() is to avoid the framebuffer setup cost. However, in this context,
	// the texture 't' will be set as the render target of the framebuffer and
	// therefore will require a framebuffer setup.

	// So using the old/standard path is faster/better albeit verbose.

        // Clear RT

	// TODO: check size of scissor before toggling it
	glDisable(GL_SCISSOR_TEST);

	u32 old_color_mask = GLState::wrgba;
	OMSetColorMaskState();

	if (T->IsBackbuffer()) {
		OMSetFBO(GL_DEFAULT_FRAMEBUFFER);

		// glDrawBuffer(GL_BACK); // this is the default when there is no FB
		// 0 will select the first drawbuffer ie GL_BACK
		glClearBufferfv(GL_COLOR, 0, c.v);
	} else {
		OMSetFBO(m_fbo);
		OMAttachRt(T);

		glClearBufferfv(GL_COLOR, 0, c.v);

	}

	OMSetColorMaskState(OMColorMaskSelector(old_color_mask));

	glEnable(GL_SCISSOR_TEST);

	T->WasCleaned();
}

void GSDeviceOGL::ClearRenderTarget(GSTexture* t, u32 c)
{
	if (!t) return;

	GSVector4 color = GSVector4::rgba32(c) * (1.0f / 255);
	ClearRenderTarget(t, color);
}

void GSDeviceOGL::ClearDepth(GSTexture* t)
{
	if (!t) return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);

	OMSetFBO(m_fbo);
	// RT must be detached, if RT is too small, depth won't be fully cleared
	// AT tolenico 2 map clip bug
	OMAttachRt(NULL);
	OMAttachDs(T);

	// TODO: check size of scissor before toggling it
	glDisable(GL_SCISSOR_TEST);
	float c = 0.0f;
	if (GLState::depth_mask)
		glClearBufferfv(GL_DEPTH, 0, &c);
	else
	{
		glDepthMask(true);
		glClearBufferfv(GL_DEPTH, 0, &c);
		glDepthMask(false);
	}
	glEnable(GL_SCISSOR_TEST);
}

void GSDeviceOGL::ClearStencil(GSTexture* t, u8 c)
{
	if (!t) return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);

	// Keep SCISSOR_TEST enabled on purpose to reduce the size
	// of clean in DATE (impact big upscaling)
	OMSetFBO(m_fbo);
	OMAttachDs(T);
	GLint color = c;

	glClearBufferiv(GL_STENCIL, 0, &color);
}

GLuint GSDeviceOGL::CreateSampler(PSSamplerSelector sel)
{
	GLuint sampler;
	glCreateSamplers(1, &sampler);

	// Bilinear filtering
	if (sel.biln) {
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	} else {
		glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	switch (static_cast<GS_MIN_FILTER>(sel.triln)) {
		case GS_MIN_FILTER::Nearest:
			// Nop based on biln
			break;
		case GS_MIN_FILTER::Linear:
			// Nop based on biln
			break;
		case GS_MIN_FILTER::Nearest_Mipmap_Nearest:
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			break;
		case GS_MIN_FILTER::Nearest_Mipmap_Linear:
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
			break;
		case GS_MIN_FILTER::Linear_Mipmap_Nearest:
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			break;
		case GS_MIN_FILTER::Linear_Mipmap_Linear:
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			break;
		default:
			break;
	}

	if (sel.tau)
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	else
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	if (sel.tav)
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
	else
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	int anisotropy = option_value(INT_PCSX2_OPT_ANISOTROPIC_FILTER, KeyOptionInt::return_type);
	if (anisotropy && sel.aniso)
	{
		if (GLExtension::Has("GL_ARB_texture_filter_anisotropic"))
			glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY, (float)anisotropy);
		else if (GLExtension::Has("GL_EXT_texture_filter_anisotropic"))
			glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)anisotropy);
	}

	return sampler;
}

GLuint GSDeviceOGL::GetSamplerID(PSSamplerSelector ssel)
{
	return m_ps_ss[ssel];
}

GSDepthStencilOGL* GSDeviceOGL::CreateDepthStencil(OMDepthStencilSelector dssel)
{
	GSDepthStencilOGL* dss = new GSDepthStencilOGL();

	if (dssel.date)
	{
		dss->EnableStencil();
		if (dssel.date_one)
			dss->SetStencil(GL_EQUAL, GL_ZERO);
		else
			dss->SetStencil(GL_EQUAL, GL_KEEP);
	}

	if(dssel.ztst != ZTST_ALWAYS || dssel.zwe)
	{
		static const GLenum ztst[] =
		{
			GL_NEVER,
			GL_ALWAYS,
			GL_GEQUAL,
			GL_GREATER
		};
		dss->EnableDepth();
		dss->SetDepth(ztst[dssel.ztst], dssel.zwe);
	}

	return dss;
}

void GSDeviceOGL::InitPrimDateTexture(GSTexture* rt, const GSVector4i& area)
{
	const GSVector2i& rtsize = rt->GetSize();

	// Create a texture to avoid the useless clean@0
	if (m_date.t == NULL)
		m_date.t = CreateTexture(rtsize.x, rtsize.y, GL_R32I);

	// Clean with the max signed value
	int max_int = 0x7FFFFFFF;
	static_cast<GSTextureOGL*>(m_date.t)->Clear(&max_int, area);

	glBindImageTexture(2, static_cast<GSTextureOGL*>(m_date.t)->GetID(), 0, false, 0, GL_READ_WRITE, GL_R32I);
}

void GSDeviceOGL::RecycleDateTexture()
{
	if (m_date.t) {
		Recycle(m_date.t);
		m_date.t = NULL;
	}
}

void GSDeviceOGL::Barrier(GLbitfield b)
{
	glMemoryBarrier(b);
}

GLuint GSDeviceOGL::CompileVS(VSSelector sel)
{
	std::string macro = format("#define VS_INT_FST %d\n", sel.int_fst);

	if (GLLoader::buggy_sso_dual_src)
		return m_shader->CompileShader("tfx_vgs.glsl", "vs_main", GL_VERTEX_SHADER, m_shader_tfx_vgs.data(), macro);
	return m_shader->Compile("tfx_vgs.glsl", "vs_main", GL_VERTEX_SHADER, m_shader_tfx_vgs.data(), macro);
}

GLuint GSDeviceOGL::CompileGS(GSSelector sel)
{
	std::string macro = format("#define GS_POINT %d\n", sel.point)
		+ format("#define GS_LINE %d\n", sel.line);

	if (GLLoader::buggy_sso_dual_src)
		return m_shader->CompileShader("tfx_vgs.glsl", "gs_main", GL_GEOMETRY_SHADER, m_shader_tfx_vgs.data(), macro);
	return m_shader->Compile("tfx_vgs.glsl", "gs_main", GL_GEOMETRY_SHADER, m_shader_tfx_vgs.data(), macro);
}

GLuint GSDeviceOGL::CompilePS(PSSelector sel)
{
	std::string macro = format("#define PS_FST %d\n", sel.fst)
		+ format("#define PS_WMS %d\n", sel.wms)
		+ format("#define PS_WMT %d\n", sel.wmt)
		+ format("#define PS_TEX_FMT %d\n", sel.tex_fmt)
		+ format("#define PS_DFMT %d\n", sel.dfmt)
		+ format("#define PS_DEPTH_FMT %d\n", sel.depth_fmt)
		+ format("#define PS_CHANNEL_FETCH %d\n", sel.channel)
		+ format("#define PS_URBAN_CHAOS_HLE %d\n", sel.urban_chaos_hle)
		+ format("#define PS_TALES_OF_ABYSS_HLE %d\n", sel.tales_of_abyss_hle)
		+ format("#define PS_TEX_IS_FB %d\n", sel.tex_is_fb)
		+ format("#define PS_INVALID_TEX0 %d\n", sel.invalid_tex0)
		+ format("#define PS_AEM %d\n", sel.aem)
		+ format("#define PS_TFX %d\n", sel.tfx)
		+ format("#define PS_TCC %d\n", sel.tcc)
		+ format("#define PS_ATST %d\n", sel.atst)
		+ format("#define PS_FOG %d\n", sel.fog)
		+ format("#define PS_CLR1 %d\n", sel.clr1)
		+ format("#define PS_FBA %d\n", sel.fba)
		+ format("#define PS_LTF %d\n", sel.ltf)
		+ format("#define PS_AUTOMATIC_LOD %d\n", sel.automatic_lod)
		+ format("#define PS_MANUAL_LOD %d\n", sel.manual_lod)
		+ format("#define PS_COLCLIP %d\n", sel.colclip)
		+ format("#define PS_DATE %d\n", sel.date)
		+ format("#define PS_TCOFFSETHACK %d\n", sel.tcoffsethack)
		+ format("#define PS_POINT_SAMPLER %d\n", sel.point_sampler)
		+ format("#define PS_BLEND_A %d\n", sel.blend_a)
		+ format("#define PS_BLEND_B %d\n", sel.blend_b)
		+ format("#define PS_BLEND_C %d\n", sel.blend_c)
		+ format("#define PS_BLEND_D %d\n", sel.blend_d)
		+ format("#define PS_IIP %d\n", sel.iip)
		+ format("#define PS_SHUFFLE %d\n", sel.shuffle)
		+ format("#define PS_READ_BA %d\n", sel.read_ba)
		+ format("#define PS_WRITE_RG %d\n", sel.write_rg)
		+ format("#define PS_FBMASK %d\n", sel.fbmask)
		+ format("#define PS_HDR %d\n", sel.hdr)
		+ format("#define PS_DITHER %d\n", sel.dither)
		+ format("#define PS_ZCLAMP %d\n", sel.zclamp)
		+ format("#define PS_PABE %d\n", sel.pabe)
	;

	if (GLLoader::buggy_sso_dual_src)
		return m_shader->CompileShader("tfx.glsl", "ps_main", GL_FRAGMENT_SHADER, m_shader_tfx_fs.data(), macro);
	return m_shader->Compile("tfx.glsl", "ps_main", GL_FRAGMENT_SHADER, m_shader_tfx_fs.data(), macro);
}

// blit a texture into an offscreen buffer
GSTexture* GSDeviceOGL::CopyOffscreen(GSTexture* src, const GSVector4& sRect, int w, int h, int format, int ps_shader)
{
	if (format == 0)
		format = GL_RGBA8;

	GSTexture* dst = CreateOffscreen(w, h, format);

	GSVector4 dRect(0, 0, w, h);

	// StretchRect will read an old target. However, the memory cache might contains
	// invalid data (for example due to SW blending).
	glTextureBarrier();

	StretchRect(src, sRect, dst, dRect, m_convert.ps[ps_shader]);

	return dst;
}

// Copy a sub part of texture (same as below but force a conversion)
void GSDeviceOGL::CopyRectConv(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, bool at_origin)
{
	if (!(sTex && dTex))
		return;

	const GLuint& sid = static_cast<GSTextureOGL*>(sTex)->GetID();
	const GLuint& did = static_cast<GSTextureOGL*>(dTex)->GetID();

	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);

	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sid, 0);
	if (at_origin)
		glCopyTextureSubImage2D(did, GL_TEX_LEVEL_0, 0, 0, r.x, r.y, r.width(), r.height());
	else
		glCopyTextureSubImage2D(did, GL_TEX_LEVEL_0, r.x, r.y, r.x, r.y, r.width(), r.height());

	glBindFramebuffer(GL_READ_FRAMEBUFFER, GL_DEFAULT_FRAMEBUFFER);
}

// Copy a sub part of a texture into another
void GSDeviceOGL::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r)
{
	if (!(sTex && dTex))
		return;

	const GLuint& sid = static_cast<GSTextureOGL*>(sTex)->GetID();
	const GLuint& did = static_cast<GSTextureOGL*>(dTex)->GetID();

	glCopyImageSubData( sid, GL_TEXTURE_2D,
			0, r.x, r.y, 0,
			did, GL_TEXTURE_2D,
			0, 0, 0, 0,
			r.width(), r.height(), 1);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, int shader, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, m_convert.ps[shader], linear);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, ps, m_NO_BLEND, OMColorMaskSelector(), linear);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha)
{
	OMColorMaskSelector cms;

	cms.wr = red;
	cms.wg = green;
	cms.wb = blue;
	cms.wa = alpha;

	StretchRect(sTex, sRect, dTex, dRect, m_convert.ps[ShaderConvert_COPY], m_NO_BLEND, cms, false);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, int bs, OMColorMaskSelector cms, bool linear)
{
	if(!sTex || !dTex)
		return;

	bool draw_in_depth = (ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT32] || ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT24] ||
		ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT16] || ps == m_convert.ps[ShaderConvert_RGB5A1_TO_FLOAT16]);

	// Performance optimization. It might be faster to use a framebuffer blit for standard case
	// instead to emulate it with shader
	// see https://www.opengl.org/wiki/Framebuffer#Blitting

	// ************************************
	// Init
	// ************************************

	GSVector2i ds = dTex->GetSize();

	m_shader->BindPipeline(ps);

	// ************************************
	// om
	// ************************************

	if (draw_in_depth)
		OMSetDepthStencilState(m_convert.dss_write);
	else
		OMSetDepthStencilState(m_convert.dss);

	if (draw_in_depth)
		OMSetRenderTargets(NULL, dTex);
	else
		OMSetRenderTargets(dTex, NULL);

	OMSetBlendState((u8)bs);
	OMSetColorMaskState(cms);

	// ************************************
	// ia
	// ************************************


	// Original code from DX
	float left   = dRect.x * 2 / ds.x - 1.0f;
	float right  = dRect.z * 2 / ds.x - 1.0f;
	// Opengl get some issues with the coordinate
	// I flip top/bottom to fix scaling of the internal resolution
	float top    = -1.0f + dRect.y * 2 / ds.y;
	float bottom = -1.0f + dRect.w * 2 / ds.y;

	// Flip y axis only when we render in the backbuffer
	// By default everything is render in the wrong order (ie dx).
	// 1/ consistency between several pass rendering (interlace)
	// 2/ in case some GS code expect thing in dx order.
	// Only flipping the backbuffer is transparent (I hope)...
	GSVector4 flip_sr = sRect;
	if (static_cast<GSTextureOGL*>(dTex)->IsBackbuffer()) {
		flip_sr.y = sRect.w;
		flip_sr.w = sRect.y;
	}

	GSVertexPT1 vertices[] =
	{
		{GSVector4(left  , top   , 0.0f, 0.0f) , GSVector2(flip_sr.x , flip_sr.y)} ,
		{GSVector4(right , top   , 0.0f, 0.0f) , GSVector2(flip_sr.z , flip_sr.y)} ,
		{GSVector4(left  , bottom, 0.0f, 0.0f) , GSVector2(flip_sr.x , flip_sr.w)} ,
		{GSVector4(right , bottom, 0.0f, 0.0f) , GSVector2(flip_sr.z , flip_sr.w)} ,
	};

	IASetVertexBuffer(vertices, 4);
	IASetPrimitiveTopology(GL_TRIANGLE_STRIP);

	// ************************************
	// Texture
	// ************************************

	PSSetShaderResource(0, sTex);
	PSSetSamplerState(linear ? m_convert.ln : m_convert.pt);

	// ************************************
	// Draw
	// ************************************
	DrawPrimitive();

	// ************************************
	// End
	// ************************************

	EndScene();
}

void GSDeviceOGL::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c)
{
	GSVector4 full_r(0.0f, 0.0f, 1.0f, 1.0f);
	bool feedback_write_2 = PMODE.EN2 && sTex[2] != nullptr && EXTBUF.FBIN == 1;
	bool feedback_write_1 = PMODE.EN1 && sTex[2] != nullptr && EXTBUF.FBIN == 0;
	bool feedback_write_2_but_blend_bg = feedback_write_2 && PMODE.SLBG == 1;

	// Merge the 2 source textures (sTex[0],sTex[1]). Final results go to dTex. Feedback write will go to sTex[2].
	// If either 2nd output is disabled or SLBG is 1, a background color will be used.
	// Note: background color is also used when outside of the unit rectangle area
	OMSetColorMaskState();
	ClearRenderTarget(dTex, c);

	// Upload constant to select YUV algo
	if (feedback_write_2 || feedback_write_1) {
		// Write result to feedback loop
		m_misc_cb_cache.EMOD_AC.x = EXTBUF.EMODA;
		m_misc_cb_cache.EMOD_AC.y = EXTBUF.EMODC;
		m_convert.cb->cache_upload(&m_misc_cb_cache);
	}

	if (sTex[1] && (PMODE.SLBG == 0 || feedback_write_2_but_blend_bg)) {
		// 2nd output is enabled and selected. Copy it to destination so we can blend it with 1st output
		// Note: value outside of dRect must contains the background color (c)
		StretchRect(sTex[1], sRect[1], dTex, dRect[1], ShaderConvert_COPY);
	}

	// Save 2nd output
	if (feedback_write_2) // FIXME I'm not sure dRect[1] is always correct
		StretchRect(dTex, full_r, sTex[2], dRect[1], ShaderConvert_YUV);

	// Restore background color to process the normal merge
	if (feedback_write_2_but_blend_bg)
		ClearRenderTarget(dTex, c);

	if (sTex[0]) {
		if (PMODE.AMOD == 1) // Keep the alpha from the 2nd output
			OMSetColorMaskState(OMColorMaskSelector(0x7));

		// 1st output is enabled. It must be blended
		if (PMODE.MMOD == 1)
		{
			// Blend with a constant alpha
			m_merge_obj.cb->cache_upload(&c.v);
			StretchRect(sTex[0], sRect[0], dTex, dRect[0], m_merge_obj.ps[1], m_MERGE_BLEND, OMColorMaskSelector());
		}
		else
			// Blend with 2 * input alpha
			StretchRect(sTex[0], sRect[0], dTex, dRect[0], m_merge_obj.ps[0], m_MERGE_BLEND, OMColorMaskSelector());
	}

	if (feedback_write_1) // FIXME I'm not sure dRect[0] is always correct
		StretchRect(dTex, full_r, sTex[2], dRect[0], ShaderConvert_YUV);
}

void GSDeviceOGL::DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset)
{
	OMSetColorMaskState();

	GSVector4 s = GSVector4(dTex->GetSize());

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0.0f, yoffset, s.x, s.y + yoffset);

	InterlaceConstantBuffer cb;

	cb.ZrH = GSVector2(0, 1.0f / s.y);
	cb.hH = s.y / 2;

	m_interlace.cb->cache_upload(&cb);

	StretchRect(sTex, sRect, dTex, dRect, m_interlace.ps[shader], linear);
}

void GSDeviceOGL::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
	// Lazy compile
	if (!m_fxaa.ps)
	{
		if (!GLLoader::found_GL_ARB_gpu_shader5) // GL4.0 extension
			return;

		std::string fxaa_macro = "#define FXAA_GLSL_130 1\n";
		fxaa_macro += "#extension GL_ARB_gpu_shader5 : enable\n";

		std::vector<char> shader(fxaa_shader_raw, fxaa_shader_raw + sizeof(fxaa_shader_raw)/sizeof(*fxaa_shader_raw));

		GLuint ps = m_shader->Compile("fxaa.fx", "ps_main", GL_FRAGMENT_SHADER, shader.data(), fxaa_macro);
		m_fxaa.ps = m_shader->LinkPipeline("FXAA pipe", m_convert.vs, 0, ps);
	}

	OMSetColorMaskState();

	GSVector2i s = dTex->GetSize();

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0, 0, s.x, s.y);

	StretchRect(sTex, sRect, dTex, dRect, m_fxaa.ps, true);
}

void GSDeviceOGL::SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm)
{
	// sfex3 (after the capcom logo), vf4 (first menu fading in), ffxii shadows, rumble roses shadows, persona4 shadows
	ClearStencil(ds, 0);

	m_shader->BindPipeline(m_convert.ps[datm ? ShaderConvert_DATM_1 : ShaderConvert_DATM_0]);

	// om

	OMSetDepthStencilState(m_date.dss);
	if (GLState::blend)
		glDisable(GL_BLEND);
	OMSetRenderTargets(NULL, ds, &GLState::scissor);

	// ia

	IASetVertexBuffer(vertices, 4);
	IASetPrimitiveTopology(GL_TRIANGLE_STRIP);


	// Texture

	PSSetShaderResource(0, rt);
	PSSetSamplerState(m_convert.pt);

	DrawPrimitive();

	if (GLState::blend)
		glEnable(GL_BLEND);

	EndScene();
}

void GSDeviceOGL::EndScene()
{
	m_va->EndScene();
}

void GSDeviceOGL::IASetVertexBuffer(const void* vertices, size_t count)
{
	m_va->UploadVB(vertices, count);
}

void GSDeviceOGL::IASetIndexBuffer(const void* index, size_t count)
{
	m_va->UploadIB(index, count);
}

void GSDeviceOGL::IASetPrimitiveTopology(GLenum topology)
{
	m_va->SetTopology(topology);
}

void GSDeviceOGL::PSSetShaderResource(int i, GSTexture* sr)
{
	// Note: Nvidia debugger doesn't support the id 0 (ie the NULL texture)
	if (sr)
	{
		GLuint id = static_cast<GSTextureOGL*>(sr)->GetID();
		if (GLState::tex_unit[i] != id) {
			GLState::tex_unit[i] = id;
			glBindTextureUnit(i, id);
		}
	}
}

void GSDeviceOGL::PSSetShaderResources(GSTexture* sr0, GSTexture* sr1)
{
	PSSetShaderResource(0, sr0);
	PSSetShaderResource(1, sr1);
}

void GSDeviceOGL::PSSetSamplerState(GLuint ss)
{
	if (GLState::ps_ss != ss) {
		GLState::ps_ss = ss;
		glBindSampler(0, ss);
	}
}

void GSDeviceOGL::OMAttachRt(GSTextureOGL* rt)
{
	GLuint id  = 0;
	if (rt)
	{
		rt->WasAttached();
		id = rt->GetID();
	}

	if (GLState::rt != id)
	{
		GLState::rt = id;
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0);
	}
}

void GSDeviceOGL::OMAttachDs(GSTextureOGL* ds)
{
	GLuint id  = 0;
	if (ds)
	{
		ds->WasAttached();
		id = ds->GetID();
	}

	if (GLState::ds != id)
	{
		GLState::ds = id;
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, id, 0);
	}
}

void GSDeviceOGL::OMSetFBO(GLuint fbo)
{
	if (GLState::fbo != fbo) {
		GLState::fbo = fbo;
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
	}
}

void GSDeviceOGL::OMSetDepthStencilState(GSDepthStencilOGL* dss)
{
	dss->SetupDepth();
	dss->SetupStencil();
}

void GSDeviceOGL::OMSetColorMaskState(OMColorMaskSelector sel)
{
	if (sel.wrgba != GLState::wrgba) {
		GLState::wrgba = sel.wrgba;

		glColorMaski(0, sel.wr, sel.wg, sel.wb, sel.wa);
	}
}

void GSDeviceOGL::OMSetBlendState(u8 blend_index, u8 blend_factor, bool is_blend_constant, bool accumulation_blend, bool blend_mix)
{
	if (blend_index) {
		if (!GLState::blend) {
			GLState::blend = true;
			glEnable(GL_BLEND);
		}

		if (is_blend_constant && GLState::bf != blend_factor) {
			GLState::bf = blend_factor;
			float bf = (float)blend_factor / 128.0f;
			glBlendColor(bf, bf, bf, bf);
		}

		HWBlend b = GetBlend(blend_index);
		if (accumulation_blend) {
			b.src = GL_ONE;
			b.dst = GL_ONE;
		}
		else if (blend_mix)
			b.src = GL_ONE;

		if (GLState::eq_RGB != b.op) {
			GLState::eq_RGB = b.op;
			glBlendEquationSeparate(b.op, GL_FUNC_ADD);
		}

		if (GLState::f_sRGB != b.src || GLState::f_dRGB != b.dst) {
			GLState::f_sRGB = b.src;
			GLState::f_dRGB = b.dst;
			glBlendFuncSeparate(b.src, b.dst, GL_ONE, GL_ZERO);
		}

	} else {
		if (GLState::blend) {
			GLState::blend = false;
			glDisable(GL_BLEND);
		}
	}
}

void GSDeviceOGL::OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor)
{
	GSTextureOGL* RT = static_cast<GSTextureOGL*>(rt);
	GSTextureOGL* DS = static_cast<GSTextureOGL*>(ds);

	if (rt == NULL || !RT->IsBackbuffer()) {
		OMSetFBO(m_fbo);
		if (rt)
			OMAttachRt(RT);
		else
			OMAttachRt();

		// Note: it must be done after OMSetFBO
		if (ds)
			OMAttachDs(DS);
		else
			OMAttachDs();

	} else {
		// Render in the backbuffer
		OMSetFBO(GL_DEFAULT_FRAMEBUFFER);
	}


	GSVector2i size = rt ? rt->GetSize() : ds ? ds->GetSize() : GLState::viewport;
	if(GLState::viewport != size)
	{
		GLState::viewport = size;
		// FIXME ViewportIndexedf or ViewportIndexedfv (GL4.1)
		glViewportIndexedf(0, 0, 0, GLfloat(size.x), GLfloat(size.y));
	}

	GSVector4i r = scissor ? *scissor : GSVector4i(size).zwxy();

	if(!GLState::scissor.eq(r))
	{
		GLState::scissor = r;
		// FIXME ScissorIndexedv (GL4.1)
		glScissorIndexed(0, r.x, r.y, r.width(), r.height());
	}
}

void GSDeviceOGL::SetupCB(const VSConstantBuffer* vs_cb, const PSConstantBuffer* ps_cb)
{
	if(m_vs_cb_cache.Update(vs_cb))
		m_vs_cb->upload(vs_cb);

	if(m_ps_cb_cache.Update(ps_cb))
		m_ps_cb->upload(ps_cb);
}

void GSDeviceOGL::SetupCBMisc(const GSVector4i& channel)
{
	m_misc_cb_cache.ChannelShuffle = channel;
	m_convert.cb->cache_upload(&m_misc_cb_cache);
}

void GSDeviceOGL::SetupPipeline(const VSSelector& vsel, const GSSelector& gsel, const PSSelector& psel)
{
	GLuint ps;
	auto i = m_ps.find(psel);

	if (i == m_ps.end())
	{
		ps = CompilePS(psel);
		m_ps[psel] = ps;
	}
	else
		ps = i->second;

	if (GLLoader::buggy_sso_dual_src)
		m_shader->BindProgram(m_vs[vsel], m_gs[gsel], ps);
	else
		m_shader->BindPipeline(m_vs[vsel], m_gs[gsel], ps);
}

void GSDeviceOGL::SetupSampler(PSSamplerSelector ssel)
{
	PSSetSamplerState(m_ps_ss[ssel]);
}

GLuint GSDeviceOGL::GetPaletteSamplerID()
{
	return m_palette_ss;
}

void GSDeviceOGL::SetupOM(OMDepthStencilSelector dssel)
{
	OMSetDepthStencilState(m_om_dss[dssel]);
}

u16 GSDeviceOGL::ConvertBlendEnum(u16 generic)
{
	switch (generic)
	{
	case SRC_COLOR       : return GL_SRC_COLOR;
	case INV_SRC_COLOR   : return GL_ONE_MINUS_SRC_COLOR;
	case DST_COLOR       : return GL_DST_COLOR;
	case INV_DST_COLOR   : return GL_ONE_MINUS_DST_COLOR;
	case SRC1_COLOR      : return GL_SRC1_COLOR;
	case INV_SRC1_COLOR  : return GL_ONE_MINUS_SRC1_COLOR;
	case SRC_ALPHA       : return GL_SRC_ALPHA;
	case INV_SRC_ALPHA   : return GL_ONE_MINUS_SRC_ALPHA;
	case DST_ALPHA       : return GL_DST_ALPHA;
	case INV_DST_ALPHA   : return GL_ONE_MINUS_DST_ALPHA;
	case SRC1_ALPHA      : return GL_SRC1_ALPHA;
	case INV_SRC1_ALPHA  : return GL_ONE_MINUS_SRC1_ALPHA;
	case CONST_COLOR     : return GL_CONSTANT_COLOR;
	case INV_CONST_COLOR : return GL_ONE_MINUS_CONSTANT_COLOR;
	case CONST_ONE       : return GL_ONE;
	case CONST_ZERO      : return GL_ZERO;
	case OP_ADD          : return GL_FUNC_ADD;
	case OP_SUBTRACT     : return GL_FUNC_SUBTRACT;
	case OP_REV_SUBTRACT : return GL_FUNC_REVERSE_SUBTRACT;
	default              : break;
	}
	return 0;
}
