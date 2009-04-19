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

#include "stdafx.h"

#include <vd2/system/bitmath.h>
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/protscope.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "VBitmap.h"
#include "crash.h"
#include "misc.h"

#include "filters.h"

#include "FilterSystem.h"

extern FilterFunctions g_filterFuncs;

///////////////////////////////////////////////////////////////////////////

class FilterSystemBitmap : public VFBitmapInternal {
public:
	int				buffer;
	LONG			lMapOffset;
};

///////////////////////////////////////////////////////////////////////////

FilterSystem::FilterSystem() {
	bitmap = NULL;
	dwFlags = 0;
	lpBuffer = NULL;
	listFilters = NULL;

	mOutputFrameRate.Assign(0, 0);
	mOutputFrameCount = 0;
}

FilterSystem::~FilterSystem() {
	DeinitFilters();
	DeallocateBuffers();
	delete[] bitmap;
}

// prepareLinearChain(): init bitmaps in a linear filtering system

void FilterSystem::prepareLinearChain(List *listFA, uint32 src_width, uint32 src_height, int src_format, const VDFraction& sourceFrameRate, sint64 sourceFrameCount) {
	FilterInstance *fa;
	uint32 flags, flags_accum=0;
	int last_bufferid = 0;

	if (dwFlags & FILTERS_INITIALIZED)
		return;

	DeallocateBuffers();

	AllocateVBitmaps(3);

	VDPixmapCreateLinearLayout(bitmap[0].mPixmapLayout, src_format, src_width, src_height, 16);

	if (VDPixmapGetInfo(src_format).palsize > 0)
		bitmap[0].mPixmapLayout.palette = mPalette;

	if (src_format == nsVDPixmap::kPixFormat_XRGB8888)
		VDPixmapLayoutFlipV(bitmap[0].mPixmapLayout);

	bitmap[0].offset			= 0;
	bitmap[0].buffer			= 0;
	bitmap[0].mFrameRateHi		= sourceFrameRate.getHi();
	bitmap[0].mFrameRateLo		= sourceFrameRate.getLo();
	bitmap[0].mFrameCount		= sourceFrameCount;
	bitmap[0].ConvertPixmapLayoutToBitmapLayout();

	bmLast = &bitmap[0];

	fa = (FilterInstance *)listFA->tail.next;

	lAdditionalBytes = 0;
	nFrameLag = 0;

	VFBitmapInternal bmTemp;
	VFBitmapInternal bmTemp2;

	while(fa->next) {
		FilterInstance *fa_next = (FilterInstance *)fa->next;

		if (!fa->IsEnabled()) {
			fa = fa_next;
			continue;
		}

		fa->mSrcBuf		= last_bufferid;
		fa->mbBlitOnEntry	= false;
		fa->mbInvalidFormat	= false;
		fa->mbInvalidFormatHandling = false;

		// check if we need to blit
		if (bmLast->mPixmapLayout.format != nsVDPixmap::kPixFormat_XRGB8888 || bmLast->mPixmapLayout.pitch > 0
			|| VDPixmapGetInfo(bmLast->mPixmapLayout.format).palsize) {
			bmTemp = *bmLast;
			VDPixmapCreateLinearLayout(bmTemp.mPixmapLayout, nsVDPixmap::kPixFormat_XRGB8888, bmLast->w, bmLast->h, 4);
			VDPixmapLayoutFlipV(bmTemp.mPixmapLayout);
			bmTemp.ConvertPixmapLayoutToBitmapLayout();

			flags = fa->Prepare(bmTemp);
			fa->mbBlitOnEntry	= true;
		} else {
			flags = fa->Prepare(*bmLast);
		}

		if (flags == FILTERPARAM_NOT_SUPPORTED || (flags & FILTERPARAM_SUPPORTS_ALTFORMATS)) {
			using namespace nsVDPixmap;
			VDASSERTCT(kPixFormat_Max_Standard < 32);
			VDASSERTCT(kPixFormat_Max_Standard == kPixFormat_YUV410_Planar + 1);
			uint32 formatMask	= (1 << kPixFormat_XRGB1555)
								| (1 << kPixFormat_RGB565)
								| (1 << kPixFormat_RGB888)
								| (1 << kPixFormat_XRGB8888)
								| (1 << kPixFormat_Y8)
								| (1 << kPixFormat_YUV422_UYVY)
								| (1 << kPixFormat_YUV422_YUYV)
								| (1 << kPixFormat_YUV444_Planar)
								| (1 << kPixFormat_YUV422_Planar)
								| (1 << kPixFormat_YUV420_Planar)
								| (1 << kPixFormat_YUV411_Planar)
								| (1 << kPixFormat_YUV410_Planar);

			static const int kStaticOrder[]={
				kPixFormat_YUV444_Planar,
				kPixFormat_YUV422_Planar,
				kPixFormat_YUV422_UYVY,
				kPixFormat_YUV422_YUYV,
				kPixFormat_YUV420_Planar
			};

			// test an invalid format and make sure the filter DOESN'T accept it
			bmTemp = *bmLast;
			bmTemp.mPixmapLayout.format = 255;
			flags = fa->Prepare(bmTemp);

			int originalFormat = bmLast->mPixmapLayout.format;
			int format = originalFormat;
			if (flags != FILTERPARAM_NOT_SUPPORTED) {
				formatMask = 0;
				fa->mbInvalidFormatHandling = true;
			} else {
				while(format && formatMask) {
					if (formatMask & (1 << format)) {
						if (format == originalFormat)
							flags = fa->Prepare(*bmLast);
						else {
							bmTemp = *bmLast;
							VDPixmapCreateLinearLayout(bmTemp.mPixmapLayout, format, bmLast->w, bmLast->h, 4);
							if (format == nsVDPixmap::kPixFormat_XRGB8888)
								VDPixmapLayoutFlipV(bmTemp.mPixmapLayout);
							bmTemp.ConvertPixmapLayoutToBitmapLayout();
							flags = fa->Prepare(bmTemp);
						}

						if (flags != FILTERPARAM_NOT_SUPPORTED)
							break;

						formatMask &= ~(1 << format);
					}

					switch(format) {
					case kPixFormat_YUV422_UYVY:
						if (formatMask & (1 << kPixFormat_YUV422_YUYV))
							format = kPixFormat_YUV422_YUYV;
						else
							format = kPixFormat_YUV422_Planar;
						break;

					case kPixFormat_YUV422_YUYV:
						if (formatMask & (1 << kPixFormat_YUV422_UYVY))
							format = kPixFormat_YUV422_UYVY;
						else
							format = kPixFormat_YUV422_Planar;
						break;

					case kPixFormat_Y8:
					case kPixFormat_YUV422_Planar:
						format = kPixFormat_YUV444_Planar;
						break;

					case kPixFormat_YUV420_Planar:
					case kPixFormat_YUV411_Planar:
						format = kPixFormat_YUV422_Planar;
						break;

					case kPixFormat_YUV410_Planar:
						format = kPixFormat_YUV420_Planar;
						break;

					default:
						if (formatMask & (1 << kPixFormat_XRGB8888))
							format = kPixFormat_XRGB8888;
						else
							format = VDFindLowestSetBit(formatMask);
						break;
					}
				}
			}

			fa->mbInvalidFormat = (formatMask == 0);
			fa->mbBlitOnEntry = (format != originalFormat);
		}

		if (fa->mbInvalidFormat) {
			fa->mbBlitOnEntry = false;
			fa->mRealSrc = *bmLast;
			fa->mRealDst = fa->mRealSrc;
			flags = 0;
		}

		if (fa->mbBlitOnEntry) {
			fa->mSrcBuf = (fa->mSrcBuf == 1) ? 2 : 1;
			uint32 srcSizeRequired = bmTemp.size + bmTemp.offset;
			if (bitmap[fa->mSrcBuf].size < srcSizeRequired)
				bitmap[fa->mSrcBuf].size = srcSizeRequired;
			VDASSERT(bitmap[fa->mSrcBuf].size >= 0);

			bmLast = &bmTemp;
		}

		if (flags & FILTERPARAM_NEEDS_LAST)
			lAdditionalBytes += fa->mRealLast.size + fa->mRealLast.offset;

		nFrameLag += (flags>>16);

		flags &= 0x0000ffff;
		flags_accum |= flags;

		fa->mDstBuf		= fa->mSrcBuf;

		int swapBuffer = 1;
		if (fa->mSrcBuf)
			swapBuffer = 3-fa->mSrcBuf;

		if (flags & FILTERPARAM_SWAP_BUFFERS) {
			// Alternate between buffers 1 and 2
			fa->mDstBuf = swapBuffer;
		}

		// allocate destination buffer
		uint32 dstSizeRequired = fa->mRealDst.size + fa->mRealDst.offset;
		if (bitmap[fa->mDstBuf].size < dstSizeRequired)
			bitmap[fa->mDstBuf].size = dstSizeRequired;
		VDASSERT(bitmap[fa->mDstBuf].size >= 0);

		// check if we have a blend curve
		if (fa->GetAlphaParameterCurve()) {
			if (!(flags & FILTERPARAM_SWAP_BUFFERS)) {
				// if this is an in-place filter and we have a blend curve, allocate other buffer as well.
				fa->mBlendBuffer = swapBuffer;
				if (bitmap[swapBuffer].size < dstSizeRequired)
					bitmap[swapBuffer].size = dstSizeRequired;
				VDASSERT(bitmap[swapBuffer].size >= 0);
			}
		}

		bmLast = &fa->mRealDst;
		last_bufferid = fa->mDstBuf;

		// Next filter.

		fa = fa_next;
	}

	// 2/3) Temp buffers
	fa = (FilterInstance *)listFA->tail.next;

	mOutputFrameRate.Assign(bmLast->mFrameRateHi, bmLast->mFrameRateLo);
	mOutputFrameCount = bmLast->mFrameCount;
}

