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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#include "stdafx.h"
#include <vd2/system/debug.h>
#include <vd2/system/int128.h>
#include <vd2/system/protscope.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "ScriptInterpreter.h"
#include "FilterFrame.h"
#include "FilterFrameRequest.h"
#include "FilterInstance.h"
#include "filters.h"

extern FilterFunctions g_VDFilterCallbacks;

/////////////////////////////////////

namespace {
	void StructCheck() {
		VDASSERTCT(sizeof(VDPixmap) == sizeof(VDXPixmap));
		VDASSERTCT(sizeof(VDPixmapLayout) == sizeof(VDXPixmapLayout));
				
		VDASSERTCT(offsetof(VDPixmap, data) == offsetof(VDXPixmap, data));
		VDASSERTCT(offsetof(VDPixmap, pitch) == offsetof(VDXPixmap, pitch));
		VDASSERTCT(offsetof(VDPixmap, format) == offsetof(VDXPixmap, format));
		VDASSERTCT(offsetof(VDPixmap, w) == offsetof(VDXPixmap, w));
		VDASSERTCT(offsetof(VDPixmap, h) == offsetof(VDXPixmap, h));
				
		VDASSERTCT(offsetof(VDPixmapLayout, data) == offsetof(VDXPixmapLayout, data));
		VDASSERTCT(offsetof(VDPixmapLayout, pitch) == offsetof(VDXPixmapLayout, pitch));
		VDASSERTCT(offsetof(VDPixmapLayout, format) == offsetof(VDXPixmapLayout, format));
		VDASSERTCT(offsetof(VDPixmapLayout, w) == offsetof(VDXPixmapLayout, w));
		VDASSERTCT(offsetof(VDPixmapLayout, h) == offsetof(VDXPixmapLayout, h));
	}
}

///////////////////////////////////////////////////////////////////////////

VFBitmapInternal::VFBitmapInternal()
	: VBitmap()
	, dwFlags(0)
	, hdc(NULL)
	, mFrameRateHi(1)
	, mFrameRateLo(0)
	, mFrameCount(0)
	, mpPixmap(reinterpret_cast<VDXPixmap *>(&mPixmap))
	, mpPixmapLayout(reinterpret_cast<VDXPixmapLayout *>(&mPixmapLayout))
	, mpBuffer(NULL)
{
	memset(&mPixmap, 0, sizeof mPixmap);
	memset(&mPixmapLayout, 0, sizeof mPixmapLayout);
}

VFBitmapInternal::VFBitmapInternal(const VFBitmapInternal& src)
	: VBitmap(static_cast<const VBitmap&>(src))
	, dwFlags(src.dwFlags)
	, hdc(src.hdc)
	, mFrameRateHi(src.mFrameRateHi)
	, mFrameRateLo(src.mFrameRateLo)
	, mFrameCount(src.mFrameCount)
	, mpPixmap(reinterpret_cast<VDXPixmap *>(&mPixmap))
	, mpPixmapLayout(reinterpret_cast<VDXPixmapLayout *>(&mPixmapLayout))
	, mPixmap(src.mPixmap)
	, mPixmapLayout(src.mPixmapLayout)
	, mpBuffer(src.mpBuffer)
{
	if (mpBuffer)
		mpBuffer->AddRef();
}

VFBitmapInternal::~VFBitmapInternal()
{
	Unbind();
}

VFBitmapInternal& VFBitmapInternal::operator=(const VFBitmapInternal& src) {
	if (this != &src) {
		static_cast<VBitmap&>(*this) = static_cast<const VBitmap&>(src);
		dwFlags			= src.dwFlags;
		hdc				= NULL;
		mFrameRateHi	= src.mFrameRateHi;
		mFrameRateLo	= src.mFrameRateLo;
		mFrameCount		= src.mFrameCount;
		mPixmap			= src.mPixmap;
		mPixmapLayout	= src.mPixmapLayout;
		mAspectRatioHi	= src.mAspectRatioHi;
		mAspectRatioLo	= src.mAspectRatioLo;
		mFrameNumber	= src.mFrameNumber;
		mFrameTimestampStart	= src.mFrameTimestampStart;
		mFrameTimestampEnd		= src.mFrameTimestampEnd;
		mCookie			= src.mCookie;

		mDIBSection.Shutdown();

		if (mpBuffer)
			mpBuffer->Release();

		mpBuffer = src.mpBuffer;

		if (mpBuffer)
			mpBuffer->AddRef();
	}

	return *this;
}

void VFBitmapInternal::Unbind() {
	data = NULL;
	mPixmap.data = NULL;
	mPixmap.data2 = NULL;
	mPixmap.data3 = NULL;

	if (mpBuffer) {
		mpBuffer->Release();
		mpBuffer = NULL;
	}
}

void VFBitmapInternal::Fixup(void *base) {
	mPixmap = VDPixmapFromLayout(mPixmapLayout, base);
	data = (uint32 *)((pitch < 0 ? (char *)base - pitch*(h-1) : (char *)base) + offset);
	VDAssertValidPixmap(mPixmap);
}

void VFBitmapInternal::ConvertBitmapLayoutToPixmapLayout() {
	mPixmapLayout.w			= w;
	mPixmapLayout.h			= h;
	mPixmapLayout.format	= nsVDPixmap::kPixFormat_XRGB8888;
	mPixmapLayout.palette	= NULL;
	mPixmapLayout.pitch		= -pitch;
	mPixmapLayout.data		= pitch<0 ? offset : offset + pitch*(h - 1);
	mPixmapLayout.data2		= 0;
	mPixmapLayout.pitch2	= 0;
	mPixmapLayout.data3		= 0;
	mPixmapLayout.pitch3	= 0;
}

void VFBitmapInternal::ConvertPixmapLayoutToBitmapLayout() {
	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(mPixmapLayout.format);
	w = mPixmapLayout.w;
	h = mPixmapLayout.h;

	switch(mPixmapLayout.format) {
		case nsVDPixmap::kPixFormat_XRGB8888:
			depth = 32;
			break;

		default:
			depth = 0;
			break;
	}

	palette = NULL;
	pitch	= -mPixmapLayout.pitch;
	offset	= mPixmapLayout.pitch < 0 ? mPixmapLayout.data + mPixmapLayout.pitch*(h - 1) : mPixmapLayout.data;

	uint32 bpr = formatInfo.qsize * ((w + formatInfo.qw - 1) / formatInfo.qw);
	modulo	= pitch - bpr;
	size	= VDPixmapLayoutGetMinSize(mPixmapLayout) - offset;

	VDASSERT((sint32)size >= 0);
}

void VFBitmapInternal::ConvertPixmapToBitmap() {
	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(mPixmapLayout.format);
	w = mPixmap.w;
	h = mPixmap.h;

	switch(mPixmap.format) {
		case nsVDPixmap::kPixFormat_XRGB8888:
			depth = 32;
			break;

		default:
			depth = 0;
			break;
	}

	palette = NULL;
	pitch	= -mPixmap.pitch;
	data	= (Pixel *)((char *)mPixmap.data + mPixmap.pitch*(h - 1));

	uint32 bpr = formatInfo.qsize * ((w + formatInfo.qw - 1) / formatInfo.qw);
	modulo	= pitch - bpr;
}

void VFBitmapInternal::BindToDIBSection(const VDFileMappingW32 *mapping) {
	mDIBSection.Init(VDAbsPtrdiff(pitch) >> 2, h, 32, mapping, offset);
	hdc = (VDXHDC)mDIBSection.GetHDC();

	int w = mPixmap.w;
	int h = mPixmap.h;
	mPixmap = mDIBSection.GetPixmap();
	mPixmap.w = w;
	mPixmap.h = h;
	ConvertPixmapToBitmap();
}

