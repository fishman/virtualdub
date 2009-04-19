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

#include "FilterInstance.h"
#include "FilterSystem.h"
#include "FilterFrame.h"
#include "FilterFrameAllocator.h"
#include "FilterFrameRequest.h"

extern FilterFunctions g_filterFuncs;

///////////////////////////////////////////////////////////////////////////

struct FilterSystem::Bitmaps {
	VFBitmapInternal		mInitialBitmap;
	vdrefptr<VDFilterFrameAllocator> mpAllocators[3];
	vdrefptr<VDFilterFrameBuffer> mpBuffers[3];
	vdrefptr<IVDFilterFrameClientRequest> mpLastRequest;
	vdrefptr<IVDFilterFrameSource> mpSource;
	IVDFilterFrameSource *mpTailSource;
};

///////////////////////////////////////////////////////////////////////////

FilterSystem::FilterSystem()
	: dwFlags(0)
	, mOutputFrameRate(0, 0)
	, mOutputFrameCount(0)
	, mpBitmaps(NULL)
	, bmLast(NULL)
	, listFilters(NULL)
	, nFrameLag(0)
	, lpBuffer(NULL)
	, lAdditionalBytes(0)
{
}

FilterSystem::~FilterSystem() {
	DeinitFilters();
	DeallocateBuffers();

	if (mpBitmaps)
		delete mpBitmaps;
}

// prepareLinearChain(): init bitmaps in a linear filtering system

