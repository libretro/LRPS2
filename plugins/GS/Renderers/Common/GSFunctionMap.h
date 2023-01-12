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

#include "../../GSCodeBuffer.h"

template<class KEY, class VALUE> class GSFunctionMap
{
protected:
	struct ActivePtr
	{
		VALUE f;
	};

	std::unordered_map<KEY, VALUE> m_map;
	std::unordered_map<KEY, ActivePtr*> m_map_active;

	ActivePtr* m_active;

	virtual VALUE GetDefaultFunction(KEY key) = 0;

public:
	GSFunctionMap()
		: m_active(NULL)
	{
	}

	virtual ~GSFunctionMap()
	{
		for(auto &i : m_map_active) delete i.second;
	}

	VALUE operator [] (KEY key)
	{
		m_active = NULL;

		auto it = m_map_active.find(key);

		if(it != m_map_active.end())
		{
			m_active = it->second;
		}
		else
		{
			auto i = m_map.find(key);

			ActivePtr* p = new ActivePtr();

			memset(p, 0, sizeof(*p));

			p->f = (i != m_map.end()) 
				? i->second 
				: GetDefaultFunction(key);

			m_active = m_map_active[key] = p;
		}

		return m_active->f;
	}
};

template<class CG, class KEY, class VALUE>
class GSCodeGeneratorFunctionMap : public GSFunctionMap<KEY, VALUE>
{
	void* m_param;
	std::unordered_map<u64, VALUE> m_cgmap;
	GSCodeBuffer m_cb;

public:
	GSCodeGeneratorFunctionMap(const char* name, void* param)
		: m_param(param) { }
	~GSCodeGeneratorFunctionMap() { }

	VALUE GetDefaultFunction(KEY key)
	{
		VALUE ret = NULL;

		auto i = m_cgmap.find(key);

		if(i != m_cgmap.end())
			return i->second;

		CG* cg = new CG(m_param, key, 
				m_cb.GetBuffer(8192), 8192);

		m_cb.ReleaseBuffer(cg->getSize());

		ret = m_cgmap[key] = (VALUE)cg->getCode();

		delete cg;

		return ret;
	}
};
