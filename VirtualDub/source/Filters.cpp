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

#include <stdarg.h>
#include <malloc.h>

#include <windows.h>
#include <commctrl.h>
#include <ctype.h>
#include <vfw.h>

#ifdef _MSC_VER
	#include <intrin.h>
#endif

#include "resource.h"
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/filesys.h>
#include <vd2/system/protscope.h>
#include <vd2/system/refcount.h>
#include <vd2/system/VDString.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/w32assist.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "gui.h"
#include "oshelper.h"
#include "misc.h"
#include "plugins.h"

#include "filter.h"
#include "filters.h"
#include "VideoSource.h"
#include <vd2/plugin/vdplugin.h>

extern vdrefptr<IVDVideoSource> inputVideo;

extern HINSTANCE	g_hInst;
extern "C" unsigned long version_num;

extern const VDScriptObject obj_VDVFiltInst;

List			g_listFA;

FilterSystem	filters;

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

/////////////////////////////////////


static bool isFPUEnabled() {
	return !!FPU_enabled;
}

static bool isMMXEnabled() {
	return !!MMX_enabled;
}

static void FilterThrowExcept(const char *format, ...) {
	va_list val;
	MyError e;

	va_start(val, format);
	e.vsetf(format, val);
	va_end(val);

	throw e;
}

static void FilterThrowExceptMemory() {
	throw MyMemoryError();
}

// This is really disgusting...

struct VDXFilterVTbls {
	void *pvtblVBitmap;
};

static void InitVTables(VDXFilterVTbls *pvtbls) {
	VBitmap tmp;
	pvtbls->pvtblVBitmap = *(void **)&tmp;
}

static long FilterGetCPUFlags() {
	return CPUGetEnabledExtensions();
}

static long FilterGetHostVersionInfo(char *buf, int len) {
	char tbuf[256];

	LoadString(g_hInst, IDS_TITLE_INITIAL, tbuf, sizeof tbuf);
	_snprintf(buf, len, tbuf, version_num,
#ifdef _DEBUG
		"debug"
#else
		"release"
#endif
		);

	return version_num;
}

FilterFunctions g_filterFuncs={
	FilterAdd,
	FilterRemove,
	isFPUEnabled,
	isMMXEnabled,
	InitVTables,
	FilterThrowExceptMemory,
	FilterThrowExcept,
	FilterGetCPUFlags,
	FilterGetHostVersionInfo,
};

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
{
}

VFBitmapInternal::~VFBitmapInternal()
{
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

		mDIBSection.Shutdown();
	}

	return *this;
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

	uint32 bpr = formatInfo.qsize * -(-((sint32)w >> formatInfo.qwbits));
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

	uint32 bpr = formatInfo.qsize * -(-((sint32)w >> formatInfo.qwbits));
	modulo	= pitch - bpr;
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
{
	VDASSERTCT(sizeof(*this) == sizeof(VDXFilterActivation));
}

VDFilterActivationImpl::VDFilterActivationImpl(const VDFilterActivationImpl& fa, VDXFBitmap& _dst, VDXFBitmap& _src, VDXFBitmap *_last)
	: filter		(fa.filter)
	, filter_data	(fa.filter_data)
	, dst			(_dst)
	, src			(_src)
	, _reserved0	(NULL)
	, last			(_last)
	, x1			(0)
	, y1			(0)
	, x2			(0)
	, y2			(0)
	, pfsi			(fa.pfsi)
	, ifp			(NULL)
	, ifp2			(NULL)
{
	VDASSERTCT(sizeof(*this) == sizeof(VDXFilterActivation));
}


///////////////////////////////////////////////////////////////////////////
//
//	FilterDefinitionInstance
//
///////////////////////////////////////////////////////////////////////////

class FilterDefinitionInstance : public ListNode2<FilterDefinitionInstance> {
public:
	FilterDefinitionInstance(VDExternalModule *pfm);
	~FilterDefinitionInstance();