void FilterSystem::prepareLinearChain(List *listFA, uint32 src_width, uint32 src_height, int src_format, const VDFraction& sourceFrameRate, sint64 sourceFrameCount, const VDFraction& sourcePixelAspect) {
	FilterInstance *fa;
	uint32 flags, flags_accum=0;
	int last_bufferid = 0;

	if (dwFlags & FILTERS_INITIALIZED)
		return;

	DeallocateBuffers();

	if (!mpBitmaps)
		mpBitmaps = new Bitmaps;

	for(int i=0; i<3; ++i) {
		mpBitmaps->mpAllocators[i] = new VDFilterFrameAllocator;
	}

	VDPixmapCreateLinearLayout(mpBitmaps->mInitialBitmap.mPixmapLayout, src_format, src_width, src_height, 16);

	if (VDPixmapGetInfo(src_format).palsize > 0)
		mpBitmaps->mInitialBitmap.mPixmapLayout.palette = mPalette;

	if (src_format == nsVDPixmap::kPixFormat_XRGB8888)
		VDPixmapLayoutFlipV(mpBitmaps->mInitialBitmap.mPixmapLayout);

	mpBitmaps->mInitialBitmap.mFrameRateHi		= sourceFrameRate.getHi();
	mpBitmaps->mInitialBitmap.mFrameRateLo		= sourceFrameRate.getLo();
	mpBitmaps->mInitialBitmap.mFrameCount		= sourceFrameCount;
	mpBitmaps->mInitialBitmap.ConvertPixmapLayoutToBitmapLayout();
	mpBitmaps->mInitialBitmap.mAspectRatioHi	= sourcePixelAspect.getHi();
	mpBitmaps->mInitialBitmap.mAspectRatioLo	= sourcePixelAspect.getLo();

	mpBitmaps->mpAllocators[0]->AddSizeRequirement(VDPixmapLayoutGetMinSize(mpBitmaps->mInitialBitmap.mPixmapLayout));

	bmLast = &mpBitmaps->mInitialBitmap;

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
		fa->mbConvertOnEntry = false;
		fa->mbAlignOnEntry	= false;

		// check if we need to blit
		if (bmLast->mPixmapLayout.format != nsVDPixmap::kPixFormat_XRGB8888 || bmLast->mPixmapLayout.pitch > 0
			|| VDPixmapGetInfo(bmLast->mPixmapLayout.format).palsize) {
			bmTemp = *bmLast;
			VDPixmapCreateLinearLayout(bmTemp.mPixmapLayout, nsVDPixmap::kPixFormat_XRGB8888, bmLast->w, bmLast->h, 16);
			VDPixmapLayoutFlipV(bmTemp.mPixmapLayout);
			bmTemp.ConvertPixmapLayoutToBitmapLayout();

			flags = fa->Prepare(bmTemp);
			fa->mbConvertOnEntry = true;
		} else {
			flags = fa->Prepare(*bmLast);
		}

		if (flags == FILTERPARAM_NOT_SUPPORTED || (flags & FILTERPARAM_SUPPORTS_ALTFORMATS)) {
			using namespace nsVDPixmap;
			VDASSERTCT(kPixFormat_Max_Standard < 32);
			VDASSERTCT(kPixFormat_Max_Standard == kPixFormat_YUV420_NV12 + 1);
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
				kPixFormat_YUV420_Planar,
				kPixFormat_YUV411_Planar,
				kPixFormat_YUV410_Planar,
				kPixFormat_XRGB8888
			};

			int staticOrderIndex = 0;

			// test an invalid format and make sure the filter DOESN'T accept it
			bmTemp = *bmLast;
			bmTemp.mPixmapLayout.format = 255;
			flags = fa->Prepare(bmTemp);

			int originalFormat = bmLast->mPixmapLayout.format;
			int format = originalFormat;
			if (flags != FILTERPARAM_NOT_SUPPORTED) {
				formatMask = 0;
				VDASSERT(fa->GetInvalidFormatHandlingState());
			} else {
				while(format && formatMask) {
					if (formatMask & (1 << format)) {
						if (format == originalFormat)
							flags = fa->Prepare(*bmLast);
						else {
							bmTemp = *bmLast;
							VDPixmapCreateLinearLayout(bmTemp.mPixmapLayout, format, bmLast->w, bmLast->h, 16);
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

					case kPixFormat_YUV422_V210:
						format = kPixFormat_YUV422_Planar;
						break;

					case kPixFormat_YUV422_UYVY_709:
						format = kPixFormat_YUV422_UYVY;
						break;

					case kPixFormat_XRGB1555:
					case kPixFormat_RGB565:
					case kPixFormat_RGB888:
						if (formatMask & (1 << kPixFormat_XRGB8888)) {
							format = kPixFormat_XRGB8888;
							break;
						}

						// fall through

					default:
						if (staticOrderIndex < sizeof(kStaticOrder)/sizeof(kStaticOrder[0]))
							format = kStaticOrder[staticOrderIndex++];
						else if (formatMask & (1 << kPixFormat_XRGB8888))
							format = kPixFormat_XRGB8888;
						else
							format = VDFindLowestSetBit(formatMask);
						break;
					}
				}
			}

			fa->mbConvertOnEntry = (format != originalFormat);
		}

		if (fa->GetInvalidFormatState()) {
			fa->mbConvertOnEntry = false;
			fa->mRealSrc = *bmLast;
			fa->mRealDst = fa->mRealSrc;
			flags = 0;
		} else if (flags & FILTERPARAM_ALIGN_SCANLINES) {
			const VDPixmapLayout& pxlsrc = fa->mRealSrc.mPixmapLayout;
			int bufcnt = VDPixmapGetInfo(pxlsrc.format).auxbufs;

			switch(bufcnt) {
			case 2:
				if ((pxlsrc.data3 | pxlsrc.pitch3) & 15)
					fa->mbAlignOnEntry = true;
				break;
			case 1:
				if ((pxlsrc.data2 | pxlsrc.pitch2) & 15)
					fa->mbAlignOnEntry = true;
				break;
			case 0:
				if ((pxlsrc.data | pxlsrc.pitch) & 15)
					fa->mbAlignOnEntry = true;
				break;
			}
		}

		fa->mbBlitOnEntry = fa->mbConvertOnEntry || fa->mbAlignOnEntry;

		// If we are blitting on entry, we need a new allocator for the source.
		if (fa->mbBlitOnEntry) {
			fa->mSrcBuf = (fa->mSrcBuf == 1) ? 2 : 1;
			uint32 srcSizeRequired = bmTemp.size + bmTemp.offset;

			mpBitmaps->mpAllocators[fa->mSrcBuf]->AddSizeRequirement(srcSizeRequired);

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
		mpBitmaps->mpAllocators[fa->mDstBuf]->AddSizeRequirement(dstSizeRequired);

		// check if we have a blend curve
		if (fa->GetAlphaParameterCurve()) {
			if (!(flags & FILTERPARAM_SWAP_BUFFERS)) {
				// if this is an in-place filter and we have a blend curve, allocate other buffer as well.
				fa->mBlendBuffer = swapBuffer;
				mpBitmaps->mpAllocators[swapBuffer]->AddSizeRequirement(dstSizeRequired);
			}
		}

		bmLast = &fa->mRealDst;
		last_bufferid = fa->mDstBuf;

		// Next filter.

		fa = fa_next;
	}

	// 2/3) Temp buffers
	fa = (FilterInstance *)listFA->tail.next;

	mOutputPixelAspect.Assign(bmLast->mAspectRatioHi, bmLast->mAspectRatioLo);
	mOutputFrameRate.Assign(bmLast->mFrameRateHi, bmLast->mFrameRateLo);
	mOutputFrameCount = bmLast->mFrameCount;
}

