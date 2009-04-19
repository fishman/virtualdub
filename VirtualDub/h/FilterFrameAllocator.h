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

#ifndef f_VD2_FILTERFRAMEALLOCATOR_H
#define f_VD2_FILTERFRAMEALLOCATOR_H

#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>

class VDFilterFrameBuffer;
struct VDFilterFrameBufferAllocatorNode;

///////////////////////////////////////////////////////////////////////////
//
//	VDFilterFrameAllocator
//
///////////////////////////////////////////////////////////////////////////

class VDFilterFrameAllocator : public vdrefcounted<IVDRefCount> {
	VDFilterFrameAllocator(const VDFilterFrameAllocator&);
	VDFilterFrameAllocator& operator=(const VDFilterFrameAllocator&);
public:
	VDFilterFrameAllocator();
	~VDFilterFrameAllocator();

	uint32 GetFrameSize() const { return mSizeRequired; }

	void Init(uint32 minFrames, uint32 maxFrames);
	void Shutdown();

	void AddSizeRequirement(uint32 bytes);

	bool Allocate(VDFilterFrameBuffer **buffer);

	void OnFrameBufferIdle(VDFilterFrameBuffer *buf);
	void OnFrameBufferActive(VDFilterFrameBuffer *buf);

protected:
	uint32	mSizeRequired;
	uint32	mMinFrames;
	uint32	mMaxFrames;
	uint32	mAllocatedFrames;
	uint32	mAllocatedBytes;
	uint32	mActiveFrames;
	uint32	mActiveBytes;

	uint32	mTrimCounter;
	uint32	mTrimPeriod;
	uint32	mCurrentWatermark;

	typedef vdlist<VDFilterFrameBufferAllocatorNode> Buffers;
	Buffers mActiveBuffers;
	Buffers mIdleBuffers;
};

#endif
