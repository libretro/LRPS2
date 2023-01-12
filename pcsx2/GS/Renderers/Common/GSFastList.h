/*
*	Copyright (C) 2017-2017 Alessandro Vetere
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
#include "Utilities/ScopedAlloc.h"
#include "../../stdafx.h"

template <class T>
struct Element {
	T  data;
	u16 next_index;
	u16 prev_index;
};

template <class T>
class FastListIterator;

template <class T>
class FastList {
	friend class FastListIterator<T>;
private:
	// The index of the first element of the list is m_buffer[0].next_index
	//     The first Element<T> of the list has prev_index equal to 0
	// The index of the last element of the list is m_buffer[0].prev_index
	//     The last Element<T> of the list has next_index equal to 0
	// All the other Element<T> of the list are chained by next_index and prev_index
	// m_buffer has dynamic size m_capacity
	// Due to m_buffer reallocation, the pointers to Element<T> stored into the array
	//     are invalidated every time Grow() is executed. But FastListIterator<T> is
	//     index based, not pointer based, and the elements are copied in order on Grow(),
	//     so there is no iterator invalidation (which is an index invalidation) until 
	//     the relevant iterator (or the index alone) are erased from the list.
	// m_buffer[0] is always present as auxiliary Element<T> of the list
	Element<T>* m_buffer;
	u16 m_capacity;
	u16 m_free_indexes_stack_top;
	// m_free_indexes_stack has dynamic size (m_capacity - 1)
	// m_buffer indexes that are free to be used are stacked here
	u16* m_free_indexes_stack;

public:
	GS_FORCEINLINE FastList() {
		m_buffer = nullptr;
		clear();
	}

	GS_FORCEINLINE ~FastList() {
		AlignedFree(m_buffer);
	}

	void clear() {
		// Initialize m_capacity to 4 so we avoid to Grow() on initial insertions
		// The code doesn't break if this value is changed with anything from 1 to USHRT_MAX
		m_capacity = 4;

		// Initialize m_buffer and m_free_indexes_stack as a contiguous block of memory starting at m_buffer
		// This should increase cache locality and reduce memory fragmentation
		AlignedFree(m_buffer);
		m_buffer = (Element<T>*)AlignedMalloc(m_capacity * sizeof(Element<T>) + (m_capacity - 1) * sizeof(u16), 64);
		m_free_indexes_stack = (u16*)&m_buffer[m_capacity];

		// Initialize m_buffer[0], data field is unused but initialized using default T constructor
		m_buffer[0] = { T(), 0, 0 };

		// m_free_indexes_stack top index is 0, bottom index is m_capacity - 2
		m_free_indexes_stack_top = 0;

		// m_buffer index 0 is reserved for auxiliary element
		for (u16 i = 0; i < m_capacity - 1; i++) {
			m_free_indexes_stack[i] = i + 1;
		}
	}

	// Insert the element in front of the list and return its position in m_buffer
	GS_FORCEINLINE u16 InsertFront(const T& data) {
		if (Full()) {
			Grow();
		}

		// Pop a free index from the stack
		const u16 free_index = m_free_indexes_stack[m_free_indexes_stack_top++];
		m_buffer[free_index].data = data;
		ListInsertFront(free_index);
		return free_index;
	}

	GS_FORCEINLINE void push_front(const T& data) {
		InsertFront(data);
	}

	GS_FORCEINLINE const T& back() const {
		return m_buffer[LastIndex()].data;
	}

	GS_FORCEINLINE void pop_back() {
		EraseIndex(LastIndex());
	}

	GS_FORCEINLINE u16 size() const {
		return m_free_indexes_stack_top;
	}

	GS_FORCEINLINE bool empty() const {
		return size() == 0;
	}	

	GS_FORCEINLINE void EraseIndex(const u16 index) {
		ListRemove(index);
		m_free_indexes_stack[--m_free_indexes_stack_top] = index;
	}

	GS_FORCEINLINE void MoveFront(const u16 index) {
		if (FirstIndex() != index) {
			ListRemove(index);
			ListInsertFront(index);
		}
	}

	GS_FORCEINLINE const FastListIterator<T> begin() const {
		return FastListIterator<T>(this, FirstIndex());
	}

	GS_FORCEINLINE const FastListIterator<T> end() const {
		return FastListIterator<T>(this, 0);
	}

	GS_FORCEINLINE FastListIterator<T> erase(FastListIterator<T> i) {
		EraseIndex(i.Index());
		return ++i;
	}

private:
	// Accessed by FastListIterator<T> using class friendship
	GS_FORCEINLINE const T& Data(const u16 index) const {
		return m_buffer[index].data;
	}

	// Accessed by FastListIterator<T> using class friendship
	GS_FORCEINLINE u16 NextIndex(const u16 index) const {
		return m_buffer[index].next_index;
	}

	// Accessed by FastListIterator<T> using class friendship
	GS_FORCEINLINE u16 PrevIndex(const u16 index) const {
		return m_buffer[index].prev_index;
	}

	GS_FORCEINLINE u16 FirstIndex() const {
		return m_buffer[0].next_index;
	}

	GS_FORCEINLINE u16 LastIndex() const {
		return m_buffer[0].prev_index;
	}

	GS_FORCEINLINE bool Full() const {
		// The minus one is due to the presence of the auxiliary element
		return size() == m_capacity - 1;
	}

	GS_FORCEINLINE void ListInsertFront(const u16 index) {
		// Update prev / next indexes to add m_buffer[index] to the chain
		Element<T>& head = m_buffer[0];
		m_buffer[index].prev_index = 0;
		m_buffer[index].next_index = head.next_index;
		m_buffer[head.next_index].prev_index = index;
		head.next_index = index;
	}

	GS_FORCEINLINE void ListRemove(const u16 index) {
		// Update prev / next indexes to remove m_buffer[index] from the chain
		const Element<T>& to_remove = m_buffer[index];
		m_buffer[to_remove.prev_index].next_index = to_remove.next_index;
		m_buffer[to_remove.next_index].prev_index = to_remove.prev_index;
	}

	void Grow() {
		const u16 new_capacity = m_capacity <= (USHRT_MAX / 2) ? (m_capacity * 2) : USHRT_MAX;

		Element<T>* new_buffer = (Element<T>*)AlignedMalloc(new_capacity * sizeof(Element<T>) + (new_capacity - 1) * sizeof(u16), 64);
		u16* new_free_indexes_stack = (u16*)&new_buffer[new_capacity];

		memcpy(new_buffer, m_buffer, m_capacity * sizeof(Element<T>));
		memcpy(new_free_indexes_stack, m_free_indexes_stack, (m_capacity - 1) * sizeof(u16));
		
		AlignedFree(m_buffer);
		
		m_buffer = new_buffer;
		m_free_indexes_stack = new_free_indexes_stack;

		// Initialize the additional space in the stack
		for (u16 i = m_capacity - 1; i < new_capacity - 1; i++) {
			m_free_indexes_stack[i] = i + 1;
		}

		m_capacity = new_capacity;
	}
};


template <class T>
// This iterator is const_iterator
class FastListIterator
{
private:
	const FastList<T>* m_fastlist;
	u16 m_index;

public:
	GS_FORCEINLINE FastListIterator(const FastList<T>* fastlist, const u16 index) {
		m_fastlist = fastlist;
		m_index = index;
	}

	GS_FORCEINLINE bool operator!=(const FastListIterator<T>& other) const {
		return (m_index != other.m_index);
	}

	GS_FORCEINLINE bool operator==(const FastListIterator<T>& other) const {
		return (m_index == other.m_index);
	}

	// Prefix increment
	GS_FORCEINLINE const FastListIterator<T>& operator++() {
		m_index = m_fastlist->NextIndex(m_index);
		return *this;
	}

	// Postfix increment
	GS_FORCEINLINE const FastListIterator<T> operator++(int) {
		FastListIterator<T> copy(*this);
		++(*this);
		return copy;
	}

	// Prefix decrement
	GS_FORCEINLINE const FastListIterator<T>& operator--() {
		m_index = m_fastlist->PrevIndex(m_index);
		return *this;
	}

	// Postfix decrement
	GS_FORCEINLINE const FastListIterator<T> operator--(int) {
		FastListIterator<T> copy(*this);
		--(*this);
		return copy;
	}

	GS_FORCEINLINE const T& operator*() const {
		return m_fastlist->Data(m_index);
	}

	GS_FORCEINLINE u16 Index() const {
		return m_index;
	}
};
