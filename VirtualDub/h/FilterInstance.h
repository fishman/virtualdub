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
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef f_FILTERINSTANCE_H
#define f_FILTERINSTANCE_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/fraction.h>
#include <vd2/system/list.h>
#include <vd2/system/VDString.h>
#include <vd2/system/refcount.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/blitter.h>
#include <vd2/VDLib/win32/DIBSection.h>
#include <vd2/VDLib/win32/FileMapping.h>
#include <vd2/VDLib/ParameterCurve.h>
#include <vd2/plugin/vdvideofilt.h>
#include "VBitmap.h"
#include "ScriptValue.h"
#include "FilterFrameQueue.h"
#include "FilterFrameAllocator.h"
#include "FilterFrameCache.h"
#include "FilterFrameSharingPredictor.h"

//////////////////

class IVDVideoDisplay;
class IVDPositionControl;
struct VDWaveFormat;
class VDTimeline;
struct VDXWaveFormat;
struct VDXFilterDefinition;

class VDScriptValue;
class IVDScriptInterpreter;

class FilterDefinitionInstance;

class VDFilterFrameBuffer;
class VDFilterFrameRequest;
class IVDFilterFrameClientRequest;

///////////////////

VDXWaveFormat *VDXCopyWaveFormat(const VDXWaveFormat *pFormat);

///////////////////

class VFBitmapInternal : public VBitmap {
public:
	VFBitmapInternal();
	VFBitmapInternal(const VFBitmapInternal&);
	~VFBitmapInternal();

	VFBitmapInternal& operator=(const VFBitmapInternal&);

	void Unbind();
	void Fixup(void *base);
	void ConvertBitmapLayoutToPixmapLayout();
	void ConvertPixmapLayoutToBitmapLayout();
	void ConvertPixmapToBitmap();
	void BindToDIBSection(const VDFileMappingW32 *mapping);
	void BindToFrameBuffer(VDFilterFrameBuffer *buffer);

	void SetFrameNumber(sint64 frame);

public:
	// Must match layout of VFBitmap!
	enum {
		NEEDS_HDC		= 0x00000001L,
	};

	uint32	dwFlags;
	VDXHDC	hdc;

	uint32	mFrameRateHi;
	uint32	mFrameRateLo;
	sint64	mFrameCount;

	VDXPixmapLayout	*mpPixmapLayout;
	const VDXPixmap	*mpPixmap;

	uint32	mAspectRatioHi;
	uint32	mAspectRatioLo;

	sint64	mFrameNumber;				///< Current frame number (zero based).
	sint64	mFrameTimestampStart;		///< Starting timestamp of frame, in 100ns units.
	sint64	mFrameTimestampEnd;			///< Ending timestamp of frame, in 100ns units.
	sint64	mCookie;					///< Cookie supplied when frame was requested.

public:
	VDDIBSectionW32	mDIBSection;
	VDPixmap		mPixmap;
	VDPixmapLayout	mPixmapLayout;

protected:
	VDFilterFrameBuffer	*mpBuffer;
};

class FilterInstanceAutoDeinit;

struct VDFilterThreadContext {
	int					tmp[16];
	void				*ESPsave;
};

class VDFilterActivationImpl {		// clone of VDXFilterActivation
	VDFilterActivationImpl(const VDFilterActivationImpl&);
	VDFilterActivationImpl& operator=(const VDFilterActivationImpl&);
public:
	VDFilterActivationImpl(VDXFBitmap& _dst, VDXFBitmap& _src, VDXFBitmap *_last);

	const VDXFilterDefinition *filter;
	void *filter_data;
	VDXFBitmap&	dst;
	VDXFBitmap&	src;
	VDXFBitmap	*_reserved0;
	VDXFBitmap	*const last;
	uint32		x1;
	uint32		y1;
	uint32		x2;
	uint32		y2;

	VDXFilterStateInfo	*pfsi;
	IVDXFilterPreview	*ifp;
	IVDXFilterPreview2	*ifp2;			// (V11+)

	uint32		mSourceFrameCount;
	VDXFBitmap *const *mpSourceFrames;	// (V14+)
	VDXFBitmap *const *mpOutputFrames;	// (V14+)

private:
	char mSizeCheckSentinel;

	VDXFBitmap *mOutputFrameArray[1];
};