void VFBitmapInternal::BindToFrameBuffer(VDFilterFrameBuffer *buffer) {
	if (mpBuffer == buffer)
		return;

	if (buffer)
		buffer->AddRef();
	if (mpBuffer)
		mpBuffer->Release();
	mpBuffer = buffer;

	if (buffer) {
		VDASSERT(buffer->GetSize() >= VDPixmapLayoutGetMinSize(mPixmapLayout));
		Fixup(buffer->GetBasePointer());
	} else {
		Unbind();
	}
}

void VFBitmapInternal::SetFrameNumber(sint64 frame) {
	mFrameNumber = frame;

	if (mFrameRateLo) {
		const double microSecsPerFrame = (double)mFrameRateLo / (double)mFrameRateHi * 10000000.0;
		mFrameTimestampStart = VDRoundToInt64(microSecsPerFrame * frame);
		mFrameTimestampEnd = VDRoundToInt64(microSecsPerFrame * (frame + 1));
	} else {
		mFrameTimestampStart = 0;
		mFrameTimestampEnd = 0;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	VDFilterActivationImpl
//
///////////////////////////////////////////////////////////////////////////

VDFilterActivationImpl::VDFilterActivationImpl(VDXFBitmap& _dst, VDXFBitmap& _src, VDXFBitmap *_last)
	: filter		(NULL)
	, filter_data	(NULL)
	, dst			(_dst)
	, src			(_src)
	, _reserved0	(NULL)
	, last			(_last)
	, x1			(0)
	, y1			(0)
	, x2			(0)
	, y2			(0)
	, pfsi			(NULL)
	, ifp			(NULL)
	, ifp2			(NULL)
	, mpOutputFrames(mOutputFrameArray)
{
	VDASSERT(((char *)&mSizeCheckSentinel - (char *)this) == sizeof(VDXFilterActivation));

	mOutputFrameArray[0] = &dst;
}

///////////////////////////////////////////////////////////////////////////
//
//	FilterInstanceAutoDeinit
//
///////////////////////////////////////////////////////////////////////////

class FilterInstanceAutoDeinit : public vdrefcounted<IVDRefCount> {};

///////////////////////////////////////////////////////////////////////////
class FilterInstance::VideoPrefetcher : public IVDXVideoPrefetcher {
public:
	VideoPrefetcher();

	bool operator==(const VideoPrefetcher& other) const;
	bool operator!=(const VideoPrefetcher& other) const;

	void Clear();
	void TransformToNearestUnique(IVDFilterFrameSource *src);
	void Finalize();

	int VDXAPIENTRY AddRef() { return 2; }
	int VDXAPIENTRY Release() { return 1; }

	void * VDXAPIENTRY AsInterface(uint32 iid) {
		if (iid == IVDXUnknown::kIID)
			return static_cast<IVDXUnknown *>(this);
		else if (iid == IVDXVideoPrefetcher::kIID)
			return static_cast<IVDXVideoPrefetcher *>(this);

		return NULL;
	}

	virtual void VDXAPIENTRY PrefetchFrame(sint32 srcIndex, sint64 frame, uint64 cookie);
	virtual void VDXAPIENTRY PrefetchFrameDirect(sint32 srcIndex, sint64 frame);
	virtual void VDXAPIENTRY PrefetchFrameSymbolic(sint32 srcIndex, sint64 frame);

	struct PrefetchInfo {
		sint64 mFrame;
		uint64 mCookie;

		bool operator==(const PrefetchInfo& other) const {
			return mFrame == other.mFrame && mCookie == other.mCookie;
		}

		bool operator!=(const PrefetchInfo& other) const {
			return mFrame != other.mFrame || mCookie != other.mCookie;
		}
	};

	typedef vdfastfixedvector<PrefetchInfo, 32> SourceFrames;
	SourceFrames mSourceFrames;

	sint64	mDirectFrame;
	sint64	mSymbolicFrame;
	const char *mpError;
};

FilterInstance::VideoPrefetcher::VideoPrefetcher()
	: mDirectFrame(-1)
	, mSymbolicFrame(-1)
	, mpError(NULL)
{
}

bool FilterInstance::VideoPrefetcher::operator==(const VideoPrefetcher& other) const {
	return mDirectFrame == other.mDirectFrame && mSourceFrames == other.mSourceFrames;
}

bool FilterInstance::VideoPrefetcher::operator!=(const VideoPrefetcher& other) const {
	return mDirectFrame != other.mDirectFrame || mSourceFrames != other.mSourceFrames;
}

void FilterInstance::VideoPrefetcher::Clear() {
	mDirectFrame = -1;
	mSourceFrames.clear();
}

void FilterInstance::VideoPrefetcher::TransformToNearestUnique(IVDFilterFrameSource *src) {
	if (mDirectFrame >= 0)
		mDirectFrame = src->GetNearestUniqueFrame(mDirectFrame);

	for(SourceFrames::iterator it(mSourceFrames.begin()), itEnd(mSourceFrames.end()); it != itEnd; ++it) {
		PrefetchInfo& info = *it;

		info.mFrame = src->GetNearestUniqueFrame(info.mFrame);
	}
}

void FilterInstance::VideoPrefetcher::Finalize() {
	if (mSymbolicFrame < 0 && !mSourceFrames.empty()) {
		vdint128 accum(0);
		int count = 0;

		for(SourceFrames::const_iterator it(mSourceFrames.begin()), itEnd(mSourceFrames.end()); it != itEnd; ++it) {
			const PrefetchInfo& info = *it;
			accum += info.mFrame;
			++count;
		}

		accum += (count >> 1);
		mSymbolicFrame = accum / count;
	}
}

void VDXAPIENTRY FilterInstance::VideoPrefetcher::PrefetchFrame(sint32 srcIndex, sint64 frame, uint64 cookie) {
	if (srcIndex) {
		mpError = "An invalid source index was specified in a prefetch operation.";
		return;
	}

	if (frame < 0)
		frame = 0;

	PrefetchInfo& info = mSourceFrames.push_back();
	info.mFrame = frame;
	info.mCookie = cookie;
}

void VDXAPIENTRY FilterInstance::VideoPrefetcher::PrefetchFrameDirect(sint32 srcIndex, sint64 frame) {
	if (srcIndex) {
		mpError = "An invalid source index was specified in a prefetch operation.";
		return;
	}

	if (mDirectFrame >= 0) {
		mpError = "PrefetchFrameDirect() was called multiple times.";
		return;
	}

	if (frame < 0)
		frame = 0;

	mDirectFrame = frame;
	mSymbolicFrame = frame;
}

void VDXAPIENTRY FilterInstance::VideoPrefetcher::PrefetchFrameSymbolic(sint32 srcIndex, sint64 frame) {
	if (srcIndex) {
		mpError = "An invalid source index was specified in a prefetch operation.";
		return;
	}

	if (mSymbolicFrame >= 0) {
		mpError = "PrefetchFrameSymbolic() was called after a symbolic frame was already set.";
		return;
	}

	if (frame < 0)
		frame = 0;

	mSymbolicFrame = frame;
}

///////////////////////////////////////////////////////////////////////////
//
//	FilterInstance
//
///////////////////////////////////////////////////////////////////////////

class VDFilterThreadContextSwapper {
public:
	VDFilterThreadContextSwapper(void *pNewContext) {
#ifdef _M_IX86
		// 0x14: NT_TIB.ArbitraryUserPointer
		mpOldContext = __readfsdword(0x14);
		__writefsdword(0x14, (unsigned long)pNewContext);
#endif
	}

	~VDFilterThreadContextSwapper() {
#ifdef _M_IX86
		__writefsdword(0x14, mpOldContext);
#endif
	}

protected:
	unsigned long	mpOldContext;
};

class FilterInstance::SamplingInfo : public vdrefcounted<IVDRefCount> {
public:
	VDXFilterPreviewSampleCallback mpCB;
	void *mpCBData;
};

FilterInstance::FilterInstance(const FilterInstance& fi)
	: VDFilterActivationImpl((VDXFBitmap&)mRealDst, (VDXFBitmap&)mRealSrc, (VDXFBitmap*)&mRealLast)
	, mRealSrcUncropped	(fi.mRealSrcUncropped)
	, mRealSrc			(fi.mRealSrc)
	, mRealDst			(fi.mRealDst)
	, mRealLast			(fi.mRealLast)
	, mbInvalidFormat	(fi.mbInvalidFormat)
	, mbInvalidFormatHandling(fi.mbInvalidFormatHandling)
	, mbBlitOnEntry		(fi.mbBlitOnEntry)
	, mbConvertOnEntry	(fi.mbConvertOnEntry)
	, mbAlignOnEntry	(fi.mbAlignOnEntry)
	, mBlendBuffer		(fi.mBlendBuffer)
	, mSrcBuf			(fi.mSrcBuf)
	, mDstBuf			(fi.mDstBuf)
	, mOrigW			(fi.mOrigW)
	, mOrigH			(fi.mOrigH)
	, mbPreciseCrop		(fi.mbPreciseCrop)
	, mCropX1			(fi.mCropX1)
	, mCropY1			(fi.mCropY1)
	, mCropX2			(fi.mCropX2)
	, mCropY2			(fi.mCropY2)
	, mScriptObj		(fi.mScriptObj)
	, mFlags			(fi.mFlags)
	, mbEnabled			(fi.mbEnabled)
	, mbStarted			(fi.mbStarted)
	, mbFirstFrame		(fi.mbFirstFrame)
	, mFilterName		(fi.mFilterName)
	, mpAutoDeinit		(fi.mpAutoDeinit)
	, mpLogicError		(fi.mpLogicError)
	, mScriptFunc		(fi.mScriptFunc)
	, mpFDInst			(fi.mpFDInst)
	, mpAlphaCurve		(fi.mpAlphaCurve)
{
	if (mpAutoDeinit)
		mpAutoDeinit->AddRef();
	else
		mbStarted = false;

	filter = const_cast<FilterDefinition *>(&fi.mpFDInst->Attach());
	filter_data = fi.filter_data;
}

FilterInstance::FilterInstance(FilterDefinitionInstance *fdi)
	: VDFilterActivationImpl((VDXFBitmap&)mRealDst, (VDXFBitmap&)mRealSrc, (VDXFBitmap*)&mRealLast)
	, mbInvalidFormat(true)
	, mbInvalidFormatHandling(false)
	, mbBlitOnEntry(false)
	, mbConvertOnEntry(false)
	, mbAlignOnEntry(false)
	, mBlendBuffer(0)
	, mSrcBuf(0)
	, mDstBuf(0)
	, mOrigW(0)
	, mOrigH(0)
	, mbPreciseCrop(true)
	, mCropX1(0)
	, mCropY1(0)
	, mCropX2(0)
	, mCropY2(0)
	, mFlags(0)
	, mbEnabled(true)
	, mbStarted(false)
	, mbFirstFrame(false)
	, mpFDInst(fdi)
	, mpAutoDeinit(NULL)
	, mpLogicError(NULL)
{
	filter = const_cast<FilterDefinition *>(&fdi->Attach());
	src.hdc = NULL;
	dst.hdc = NULL;
	last->hdc = NULL;
	x1 = 0;
	y1 = 0;
	x2 = 0;
	y2 = 0;

	if (filter->inst_data_size) {
		if (!(filter_data = allocmem(filter->inst_data_size)))
			throw MyMemoryError();

		memset(filter_data, 0, filter->inst_data_size);

		if (filter->initProc) {
			try {
				vdrefptr<FilterInstanceAutoDeinit> autoDeinit;
				
				if (!filter->copyProc && !filter->copyProc2 && filter->deinitProc)
					autoDeinit = new FilterInstanceAutoDeinit;

				VDFilterThreadContextSwapper autoContextSwap(&mThreadContext);
				if (filter->initProc(AsVDXFilterActivation(), &g_VDFilterCallbacks)) {
					if (filter->deinitProc)
						filter->deinitProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);

					freemem(filter_data);
					throw MyError("Filter failed to initialize.");
				}

				mpAutoDeinit = autoDeinit.release();
			} catch(const MyError& e) {
				throw MyError("Cannot initialize filter '%s': %s", filter->name, e.gets());
			}
		}

		mFilterName = VDTextAToW(filter->name);
	} else
		filter_data = NULL;


	// initialize script object
	mScriptObj.mpName		= NULL;
	mScriptObj.obj_list		= NULL;
	mScriptObj.Lookup		= NULL;
	mScriptObj.func_list	= NULL;
	mScriptObj.pNextObject	= NULL;

	if (filter->script_obj) {
		const ScriptFunctionDef *pf = filter->script_obj->func_list;

		if (pf) {
			for(; pf->func_ptr; ++pf) {
				VDScriptFunctionDef def;

				def.arg_list	= pf->arg_list;
				def.name		= pf->name;

				switch(def.arg_list[0]) {
				default:
				case '0':
					def.func_ptr	= ScriptFunctionThunkVoid;
					break;
				case 'i':
					def.func_ptr	= ScriptFunctionThunkInt;
					break;
				case 'v':
					def.func_ptr	= ScriptFunctionThunkVariadic;
					break;
				}

				mScriptFunc.push_back(def);
			}

			VDScriptFunctionDef def_end = {NULL};
			mScriptFunc.push_back(def_end);

			mScriptObj.func_list	= &mScriptFunc[0];
		}
	}
}

FilterInstance::~FilterInstance() {
	VDFilterThreadContextSwapper autoContextSwap(&mThreadContext);

	if (mpAutoDeinit) {
		if (!mpAutoDeinit->Release())
			filter->deinitProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
		mpAutoDeinit = NULL;
	} else if (filter->deinitProc) {
		filter->deinitProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
	}

	freemem(filter_data);

	mpFDInst->Detach();

	VDFilterFrameRequestError *oldError = mpLogicError.xchg(NULL);
	if (oldError)
		oldError->Release();
}

FilterInstance *FilterInstance::Clone() {
	FilterInstance *fi = new FilterInstance(*this);

	if (!fi) throw MyMemoryError();

	if (fi->filter_data) {
		const VDXFilterDefinition *def = fi->filter;
		fi->filter_data = allocmem(def->inst_data_size);

		if (!fi->filter_data) {
			delete fi;
			throw MyMemoryError();
		}

		VDFilterThreadContextSwapper autoContextSwap(&mThreadContext);
		if (def->copyProc2)
			def->copyProc2(AsVDXFilterActivation(), &g_VDFilterCallbacks, fi->filter_data, fi->AsVDXFilterActivation(), &g_VDFilterCallbacks);
		else if (def->copyProc)
			def->copyProc(AsVDXFilterActivation(), &g_VDFilterCallbacks, fi->filter_data);
		else
			memcpy(fi->filter_data, filter_data, def->inst_data_size);
	}

	return fi;
}

const char *FilterInstance::GetName() const {
	return filter->name;
}

bool FilterInstance::IsCroppingEnabled() const {
	return (mCropX1 | mCropY1 | mCropX2 | mCropY2) != 0;
}

bool FilterInstance::IsPreciseCroppingEnabled() const {
	return mbPreciseCrop;
}

vdrect32 FilterInstance::GetCropInsets() const {
	return vdrect32(mCropX1, mCropY1, mCropX2, mCropY2);
}

void FilterInstance::SetCrop(int x1, int y1, int x2, int y2, bool precise) {
	mCropX1 = x1;
	mCropY1 = y1;
	mCropX2 = x2;
	mCropY2 = y2;
	mbPreciseCrop = precise;
}

bool FilterInstance::IsConfigurable() const {
	return filter->configProc != NULL;
}

bool FilterInstance::IsInPlace() const {
	return (mFlags & FILTERPARAM_SWAP_BUFFERS) == 0;
}

bool FilterInstance::Configure(VDXHWND parent, IVDXFilterPreview2 *ifp2) {
	VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
	bool success;

	this->ifp = ifp2;
	this->ifp2 = ifp2;

	vdprotected1("configuring filter \"%s\"", const char *, filter->name) {
		VDFilterThreadContextSwapper autoSwap(&mThreadContext);

		success = !filter->configProc(AsVDXFilterActivation(), &g_VDFilterCallbacks, parent);
	}

	this->ifp2 = NULL;
	this->ifp = NULL;

	return success;
}

uint32 FilterInstance::Prepare(const VFBitmapInternal& input) {
	bool testingInvalidFormat = input.mpPixmapLayout->format == 255;
	bool invalidCrop = false;

	mbInvalidFormat	= false;

	if (testingInvalidFormat)
		mbInvalidFormatHandling = false;

	mOrigW			= input.w;
	mOrigH			= input.h;

	mRealSrcUncropped	= input;
	mRealSrc			= input;
	if (testingInvalidFormat) {
		mRealSrc.mpPixmapLayout->format = nsVDXPixmap::kPixFormat_XRGB8888;
	} else {
		// Clamp the crop rect at this point to avoid going below 1x1.
		// We will throw an exception later during init.
		const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(mRealSrc.mPixmapLayout.format);
		int xmask = ~((1 << (formatInfo.qwbits + formatInfo.auxwbits)) - 1);
		int ymask = ~((1 << (formatInfo.qhbits + formatInfo.auxhbits)) - 1);

		if (mbPreciseCrop) {
			if ((mCropX1 | mCropY1) & ~xmask)
				invalidCrop = true;

			if ((mCropX2 | mCropY2) & ~ymask)
				invalidCrop = true;
		}

		int qx1 = (mCropX1 & xmask) >> formatInfo.qwbits;
		int qy1 = (mCropY1 & ymask) >> formatInfo.qhbits;
		int qx2 = (mCropX2 & xmask) >> formatInfo.qwbits;
		int qy2 = (mCropY2 & ymask) >> formatInfo.qhbits;

		int qw = input.w >> formatInfo.qwbits;
		int qh = input.h >> formatInfo.qhbits;

		if (qx1 >= qw)
			qx1 = qw - 1;

		if (qy1 >= qh)
			qy1 = qh - 1;

		if (qx1 + qx2 >= qw)
			qx2 = (qw - qx1) - 1;

		if (qy1 + qy2 >= qh)
			qy2 = (qh - qy1) - 1;

		VDASSERT(qx1 >= 0 && qy1 >= 0 && qx2 >= 0 && qy2 >= 0);
		VDASSERT(qx1 + qx2 < qw);
		VDASSERT(qy1 + qy2 < qh);

		mRealSrc.mPixmapLayout = VDPixmapLayoutOffset(mRealSrc.mPixmapLayout, qx1 << formatInfo.qwbits, qy1 << formatInfo.qhbits);
		mRealSrc.mPixmapLayout.w -= (qx1+qx2) << formatInfo.qwbits;
		mRealSrc.mPixmapLayout.h -= (qy1+qy2) << formatInfo.qhbits;
	}

	mRealSrc.ConvertPixmapLayoutToBitmapLayout();

	mRealSrc.dwFlags	= 0;
	mRealSrc.hdc		= NULL;

	mRealLast			= mRealSrc;
	mRealLast.dwFlags	= 0;
	mRealLast.hdc		= NULL;

	mRealDst			= mRealSrc;
	mRealDst.dwFlags	= 0;
	mRealDst.hdc		= NULL;

	if (testingInvalidFormat) {
		mRealSrc.mPixmapLayout.format = 255;
		mRealDst.mPixmapLayout.format = 255;
	}

	pfsi	= &mfsi;

	uint32 flags = FILTERPARAM_SWAP_BUFFERS;
	if (filter->paramProc) {
		VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);

		vdprotected1("preparing filter \"%s\"", const char *, filter->name) {
			VDFilterThreadContextSwapper autoSwap(&mThreadContext);

			flags = filter->paramProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
		}
	}

	if (testingInvalidFormat) {
		mRealSrc.mPixmapLayout.format = nsVDXPixmap::kPixFormat_XRGB8888;
		mRealDst.mPixmapLayout.format = nsVDXPixmap::kPixFormat_XRGB8888;
	}

	mFlags = flags;
	mLag = (uint32)mFlags >> 16;

	if (mRealDst.depth) {
		mRealDst.modulo	= mRealDst.pitch - 4*mRealDst.w;
		mRealDst.size	= mRealDst.offset + vdptrdiffabs(mRealDst.pitch) * mRealDst.h;
		VDASSERT((sint32)mRealDst.size >= 0);

		mRealDst.ConvertBitmapLayoutToPixmapLayout();
	} else {
		if (!mRealDst.mPixmapLayout.pitch) {
			VDPixmapCreateLinearLayout(mRealDst.mPixmapLayout, mRealDst.mPixmapLayout.format, mRealDst.mPixmapLayout.w, mRealDst.mPixmapLayout.h, 16);

			if (mRealDst.mPixmapLayout.format == nsVDPixmap::kPixFormat_XRGB8888)
				VDPixmapLayoutFlipV(mRealDst.mPixmapLayout);
		}

		mRealDst.ConvertPixmapLayoutToBitmapLayout();
	}

	*mRealLast.mpPixmapLayout = *mRealSrc.mpPixmapLayout;
	mRealLast.ConvertPixmapLayoutToBitmapLayout();

	mfsi.lMicrosecsPerSrcFrame	= VDRoundToInt((double)mRealSrc.mFrameRateLo / (double)mRealSrc.mFrameRateHi * 1000000.0);
	mfsi.lMicrosecsPerFrame		= VDRoundToInt((double)mRealDst.mFrameRateLo / (double)mRealDst.mFrameRateHi * 1000000.0);

	if (testingInvalidFormat && (flags != FILTERPARAM_NOT_SUPPORTED))
		mbInvalidFormatHandling = true;

	if (invalidCrop)
		flags = FILTERPARAM_NOT_SUPPORTED;

	if (flags == FILTERPARAM_NOT_SUPPORTED)
		mbInvalidFormat = true;

	return flags;
}

