/*
 *	Copyright (C) 2011-2013 Gregory hainaut
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

#include "stdafx.h"
#include "GSShaderOGL.h"
#include "GLState.h"

#ifdef _WIN32
#include "resource.h"
#else
#include "GSdxResources.h"
#endif

GSShaderOGL::GSShaderOGL() : m_pipeline(0)
{
	theApp.LoadResource(IDR_COMMON_GLSL, m_common_header);

	// Create a default pipeline
	m_pipeline = LinkPipeline("HW pipe", 0, 0, 0);
	BindPipeline(m_pipeline);
}

GSShaderOGL::~GSShaderOGL()
{
	printf("Delete %zu Shaders, %zu Programs, %zu Pipelines\n",
			m_shad_to_delete.size(), m_prog_to_delete.size(), m_pipe_to_delete.size());

	for (auto s : m_shad_to_delete) glDeleteShader(s);
	for (auto p : m_prog_to_delete) glDeleteProgram(p);
	glDeleteProgramPipelines(m_pipe_to_delete.size(), &m_pipe_to_delete[0]);
}

GLuint GSShaderOGL::LinkPipeline(const std::string& pretty_print, GLuint vs, GLuint gs, GLuint ps)
{
	GLuint p;
	glCreateProgramPipelines(1, &p);
	glUseProgramStages(p, GL_VERTEX_SHADER_BIT, vs);
	glUseProgramStages(p, GL_GEOMETRY_SHADER_BIT, gs);
	glUseProgramStages(p, GL_FRAGMENT_SHADER_BIT, ps);

	glObjectLabel(GL_PROGRAM_PIPELINE, p, pretty_print.size(), pretty_print.c_str());

	m_pipe_to_delete.push_back(p);

	return p;
}

GLuint GSShaderOGL::LinkProgram(GLuint vs, GLuint gs, GLuint ps)
{
	uint32 hash = ((vs ^ gs) << 24) ^ ps;
	auto it = m_program.find(hash);
	if (it != m_program.end())
		return it->second;

	GLuint p = glCreateProgram();
	if (vs) glAttachShader(p, vs);
	if (ps) glAttachShader(p, ps);
	if (gs) glAttachShader(p, gs);

	glLinkProgram(p);

	m_prog_to_delete.push_back(p);
	m_program[hash] = p;

	return p;
}

void GSShaderOGL::BindProgram(GLuint vs, GLuint gs, GLuint ps)
{
	GLuint p = LinkProgram(vs, gs, ps);

	if (GLState::program != p) {
		GLState::program = p;
		glUseProgram(p);
	}
}

void GSShaderOGL::BindProgram(GLuint p)
{
	if (GLState::program != p) {
		GLState::program = p;
		glUseProgram(p);
	}
}

void GSShaderOGL::BindPipeline(GLuint vs, GLuint gs, GLuint ps)
{
	BindPipeline(m_pipeline);

	if (GLState::vs != vs) {
		GLState::vs = vs;
		glUseProgramStages(m_pipeline, GL_VERTEX_SHADER_BIT, vs);
	}

	if (GLState::gs != gs) {
		GLState::gs = gs;
		glUseProgramStages(m_pipeline, GL_GEOMETRY_SHADER_BIT, gs);
	}

#ifdef _DEBUG
	// In debug always sets the program. It allow to replace the program in apitrace easily.
	if (true)
#else
	if (GLState::ps != ps)
#endif
	{
		GLState::ps = ps;
		glUseProgramStages(m_pipeline, GL_FRAGMENT_SHADER_BIT, ps);
	}
}

void GSShaderOGL::BindPipeline(GLuint pipe)
{
	if (GLState::pipeline != pipe) {
		GLState::pipeline = pipe;
		glBindProgramPipeline(pipe);
	}

	if (GLState::program) {
		GLState::program = 0;
		glUseProgram(0);
	}
}

std::string GSShaderOGL::GenGlslHeader(const std::string& entry, GLenum type, const std::string& macro)
{
	std::string header;
	header = "#version 330 core\n";
	// Need GL version 420
	header += "#extension GL_ARB_shading_language_420pack: require\n";
	// Need GL version 410
	header += "#extension GL_ARB_separate_shader_objects: require\n";
	if (GLLoader::found_GL_ARB_shader_image_load_store) {
		// Need GL version 420
		header += "#extension GL_ARB_shader_image_load_store: require\n";
	} else {
		header += "#define DISABLE_GL42_image\n";
	}
	if (GLLoader::vendor_id_amd || GLLoader::vendor_id_intel)
		header += "#define BROKEN_DRIVER as_usual\n";

	// Stupid GL implementation (can't use GL_ES)
	// AMD/nvidia define it to 0
	// intel window don't define it
	// intel linux refuse to define it
	header += "#define pGL_ES 0\n";

	// Allow to puts several shader in 1 files
	switch (type) {
		case GL_VERTEX_SHADER:
			header += "#define VERTEX_SHADER 1\n";
			break;
		case GL_GEOMETRY_SHADER:
			header += "#define GEOMETRY_SHADER 1\n";
			break;
		case GL_FRAGMENT_SHADER:
			header += "#define FRAGMENT_SHADER 1\n";
			break;
		default: ASSERT(0);
	}

	// Select the entry point ie the main function
	header += format("#define %s main\n", entry.c_str());

	header += macro;

	return header;
}

GLuint GSShaderOGL::Compile(const std::string& glsl_file, const std::string& entry, GLenum type, const char* glsl_h_code, const std::string& macro_sel)
{
	ASSERT(glsl_h_code != NULL);

	GLuint program = 0;

	// Note it is better to separate header and source file to have the good line number
	// in the glsl compiler report
	const int shader_nb = 3;
	const char* sources[shader_nb];

	std::string header = GenGlslHeader(entry, type, macro_sel);

	sources[0] = header.c_str();
	sources[1] = m_common_header.data();
	sources[2] = glsl_h_code;

	program = glCreateShaderProgramv(type, shader_nb, sources);

	m_prog_to_delete.push_back(program);

	return program;
}

// Same as above but for not-separated build
GLuint GSShaderOGL::CompileShader(const std::string& glsl_file, const std::string& entry, GLenum type, const char* glsl_h_code, const std::string& macro_sel)
{
	ASSERT(glsl_h_code != NULL);

	GLuint shader = 0;

	// Note it is better to separate header and source file to have the good line number
	// in the glsl compiler report
	const int shader_nb = 3;
	const char* sources[shader_nb];

	std::string header = GenGlslHeader(entry, type, macro_sel);

	sources[0] = header.c_str();
	sources[1] = m_common_header.data();
	sources[2] = glsl_h_code;

	shader =  glCreateShader(type);
	glShaderSource(shader, shader_nb, sources, NULL);
	glCompileShader(shader);

	m_shad_to_delete.push_back(shader);

	return shader;
}