class IVDFilterFrameSource : public IVDRefCount {
public:
	virtual void SetAllocator(VDFilterFrameAllocator *alloc) = 0;
	virtual bool CreateRequest(sint64 outputFrame, bool writable, IVDFilterFrameClientRequest **req) = 0;
	virtual bool GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex) = 0;
	virtual sint64 GetSourceFrame(sint64 outputFrame) = 0;
	virtual sint64 GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source) = 0;
	virtual sint64 GetNearestUniqueFrame(sint64 outputFrame) = 0;
	virtual const VDPixmapLayout& GetOutputLayout() = 0;
};

class FilterInstance : public ListNode, protected VDFilterActivationImpl, public vdrefcounted<IVDFilterFrameSource> {
	FilterInstance& operator=(const FilterInstance&);		// outlaw copy assignment

public:
	FilterInstance(const FilterInstance& fi);
	FilterInstance(FilterDefinitionInstance *);

	FilterInstance *Clone();

	bool	IsEnabled() const { return mbEnabled; }
	void	SetEnabled(bool enabled) { mbEnabled = enabled; }

	bool	IsAlignmentRequired() const { return mbAlignOnEntry; }
	bool	IsConversionRequired() const { return mbConvertOnEntry; }
	bool	IsInPlace() const;

	const char *GetName() const;
	uint32	GetFlags() const { return mFlags; }
	uint32	GetFrameDelay() const { return mFlags >> 16; }

	bool	GetInvalidFormatState() const { return mbInvalidFormat; }
	bool	GetInvalidFormatHandlingState() const { return mbInvalidFormatHandling; }

	bool	IsCroppingEnabled() const;
	bool	IsPreciseCroppingEnabled() const;
	vdrect32 GetCropInsets() const;
	void	SetCrop(int x1, int y1, int x2, int y2, bool precise);

	bool	IsConfigurable() const;
	bool	Configure(VDXHWND parent, IVDXFilterPreview2 *ifp2);

	uint32	Prepare(const VFBitmapInternal& input);

	void	Start(int accumulatedDelay, uint32 flags, IVDFilterFrameSource *pSource, VDFilterFrameAllocator *pSourceAllocator);
	void	Stop();

	void	SetAllocator(VDFilterFrameAllocator *alloc);
	bool	CreateRequest(sint64 outputFrame, bool writable, IVDFilterFrameClientRequest **req);
	bool	CreateSamplingRequest(sint64 outputFrame, VDXFilterPreviewSampleCallback sampleCB, void *sampleCBData, IVDFilterFrameClientRequest **req);
	bool	RunRequests();

	bool	Run(VDFilterFrameRequest& request);
	bool	Run(VDFilterFrameRequest& request, uint32 sourceOffset, uint32 sourceCountLimit, VDPosition outputFrameOverride);
	void	Run(sint64 sourceFrame, sint64 outputFrame, VDXFilterPreviewSampleCallback sampleCB, void *sampleCBData);

	void	RunSamplingCallback(long frame, long frameCount, VDXFilterPreviewSampleCallback cb, void *cbdata);

	void	InvalidateAllCachedFrames();

	bool	GetScriptString(VDStringA& buf);
	bool	GetSettingsString(VDStringA& buf) const;

	sint64	GetSourceFrame(sint64 frame);
	sint64	GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source);

	bool	IsFiltered(sint64 outputFrame) const;
	bool	IsFadedOut(sint64 outputFrame) const;
	bool	GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex);
	sint64	GetNearestUniqueFrame(sint64 outputFrame);

	const VDScriptObject *GetScriptObject() const;

	uint32		GetPreCropWidth()		const { return mOrigW; }
	uint32		GetPreCropHeight()		const { return mOrigH; }

	IVDFilterFrameSource *GetSource() const { return mpSource; }

	uint32		GetSourceFrameWidth()	const { return mRealSrc.w; }
	uint32		GetSourceFrameHeight()	const { return mRealSrc.h; }
	VDFraction	GetSourceFrameRate()	const { return VDFraction(mRealSrc.mFrameRateHi, mRealSrc.mFrameRateLo); }
	sint64		GetSourceFrameCount()	const { return mRealSrc.mFrameCount; }

	uint32		GetOutputFrameWidth()	const { return mRealDst.w; }
	uint32		GetOutputFrameHeight()	const { return mRealDst.h; }
	VDFraction	GetOutputFrameRate()	const { return VDFraction(mRealDst.mFrameRateHi, mRealDst.mFrameRateLo); }
	sint64		GetOutputFrameCount()	const { return mRealDst.mFrameCount; }
	const VDPixmapLayout& GetOutputLayout() { return mRealDst.mPixmapLayout; }

	sint64		GetLastSourceFrame()	const { return mfsi.lCurrentSourceFrame; }
	sint64		GetLastOutputFrame()	const { return mfsi.lCurrentFrame; }

	VDParameterCurve *GetAlphaParameterCurve() const { return mpAlphaCurve; }
	void SetAlphaParameterCurve(VDParameterCurve *p) { mpAlphaCurve = p; }

