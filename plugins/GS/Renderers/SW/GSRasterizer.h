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

#include <thread>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <vector>

#include "Pcsx2Types.h"

#include "../../GS.h"
#include "GSVertexSW.h"
#include "../../GSAlignedClass.h"

#include "Utilities/boost_spsc_queue.hpp"

template<class T, int CAPACITY> class GSJobQueue final
{
private:
	std::thread m_thread;
	std::function<void(T&)> m_func;
	bool m_exit;
	ringbuffer_base<T, CAPACITY> m_queue;

	std::mutex m_lock;
	std::mutex m_wait_lock;
	std::condition_variable m_empty;
	std::condition_variable m_notempty;

	void ThreadProc() {
		std::unique_lock<std::mutex> l(m_lock);

		while (true) {

			while (m_queue.empty()) {
				if (m_exit)
					return;

				m_notempty.wait(l);
			}

			l.unlock();

			while (m_queue.consume_one(*this))
				;

			{
				std::lock_guard<std::mutex> wait_guard(m_wait_lock);
			}
			m_empty.notify_one();

			l.lock();
		}
	}

public:
	GSJobQueue(std::function<void(T&)> func) :
		m_func(func),
		m_exit(false)
	{
		m_thread = std::thread(&GSJobQueue::ThreadProc, this);
	}

	~GSJobQueue()
	{
		{
			std::lock_guard<std::mutex> l(m_lock);
			m_exit = true;
		}
		m_notempty.notify_one();

		m_thread.join();
	}

	bool IsEmpty()
	{
		return m_queue.empty();
	}

	void Push(const T& item) {
		while(!m_queue.push(item))
			std::this_thread::yield();

		{
			std::lock_guard<std::mutex> l(m_lock);
		}
		m_notempty.notify_one();
	}

	void Wait()
	{
		if (IsEmpty())
			return;

		std::unique_lock<std::mutex> l(m_wait_lock);
		while (!IsEmpty())
			m_empty.wait(l);
	}

	void operator() (T& item) {
		m_func(item);
	}
};

class alignas(32) GSRasterizerData : public GSAlignedClass<32>
{
	static int s_counter;

public:
	GSVector4i scissor;
	GSVector4i bbox;
	GS_PRIM_CLASS primclass;
	u8* buff;
	GSVertexSW* vertex;
	int vertex_count;
	u32* index;
	int index_count;
	u64 frame;
	int pixels;
	int counter;

	GSRasterizerData() 
		: scissor(GSVector4i::zero())
		, bbox(GSVector4i::zero())
		, primclass(GS_INVALID_CLASS)
		, buff(NULL)
		, vertex(NULL)
		, vertex_count(0)
		, index(NULL)
		, index_count(0)
		, frame(0)
		, pixels(0)
	{
		counter = s_counter++;
	}

	virtual ~GSRasterizerData() 
	{
		if(buff) AlignedFree(buff);
	}
};

class IDrawScanline : public GSAlignedClass<32>
{
public:
	typedef void (*SetupPrimPtr)(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan);
	typedef void (*DrawScanlinePtr)(int pixels, int left, int top, const GSVertexSW& scan);
	typedef void (IDrawScanline::*DrawRectPtr)(const GSVector4i& r, const GSVertexSW& v); // TODO: jit

protected:
	SetupPrimPtr m_sp;
	DrawScanlinePtr m_ds;
	DrawScanlinePtr m_de;
	DrawRectPtr m_dr;

public:
	IDrawScanline() : m_sp(NULL), m_ds(NULL), m_de(NULL), m_dr(NULL) {}
	virtual ~IDrawScanline() {}

	virtual void BeginDraw(const GSRasterizerData* data) = 0;
	virtual void EndDraw(u64 frame, int actual, int total) = 0;

	GS_FORCEINLINE void SetupPrim(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan) {m_sp(vertex, index, dscan);}
	GS_FORCEINLINE void DrawScanline(int pixels, int left, int top, const GSVertexSW& scan) {m_ds(pixels, left, top, scan);}
	GS_FORCEINLINE void DrawEdge(int pixels, int left, int top, const GSVertexSW& scan) {m_de(pixels, left, top, scan);}
	GS_FORCEINLINE void DrawRect(const GSVector4i& r, const GSVertexSW& v) {(this->*m_dr)(r, v);}