// initLinearChain(): prepare for a linear filtering system

namespace {
	FilterInstance *GetNextEnabledFilterInstance(FilterInstance *inst) {
		FilterInstance *next = (FilterInstance *)inst->next;
		if (!next)
			return NULL;

		inst = next;
		for(;;) {
			next = (FilterInstance *)inst->next;

			if (!next)
				return NULL;

			if (inst->IsEnabled())
				return inst;

			inst = next;
		}
	}
}

void FilterSystem::initLinearChain(List *listFA, uint32 src_width, uint32 src_height, int src_format, const uint32 *palette, const VDFraction& sourceFrameRate, sint64 sourceFrameCount) {
	FilterInstance *fa;
	long lLastBufPtr = 0;
	long lRequiredSize;
	int i;

	DeinitFilters();
	DeallocateBuffers();

	listFilters = listFA;

	// buffers required:
	//
	// 1) Input buffer (8/16/24/32 bits)
	// 2) Temp buffer #1 (32 bits)
	// 3) Temp buffer #2 (32 bits)
	//
	// All temporary buffers must be aligned on an 8-byte boundary, and all
	// pitches must be a multiple of 8 bytes.  The exceptions are the source
	// and destination buffers, which may have pitches that are only 4-byte
	// multiples.

	int palSize = VDPixmapGetInfo(src_format).palsize;
	if (palette && palSize)
		memcpy(mPalette, palette, palSize*sizeof(uint32));

	prepareLinearChain(listFA, src_width, src_height, src_format, sourceFrameRate, sourceFrameCount);

	lRequiredSize = lAdditionalBytes;

	for(i=0; i<iBitmapCount; i++) {
		bitmap[i].buffer		= i;
		bitmap[i].lMapOffset	= lRequiredSize;

		VDASSERT(bitmap[i].size >= 0);
		lRequiredSize		+= bitmap[i].size;

		// align lRequiredSize up to next 16

		lRequiredSize = (lRequiredSize+15) & -16;

	}

	AllocateBuffers(lRequiredSize);

	for(i=0; i<iBitmapCount; i++) {
		FilterSystemBitmap& bm = bitmap[i];
		bm.data		= (Pixel *)(lpBuffer + bm.lMapOffset);

		if (i == 0) {
			bm.mPixmap	= VDPixmapFromLayout(bm.mPixmapLayout, bm.data);
			VDAssertValidPixmap(bm.mPixmap);
		}
	}

	fa = (FilterInstance *)listFA->tail.next;

	while(fa->next) {
		FilterInstance *fa_next = (FilterInstance *)fa->next;

		if (!fa->IsEnabled()) {
			fa = fa_next;
			continue;
		}

		if (fa->mRealDst.w < 1 || fa->mRealDst.h < 1)
			throw MyError("Cannot start filter chain: The output of filter \"%s\" is smaller than 1x1.", fa->filter->name);

		fa->mRealSrcUncropped.Fixup(bitmap[fa->mSrcBuf].data);
		fa->mRealSrc.Fixup(bitmap[fa->mSrcBuf].data);
		fa->mRealDst.Fixup(bitmap[fa->mDstBuf].data);

		if (fa->GetAlphaParameterCurve()) {
				// size check
			if (fa->src.w != fa->dst.w || fa->src.h != fa->dst.h) {
				throw MyError("Cannot start filter chain: Filter \"%s\" has a blend curve attached and has differing input and output sizes (%dx%d -> %dx%d). Input and output sizes must match."
					, fa->filter->name
					, fa->src.w
					, fa->src.h
					, fa->dst.w
					, fa->dst.h
					);
			}

			if ((fa->GetFlags() & FILTERPARAM_SWAP_BUFFERS) && fa->mRealSrc.mPixmap.format == fa->mRealDst.mPixmap.format) {
				fa->mBlendPixmap = fa->mRealSrc.mPixmap;
			} else {
				VFBitmapInternal blend(fa->mRealDst);
				blend.Fixup(bitmap[fa->mBlendBuffer].data);
				fa->mBlendPixmap = blend.mPixmap;
			}
		}

		fa = (FilterInstance *)fa->next;
	}

	// Does the first filter require a display context?
	fa = (FilterInstance *)listFA->tail.next;

	while(fa->next) {
		FilterInstance *fa_next = GetNextEnabledFilterInstance(fa);

		VDFileMappingW32 *mapping = NULL;
		if (((fa->mRealSrc.dwFlags | fa->mRealDst.dwFlags) & VFBitmapInternal::NEEDS_HDC) && !(fa->GetFlags() & FILTERPARAM_SWAP_BUFFERS)) {
			uint32 mapSize = std::max<uint32>(VDPixmapLayoutGetMinSize(fa->mRealSrc.mPixmapLayout), VDPixmapLayoutGetMinSize(fa->mRealDst.mPixmapLayout));

			if (!fa->mFileMapping.Init(mapSize))
				throw MyMemoryError();

			mapping = &fa->mFileMapping;
		}

		fa->mRealSrc.hdc = NULL;
		fa->mExternalSrc = fa->mRealSrc.mPixmap;

		if (mapping || (fa->mRealSrc.dwFlags & VDXFBitmap::NEEDS_HDC)) {
			fa->mRealSrc.mDIBSection.Init(VDAbsPtrdiff(fa->mRealSrc.pitch) >> 2, fa->mRealSrc.h, 32, mapping, fa->mRealSrc.offset);
			fa->mRealSrc.hdc = fa->mRealSrc.mDIBSection.GetHDC();

			fa->mRealSrc.mPixmap = fa->mRealSrc.mDIBSection.GetPixmap();
			fa->mRealSrc.mPixmap.w = fa->mRealSrc.w;
			fa->mRealSrc.mPixmap.h = fa->mRealSrc.h;
			fa->mRealSrc.ConvertPixmapToBitmap();
		}

		fa->mRealDst.hdc = NULL;
		fa->mExternalDst = fa->mRealDst.mPixmap;
		if (mapping || (fa->mRealDst.dwFlags & VDXFBitmap::NEEDS_HDC)) {
			fa->mRealDst.mDIBSection.Init(VDAbsPtrdiff(fa->mRealDst.pitch) >> 2, fa->mRealDst.h, 32, mapping, fa->mRealDst.offset);
			fa->mRealDst.hdc = fa->mRealDst.mDIBSection.GetHDC();

			fa->mRealDst.mPixmap = fa->mRealDst.mDIBSection.GetPixmap();
			fa->mRealDst.mPixmap.w = fa->mRealDst.w;
			fa->mRealDst.mPixmap.h = fa->mRealDst.h;
			fa->mRealDst.ConvertPixmapToBitmap();
		}

		fa->mRealLast.hdc = NULL;
		if (fa->GetFlags() & FILTERPARAM_NEEDS_LAST) {
			fa->mRealLast.Fixup(lpBuffer + lLastBufPtr);

			if (fa->mRealLast.dwFlags & VDXFBitmap::NEEDS_HDC) {
				fa->mRealLast.mDIBSection.Init(VDAbsPtrdiff(fa->mRealLast.pitch) >> 2, fa->mRealLast.h, 32);
				fa->mRealLast.hdc = fa->mRealLast.mDIBSection.GetHDC();
				fa->mRealLast.mPixmap = fa->mRealLast.mDIBSection.GetPixmap();
			}

			lLastBufPtr += fa->last->size + fa->last->offset;
		}

		fa = fa_next;
		if (!fa)
			break;
	}
}

