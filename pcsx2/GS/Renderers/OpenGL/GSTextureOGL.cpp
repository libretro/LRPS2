/*
 *	Copyright (C) 2011-2011 Gregory hainaut
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

#include <limits.h>
#include <cmath>

#include "../../stdafx.h"

#include "Utilities/ScopedAlloc.h"

#include "GSTextureOGL.h"
#include "GLState.h"

// FIXME OGL4: investigate, only 1 unpack buffer always bound
namespace PboPool {

	const  u32 m_pbo_size = 64*1024*1024;
	const  u32 m_seg_size = 16*1024*1024;

	GLuint m_buffer;
	uptr   m_offset;
	char*  m_map;
	u32 m_size;
	GLsync m_fence[m_pbo_size/m_seg_size];

	// Option for buffer storage
	// XXX: actually does I really need coherent and barrier???
	// As far as I understand glTexSubImage2D is a client-server transfer so no need to make
	// the value visible to the server
	const GLbitfield common_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
	const GLbitfield map_flags = common_flags | GL_MAP_FLUSH_EXPLICIT_BIT;
	const GLbitfield create_flags = common_flags | GL_CLIENT_STORAGE_BIT;

	void Init() {
		glGenBuffers(1, &m_buffer);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_buffer);
		glBufferStorage(GL_PIXEL_UNPACK_BUFFER, m_pbo_size, NULL, create_flags);
		m_map    = (char*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, m_pbo_size, map_flags);
		m_offset = 0;

		for (size_t i = 0; i < ARRAY_SIZE(m_fence); i++)
			m_fence[i] = 0;

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}

	char* Map(u32 size) {
		char* map;
		// Note: keep offset aligned for SSE/AVX
		m_size = (size + 63) & ~0x3F;

		// Note: texsubimage will access currently bound buffer
		// Pbo ready let's get a pointer
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_buffer);

		Sync();

		map = m_map + m_offset;

		return map;
	}

	void Destroy() {
		m_map    = NULL;
		m_offset = 0;

		for (size_t i = 0; i < ARRAY_SIZE(m_fence); i++)
			glDeleteSync(m_fence[i]);

		glDeleteBuffers(1, &m_buffer);
	}

	void Sync() {
		u32 segment_current = m_offset / m_seg_size;
		u32 segment_next    = (m_offset + m_size) / m_seg_size;

		if (segment_current != segment_next) {
			if (segment_next >= ARRAY_SIZE(m_fence))
				segment_next = 0;
			// Align current transfer on the start of the segment
			m_offset = m_seg_size * segment_next;

			// protect the left segment
			m_fence[segment_current] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

			// Check next segment is free
			if (m_fence[segment_next]) {
				glClientWaitSync(m_fence[segment_next], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
				// Potentially it doesn't work on AMD driver which might always return GL_CONDITION_SATISFIED

				glDeleteSync(m_fence[segment_next]);
				m_fence[segment_next] = 0;
			}
		}
	}
}

GSTextureOGL::GSTextureOGL(int type, int w, int h, int format, GLuint fbo_read, bool mipmap)
	: m_clean(false), m_generate_mipmap(true), m_local_buffer(nullptr), m_r_x(0), m_r_y(0), m_r_w(0), m_r_h(0), m_layer(0)
{
	// OpenGL didn't like dimensions of size 0
	m_size.x = std::max(1,w);
	m_size.y = std::max(1,h);
	m_format = format;
	m_type   = type;
	m_fbo_read = fbo_read;
	m_texture_id = 0;
	m_max_layer = 1;

	// Bunch of constant parameter
	switch (m_format) {
			// 1 Channel integer
		case GL_R32UI:
		case GL_R32I:
			m_int_format    = GL_RED_INTEGER;
			m_int_type      = (m_format == GL_R32UI) ? GL_UNSIGNED_INT : GL_INT;
			m_int_shift     = 2;
			break;
		case GL_R16UI:
			m_int_format    = GL_RED_INTEGER;
			m_int_type      = GL_UNSIGNED_SHORT;
			m_int_shift     = 1;
			break;

			// 1 Channel normalized
		case GL_R8:
			m_int_format    = GL_RED;
			m_int_type      = GL_UNSIGNED_BYTE;
			m_int_shift     = 0;
			break;

			// 4 channel normalized
		case GL_RGBA16:
			m_int_format    = GL_RGBA;
			m_int_type      = GL_UNSIGNED_SHORT;
			m_int_shift     = 3;
			break;
		case GL_RGBA8:
			m_int_format    = GL_RGBA;
			m_int_type      = GL_UNSIGNED_BYTE;
			m_int_shift     = 2;
			break;

			// 4 channel integer
		case GL_RGBA16I:
		case GL_RGBA16UI:
			m_int_format    = GL_RGBA_INTEGER;
			m_int_type      = (m_format == GL_R16UI) ? GL_UNSIGNED_SHORT : GL_SHORT;
			m_int_shift     = 3;
			break;

			// 4 channel float
		case GL_RGBA32F:
			m_int_format    = GL_RGBA;
			m_int_type      = GL_FLOAT;
			m_int_shift     = 4;
			break;
		case GL_RGBA16F:
			m_int_format    = GL_RGBA;
			m_int_type      = GL_HALF_FLOAT;
			m_int_shift     = 3;
			break;

			// Depth buffer
		case GL_DEPTH32F_STENCIL8:
			m_int_format    = GL_DEPTH_STENCIL;
			m_int_type      = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
			m_int_shift     = 3; // 4 bytes for depth + 4 bytes for stencil by texels
			break;

			// Backbuffer
		case 0:
			m_int_format    = 0;
			m_int_type      = 0;
			m_int_shift     = 2; // 4 bytes by texels
			break;

		default:
			m_int_format    = 0;
			m_int_type      = 0;
			m_int_shift     = 0;
			break;
	}

	switch (m_type) {
		case GSTexture::Backbuffer:
			return; // backbuffer isn't a real texture
		case GSTexture::Offscreen:
			// Offscreen is only used to read color. So it only requires 4B by pixel
			m_local_buffer = (u8*)AlignedMalloc(m_size.x * m_size.y * 4, 32);
			break;
		case GSTexture::Texture:
			// Only 32 bits input texture will be supported for mipmap
			m_max_layer = mipmap && m_format == GL_RGBA8 ? (int)log2(std::max(w,h)) : 1;
			break;
		default:
			break;
	}

	// Create a GL object (texture isn't allocated here)
	glCreateTextures(GL_TEXTURE_2D, 1, &m_texture_id);

	// Emulate DX behavior, besides it avoids special code in shader to differentiate
	// palette texture from a GL_RGBA target or a GL_R texture.
	if (m_format == GL_R8)
		glTextureParameteri(m_texture_id, GL_TEXTURE_SWIZZLE_A, GL_RED);

	glTextureStorage2D(m_texture_id, m_max_layer + GL_TEX_LEVEL_0, m_format, m_size.x, m_size.y);
}

GSTextureOGL::~GSTextureOGL()
{
	/* Unbind the texture from our local state */

	if (m_texture_id == GLState::rt)
		GLState::rt = 0;
	if (m_texture_id == GLState::ds)
		GLState::ds = 0;
	for (size_t i = 0; i < ARRAY_SIZE(GLState::tex_unit); i++) {
		if (m_texture_id == GLState::tex_unit[i])
			GLState::tex_unit[i] = 0;
	}

	glDeleteTextures(1, &m_texture_id);

	if (m_local_buffer)
		AlignedFree(m_local_buffer);
}

