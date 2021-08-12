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
#include "GS.h"
#include <fstream>

GSdxApp theApp;

GSdxApp::GSdxApp()
{
	// Empty constructor causes an illegal instruction exception on an SSE4.2 machine on Windows.
	// Non-empty doesn't, but raises a SIGILL signal when compiled against GCC 6.1.1.
	// So here's a compromise.
#ifdef _WIN32
	Init();
#endif
}

void GSdxApp::Init()
{
	static bool is_initialised = false;
	if (is_initialised)
		return;
	is_initialised = true;

	m_current_renderer_type = GSRendererType::Undefined;

	// Init configuration map with default values
	// later in the flow they will be overwritten by custom config

	// Avoid to clutter the ini file with useless options
#ifdef _WIN32
	m_current_configuration["dx_break_on_severity"]                       = "0";
	// D3D Blending option
	m_current_configuration["accurate_blending_unit_d3d11"]               = "1";
#endif
	m_current_configuration["aa1"]                                        = "0";
	m_current_configuration["accurate_date"]                              = "1";
	m_current_configuration["accurate_blending_unit"]                     = "1";
	m_current_configuration["AspectRatio"]                                = "1";
	m_current_configuration["autoflush_sw"]                               = "1";
	m_current_configuration["clut_load_before_draw"]                      = "0";
	m_current_configuration["crc_hack_level"]                             = std::to_string(static_cast<int8>(CRCHackLevel::Automatic));
	m_current_configuration["CrcHacksExclusions"]                         = "";
	m_current_configuration["debug_glsl_shader"]                          = "0";
	m_current_configuration["debug_opengl"]                               = "0";
	m_current_configuration["disable_hw_gl_draw"]                         = "0";
	m_current_configuration["dithering_ps2"]                              = "2";
	m_current_configuration["dump"]                                       = "0";
	m_current_configuration["extrathreads"]                               = "2";
	m_current_configuration["extrathreads_height"]                        = "4";
	m_current_configuration["filter"]                                     = std::to_string(static_cast<int8>(BiFiltering::PS2));
	m_current_configuration["force_texture_clear"]                        = "0";
	m_current_configuration["fxaa"]                                       = "0";
	m_current_configuration["interlace"]                                  = "7";
	m_current_configuration["large_framebuffer"]                          = "0";
	m_current_configuration["linear_present"]                             = "1";
	m_current_configuration["MaxAnisotropy"]                              = "0";
	m_current_configuration["mipmap"]                                     = "1";
	m_current_configuration["mipmap_hw"]                                  = std::to_string(static_cast<int>(HWMipmapLevel::Automatic));
	m_current_configuration["ModeHeight"]                                 = "480";
	m_current_configuration["ModeWidth"]                                  = "640";
	m_current_configuration["NTSC_Saturation"]                            = "1";
	m_current_configuration["override_geometry_shader"]                   = "-1";
	m_current_configuration["override_GL_ARB_compute_shader"]             = "-1";
	m_current_configuration["override_GL_ARB_copy_image"]                 = "-1";
	m_current_configuration["override_GL_ARB_clear_texture"]              = "-1";
	m_current_configuration["override_GL_ARB_clip_control"]               = "-1";
	m_current_configuration["override_GL_ARB_direct_state_access"]        = "-1";
	m_current_configuration["override_GL_ARB_draw_buffers_blend"]         = "-1";
	m_current_configuration["override_GL_ARB_get_texture_sub_image"]      = "-1";
	m_current_configuration["override_GL_ARB_gpu_shader5"]                = "-1";
	m_current_configuration["override_GL_ARB_multi_bind"]                 = "-1";
	m_current_configuration["override_GL_ARB_shader_image_load_store"]    = "-1";
	m_current_configuration["override_GL_ARB_shader_storage_buffer_object"] = "-1";
	m_current_configuration["override_GL_ARB_sparse_texture"]             = "-1";
	m_current_configuration["override_GL_ARB_sparse_texture2"]            = "-1";
	m_current_configuration["override_GL_ARB_texture_view"]               = "-1";
	m_current_configuration["override_GL_ARB_vertex_attrib_binding"]      = "-1";
	m_current_configuration["override_GL_ARB_texture_barrier"]            = "-1";
	m_current_configuration["paltex"]                                     = "0";
	m_current_configuration["png_compression_level"]                      = std::to_string(Z_BEST_SPEED);
	m_current_configuration["preload_frame_with_gs_data"]                 = "0";
	m_current_configuration["Renderer"]                                   = std::to_string(static_cast<int>(GSRendererType::Default));
	m_current_configuration["resx"]                                       = "1024";
	m_current_configuration["resy"]                                       = "1024";
	m_current_configuration["save"]                                       = "0";
	m_current_configuration["savef"]                                      = "0";
	m_current_configuration["savel"]                                      = "5000";
	m_current_configuration["saven"]                                      = "0";
	m_current_configuration["savet"]                                      = "0";
	m_current_configuration["savez"]                                      = "0";
	m_current_configuration["shaderfx"]                                   = "0";
	m_current_configuration["shaderfx_conf"]                              = "shaders/GSdx_FX_Settings.ini";
	m_current_configuration["shaderfx_glsl"]                              = "shaders/GSdx.fx";
	m_current_configuration["TVShader"]                                   = "0";
	m_current_configuration["upscale_multiplier"]                         = "1";
	m_current_configuration["UserHacks"]                                  = "0";
	m_current_configuration["UserHacks_align_sprite_X"]                   = "0";
	m_current_configuration["UserHacks_AutoFlush"]                        = "0";
	m_current_configuration["UserHacks_DisableDepthSupport"]              = "0";
	m_current_configuration["UserHacks_Disable_Safe_Features"]            = "0";
	m_current_configuration["UserHacks_DisablePartialInvalidation"]       = "0";
	m_current_configuration["UserHacks_CPU_FB_Conversion"]                = "0";
	m_current_configuration["UserHacks_Half_Bottom_Override"]             = "-1";
	m_current_configuration["UserHacks_HalfPixelOffset"]                  = "0";
	m_current_configuration["UserHacks_merge_pp_sprite"]                  = "0";
	m_current_configuration["UserHacks_round_sprite_offset"]              = "0";
	m_current_configuration["UserHacks_SkipDraw"]                         = "0";
	m_current_configuration["UserHacks_SkipDraw_Offset"]                  = "0";
	m_current_configuration["UserHacks_TCOffsetX"]                        = "0";
	m_current_configuration["UserHacks_TCOffsetY"]                        = "0";
	m_current_configuration["UserHacks_TextureInsideRt"]                  = "0";
	m_current_configuration["UserHacks_TriFilter"]                        = std::to_string(static_cast<int8>(TriFiltering::None));
	m_current_configuration["UserHacks_WildHack"]                         = "0";
	m_current_configuration["wrap_gs_mem"]                                = "0";
	m_current_configuration["vsync"]                                      = "0";
}


std::string GSdxApp::GetConfigS(const char* entry)
{
	
	return m_current_configuration[entry];
}

void GSdxApp::SetConfig(const char* entry, const char* value)
{
	m_current_configuration[entry] = value;
}

int GSdxApp::GetConfigI(const char* entry)
{
	return std::stoi(m_current_configuration[entry]);

}

bool GSdxApp::GetConfigB(const char* entry)
{
	return !!GetConfigI(entry);
}

void GSdxApp::SetConfig(const char* entry, int value)
{
	char buff[32] = {0};

	sprintf(buff, "%d", value);

	SetConfig(entry, buff);
}

void GSdxApp::SetCurrentRendererType(GSRendererType type)
{
	m_current_renderer_type = type;
}

GSRendererType GSdxApp::GetCurrentRendererType() const
{
	return m_current_renderer_type;
}