void FilterSystem::ReadyFilters() {
	FilterInstance *fa = (FilterInstance *)listFilters->tail.next;

	if (dwFlags & FILTERS_INITIALIZED)
		return;

	dwFlags &= ~FILTERS_ERROR;

	int accumulatedDelay = 0;

	try {
		while(fa->next) {
			if (fa->IsEnabled()) {
				if (fa->mbInvalidFormatHandling)
					throw MyError("Cannot start filters: Filter \"%s\" is not handling image formats correctly.",
						fa->filter->name);

				if (fa->mbInvalidFormat)
					throw MyError("Cannot start filters: An instance of filter \"%s\" cannot process its input of %ux%d (%s).",
						fa->filter->name,
						fa->mRealSrc.mpPixmapLayout->w,
						fa->mRealSrc.mpPixmapLayout->h,
						VDPixmapGetInfo(fa->mRealSrc.mpPixmapLayout->format).name);

				int nDelay = fa->GetFrameDelay();

				accumulatedDelay += nDelay;

				fa->Start(accumulatedDelay);
			}

			fa = (FilterInstance *)fa->next;
		}
	} catch(const MyError&) {
		// roll back previously initialized filters (similar to deinit)
		while(fa->prev) {
			fa->Stop();

			fa = (FilterInstance *)fa->prev;
		}

		delete[] bitmap;
		bitmap = NULL;

		throw;
	}

	dwFlags |= FILTERS_INITIALIZED;

	RestartFilters();
}