void FilterInstance::Start(int accumulatedDelay, uint32 flags, IVDFilterFrameSource *pSource, VDFilterFrameAllocator *pSourceAllocator) {
	if (mbStarted)
		return;

	VDASSERT(!mbBlitOnEntry || mbConvertOnEntry || mbAlignOnEntry);

	mfsi.flags = flags;

	mFsiDelayRing.resize(accumulatedDelay);
	mDelayRingPos = 0;

	mLastResultFrame = -1;

	// Note that we set this immediately so we can Stop() the filter, even if
	// it fails.
	mbStarted = true;
	if (filter->startProc) {
		int rcode;
		try {
			VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);

			vdprotected1("starting filter \"%s\"", const char *, filter->name) {
				VDFilterThreadContextSwapper autoSwap(&mThreadContext);

				rcode = filter->startProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
			}
		} catch(const MyError& e) {
			Stop();
			throw MyError("Cannot start filter '%s': %s", filter->name, e.gets());
		}

		if (rcode) {
			Stop();
			throw MyError("Cannot start filter '%s': Unknown failure.", filter->name);
		}

		mbFirstFrame = true;
	}

	mpSource = pSource;
	mpSourceAllocator = pSourceAllocator;
	mbCanStealSourceBuffer = IsInPlace() && !mRealSrc.hdc && !mbBlitOnEntry;
	mSharingPredictor.Clear();

	if (mpLogicError)
		throw MyError("Cannot start filter '%s': %s", mpLogicError->mError.c_str());
}

