//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef f_VD2_FILTERFRAME_H
#define f_VD2_FILTERFRAME_H

#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>

class VDFilterFrameAllocator;
class VDFilterFrameCache;

struct VDFilterFrameBufferCacheLinkNode : public vdlist_node {
	VDFilterFrameCache *mpCache;
};

///////////////////////////////////////////////////////////////////////////
//
//	VDFilterFrameBuffer
//
///////////////////////////////////////////////////////////////////////////

struct VDFilterFrameBufferAllocatorNode : public vdlist_node {};

class VDFilterFrameBuffer	: public vdrefcounted<IVDRefCount>
							, public VDFilterFrameBufferAllocatorNode
{
	VDFilterFrameBuffer(const VDFilterFrameBuffer&);
	VDFilterFrameBuffer& operator=(const VDFilterFrameBuffer&);
public:
	VDFilterFrameBuffer();

	int AddRef();
	int Release();

	void Init(uint32 size);
	void Shutdown();

	void SetAllocator(VDFilterFrameAllocator *allocator);

	void *GetBasePointer() { return mpBuffer; }
	const void *GetBasePointer() const { return mpBuffer; }

	uint32 GetSize() const { return mBufferSize; }

	void AddCacheReference(VDFilterFrameBufferCacheLinkNode *cacheLink);
	void RemoveCacheReference(VDFilterFrameBufferCacheLinkNode *cacheLink);
	VDFilterFrameBufferCacheLinkNode *GetCacheReference(VDFilterFrameCache *cache);

	bool Steal(uint32 references);
	void EvictFromCaches();

protected:
	~VDFilterFrameBuffer();

	void *mpBuffer;
	uint32 mBufferSize;
	bool	mbVirtAlloc;

	VDFilterFrameAllocator *mpAllocator;

	typedef vdlist<VDFilterFrameBufferCacheLinkNode> Caches;
	Caches mCaches;
};

#endif	// f_VD2_FILTERFRAME_H