// initLinearChain(): prepare for a linear filtering system
void FilterSystem::initLinearChain(List *listFA, IVDFilterFrameSource *src, uint32 src_width, uint32 src_height, int src_format, const uint32 *palette, const VDFraction& sourceFrameRate, sint64 sourceFrameCount, const VDFraction& sourcePixelAspect) {
	FilterInstance *fa;
	long lLastBufPtr = 0;
	long lRequiredSize;

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

	prepareLinearChain(listFA, src_width, src_height, src_format, sourceFrameRate, sourceFrameCount, sourcePixelAspect);

	lRequiredSize = lAdditionalBytes;

	AllocateBuffers(lRequiredSize);

	mpBitmaps->mpSource = src;
	mpBitmaps->mpSource->SetAllocator(mpBitmaps->mpAllocators[0]);

	mpBitmaps->mInitialBitmap.mPixmap = VDPixmapFromLayout(mpBitmaps->mInitialBitmap.mPixmapLayout, mpBitmaps->mpBuffers[0]->GetBasePointer());
	VDAssertValidPixmap(mpBitmaps->mInitialBitmap.mPixmap);

	fa = (FilterInstance *)listFA->tail.next;

	const VFBitmapInternal *rawSrc = &mpBitmaps->mInitialBitmap;

	while(fa->next) {
		FilterInstance *fa_next = (FilterInstance *)fa->next;

		if (!fa->IsEnabled()) {
			fa = fa_next;
			continue;
		}

		if (fa->GetInvalidFormatHandlingState())
			throw MyError("Cannot start filters: Filter \"%s\" is not handling image formats correctly.",
				fa->GetName());

		if (fa->mRealDst.w < 1 || fa->mRealDst.h < 1)
			throw MyError("Cannot start filter chain: The output of filter \"%s\" is smaller than 1x1.", fa->GetName());

		fa->mRealSrcUncropped.Fixup(mpBitmaps->mpBuffers[fa->mSrcBuf]->GetBasePointer());
		fa->mRealSrc.Fixup(mpBitmaps->mpBuffers[fa->mSrcBuf]->GetBasePointer());
		fa->mRealDst.Fixup(mpBitmaps->mpBuffers[fa->mDstBuf]->GetBasePointer());

		fa->mExternalSrc = *rawSrc;
		fa->mExternalDst = fa->mRealDst;

		if (fa->GetAlphaParameterCurve()) {
				// size check
			if (fa->mRealSrc.w != fa->mRealDst.w || fa->mRealSrc.h != fa->mRealDst.h) {
				throw MyError("Cannot start filter chain: Filter \"%s\" has a blend curve attached and has differing input and output sizes (%dx%d -> %dx%d). Input and output sizes must match."
					, fa->GetName()
					, fa->mRealSrc.w
					, fa->mRealSrc.h
					, fa->mRealDst.w
					, fa->mRealDst.h
					);
			}

			if ((fa->GetFlags() & FILTERPARAM_SWAP_BUFFERS) && fa->mRealSrc.mPixmap.format == fa->mRealDst.mPixmap.format) {
				fa->mBlendPixmap = fa->mRealSrc.mPixmap;
			} else {
				VFBitmapInternal blend(fa->mRealDst);
				blend.Fixup(mpBitmaps->mpBuffers[fa->mBlendBuffer]->GetBasePointer());
				fa->mBlendPixmap = blend.mPixmap;
			}
		}

		VDFileMappingW32 *mapping = NULL;
		if (((fa->mRealSrc.dwFlags | fa->mRealDst.dwFlags) & VFBitmapInternal::NEEDS_HDC) && !(fa->GetFlags() & FILTERPARAM_SWAP_BUFFERS)) {
			uint32 mapSize = std::max<uint32>(VDPixmapLayoutGetMinSize(fa->mRealSrc.mPixmapLayout), VDPixmapLayoutGetMinSize(fa->mRealDst.mPixmapLayout));

			if (!fa->mFileMapping.Init(mapSize))
				throw MyMemoryError();

			mapping = &fa->mFileMapping;
		}

		fa->mRealSrc.hdc = NULL;
		if (mapping || (fa->mRealSrc.dwFlags & VDXFBitmap::NEEDS_HDC))
			fa->mRealSrc.BindToDIBSection(mapping);

		fa->mRealDst.hdc = NULL;
		if (mapping || (fa->mRealDst.dwFlags & VDXFBitmap::NEEDS_HDC))
			fa->mRealDst.BindToDIBSection(mapping);

		fa->mRealLast.hdc = NULL;
		if (fa->GetFlags() & FILTERPARAM_NEEDS_LAST) {
			fa->mRealLast.Fixup(lpBuffer + lLastBufPtr);

			if (fa->mRealLast.dwFlags & VDXFBitmap::NEEDS_HDC)
				fa->mRealLast.BindToDIBSection(NULL);

			lLastBufPtr += fa->mRealLast.size + fa->mRealLast.offset;
		}

		rawSrc = &fa->mExternalDst;

		fa = fa_next;
	}
}