void FilterInstance::Stop() {
	if (!mbStarted)
		return;

	mbStarted = false;

	if (filter->endProc) {
		VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
		vdprotected1("stopping filter \"%s\"", const char *, filter->name) {
			VDFilterThreadContextSwapper autoSwap(&mThreadContext);

			filter->endProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
		}
	}

	mFsiDelayRing.clear();

	mRealLast.mDIBSection.Shutdown();
	mRealDst.mDIBSection.Shutdown();
	mRealSrc.mDIBSection.Shutdown();
	mFileMapping.Shutdown();

	mFrameQueueWaiting.Shutdown();
	mFrameQueueInProgress.Shutdown();
	mFrameCache.Flush();

	mpSource = NULL;
	mpSourceAllocator = NULL;
	mpResultAllocator = NULL;

	mpSourceConversionBlitter = NULL;
}

void FilterInstance::SetAllocator(VDFilterFrameAllocator *alloc) {
	mpResultAllocator = alloc;
}

bool FilterInstance::CreateRequest(sint64 outputFrame, bool writable, IVDFilterFrameClientRequest **req) {
	VDASSERT(mbStarted);

	if (outputFrame < 0)
		outputFrame = 0;

	if (mRealDst.mFrameCount > 0 && outputFrame >= mRealDst.mFrameCount)
		outputFrame = mRealDst.mFrameCount - 1;

	mSharingPredictor.OnRequest(outputFrame);

	vdrefptr<VDFilterFrameRequest> r;
	bool cached = false;
	bool newRequest = false;

	if (!mFrameQueueWaiting.GetRequest(outputFrame, ~r) && !mFrameQueueInProgress.GetRequest(outputFrame, ~r)) {
		newRequest = true;

		mFrameQueueWaiting.CreateRequest(~r);

		VDFilterFrameRequestTiming timing;
		timing.mSourceFrame = GetSourceFrame(outputFrame);
		timing.mOutputFrame = outputFrame;
		r->SetTiming(timing);

		vdrefptr<VDFilterFrameBuffer> buf;

		if (mFrameCache.Lookup(outputFrame, ~buf)) {
			cached = true;

			r->SetResultBuffer(buf);
		} else {
			VideoPrefetcher vp;

			if (mLag) {
				for(uint32 i=0; i<=mLag; ++i) {
					VDPosition laggedSrc = outputFrame + i;

					if (laggedSrc >= 0)
						GetPrefetchInfo(laggedSrc, vp, false);
				}
			} else {
				GetPrefetchInfo(outputFrame, vp, false);
			}

			if (mpSource) {
				if (vp.mDirectFrame >= 0) {
					r->SetSourceCount(1);

					vdrefptr<IVDFilterFrameClientRequest> srcreq;
					mpSource->CreateRequest(vp.mDirectFrame, false, ~srcreq);

					srcreq->Start(NULL, 0);

					r->SetSourceRequest(0, srcreq);
				} else {
					r->SetSourceCount(vp.mSourceFrames.size());

					uint32 idx = 0;
					for(VideoPrefetcher::SourceFrames::const_iterator it(vp.mSourceFrames.begin()), itEnd(vp.mSourceFrames.end()); it != itEnd; ++it) {
						const VideoPrefetcher::PrefetchInfo& prefetchInfo = *it;
						bool writable = (idx == 0) && IsInPlace();

						vdrefptr<IVDFilterFrameClientRequest> srcreq;
						mpSource->CreateRequest(prefetchInfo.mFrame, writable, ~srcreq);

						srcreq->Start(NULL, prefetchInfo.mCookie);

						r->SetSourceRequest(idx++, srcreq);
					}
				}
			}
		}
	}

	vdrefptr<IVDFilterFrameClientRequest> creq;
	r->CreateClient(writable, ~creq);

	if (mSharingPredictor.IsSharingPredicted(outputFrame))
		r->SetStealable(false);

	if (cached)
		r->MarkComplete(true);
	else if (newRequest)
		mFrameQueueWaiting.Add(r);

	*req = creq.release();
	return true;
}