	void Assign(const FilterDefinition& def, int len);

	const FilterDefinition& Attach();
	void Detach();

	const FilterDefinition& GetDef() const { return mDef; }
	VDExternalModule	*GetModule() const { return mpExtModule; }

	const VDStringA&	GetName() const { return mName; }
	const VDStringA&	GetAuthor() const { return mAuthor; }
	const VDStringA&	GetDescription() const { return mDescription; }

protected:
	VDExternalModule	*mpExtModule;
	FilterDefinition	mDef;
	VDAtomicInt			mRefCount;
	VDStringA			mName;
	VDStringA			mAuthor;
	VDStringA			mDescription;
};

FilterDefinitionInstance::FilterDefinitionInstance(VDExternalModule *pfm)
	: mpExtModule(pfm)
	, mRefCount(0)
{
}

FilterDefinitionInstance::~FilterDefinitionInstance() {
	VDASSERT(mRefCount==0);
}

void FilterDefinitionInstance::Assign(const FilterDefinition& def, int len) {
	memset(&mDef, 0, sizeof mDef);
	memcpy(&mDef, &def, std::min<size_t>(sizeof mDef, len));

	mName			= def.name;
	mAuthor			= def.maker ? def.maker : "(internal)";
	mDescription	= def.desc;

	mDef._module	= const_cast<VDXFilterModule *>(&mpExtModule->GetFilterModuleInfo());
}

const FilterDefinition& FilterDefinitionInstance::Attach() {
	if (mpExtModule)
		mpExtModule->Lock();

	++mRefCount;

	return mDef;
}

void FilterDefinitionInstance::Detach() {
	VDASSERT(mRefCount.dec() >= 0);

	if (mpExtModule)
		mpExtModule->Unlock();
}

///////////////////////////////////////////////////////////////////////////
//
//	FilterInstanceAutoDeinit
//
///////////////////////////////////////////////////////////////////////////

class FilterInstanceAutoDeinit : public vdrefcounted<IVDRefCount> {};

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

FilterInstance::FilterInstance(const FilterInstance& fi)
	: VDFilterActivationImpl	(fi, (VDXFBitmap&)mRealDst, (VDXFBitmap&)mRealSrc, (VDXFBitmap*)&mRealLast)
	, mRealSrcUncropped	(fi.mRealSrcUncropped)
	, mRealSrc			(fi.mRealSrc)
	, mRealDst			(fi.mRealDst)
	, mRealLast			(fi.mRealLast)
	, mbInvalidFormat	(fi.mbInvalidFormat)
	, mbInvalidFormatHandling(fi.mbInvalidFormatHandling)
	, mbBlitOnEntry		(fi.mbBlitOnEntry)
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
	, mFilterName		(fi.mFilterName)
	, mpAutoDeinit		(fi.mpAutoDeinit)
	, mScriptFunc		(fi.mScriptFunc)
	, mpFDInst			(fi.mpFDInst)
	, mpAlphaCurve		(fi.mpAlphaCurve)
{
	if (mpAutoDeinit)
		mpAutoDeinit->AddRef();
	else
		mbStarted = false;

	filter = const_cast<FilterDefinition *>(&fi.mpFDInst->Attach());
}