void FilterSystem::ReadyFilters(uint32 flags) {
	FilterInstance *fa = static_cast<FilterInstance *>(listFilters->tail.next);

	if (dwFlags & FILTERS_INITIALIZED)
		return;

	dwFlags &= ~FILTERS_ERROR;

	int accumulatedDelay = 0;

	IVDFilterFrameSource *pLastSource = mpBitmaps->mpSource;

	try {
		while(fa->next) {
			if (fa->IsEnabled()) {
				if (fa->GetInvalidFormatHandlingState())
					throw MyError("Cannot start filters: Filter \"%s\" is not handling image formats correctly.",
						fa->GetName());

				if (fa->GetInvalidFormatState())
					throw MyError("Cannot start filters: An instance of filter \"%s\" cannot process its input of %ux%d (%s).",
						fa->GetName(),
						fa->mRealSrc.mpPixmapLayout->w,
						fa->mRealSrc.mpPixmapLayout->h,
						VDPixmapGetInfo(fa->mRealSrc.mpPixmapLayout->format).name);

				int nDelay = fa->GetFrameDelay();

				accumulatedDelay += nDelay;

				fa->SetAllocator(mpBitmaps->mpAllocators[fa->mDstBuf]);
				fa->Start(accumulatedDelay, flags, pLastSource, mpBitmaps->mpAllocators[fa->mSrcBuf]);
				pLastSource = fa;
			}

			fa = (FilterInstance *)fa->next;
		}
	} catch(const MyError&) {
		// roll back previously initialized filters (similar to deinit)
		while(fa->prev) {
			fa->Stop();

			fa = (FilterInstance *)fa->prev;
		}

		throw;
	}

	dwFlags |= FILTERS_INITIALIZED;
	mpBitmaps->mpTailSource = pLastSource;
}

bool FilterSystem::RequestFrame(sint64 outputFrame, IVDFilterFrameClientRequest **creq) {
	if (!mpBitmaps->mpTailSource)
		return false;

	if (!mpBitmaps->mpTailSource->CreateRequest(outputFrame, false, ~mpBitmaps->mpLastRequest))
		return false;

	if (creq)
		*creq = mpBitmaps->mpLastRequest.release();

	return true;
}

void FilterSystem::AbortFrame() {
	mpBitmaps->mpLastRequest = NULL;
}

bool FilterSystem::RunToCompletion() {
	if (dwFlags & FILTERS_ERROR)
		return false;

	if (!(dwFlags & FILTERS_INITIALIZED))
		return false;

	bool activity = false;
	for(;;) {
		bool didSomething = false;

		FilterInstance *fa = static_cast<FilterInstance *>(listFilters->AtTail());
		while(fa->next) {
			// Run the filter.

			if (fa->IsEnabled()) {
				try {
					while(fa->RunRequests())
						didSomething = true;
				} catch(const MyError&) {
					dwFlags |= FILTERS_ERROR;
					throw;
				}
			}

			fa = (FilterInstance *)fa->next;
		}

		if (!didSomething)
			break;

		activity = true;
	}

	if (!mpBitmaps->mpLastRequest)
		return activity;

	if (mpBitmaps->mpLastRequest->IsCompleted())
		mpBitmaps->mpLastRequest = NULL;

	return activity;
}

