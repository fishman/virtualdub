//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef f_VIRTUALDUB_FILTERSYSTEM_H
#define f_VIRTUALDUB_FILTERSYSTEM_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/list.h>
#include <vd2/system/fraction.h>
#include "filter.h"

class FilterInstance;
class VDXFilterStateInfo;
class FilterSystemBitmap;
class VFBitmapInternal;
struct VDPixmap;
struct VDPixmapLayout;

class FilterSystem {
public:
	FilterSystem();
	~FilterSystem();
	void prepareLinearChain(List *listFA, uint32 src_width, uint32 src_height, int src_format, const VDFraction& sourceFrameRate, sint64 sourceFrameCount);
	void initLinearChain(List *listFA, uint32 src_width, uint32 src_height, int src_format, const uint32 *palette, const VDFraction& sourceFrameRate, sint64 sourceFrameCount);
	void ReadyFilters();
	void RestartFilters();
	bool RunFilters(sint64 outputFrame, sint64 timelineFrame, sint64 sequenceFrame, sint64 sequenceTimeMS, FilterInstance *pfiStopPoint, uint32 flags);
	void DeinitFilters();
	void DeallocateBuffers();
	const VDPixmap& GetInput() const;
	const VDPixmapLayout& GetInputLayout() const;
	const VDPixmap& GetOutput() const;
	const VDPixmapLayout& GetOutputLayout() const;
	bool isRunning();
	bool isEmpty() const { return listFilters->IsEmpty(); }

	int getFrameLag();

	bool	IsFiltered(VDPosition frame) const;
	sint64	GetSourceFrame(sint64 outframe) const;
	const VDFraction GetOutputFrameRate() const;
	sint64	GetOutputFrameCount() const;

private:
	void AllocateVBitmaps(int count);
	void AllocateBuffers(uint32 lTotalBufferNeeded);

	uint32 dwFlags;
	int iBitmapCount;

	enum {
		FILTERS_INITIALIZED = 0x00000001L,
		FILTERS_ERROR		= 0x00000002L,
	};

	VDFraction	mOutputFrameRate;
	sint64		mOutputFrameCount;
	FilterSystemBitmap *bitmap;
	VFBitmapInternal *bmLast;
	List *listFilters;
	int nFrameLag;
	int mFrameDelayLeft;
	bool mbFirstFrame;

	unsigned char *lpBuffer;
	long lAdditionalBytes;
	bool fSharedWindow;

	struct RequestInfo {
		sint64	mSourceFrame;
		sint64	mOutputFrame;
		sint64	mTimelineFrame;
	};

	vdfastvector<RequestInfo>	mRequestStack;

	uint32	mPalette[256];
};

#endif
