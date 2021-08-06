/*
 *	Copyright (C) 2007-2012 Gabest
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
#include "GSWnd.h"

class GSWndRetroGL : public GSWndGL
{
public:
	GSWndRetroGL();
	virtual ~GSWndRetroGL() {}

	bool Create() final;

	GSVector4i GetClientRect();

	void* GetProcAddress(const char* name, bool opt = false) final;

	void Flip() final;
};

class GSWndRetro : public GSWnd
{
public:
	GSWndRetro() {}
	virtual ~GSWndRetro() {}

	bool Create();

	GSVector4i GetClientRect();

	void Flip() final;
};