	GS_FORCEINLINE bool HasEdge() const {return m_de != NULL;}
	GS_FORCEINLINE bool IsSolidRect() const {return m_dr != NULL;}
};

class IRasterizer : public GSAlignedClass<32>
{
public:
	virtual ~IRasterizer() {}

	virtual void Queue(const std::shared_ptr<GSRasterizerData>& data) = 0;
	virtual void Sync() = 0;
	virtual bool IsSynced() const = 0;
	virtual int GetPixels(bool reset = true) = 0;
};

class alignas(32) GSRasterizer : public IRasterizer
{
protected:
	IDrawScanline* m_ds;
	int m_id;
	int m_threads;
	int m_thread_height;
	u8* m_scanline;
	GSVector4i m_scissor;
	GSVector4 m_fscissor_x;
	GSVector4 m_fscissor_y;
	struct {GSVertexSW* buff; int count;} m_edge;
	struct {int sum, actual, total;} m_pixels;

	typedef void (GSRasterizer::*DrawPrimPtr)(const GSVertexSW* v, int count);

	template<bool scissor_test>
	void DrawPoint(const GSVertexSW* vertex, int vertex_count, const u32* index, int index_count);
	void DrawLine(const GSVertexSW* vertex, const u32* index);
	void DrawTriangle(const GSVertexSW* vertex, const u32* index);
	void DrawSprite(const GSVertexSW* vertex, const u32* index);

	#if _M_SSE >= 0x501
	GS_FORCEINLINE void DrawTriangleSection(int top, int bottom, GSVertexSW2& edge, const GSVertexSW2& dedge, const GSVertexSW2& dscan, const GSVector4& p0);
	#else
	GS_FORCEINLINE void DrawTriangleSection(int top, int bottom, GSVertexSW& edge, const GSVertexSW& dedge, const GSVertexSW& dscan, const GSVector4& p0);
	#endif

	void DrawEdge(const GSVertexSW& v0, const GSVertexSW& v1, const GSVertexSW& dv, int orientation, int side);

	GS_FORCEINLINE void AddScanline(GSVertexSW* e, int pixels, int left, int top, const GSVertexSW& scan);
	GS_FORCEINLINE void Flush(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan, bool edge = false);

	GS_FORCEINLINE void DrawScanline(int pixels, int left, int top, const GSVertexSW& scan);
	GS_FORCEINLINE void DrawEdge(int pixels, int left, int top, const GSVertexSW& scan);

public:
	GSRasterizer(IDrawScanline* ds, int id, int threads);
	virtual ~GSRasterizer();

	GS_FORCEINLINE bool IsOneOfMyScanlines(int top) const;
	GS_FORCEINLINE bool IsOneOfMyScanlines(int top, int bottom) const;
	GS_FORCEINLINE int FindMyNextScanline(int top) const;

	void Draw(GSRasterizerData* data);

	// IRasterizer

	void Queue(const std::shared_ptr<GSRasterizerData>& data);
	void Sync() {}
	bool IsSynced() const {return true;}
	int GetPixels(bool reset);
};

class GSRasterizerList : public IRasterizer
{
protected:
	using GSWorker = GSJobQueue<std::shared_ptr<GSRasterizerData>, 65536>;

	// Worker threads depend on the rasterizers, so don't change the order.
	std::vector<std::unique_ptr<GSRasterizer>> m_r;
	std::vector<std::unique_ptr<GSWorker>> m_workers;
	u8* m_scanline;
	int m_thread_height;

	GSRasterizerList(int threads);

public:
	virtual ~GSRasterizerList();

	template<class DS> static IRasterizer* Create(int threads)
	{
		threads = std::max<int>(threads, 0);

		if(threads == 0)
		{
			return new GSRasterizer(new DS(), 0, 1);
		}

		GSRasterizerList* rl = new GSRasterizerList(threads);

		for(int i = 0; i < threads; i++)
		{
			rl->m_r.push_back(std::unique_ptr<GSRasterizer>(new GSRasterizer(new DS(), i, threads)));
			auto &r = *rl->m_r[i];
			rl->m_workers.push_back(std::unique_ptr<GSWorker>(new GSWorker(
				[&r](std::shared_ptr<GSRasterizerData> &item) { r.Draw(item.get()); })));
		}

		return rl;
	}

	// IRasterizer

	void Queue(const std::shared_ptr<GSRasterizerData>& data);
	void Sync();
	bool IsSynced() const;
	int GetPixels(bool reset);
};
