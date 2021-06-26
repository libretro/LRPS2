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
#include "GSPerfMon.h"

GSPerfMon::GSPerfMon()
	: m_frame(0)
{
	memset(m_stats, 0, sizeof(m_stats));
	memset(m_total, 0, sizeof(m_total));
	memset(m_begin, 0, sizeof(m_begin));
}

void GSPerfMon::Put(counter_t c, double val)
{
}

void GSPerfMon::Update()
{
}

void GSPerfMon::Start(int timer)
{
}

void GSPerfMon::Stop(int timer)
{
}

int GSPerfMon::CPU(int timer, bool reset)
{
	int percent = (int)(100 * m_total[timer] / (__rdtsc() - m_begin[timer]));

	if(reset)
	{
		m_begin[timer] = 0;
		m_start[timer] = 0;
		m_total[timer] = 0;
	}

	return percent;
}