bool FilterInstance::CreateSamplingRequest(sint64 outputFrame, VDXFilterPreviewSampleCallback sampleCB, void *sampleCBData, IVDFilterFrameClientRequest **req) {
	VDASSERT(mbStarted);

	if (outputFrame < 0)
		outputFrame = 0;

	if (mRealDst.mFrameCount > 0 && outputFrame >= mRealDst.mFrameCount)
		outputFrame = mRealDst.mFrameCount - 1;

	vdrefptr<SamplingInfo> sampInfo(new SamplingInfo);
	sampInfo->mpCB = sampleCB;
	sampInfo->mpCBData = sampleCBData;

	vdrefptr<VDFilterFrameRequest> r;

	mFrameQueueWaiting.CreateRequest(~r);

	VDFilterFrameRequestTiming timing;
	timing.mSourceFrame = GetSourceFrame(outputFrame);
	timing.mOutputFrame = outputFrame;
	r->SetTiming(timing);
	r->SetCacheable(false);
	r->SetExtraInfo(sampInfo);

	vdrefptr<VDFilterFrameBuffer> buf;

	VideoPrefetcher vp;

	GetPrefetchInfo(outputFrame, vp, false);

	if (mpSource) {
		r->SetSourceCount(vp.mSourceFrames.size());

		uint32 idx = 0;
		for(VideoPrefetcher::SourceFrames::const_iterator it(vp.mSourceFrames.begin()), itEnd(vp.mSourceFrames.end()); it != itEnd; ++it) {
			const VideoPrefetcher::PrefetchInfo& prefetchInfo = *it;
			bool writable = (idx == 0) && IsInPlace();

			vdrefptr<IVDFilterFrameClientRequest> srcreq;
			mpSource->CreateRequest(prefetchInfo.mFrame, writable, ~srcreq);

			srcreq->Start(NULL, prefetchInfo.mCookie);

			r->SetSourceRequest(idx++, srcreq);
		}
	}

	vdrefptr<IVDFilterFrameClientRequest> creq;
	r->CreateClient(false, ~creq);

	mFrameQueueWaiting.Add(r);

	*req = creq.release();
	return true;
}

bool FilterInstance::RunRequests() {
	vdrefptr<VDFilterFrameRequest> req;
	if (mFrameQueueWaiting.GetNextRequest(~req)) {
		mFrameQueueInProgress.Add(req);

		const SamplingInfo *samplingInfo = static_cast<const SamplingInfo *>(req->GetExtraInfo());

		if (samplingInfo || !mLag) {
			vdrefptr<VDFilterFrameBuffer> buf;
			bool stolen = false;

			if (mbCanStealSourceBuffer && req->GetSourceCount()) {
				IVDFilterFrameClientRequest *creqsrc0 = req->GetSourceRequest(0);

				if (!creqsrc0 || creqsrc0->IsResultBufferStealable()) {
					VDFilterFrameBuffer *srcbuf = req->GetSource(0);
					uint32 srcRefs = 1;

					if (creqsrc0)
						++srcRefs;

					if (srcbuf->Steal(srcRefs)) {
						buf = srcbuf;
						stolen = true;
					}
				}
			}

			if (stolen || mpResultAllocator->Allocate(~buf)) {
				req->SetResultBuffer(buf);
				if (Run(*req)) {
					req->MarkComplete(true);

					if (!samplingInfo)
						mFrameCache.Add(buf, req->GetTiming().mOutputFrame);
				} else {
					req->MarkComplete(false);
				}
			} else {
				req->MarkComplete(false);
			}
		} else {
			VDPosition targetFrame = req->GetTiming().mOutputFrame;
			VDPosition startFrame = targetFrame;
			VDPosition endFrame = targetFrame + mLag;

			bool enableIntermediateFrameCaching = false;
			if (startFrame <= mLastResultFrame + 1 && mLastResultFrame < endFrame) {
				startFrame = mLastResultFrame + 1;
				enableIntermediateFrameCaching = true;
			}

			vdrefptr<VDFilterFrameBuffer> buf;
			if (mpResultAllocator->Allocate(~buf)) {
				req->SetResultBuffer(buf);

				for(VDPosition currentFrame = startFrame; currentFrame <= endFrame; ++currentFrame) {
					VDPosition resultFrameUnlagged = currentFrame - mLag;

					if (!Run(*req, (uint32)(currentFrame - targetFrame), 1, currentFrame)) {
						req->MarkComplete(false);
						break;
					}

					if (currentFrame == endFrame) {
						mFrameCache.Add(buf, targetFrame);
						req->MarkComplete(true);
					} else if (enableIntermediateFrameCaching && resultFrameUnlagged >= 0) {
						mFrameCache.Add(buf, resultFrameUnlagged);
						mFrameQueueWaiting.CompleteRequests(resultFrameUnlagged, buf);

						if (!mpResultAllocator->Allocate(~buf))
							break;
					}
				}
			} else {
				req->MarkComplete(false);
			}
		}

		mFrameQueueInProgress.Remove(req);

		return true;
	}

	return false;
}