void GSTextureOGL::Clear(const void* data)
{
	glClearTexImage(m_texture_id, GL_TEX_LEVEL_0, m_int_format, m_int_type, data);
}

void GSTextureOGL::Clear(const void* data, const GSVector4i& area)
{
	glClearTexSubImage(m_texture_id, GL_TEX_LEVEL_0, area.x, area.y, 0, area.width(), area.height(), 1, m_int_format, m_int_type, data);
}

bool GSTextureOGL::Update(const GSVector4i& r, const void* data, int pitch, int layer)
{
	if (layer >= m_max_layer)
		return true;

	// Default upload path for the texture is the Map/Unmap
	// This path is mostly used for palette. But also for texture that could
	// overflow the pbo buffer
	// Data upload is rather small typically 64B or 1024B. So don't bother with PBO
	// and directly send the data to the GL synchronously

	m_clean = false;

	u32 row_byte = r.width() << m_int_shift;
	u32 map_size = r.height() * row_byte;

	// The complex solution with PBO
	char* src = (char*)data;
	char* map = PboPool::Map(map_size);

	// PERF: slow path of the texture upload. Dunno if we could do better maybe check if TC can keep row_byte == pitch
	// Note: row_byte != pitch
	for (int h = 0; h < r.height(); h++)
	{
		memcpy(map, src, row_byte);
		map += row_byte;
		src += pitch;
	}

	glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, PboPool::m_offset, PboPool::m_size);

	glTextureSubImage2D(m_texture_id, layer, r.x, r.y, r.width(), r.height(), m_int_format, m_int_type, (const void*)PboPool::m_offset);

	// FIXME OGL4: investigate, only 1 unpack buffer always bound
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	PboPool::m_offset += PboPool::m_size;

	m_generate_mipmap = true;

	return true;
}