FilterInstance::FilterInstance(FilterDefinitionInstance *fdi)
	: VDFilterActivationImpl((VDXFBitmap&)mRealDst, (VDXFBitmap&)mRealSrc, (VDXFBitmap*)&mRealLast)
	, mbInvalidFormat(true)
	, mbInvalidFormatHandling(false)
	, mbBlitOnEntry(false)
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
	, mpFDInst(fdi)
	, mpAutoDeinit(NULL)
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

		if (filter->initProc)
			try {
				vdrefptr<FilterInstanceAutoDeinit> autoDeinit;
				
				if (!filter->copyProc && filter->deinitProc)
					autoDeinit = new FilterInstanceAutoDeinit;

				VDFilterThreadContextSwapper autoContextSwap(&mThreadContext);
				if (filter->initProc(AsVDXFilterActivation(), &g_filterFuncs)) {
					if (filter->deinitProc)
						filter->deinitProc(AsVDXFilterActivation(), &g_filterFuncs);

					freemem(filter_data);
					throw MyError("Filter failed to initialize.");
				}

				mpAutoDeinit = autoDeinit.release();
			} catch(const MyError& e) {
				throw MyError("Cannot initialize filter '%s': %s", filter->name, e.gets());
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
			filter->deinitProc(AsVDXFilterActivation(), &g_filterFuncs);
		mpAutoDeinit = NULL;
	} else if (filter->deinitProc) {
		filter->deinitProc(AsVDXFilterActivation(), &g_filterFuncs);
	}

	freemem(filter_data);

	mpFDInst->Detach();
}

FilterInstance *FilterInstance::Clone() {
	FilterInstance *fi = new FilterInstance(*this);

	if (!fi) throw MyMemoryError();

	if (fi->filter_data) {
		fi->filter_data = allocmem(fi->filter->inst_data_size);

		if (!fi->filter_data) {
			delete fi;
			throw MyMemoryError();
		}

		VDFilterThreadContextSwapper autoContextSwap(&mThreadContext);
		if (fi->filter->copyProc)
			fi->filter->copyProc(AsVDXFilterActivation(), &g_filterFuncs, fi->filter_data);
		else
			memcpy(fi->filter_data, filter_data, fi->filter->inst_data_size);
	}

	return fi;
}

void FilterInstance::Destroy() {
	delete this;
}

uint32 FilterInstance::Prepare(const VFBitmapInternal& input) {
	bool testingInvalidFormat = input.mpPixmapLayout->format == 255;
	bool invalidCrop = false;

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

		if (qy1 + qy2 >= qw)
			qy2 = (qw - qy1) - 1;

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

			flags = filter->paramProc(AsVDXFilterActivation(), &g_filterFuncs);
		}
	}

	if (testingInvalidFormat) {
		mRealSrc.mPixmapLayout.format = nsVDXPixmap::kPixFormat_XRGB8888;
		mRealDst.mPixmapLayout.format = nsVDXPixmap::kPixFormat_XRGB8888;
	}

	mFlags = flags;

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

	if (invalidCrop)
		flags |= FILTERPARAM_NOT_SUPPORTED;

	return flags;
}

void FilterInstance::Start(int accumulatedDelay) {
	if (!mbStarted) {
		mFsiDelayRing.resize(accumulatedDelay);
		mDelayRingPos = 0;

		// Note that we set this immediately so we can Stop() the filter, even if
		// it fails.
		mbStarted = true;
		if (filter->startProc) {
			int rcode;
			try {
				VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);

				vdprotected1("starting filter \"%s\"", const char *, filter->name) {
					VDFilterThreadContextSwapper autoSwap(&mThreadContext);

					rcode = filter->startProc(AsVDXFilterActivation(), &g_filterFuncs);
				}
			} catch(const MyError& e) {
				Stop();
				throw MyError("Cannot start filter '%s': %s", filter->name, e.gets());
			}

			if (rcode) {
				Stop();
				throw MyError("Cannot start filter '%s': Unknown failure.", filter->name);
			}
		}
	}
}

void FilterInstance::Stop() {
	if (mbStarted) {
		mbStarted = false;

		if (filter->endProc) {
			VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
			vdprotected1("stopping filter \"%s\"", const char *, filter->name) {
				VDFilterThreadContextSwapper autoSwap(&mThreadContext);

				filter->endProc(AsVDXFilterActivation(), &g_filterFuncs);
			}
		}

		mFsiDelayRing.clear();

		mRealLast.mDIBSection.Shutdown();
		mRealDst.mDIBSection.Shutdown();
		mRealSrc.mDIBSection.Shutdown();
		mFileMapping.Shutdown();
	}
}

