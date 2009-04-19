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

#include "stdafx.h"
#include <vd2/system/error.h>
#include "FilterFrame.h"
#include "FilterFrameAllocator.h"
#include "FilterFrameCache.h"

///////////////////////////////////////////////////////////////////////////

VDFilterFrameBuffer::VDFilterFrameBuffer()
	: mpBuffer(NULL)
	, mBufferSize(0)
	, mbVirtAlloc(false)
	, mpAllocator(NULL)
{
}

VDFilterFrameBuffer::~VDFilterFrameBuffer() {
	Shutdown();
}

int VDFilterFrameBuffer::AddRef() {
	int rc = vdrefcounted<IVDRefCount>::AddRef();
	if (rc == 2) {
		if (mpAllocator)
			mpAllocator->OnFrameBufferActive(this);
	}

	return rc;
}

int VDFilterFrameBuffer::Release() {
	int rc = vdrefcounted<IVDRefCount>::Release();
	if (rc == 1) {
		if (mpAllocator)
			mpAllocator->OnFrameBufferIdle(this);
	}

	return rc;
}

void VDFilterFrameBuffer::Init(uint32 size) {
	VDASSERT(!mpBuffer);

	if (size >= 262144) {
		mbVirtAlloc = true;
		mpBuffer = VDAlignedVirtualAlloc(size);
	} else {
		mbVirtAlloc = false;
		mpBuffer = VDAlignedMalloc(size, 16);
	}

	if (!mpBuffer)
		throw MyMemoryError();

	mBufferSize = size;
}

void VDFilterFrameBuffer::Shutdown() {
	EvictFromCaches();

	if (mpBuffer) {
		if (mbVirtAlloc)
			VDAlignedVirtualFree(mpBuffer);
		else
			VDAlignedFree(mpBuffer);
		mpBuffer = NULL;
		mBufferSize = 0;
	}
}

void VDFilterFrameBuffer::SetAllocator(VDFilterFrameAllocator *allocator) {
	mpAllocator = allocator;
}

void VDFilterFrameBuffer::AddCacheReference(VDFilterFrameBufferCacheLinkNode *cacheLink) {
	mCaches.push_back(cacheLink);
}

void VDFilterFrameBuffer::RemoveCacheReference(VDFilterFrameBufferCacheLinkNode *cacheLink) {
	VDASSERT(GetCacheReference(cacheLink->mpCache) == cacheLink);
	mCaches.erase(cacheLink);
	cacheLink->mListNodePrev = NULL;
	cacheLink->mListNodeNext = NULL;
}

VDFilterFrameBufferCacheLinkNode *VDFilterFrameBuffer::GetCacheReference(VDFilterFrameCache *cache) {
	for(Caches::iterator it(mCaches.begin()), itEnd(mCaches.end()); it != itEnd; ++it) {
		VDFilterFrameBufferCacheLinkNode *linkNode = *it;

		if (linkNode->mpCache == cache)
			return linkNode;
	}

	return NULL;
}

bool VDFilterFrameBuffer::Steal(uint32 references) {
	if (mRefCount > 1 + (int)references)
		return false;

	EvictFromCaches();
	return true;
}

void VDFilterFrameBuffer::EvictFromCaches() {
	while(!mCaches.empty()) {
		VDFilterFrameBufferCacheLinkNode *linkNode = mCaches.front();

		linkNode->mpCache->Evict(linkNode);
	}
}