bool FilterInstance::Run(VDFilterFrameRequest& request) {
	return Run(request, 0, 0xffff, -1);
}

bool FilterInstance::Run(VDFilterFrameRequest& request, uint32 sourceOffset, uint32 sourceCountLimit, VDPosition outputFrameOverride) {
	VDFilterFrameRequestError *logicError = mpLogicError;
	if (logicError) {
		request.SetError(logicError);
		return false;
	}

	bool success = true;
	const VDFilterFrameRequestTiming& timing = request.GetTiming();

	IVDFilterFrameClientRequest *creqsrc0 = request.GetSourceRequest(sourceOffset);
	vdrefptr<VDFilterFrameRequestError> err(creqsrc0->GetError());

	if (err) {
		request.SetError(err);
		return false;
	}

	uint32 sourceCount = request.GetSourceCount();

	VDASSERT(sourceOffset < sourceCount);
	sourceCount -= sourceOffset;
	if (sourceCount > sourceCountLimit)
		sourceCount = sourceCountLimit;

	mExternalSrc.BindToFrameBuffer(request.GetSource(sourceOffset));

	VDFilterFrameBuffer *resultBuffer = request.GetResultBuffer();
	bool unbindSrcOnExit = false;
	if (mbBlitOnEntry || mRealSrc.hdc) {
		if (!mRealSrc.hdc && IsInPlace()) {
			mRealSrc.BindToFrameBuffer(resultBuffer);
			mRealSrcUncropped.BindToFrameBuffer(resultBuffer);
		}

		if (!mpSourceConversionBlitter)
			mpSourceConversionBlitter = VDPixmapCreateBlitter(mRealSrc.mPixmap, mExternalSrc.mPixmap);

		mpSourceConversionBlitter->Blit(mRealSrcUncropped.mPixmap, mExternalSrc.mPixmap);
	} else {
		if (IsInPlace()) {
			if (!resultBuffer) {
				resultBuffer = request.GetSource(sourceOffset);

				mRealSrc.BindToFrameBuffer(resultBuffer);				
			} else {
				VDFilterFrameBuffer *buf = resultBuffer;
				mRealSrc.BindToFrameBuffer(buf);
				mRealSrcUncropped.BindToFrameBuffer(buf);

				if (!mpSourceConversionBlitter)
					mpSourceConversionBlitter = VDPixmapCreateBlitter(mRealSrc.mPixmap, mExternalSrc.mPixmap);

				mpSourceConversionBlitter->Blit(mRealSrcUncropped.mPixmap, mExternalSrc.mPixmap);
				mRealSrcUncropped.Unbind();
			}
		} else {
			mRealSrc.BindToFrameBuffer(request.GetSource(sourceOffset));
		}

		unbindSrcOnExit = true;
	}

	mRealSrc.SetFrameNumber(creqsrc0->GetFrameNumber());
	mRealSrc.mCookie = creqsrc0->GetCookie();

	if (!mRealDst.hdc)
		mRealDst.BindToFrameBuffer(resultBuffer);

	mRealDst.SetFrameNumber(timing.mOutputFrame);
	mExternalDst.BindToFrameBuffer(resultBuffer);

	mSourceFrameArray.resize(sourceCount);
	mSourceFrames.resize(sourceCount);
	mpSourceFrames = mSourceFrameArray.data();
	mSourceFrameCount = sourceCount;

	if (sourceCount)
		mSourceFrameArray[0] = (VDXFBitmap *)&mRealSrc;

	for(uint32 i=1; i<sourceCount; ++i) {
		mSourceFrameArray[i] = (VDXFBitmap *)&mSourceFrames[i];

		IVDFilterFrameClientRequest *creqsrc = request.GetSourceRequest(sourceOffset + i);
		VDFilterFrameRequestError *localError = creqsrc->GetError();

		if (localError) {
			err = localError;
			success = false;
			break;
		}

		VFBitmapInternal& bm = mSourceFrames[i];
		VDFilterFrameBuffer *fb = request.GetSource(sourceOffset + i);

		if (mbBlitOnEntry) {
			vdrefptr<VDFilterFrameBuffer> fbconv;
			mpSourceAllocator->Allocate(~fbconv);

			bm.mPixmapLayout = mRealSrc.mPixmapLayout;
			bm.BindToFrameBuffer(fbconv);

			VFBitmapInternal bmUncropped(mRealSrcUncropped);
			bmUncropped.BindToFrameBuffer(fbconv);

			VDPixmap convSrc(VDPixmapFromLayout(mExternalSrc.mPixmapLayout, fb->GetBasePointer()));
			if (!mpSourceConversionBlitter)
				mpSourceConversionBlitter = VDPixmapCreateBlitter(bmUncropped.mPixmap, convSrc);

			mpSourceConversionBlitter->Blit(bmUncropped.mPixmap, convSrc);
		} else {
			bm.mPixmapLayout = mExternalSrc.mPixmapLayout;
			bm.BindToFrameBuffer(fb);
		}

		bm.mFrameCount = mRealSrc.mFrameCount;
		bm.mFrameRateLo = mRealSrc.mFrameRateLo;
		bm.mFrameRateHi = mRealSrc.mFrameRateHi;
		bm.SetFrameNumber(creqsrc->GetFrameNumber());
		bm.mCookie = creqsrc->GetCookie();
	}

	const SamplingInfo *sampInfo = (const SamplingInfo *)request.GetExtraInfo();

	VDPosition resultFrame = outputFrameOverride >= 0 ? outputFrameOverride : timing.mOutputFrame;

	try {
		Run(timing.mSourceFrame,
			resultFrame,
			sampInfo ? sampInfo->mpCB : NULL,
			sampInfo ? sampInfo->mpCBData : NULL);
	} catch(const MyError& e) {
		err = new_nothrow VDFilterFrameRequestError;

		if (err)
			err->mError = e.gets();

		success = false;
	}

	request.SetError(err);

	for(uint32 i=1; i<sourceCount; ++i) {
		mSourceFrames[i].Unbind();
	}

	if (unbindSrcOnExit)
		mRealSrc.Unbind();
	mExternalSrc.Unbind();

	if (!mRealDst.hdc)
		mRealDst.Unbind();

	mExternalDst.Unbind();

	mLastResultFrame = resultFrame;
	return success;
}

