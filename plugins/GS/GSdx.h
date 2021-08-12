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

#include "GS.h"

class GSdxApp
{
	std::map< std::string, std::string > m_current_configuration;
	GSRendererType m_current_renderer_type;

public:
	GSdxApp();

	void Init();

	void SetConfig(const char* entry, const char* value);
	void SetConfig(const char* entry, int value);
	// Avoid issue with overloading
	template<typename T>
	T      GetConfigT(const char* entry) { return static_cast<T>(GetConfigI(entry)); }
	int    GetConfigI(const char* entry);
	bool   GetConfigB(const char* entry);
	std::string GetConfigS(const char* entry);

	void SetCurrentRendererType(GSRendererType type);
	GSRendererType GetCurrentRendererType() const;

};

struct GSDXError {};
struct GSDXRecoverableError : GSDXError {};
struct GSDXErrorGlVertexArrayTooSmall : GSDXError {};

extern GSdxApp theApp;
