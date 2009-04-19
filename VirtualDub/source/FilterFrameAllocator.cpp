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
#include <vd2/system/profile.h>
#include "FilterFrame.h"
#include "FilterFrameAllocator.h"
#include "FilterFrameCache.h"

VDFilterFrameAllocator::VDFilterFrameAllocator()
	: mSizeRequired(0)
	, mMinFrames(0)
	, mMaxFrames(0x7fffffff)
	, mAllocatedFrames(0)
	, mAllocatedBytes(0)
	, mActiveFrames(0)
	, mActiveBytes(0)
	, mTrimCounter(0)
	, mTrimPeriod(0)
{
}

VDFilterFrameAllocator::~VDFilterFrameAllocator() {
	Shutdown();
}

void VDFilterFrameAllocator::Init(uint32 minFrames, uint32 maxFrames) {
	mMinFrames = minFrames;
	mMaxFrames = maxFrames;

	VDASSERT(mActiveBuffers.empty());
	VDASSERT(mIdleBuffers.empty());

	for(int i=0; i<mMinFrames; ++i) {
		vdrefptr<VDFilterFrameBuffer> buf(new VDFilterFrameBuffer);

		buf->Init(mSizeRequired);
		buf->SetAllocator(this);

		mIdleBuffers.push_back(buf);
		buf.release();
	}

	mAllocatedFrames = mMinFrames;
	mAllocatedBytes = mMinFrames * mSizeRequired;
	mActiveFrames = 0;
	mActiveBytes = 0;
	mTrimCounter = 0;
	mTrimPeriod = 50;
	mCurrentWatermark = 0;

	VDRTProfiler *profiler = VDGetRTProfiler();
	if (profiler) {
		profiler->RegisterCounterU32("Allocated frames", &mAllocatedFrames);
		profiler->RegisterCounterU32("Allocated bytes", &mAllocatedBytes);
		profiler->RegisterCounterU32("Active frames", &mActiveFrames);
		profiler->RegisterCounterU32("Active bytes", &mActiveBytes);
	}
}

void VDFilterFrameAllocator::Shutdown() {
	VDRTProfiler *profiler = VDGetRTProfiler();
	if (profiler) {
		profiler->UnregisterCounter(&mAllocatedFrames);
		profiler->UnregisterCounter(&mAllocatedBytes);
		profiler->UnregisterCounter(&mActiveFrames);
		profiler->UnregisterCounter(&mActiveBytes);
	}

	mSizeRequired = 0;
	mAllocatedFrames = 0;
	mAllocatedBytes = 0;
	mActiveFrames = 0;
	mActiveBytes = 0;

	mIdleBuffers.splice(mIdleBuffers.end(), mActiveBuffers);

	while(!mIdleBuffers.empty()) {
		VDFilterFrameBuffer *buf = static_cast<VDFilterFrameBuffer *>(mIdleBuffers.back());
		mIdleBuffers.pop_back();

		buf->SetAllocator(NULL);
		buf->Release();
	}
}

void VDFilterFrameAllocator::AddSizeRequirement(uint32 bytes) {
	if (mSizeRequired < bytes)
		mSizeRequired = bytes;
}

bool VDFilterFrameAllocator::Allocate(VDFilterFrameBuffer **buffer) {
	vdrefptr<VDFilterFrameBuffer> buf;

	if (mIdleBuffers.empty()) {
		if (mAllocatedFrames >= mMaxFrames)
			return false;

		buf = new VDFilterFrameBuffer;
		buf->Init(mSizeRequired);

		buf->AddRef();
		mActiveBuffers.push_back(buf);

		// must do this AFTER refcount hits 2 to avoid OnFrameBufferActive() callback
		buf->SetAllocator(this);

		++mAllocatedFrames;
		mAllocatedBytes += mSizeRequired;
		++mActiveFrames;
		mActiveBytes += mSizeRequired;
	} else {
		// The implicit AddRef() here will knock it out of the idle buffers list to
		// the active list.
		buf = static_cast<VDFilterFrameBuffer *>(mIdleBuffers.front());

		buf->EvictFromCaches();
	}

	if (mCurrentWatermark < mAllocatedFrames)
		mCurrentWatermark = mAllocatedFrames;

	if (++mTrimCounter >= mTrimPeriod) {
		if (mAllocatedFrames > mCurrentWatermark && !mIdleBuffers.empty()) {
			VDFilterFrameBuffer *trimBuf = static_cast<VDFilterFrameBuffer *>(mIdleBuffers.front());
			mIdleBuffers.pop_front();
			--mAllocatedFrames;
			mAllocatedBytes -= mSizeRequired;

			trimBuf->SetAllocator(NULL);
			trimBuf->Release();
		}

		mTrimCounter = 0;
		mCurrentWatermark = 0;
	}

	*buffer = buf.release();
	return true;
}

void VDFilterFrameAllocator::OnFrameBufferIdle(VDFilterFrameBuffer *buf) {
	mIdleBuffers.splice(mIdleBuffers.end(), mActiveBuffers, buf);

	--mActiveFrames;
	mActiveBytes -= mSizeRequired;
}

void VDFilterFrameAllocator::OnFrameBufferActive(VDFilterFrameBuffer *buf) {
	mActiveBuffers.splice(mActiveBuffers.begin(), mIdleBuffers, buf);

	++mActiveFrames;
	mActiveBytes += mSizeRequired;
}