void FilterSystem::RestartFilters() {
	mbFirstFrame = true;
	mFrameDelayLeft = nFrameLag;
}

bool FilterSystem::RunFilters(sint64 outputFrame, sint64 timelineFrame, sint64 sequenceFrame, sint64 sequenceTimeMS, FilterInstance *pfiStopPoint, uint32 flags) {
	if (listFilters->IsEmpty())
		return true;

	if (dwFlags & FILTERS_ERROR)
		return false;

	if (!(dwFlags & FILTERS_INITIALIZED))
		return false;

	// begin iteration from the end and build a stack of request objects
	FilterInstance *fa = static_cast<FilterInstance *>(listFilters->head.prev);

	RequestInfo ri;
	ri.mTimelineFrame = timelineFrame;
	ri.mOutputFrame = outputFrame;

	mRequestStack.clear();
	for(; fa->prev; fa = static_cast<FilterInstance *>(fa->prev)) {
		if (!fa->IsEnabled())
			continue;

		ri.mSourceFrame = fa->GetSourceFrame(ri.mOutputFrame);

		mRequestStack.push_back(ri);

		ri.mOutputFrame = ri.mSourceFrame;
	}

	// begin filter execution
	fa = static_cast<FilterInstance *>(listFilters->tail.next);

	const VFBitmapInternal *last = &bitmap[0];

	while(fa->next) {
		// Run the filter.

		if (fa->IsEnabled()) {
			try {
				const RequestInfo& ri = mRequestStack.back();

				// We need to make sure that the entry blit happens on the stop filter for filter preview
				// and cropping to work.
				if (fa->mbBlitOnEntry) {
					VDPixmapBlt(fa->mRealSrcUncropped.mPixmap, last->mPixmap);
				}

				if (fa == pfiStopPoint)
					break;

				fa->Run(mbFirstFrame, ri.mSourceFrame, ri.mOutputFrame, ri.mTimelineFrame, sequenceFrame, sequenceTimeMS, flags);

				last = &fa->mRealDst;
				mRequestStack.pop_back();
			} catch(const MyError&) {
				dwFlags |= FILTERS_ERROR;
				throw;
			}
		} else {
			if (fa == pfiStopPoint)
				break;
		}

		fa = (FilterInstance *)fa->next;
	}

	mbFirstFrame = false;

	if (mFrameDelayLeft) {
		--mFrameDelayLeft;
		return false;
	}

	return true;
}