void FilterInstance::Run(bool firstFrame, sint64 sourceFrame, sint64 outputFrame, sint64 timelineFrame, sint64 sequenceFrame, sint64 sequenceTimeMS, uint32 flags) {
	if (mRealSrc.dwFlags & VDXFBitmap::NEEDS_HDC)
		VDPixmapBlt(mRealSrc.mPixmap, mExternalSrc);

	if (mRealDst.dwFlags & VDXFBitmap::NEEDS_HDC) {
		SetViewportOrgEx(mRealDst.hdc, 0, 0, NULL);
		SelectClipRgn(mRealDst.hdc, NULL);

		VDPixmapBlt(mRealDst.mPixmap, mExternalDst);
	}

	// If the filter has a delay ring...
	DelayInfo di;

	di.mSourceFrame		= sourceFrame;
	di.mOutputFrame		= outputFrame;
	di.mTimelineFrame	= timelineFrame;

	if (!mFsiDelayRing.empty()) {
		if (firstFrame)
			std::fill(mFsiDelayRing.begin(), mFsiDelayRing.end(), di);

		DelayInfo diOut = mFsiDelayRing[mDelayRingPos];
		mFsiDelayRing[mDelayRingPos] = di;

		if (++mDelayRingPos >= mFsiDelayRing.size())
			mDelayRingPos = 0;

		sourceFrame		= diOut.mSourceFrame;
		outputFrame		= diOut.mOutputFrame;
		timelineFrame	= diOut.mTimelineFrame;
	}

	// Update FilterStateInfo structure.
	mfsi.lCurrentSourceFrame	= VDClampToSint32(di.mSourceFrame);
	mfsi.lCurrentFrame			= VDClampToSint32(sequenceFrame);
	mfsi.lSourceFrameMS			= VDClampToSint32(VDRoundToInt64((double)mRealSrc.mFrameRateLo / (double)mRealSrc.mFrameRateHi * (double)di.mSourceFrame * 1000.0));
	mfsi.lDestFrameMS			= VDClampToSint32(sequenceTimeMS);
	mfsi.flags					= flags;
	mfsi.mOutputFrame			= outputFrame;

	// Compute alpha blending value.
	float alpha = 1.0f;

	VDParameterCurve *pAlphaCurve = GetAlphaParameterCurve();
	if (pAlphaCurve)
		alpha = (float)(*pAlphaCurve)((double)timelineFrame).mY;

	// If this is an in-place filter with an alpha curve, save off the old image.
	bool skipFilter = false;
	bool skipBlit = false;

	if (alpha < 254.5f / 255.0f) {
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

				// Deliberately ignore the return code. It was supposed to be an error value,
				// but earlier versions didn't check it and logoaway returns true in some cases.
				filter->runProc(AsVDXFilterActivation(), &g_filterFuncs);
			}
		} catch(const MyError& e) {
			throw MyError("Error running filter '%s': %s", filter->name, e.gets());
		}

		if (mRealDst.dwFlags & VDXFBitmap::NEEDS_HDC) {
			::GdiFlush();
			VDPixmapBlt(mExternalDst, mRealDst.mPixmap);
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
}