bool GSTextureOGL::Map(GSMap& m, const GSVector4i* _r, int layer)
{
	if (layer >= m_max_layer)
		return false;

	GSVector4i r = _r ? *_r : GSVector4i(0, 0, m_size.x, m_size.y);

	u32 row_byte = r.width() << m_int_shift;
	m.pitch = row_byte;

	if (m_type == GSTexture::Offscreen) {
		// The fastest way will be to use a PBO to read the data asynchronously. Unfortunately GS
		// architecture is waiting the data right now.

		// Bind the texture to the read framebuffer to avoid any disturbance
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);
		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture_id, 0);

		// In case a target is 16 bits (GT4)
		glPixelStorei(GL_PACK_ALIGNMENT, 1u << m_int_shift);

		glReadPixels(r.x, r.y, r.width(), r.height(), m_int_format, m_int_type, m_local_buffer);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, GL_DEFAULT_FRAMEBUFFER);

		m.bits = m_local_buffer;

		return true;
	} else if (m_type == GSTexture::Texture || m_type == GSTexture::RenderTarget) {
		m_clean = false;

		u32 map_size = r.height() * row_byte;

		m.bits = (u8*)PboPool::Map(map_size);

		// Save the area for the unmap
		m_r_x = r.x;
		m_r_y = r.y;
		m_r_w = r.width();
		m_r_h = r.height();
		m_layer = layer;

		return true;
	}

	return false;
}

void GSTextureOGL::Unmap()
{
	if (m_type == GSTexture::Texture || m_type == GSTexture::RenderTarget) {

		glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, PboPool::m_offset, PboPool::m_size);

		glTextureSubImage2D(m_texture_id, m_layer, m_r_x, m_r_y, m_r_w, m_r_h, m_int_format, m_int_type, (const void*)PboPool::m_offset);

		// FIXME OGL4: investigate, only 1 unpack buffer always bound
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

		PboPool::m_offset += PboPool::m_size;

		m_generate_mipmap = true;
	}
}

void GSTextureOGL::GenerateMipmap()
{
	if (m_generate_mipmap && m_max_layer > 1) {
		glGenerateTextureMipmap(m_texture_id);
		m_generate_mipmap = false;
	}
}