void FilterSystem::DeinitFilters() {
	if (!listFilters)
		return;

	FilterInstance *fa = (FilterInstance *)listFilters->tail.next;

	if (!(dwFlags & FILTERS_INITIALIZED))
		return;

	// send all filters a 'stop'

	while(fa->next) {
		fa->Stop();

		fa = (FilterInstance *)fa->next;
	}

	dwFlags &= ~FILTERS_INITIALIZED;

	delete[] bitmap;
	bitmap = NULL;
}

const VDPixmap& FilterSystem::GetInput() const {
	return bitmap[0].mPixmap;
}

const VDPixmapLayout& FilterSystem::GetInputLayout() const {
	return bitmap[0].mPixmapLayout;
}

const VDPixmap& FilterSystem::GetOutput() const {
	return bmLast->mPixmap;
}

const VDPixmapLayout& FilterSystem::GetOutputLayout() const {
	return bmLast->mPixmapLayout;
}

bool FilterSystem::isRunning() {
	return !!(dwFlags & FILTERS_INITIALIZED);
}

int FilterSystem::getFrameLag() {
	return nFrameLag;
}

bool FilterSystem::IsFiltered(VDPosition frame) const {
	if (listFilters->IsEmpty())
		return false;

	if (dwFlags & FILTERS_ERROR)
		return false;

	FilterInstance *fa = (FilterInstance *)listFilters->tail.next;

	if (!(dwFlags & FILTERS_INITIALIZED))
		return false;

	while(fa->next) {
		if (fa->IsEnabled()) {
			VDParameterCurve *pAlphaCurve = fa->GetAlphaParameterCurve();
			if (!pAlphaCurve)
				return true;

			float alpha = (float)(*pAlphaCurve)((double)frame).mY;

			if (alpha >= (0.5f / 255.0f))
				return true;
		}

		fa = (FilterInstance *)fa->next;
	}

	return false;
}