void FilterSystem::InvalidateCachedFrames(FilterInstance *startingFilter) {
	FilterInstance *fi = startingFilter;

	if (!fi)
		fi = static_cast<FilterInstance *>(listFilters->AtTail());

	while(fi->next) {
		fi->InvalidateAllCachedFrames();

		fi = (FilterInstance *)fi->next;
	}
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

	mpBitmaps->mpTailSource = NULL;
	dwFlags &= ~FILTERS_INITIALIZED;
}

const VDPixmapLayout& FilterSystem::GetInputLayout() const {
	return mpBitmaps->mInitialBitmap.mPixmapLayout;
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

bool FilterSystem::GetDirectFrameMapping(VDPosition outputFrame, VDPosition& sourceFrame, int& sourceIndex) const {
	if (listFilters->IsEmpty())
		return false;

	if (dwFlags & FILTERS_ERROR)
		return false;

	if (!(dwFlags & FILTERS_INITIALIZED))
		return false;

	return mpBitmaps->mpTailSource->GetDirectMapping(outputFrame, sourceFrame, sourceIndex);
}

sint64 FilterSystem::GetSourceFrame(sint64 frame) const {
	if (!(dwFlags & FILTERS_INITIALIZED))
		return frame;

	if (listFilters->IsEmpty())
		return frame;

	if (dwFlags & FILTERS_ERROR)
		return frame;

	FilterInstance *fa = (FilterInstance *)listFilters->tail.next;

	while(fa->next) {
		if (fa->IsEnabled()) {
			frame = fa->GetSourceFrame(frame);
		}

		fa = (FilterInstance *)fa->next;
	}

	return frame;
}

sint64 FilterSystem::GetSymbolicFrame(sint64 outframe, IVDFilterFrameSource *source) const {
	if (!(dwFlags & FILTERS_INITIALIZED))
		return outframe;

	if (listFilters->IsEmpty())
		return outframe;

	if (dwFlags & FILTERS_ERROR)
		return outframe;

	return mpBitmaps->mpTailSource->GetSymbolicFrame(outframe, source);
}

sint64 FilterSystem::GetNearestUniqueFrame(sint64 outframe) const {
	if (!(dwFlags & FILTERS_INITIALIZED))
		return outframe;

	return mpBitmaps->mpTailSource->GetNearestUniqueFrame(outframe);
}

const VDFraction FilterSystem::GetOutputFrameRate() const {
	return mOutputFrameRate;
}

const VDFraction FilterSystem::GetOutputPixelAspect() const {
	return mOutputPixelAspect;
}

sint64 FilterSystem::GetOutputFrameCount() const {
	return mOutputFrameCount;
}

/////////////////////////////////////////////////////////////////////////////
//
//	FilterSystem::private_methods
//
/////////////////////////////////////////////////////////////////////////////

void FilterSystem::AllocateBuffers(uint32 lTotalBufferNeeded) {
	DeallocateBuffers();

	if (!(lpBuffer = (unsigned char *)VirtualAlloc(NULL, lTotalBufferNeeded+8, MEM_COMMIT, PAGE_READWRITE)))
		throw MyMemoryError();

	memset(lpBuffer, 0, lTotalBufferNeeded+8);

	for(int i=0; i<3; ++i) {
		mpBitmaps->mpAllocators[i]->Init(0, 0x7fffffff);

		mpBitmaps->mpBuffers[i] = new VDFilterFrameBuffer;
		mpBitmaps->mpBuffers[i]->Init(mpBitmaps->mpAllocators[i]->GetFrameSize());
	}
}

void FilterSystem::DeallocateBuffers() {
	if (mpBitmaps) {
		for(int i=0; i<3; ++i) {
			mpBitmaps->mpBuffers[i] = NULL;
		}

		mpBitmaps->mpSource = NULL;
	}

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