sint64 FilterInstance::GetSourceFrame(sint64 frame) const {
	if (filter->prefetchProc) {
		VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
		vdprotected1("prefetching filter \"%s\"", const char *, filter->name) {
			frame = filter->prefetchProc(AsVDXFilterActivation(), &g_filterFuncs, frame);
		}

		return frame;
	} else {
		double factor = ((double)mRealSrc.mFrameRateHi * (double)mRealDst.mFrameRateLo) / ((double)mRealSrc.mFrameRateLo * (double)mRealDst.mFrameRateHi);

		return VDFloorToInt64((frame + 0.5f) * factor);
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
	FilterInstance *const thisPtr = static_cast<FilterInstance *>((VDFilterActivationImpl *)argv[-1].asObjectPtr());
	int funcidx = isi->GetCurrentMethod() - &thisPtr->mScriptFunc[0];

	const ScriptFunctionDef& fd = thisPtr->filter->script_obj->func_list[funcidx];
	VDXScriptVoidFunctionPtr pf = (VDXScriptVoidFunctionPtr)fd.func_ptr;

	std::vector<CScriptValue> params(argc ? argc : 1);

	ConvertParameters(&params[0], argv, argc);

	VDScriptInterpreterAdapter adapt(isi);
	pf(&adapt, thisPtr->AsVDXFilterActivation(), &params[0], argc);
}

void FilterInstance::ScriptFunctionThunkInt(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	FilterInstance *const thisPtr = static_cast<FilterInstance *>((VDFilterActivationImpl *)argv[-1].asObjectPtr());
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
	FilterInstance *const thisPtr = static_cast<FilterInstance *>((VDFilterActivationImpl *)argv[-1].asObjectPtr());
	int funcidx = isi->GetCurrentMethod() - &thisPtr->mScriptFunc[0];

	const ScriptFunctionDef& fd = thisPtr->filter->script_obj->func_list[funcidx];
	ScriptFunctionPtr pf = fd.func_ptr;

	std::vector<CScriptValue> params(argc ? argc : 1);

	ConvertParameters(&params[0], argv, argc);

	VDScriptInterpreterAdapter adapt(isi);
	CScriptValue v(pf(&adapt, thisPtr->AsVDXFilterActivation(), &params[0], argc));

	ConvertValue(argv[0], v);
}

///////////////////////////////////////////////////////////////////////////
//
//	Filter global functions
//
///////////////////////////////////////////////////////////////////////////

static ListAlloc<FilterDefinitionInstance>	g_filterDefs;

FilterDefinition *FilterAdd(VDXFilterModule *fm, FilterDefinition *pfd, int fd_len) {
	VDExternalModule *pExtModule = VDGetExternalModuleByFilterModule(fm);

	if (pExtModule) {
		List2<FilterDefinitionInstance>::fwit it2(g_filterDefs.begin());

		for(; it2; ++it2) {
			FilterDefinitionInstance& fdi = *it2;

			if (fdi.GetModule() == pExtModule && fdi.GetName() == pfd->name) {
				fdi.Assign(*pfd, fd_len);
				return const_cast<FilterDefinition *>(&fdi.GetDef());
			}
		}

		vdautoptr<FilterDefinitionInstance> pfdi(new FilterDefinitionInstance(pExtModule));
		pfdi->Assign(*pfd, fd_len);

		const FilterDefinition *pfdi2 = &pfdi->GetDef();
		g_filterDefs.AddTail(pfdi.release());

		return const_cast<FilterDefinition *>(pfdi2);
	}

	return NULL;
}

void FilterAddBuiltin(const FilterDefinition *pfd) {
	vdautoptr<FilterDefinitionInstance> fdi(new FilterDefinitionInstance(NULL));
	fdi->Assign(*pfd, sizeof(FilterDefinition));

	g_filterDefs.AddTail(fdi.release());
}

void FilterRemove(FilterDefinition *fd) {
#if 0
	List2<FilterDefinitionInstance>::fwit it(g_filterDefs.begin());

	for(; it; ++it) {
		FilterDefinitionInstance& fdi = *it;

		if (&fdi.GetDef() == fd) {
			fdi.Remove();
			delete &fdi;
			return;
		}
	}
#endif
}

void FilterEnumerateFilters(std::list<FilterBlurb>& blurbs) {
	List2<FilterDefinitionInstance>::fwit it(g_filterDefs.begin());

	for(; it; ++it) {
		FilterDefinitionInstance& fd = *it;

		blurbs.push_back(FilterBlurb());
		FilterBlurb& fb = blurbs.back();

		fb.key			= &fd;
		fb.name			= fd.GetName();
		fb.author		= fd.GetAuthor();
		fb.description	= fd.GetDescription();
	}
}

//////////////////

struct FilterValueInit {
	LONG lMin, lMax;
	LONG cVal;
	const char *title;
	IFilterPreview *ifp;
	void (*mpUpdateFunction)(long value, void *data);
	void *mpUpdateFunctionData;
};

static INT_PTR CALLBACK FilterValueDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	FilterValueInit *fvi;

    switch (message)
    {
        case WM_INITDIALOG:
			fvi = (FilterValueInit *)lParam;
			SendMessage(hDlg, WM_SETTEXT, 0, (LPARAM)fvi->title);
			SendMessage(GetDlgItem(hDlg, IDC_SLIDER), TBM_SETRANGE, (WPARAM)FALSE, MAKELONG(fvi->lMin, fvi->lMax));
			SendMessage(GetDlgItem(hDlg, IDC_SLIDER), TBM_SETPOS, (WPARAM)TRUE, fvi->cVal); 
			SetWindowLongPtr(hDlg, DWLP_USER, (LONG)fvi);
			SetDlgItemInt(hDlg, IDC_SETTING, fvi->cVal, TRUE);

			if (fvi->ifp) {
				HWND hwndPreviewButton = GetDlgItem(hDlg, IDC_PREVIEW);
				EnableWindow(hwndPreviewButton, TRUE);
				fvi->ifp->InitButton((VDXHWND)hwndPreviewButton);
			}
            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				fvi = (FilterValueInit *)GetWindowLongPtr(hDlg, DWLP_USER);
				fvi->cVal = SendMessage(GetDlgItem(hDlg, IDC_SLIDER), TBM_GETPOS, 0,0);
				EndDialog(hDlg, TRUE);
				break;
			case IDCANCEL:
	            EndDialog(hDlg, FALSE);  
				break;
			case IDC_PREVIEW:
				fvi = (FilterValueInit *)GetWindowLongPtr(hDlg, DWLP_USER);
				if (fvi->ifp)
					fvi->ifp->Toggle((VDXHWND)hDlg);
				break;
			default:
				return FALSE;
			}
			SetWindowLongPtr(hDlg, DWLP_MSGRESULT, 0);
			return TRUE;

		case WM_HSCROLL:
			if (lParam) {
				HWND hwndScroll = (HWND)lParam;

				fvi = (FilterValueInit *)GetWindowLongPtr(hDlg, DWLP_USER);
				fvi->cVal = SendMessage(hwndScroll, TBM_GETPOS, 0, 0);

				SetDlgItemInt(hDlg, IDC_SETTING, fvi->cVal, TRUE);
				if (fvi->mpUpdateFunction)
					fvi->mpUpdateFunction(fvi->cVal, fvi->mpUpdateFunctionData);

				if (fvi->ifp)
					fvi->ifp->RedoFrame();
			}
			SetWindowLongPtr(hDlg, DWLP_MSGRESULT, 0);
			return TRUE;
    }
    return FALSE;
}

LONG FilterGetSingleValue(HWND hWnd, LONG cVal, LONG lMin, LONG lMax, char *title, IFilterPreview *ifp, void (*pUpdateFunction)(long value, void *data), void *pUpdateFunctionData) {
	FilterValueInit fvi;
	VDStringA tbuf;
	tbuf.sprintf("Filter: %s", title);

	fvi.cVal = cVal;
	fvi.lMin = lMin;
	fvi.lMax = lMax;
	fvi.title = tbuf.c_str();
	fvi.ifp = ifp;
	fvi.mpUpdateFunction = pUpdateFunction;
	fvi.mpUpdateFunctionData = pUpdateFunctionData;

	if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_SINGVAR), hWnd, FilterValueDlgProc, (LPARAM)&fvi))
		return fvi.cVal;

	return cVal;
}
