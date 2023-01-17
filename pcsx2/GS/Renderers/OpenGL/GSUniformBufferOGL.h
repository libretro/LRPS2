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

#pragma once

#include "Pcsx2Types.h"

#include "GLState.h"

class GSUniformBufferOGL {
	GLuint m_buffer;		// data object
	GLuint m_index;		// GLSL slot
	u32 m_size;	    // size of the data
	u8* m_cache;       // content of the previous upload

public:
	GSUniformBufferOGL(const std::string& pretty_name, GLuint index, u32 size)
		: m_index(index), m_size(size)
	{
		glGenBuffers(1, &m_buffer);
		bind();
		allocate();
		attach();
		m_cache = (u8*)AlignedMalloc(m_size, 32);
		memset(m_cache, 0, m_size);
	}

	void bind()
	{
		if (GLState::ubo != m_buffer) {
			GLState::ubo = m_buffer;
			glBindBuffer(GL_UNIFORM_BUFFER, m_buffer);
		}
	}

	void allocate()
	{
		glBufferData(GL_UNIFORM_BUFFER, m_size, NULL, GL_DYNAMIC_DRAW);
	}

	void attach()
	{
		// From the opengl manpage:
		// glBindBufferBase also binds buffer to the generic buffer binding point specified by target
		GLState::ubo = m_buffer;
		glBindBufferBase(GL_UNIFORM_BUFFER, m_index, m_buffer);
	}

	void upload(const void* src)
	{
		bind();
		// glMapBufferRange allow to set various parameter but the call is
		// synchronous whereas glBufferSubData could be asynchronous.
		// TODO: investigate the extension ARB_invalidate_subdata
		glBufferSubData(GL_UNIFORM_BUFFER, 0, m_size, src);
	}

	void cache_upload(const void* src)
	{
		if (memcmp(m_cache, src, m_size) != 0) {
			memcpy(m_cache, src, m_size);
			upload(src);
		}
	}

	~GSUniformBufferOGL()
	{
		glDeleteBuffers(1, &m_buffer);
		AlignedFree(m_cache);
	}
};