void FilterInstance::Run(sint64 sourceFrame, sint64 outputFrame, VDXFilterPreviewSampleCallback sampleCB, void *sampleCBData) {
	VDASSERT(outputFrame >= 0);

	if (mRealSrc.dwFlags & VDXFBitmap::NEEDS_HDC)
		VDPixmapBlt(mRealSrc.mPixmap, mExternalSrc.mPixmap);

	if (mRealDst.dwFlags & VDXFBitmap::NEEDS_HDC) {
		::SetViewportOrgEx((HDC)mRealDst.hdc, 0, 0, NULL);
		::SelectClipRgn((HDC)mRealDst.hdc, NULL);

		VDPixmapBlt(mRealDst.mPixmap, mExternalDst.mPixmap);
	}

	// If the filter has a delay ring...
	DelayInfo di;

	di.mSourceFrame		= sourceFrame;
	di.mOutputFrame		= outputFrame;

	if (!mFsiDelayRing.empty()) {
		if (mbFirstFrame)
			std::fill(mFsiDelayRing.begin(), mFsiDelayRing.end(), di);

		DelayInfo diOut = mFsiDelayRing[mDelayRingPos];
		mFsiDelayRing[mDelayRingPos] = di;

		if (++mDelayRingPos >= mFsiDelayRing.size())
			mDelayRingPos = 0;

		sourceFrame		= diOut.mSourceFrame;
		outputFrame		= diOut.mOutputFrame;
	}

	// Update FilterStateInfo structure.
	mfsi.lCurrentSourceFrame	= VDClampToSint32(di.mSourceFrame);
	mfsi.lCurrentFrame			= VDClampToSint32(outputFrame);
	mfsi.lSourceFrameMS			= VDClampToSint32(VDRoundToInt64((double)di.mSourceFrame * (double)mRealSrc.mFrameRateLo / (double)mRealSrc.mFrameRateHi * 1000.0));
	mfsi.lDestFrameMS			= VDClampToSint32(VDRoundToInt64((double)outputFrame * (double)mRealDst.mFrameRateLo / (double)mRealDst.mFrameRateHi * 1000.0));
	mfsi.mOutputFrame			= VDClampToSint32(outputFrame);

	// Compute alpha blending value.
	float alpha = 1.0f;

	VDParameterCurve *pAlphaCurve = GetAlphaParameterCurve();
	if (pAlphaCurve)
		alpha = (float)(*pAlphaCurve)((double)outputFrame).mY;

	// If this is an in-place filter with an alpha curve, save off the old image.
	bool skipFilter = false;
	bool skipBlit = false;

	if (!sampleCB && alpha < 254.5f / 255.0f) {
		if (mFlags & FILTERPARAM_SWAP_BUFFERS) {
			if (alpha < 0.5f / 255.0f)
				skipFilter = true;
		} else {
			if (alpha < 0.5f / 255.0f) {
				skipFilter = true;

				if (mRealSrc.data == mRealDst.data && mRealSrc.pitch == mRealDst.pitch)
					skipBlit = true;
			}

			if (!skipBlit)
				VDPixmapBlt(mBlendPixmap, mRealSrc.mPixmap);
		}
	}

	if (!skipFilter) {
		try {
			VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
			vdprotected1("running filter \"%s\"", const char *, filter->name) {
				VDFilterThreadContextSwapper autoSwap(&mThreadContext);

				if (sampleCB) {
					sampleCB(&AsVDXFilterActivation()->src, mfsi.mOutputFrame, VDClampToSint32(mRealDst.mFrameCount), sampleCBData);
				} else {
					// Deliberately ignore the return code. It was supposed to be an error value,
					// but earlier versions didn't check it and logoaway returns true in some cases.
					filter->runProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
				}
			}
		} catch(const MyError& e) {
			throw MyError("Error processing frame %lld with filter '%s': %s", outputFrame, filter->name, e.gets());
		}

		if (mRealDst.dwFlags & VDXFBitmap::NEEDS_HDC) {
			::GdiFlush();
			VDPixmapBlt(mExternalDst.mPixmap, mRealDst.mPixmap);
		}
	}

	if (!skipBlit && alpha < 254.5f / 255.0f) {
		if (alpha > 0.5f / 255.0f)
			VDPixmapBltAlphaConst(mRealDst.mPixmap, mBlendPixmap, 1.0f - alpha);
		else
			VDPixmapBlt(mRealDst.mPixmap, mBlendPixmap);
	}

	if (mFlags & FILTERPARAM_NEEDS_LAST)
		VDPixmapBlt(mRealLast.mPixmap, mRealSrc.mPixmap);

	mbFirstFrame = false;
}

void FilterInstance::RunSamplingCallback(long frame, long frameCount, VDXFilterPreviewSampleCallback cb, void *cbdata) {
	VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
	vdprotected1("running filter \"%s\"", const char *, filter->name) {
		cb(&src, frame, frameCount, cbdata);
	}
}

void FilterInstance::InvalidateAllCachedFrames() {
	mFrameCache.InvalidateAllFrames();

	if (mbStarted && filter->eventProc) {
		vdprotected1("invalidating internal caches on filter \"%s\"", const char *, filter->name) {
			VDFilterThreadContext context;
			VDFilterThreadContextSwapper autoSwap(&context);

			filter->eventProc(AsVDXFilterActivation(), &g_VDFilterCallbacks, kVDXVFEvent_InvalidateCaches, NULL);
		}
	}
}

bool FilterInstance::GetScriptString(VDStringA& buf) {
	buf.clear();

	if (!filter->fssProc)
		return false;

	char tbuf[4096];
	tbuf[0] = 0;

	VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
	vdprotected1("querying filter \"%s\" for script string", const char *, filter->name) {
		VDFilterThreadContext context;
		VDFilterThreadContextSwapper autoSwap(&context);

		if (!filter->fssProc(AsVDXFilterActivation(), &g_VDFilterCallbacks, tbuf, sizeof tbuf))
			return false;
	}

	tbuf[4095] = 0;

	buf = tbuf;
	return true;
}

bool FilterInstance::GetSettingsString(VDStringA& buf) const {
	buf.clear();

	if (!filter->stringProc && !filter->stringProc2)
		return false;

	char tbuf[2048];
	tbuf[0] = 0;

	VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
	vdprotected1("querying filter \"%s\" for settings string", const char *, filter->name) {
		VDFilterThreadContext context;
		VDFilterThreadContextSwapper autoSwap(&context);

		if (filter->stringProc2)
			filter->stringProc2(AsVDXFilterActivation(), &g_VDFilterCallbacks, tbuf, sizeof tbuf);
		else
			filter->stringProc(AsVDXFilterActivation(), &g_VDFilterCallbacks, tbuf);
	}

	tbuf[2047] = 0;

	buf = tbuf;
	return true;
}

sint64 FilterInstance::GetSourceFrame(sint64 frame) {
	VideoPrefetcher vp;

	GetPrefetchInfo(frame, vp, true);

	if (vp.mDirectFrame >= 0)
		return vp.mDirectFrame;

	return vp.mSourceFrames[0].mFrame;
}

sint64 FilterInstance::GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source) {
	if (source == this)
		return outputFrame;

	VideoPrefetcher vp;
	GetPrefetchInfo(outputFrame, vp, true);
	vp.Finalize();

	if (vp.mSymbolicFrame >= 0)
		return mpSource->GetSymbolicFrame(vp.mSymbolicFrame, source);

	return -1;
}

bool FilterInstance::IsFiltered(sint64 outputFrame) const {
	return !IsFadedOut(outputFrame);
}

bool FilterInstance::IsFadedOut(sint64 outputFrame) const {
	if (!mpAlphaCurve)
		return true;

	float alpha = (float)(*mpAlphaCurve)((double)outputFrame).mY;

	return (alpha < (0.5f / 255.0f));
}