sint64 FilterSystem::GetSourceFrame(sint64 frame) const {
	if (listFilters->IsEmpty())
		return frame;

	if (dwFlags & FILTERS_ERROR)
		return frame;

	FilterInstance *fa = (FilterInstance *)listFilters->tail.next;

	if (!(dwFlags & FILTERS_INITIALIZED))
		return frame;

	while(fa->next) {
		if (fa->IsEnabled()) {
			frame = fa->GetSourceFrame(frame);
		}

		fa = (FilterInstance *)fa->next;
	}

	return frame;
}

const VDFraction FilterSystem::GetOutputFrameRate() const {
	return mOutputFrameRate;
}

sint64 FilterSystem::GetOutputFrameCount() const {
	return mOutputFrameCount;
}

/////////////////////////////////////////////////////////////////////////////
//
//	FilterSystem::private_methods
//
/////////////////////////////////////////////////////////////////////////////

void FilterSystem::AllocateVBitmaps(int count) {
	delete[] bitmap;

	if (!(bitmap = new FilterSystemBitmap[count])) throw MyMemoryError();
//	memset(bitmap, 0, sizeof(VBitmap) * count);

	for(int i=0; i<count; i++) {
		bitmap[i].data		= NULL;
		bitmap[i].palette	= NULL;
		bitmap[i].buffer	= 0;
		bitmap[i].depth		= 0;
		bitmap[i].w			= 0;
		bitmap[i].h			= 0;
		bitmap[i].pitch		= 0;
		bitmap[i].modulo	= 0;
		bitmap[i].size		= 0;
		bitmap[i].offset	= 0;
	}

	iBitmapCount = count;
}

void FilterSystem::AllocateBuffers(uint32 lTotalBufferNeeded) {
	DeallocateBuffers();

	if (!(lpBuffer = (unsigned char *)VirtualAlloc(NULL, lTotalBufferNeeded+8, MEM_COMMIT, PAGE_READWRITE)))
		throw MyMemoryError();

	memset(lpBuffer, 0, lTotalBufferNeeded+8);
}

void FilterSystem::DeallocateBuffers() {
	if (listFilters) {
		// delete hdcs

		FilterInstance *fa = (FilterInstance *)listFilters->tail.next;

		while(fa->next) {
			fa->mRealSrc.mDIBSection.Shutdown();
			fa->mRealDst.mDIBSection.Shutdown();
			fa->mRealLast.mDIBSection.Shutdown();

			fa = (FilterInstance *)fa->next;
		}
	}

	if (lpBuffer) {
		VirtualFree(lpBuffer, 0, MEM_RELEASE);

		lpBuffer = NULL;
	}
}