protected:
	class SamplingInfo;

	~FilterInstance();

	const VDXFilterActivation *AsVDXFilterActivation() const { return (const VDXFilterActivation *)static_cast<const VDFilterActivationImpl *>(this); }
	VDXFilterActivation *AsVDXFilterActivation() { return (VDXFilterActivation *)static_cast<VDFilterActivationImpl *>(this); }

	class VideoPrefetcher;
	void GetPrefetchInfo(sint64 frame, VideoPrefetcher& prefetcher, bool requireOutput) const;

	void SetLogicErrorF(const char *format, ...) const;

	static void ConvertParameters(VDXScriptValue *dst, const VDScriptValue *src, int argc);
	static void ConvertValue(VDScriptValue& dst, const VDXScriptValue& src);
	static void ScriptFunctionThunkVoid(IVDScriptInterpreter *, VDScriptValue *, int);
	static void ScriptFunctionThunkInt(IVDScriptInterpreter *, VDScriptValue *, int);
	static void ScriptFunctionThunkVariadic(IVDScriptInterpreter *, VDScriptValue *, int);

public:
	VDFileMappingW32	mFileMapping;
	VFBitmapInternal	mRealSrcUncropped;
	VFBitmapInternal	mRealSrc;
	VFBitmapInternal	mRealDst;
	VFBitmapInternal	mRealLast;
	VFBitmapInternal	mExternalSrc;
	VFBitmapInternal	mExternalDst;

	bool	mbBlitOnEntry;
	bool	mbAlignOnEntry;
	bool	mbConvertOnEntry;
	int		mBlendBuffer;

	int		mSrcBuf;
	int		mDstBuf;

	VDPixmap	mBlendPixmap;

protected:
	bool	mbInvalidFormat;
	bool	mbInvalidFormatHandling;

	int		mOrigW;
	int		mOrigH;

	bool	mbPreciseCrop;
	int		mCropX1;
	int		mCropY1;
	int		mCropX2;
	int		mCropY2;

	struct DelayInfo {
		sint64	mSourceFrame;
		sint64	mOutputFrame;
	};

	vdfastvector<DelayInfo>	mFsiDelayRing;
	uint32		mDelayRingPos;
	VDXFilterStateInfo mfsi;

	VDScriptObject	mScriptObj;

	uint32		mFlags;
	uint32		mLag;

	VDPosition	mLastResultFrame;

	bool		mbEnabled;
	bool		mbStarted;
	bool		mbFirstFrame;
	bool		mbCanStealSourceBuffer;

	VDStringW	mFilterName;

	FilterInstanceAutoDeinit	*mpAutoDeinit;
	mutable VDAtomicPtr<VDFilterFrameRequestError> mpLogicError;

	vdfastvector<VDScriptFunctionDef>	mScriptFunc;
	std::vector<VFBitmapInternal> mSourceFrames;
	vdfastvector<VDXFBitmap *> mSourceFrameArray;

	FilterDefinitionInstance *mpFDInst;

	vdrefptr<VDParameterCurve> mpAlphaCurve;

	vdrefptr<IVDFilterFrameSource> mpSource;
	vdrefptr<VDFilterFrameAllocator> mpSourceAllocator;
	vdrefptr<VDFilterFrameAllocator> mpResultAllocator;

	VDFilterFrameQueue		mFrameQueueWaiting;
	VDFilterFrameQueue		mFrameQueueInProgress;
	VDFilterFrameCache		mFrameCache;
	VDFilterFrameSharingPredictor	mSharingPredictor;

	vdautoptr<IVDPixmapBlitter>	mpSourceConversionBlitter;

	VDFilterThreadContext	mThreadContext;
};

#endif