bool FilterInstance::GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex) {
	VideoPrefetcher prefetcher;
	GetPrefetchInfo(outputFrame, prefetcher, false);

	// If the filter directly specifies a direct mapping, use it.
	if (prefetcher.mDirectFrame >= 0)
		return mpSource->GetDirectMapping(prefetcher.mDirectFrame, sourceFrame, sourceIndex);

	// The filter doesn't have a direct mapping. If it doesn't pull from any source frames,
	// then assume it is not mappable.
	uint32 sourceFrameCount = prefetcher.mSourceFrames.size();
	if (!sourceFrameCount)
		return false;

	// If the filter isn't faded out or we don't have the info needed to determine that, bail.
	if (!IsFadedOut(outputFrame))
		return false;

	// Check if there are any source frames. If not, we have to process that frame.
	if (prefetcher.mSourceFrames.empty())
		return false;

	// Filter's faded out, so assume we're going to map to the first source frame.
	return mpSource->GetDirectMapping(prefetcher.mSourceFrames[0].mFrame, sourceFrame, sourceIndex);
}

sint64 FilterInstance::GetNearestUniqueFrame(sint64 outputFrame) {
	if (!(mFlags & FILTERPARAM_PURE_TRANSFORM))
		return outputFrame;

	VideoPrefetcher prefetcher;
	GetPrefetchInfo(outputFrame, prefetcher, false);
	prefetcher.TransformToNearestUnique(mpSource);

	VideoPrefetcher prefetcher2;
	while(outputFrame >= 0) {
		prefetcher2.Clear();
		GetPrefetchInfo(outputFrame - 1, prefetcher2, false);
		prefetcher2.TransformToNearestUnique(mpSource);

		if (prefetcher != prefetcher2)
			break;

		--outputFrame;
	}

	return outputFrame;
}

const VDScriptObject *FilterInstance::GetScriptObject() const {
	return filter->script_obj ? &mScriptObj : NULL;
}

void FilterInstance::GetPrefetchInfo(sint64 outputFrame, VideoPrefetcher& prefetcher, bool requireOutput) const {
	if (filter->prefetchProc2) {
		bool handled = false;

		VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
		vdprotected1("prefetching filter \"%s\"", const char *, filter->name) {
			handled = filter->prefetchProc2(AsVDXFilterActivation(), &g_VDFilterCallbacks, outputFrame, &prefetcher);
		}

		if (handled) {
			if (prefetcher.mpError)
				SetLogicErrorF("A logic error was detected in filter '%s': %s", filter->name, prefetcher.mpError);

			if (!requireOutput || !prefetcher.mSourceFrames.empty() || prefetcher.mDirectFrame >= 0)
				return;
		}
	} else if (filter->prefetchProc) {
		sint64 inputFrame;

		VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
		vdprotected1("prefetching filter \"%s\"", const char *, filter->name) {
			inputFrame = filter->prefetchProc(AsVDXFilterActivation(), &g_VDFilterCallbacks, outputFrame);
		}

		prefetcher.PrefetchFrame(0, inputFrame, 0);
		return;
	}

	double factor = ((double)mRealSrc.mFrameRateHi * (double)mRealDst.mFrameRateLo) / ((double)mRealSrc.mFrameRateLo * (double)mRealDst.mFrameRateHi);

	prefetcher.PrefetchFrame(0, VDFloorToInt64((outputFrame + 0.5f) * factor), 0);
}

void FilterInstance::SetLogicErrorF(const char *format, ...) const {
	if (mpLogicError)
		return;

	vdrefptr<VDFilterFrameRequestError> error(new_nothrow VDFilterFrameRequestError);

	if (error) {
		va_list val;
		va_start(val, format);
		error->mError.append_vsprintf(format, val);
		va_end(val);

		VDFilterFrameRequestError *oldError = mpLogicError.compareExchange(error, NULL);
		
		if (!oldError)
			error.release();
	}
}

void FilterInstance::ConvertParameters(CScriptValue *dst, const VDScriptValue *src, int argc) {
	int idx = 0;
	while(argc-->0) {
		const VDScriptValue& v = *src++;

		switch(v.type) {
			case VDScriptValue::T_INT:
				*dst = CScriptValue(v.asInt());
				break;
			case VDScriptValue::T_STR:
				*dst = CScriptValue(v.asString());
				break;
			case VDScriptValue::T_LONG:
				*dst = CScriptValue(v.asLong());
				break;
			case VDScriptValue::T_DOUBLE:
				*dst = CScriptValue(v.asDouble());
				break;
			case VDScriptValue::T_VOID:
				*dst = CScriptValue();
				break;
			default:
				throw MyError("Script: Parameter %d is not of a supported type for filter configuration functions.");
				break;
		}

		++dst;
		++idx;
	}
}

void FilterInstance::ConvertValue(VDScriptValue& dst, const CScriptValue& v) {
	switch(v.type) {
		case VDScriptValue::T_INT:
			dst = VDScriptValue(v.asInt());
			break;
		case VDScriptValue::T_STR:
			dst = VDScriptValue(v.asString());
			break;
		case VDScriptValue::T_LONG:
			dst = VDScriptValue(v.asLong());
			break;
		case VDScriptValue::T_DOUBLE:
			dst = VDScriptValue(v.asDouble());
			break;
		case VDScriptValue::T_VOID:
		default:
			dst = VDScriptValue();
			break;
	}
}

namespace {
	class VDScriptInterpreterAdapter : public IScriptInterpreter{
	public:
		VDScriptInterpreterAdapter(IVDScriptInterpreter *p) : mpInterp(p) {}

		void ScriptError(int e) {
			mpInterp->ScriptError(e);
		}

		char** AllocTempString(long l) {
			return mpInterp->AllocTempString(l);
		}

	protected:
		IVDScriptInterpreter *mpInterp;
	};
}

void FilterInstance::ScriptFunctionThunkVoid(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	FilterInstance *const thisPtr = (FilterInstance *)argv[-1].asObjectPtr();
	int funcidx = isi->GetCurrentMethod() - &thisPtr->mScriptFunc[0];

	const ScriptFunctionDef& fd = thisPtr->filter->script_obj->func_list[funcidx];
	VDXScriptVoidFunctionPtr pf = (VDXScriptVoidFunctionPtr)fd.func_ptr;

	std::vector<CScriptValue> params(argc ? argc : 1);

	ConvertParameters(&params[0], argv, argc);

	VDScriptInterpreterAdapter adapt(isi);
	pf(&adapt, thisPtr->AsVDXFilterActivation(), &params[0], argc);
}

void FilterInstance::ScriptFunctionThunkInt(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	FilterInstance *const thisPtr = (FilterInstance *)argv[-1].asObjectPtr();
	int funcidx = isi->GetCurrentMethod() - &thisPtr->mScriptFunc[0];

	const ScriptFunctionDef& fd = thisPtr->filter->script_obj->func_list[funcidx];
	VDXScriptIntFunctionPtr pf = (VDXScriptIntFunctionPtr)fd.func_ptr;

	std::vector<CScriptValue> params(argc ? argc : 1);

	ConvertParameters(&params[0], argv, argc);

	VDScriptInterpreterAdapter adapt(isi);
	int rval = pf(&adapt, thisPtr->AsVDXFilterActivation(), &params[0], argc);

	argv[0] = rval;
}

void FilterInstance::ScriptFunctionThunkVariadic(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	FilterInstance *const thisPtr = (FilterInstance *)argv[-1].asObjectPtr();
	int funcidx = isi->GetCurrentMethod() - &thisPtr->mScriptFunc[0];

	const ScriptFunctionDef& fd = thisPtr->filter->script_obj->func_list[funcidx];
	ScriptFunctionPtr pf = fd.func_ptr;

	std::vector<CScriptValue> params(argc ? argc : 1);

	ConvertParameters(&params[0], argv, argc);

	VDScriptInterpreterAdapter adapt(isi);
	CScriptValue v(pf(&adapt, thisPtr->AsVDXFilterActivation(), &params[0], argc));

	ConvertValue(argv[0], v);
}
