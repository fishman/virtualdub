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

#ifndef f_FILTERS_H
#define f_FILTERS_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <malloc.h>

#include <windows.h>

#include <list>
#include <vector>
#include <vd2/system/list.h>
#include <vd2/system/error.h>
#include <vd2/system/VDString.h>
#include <vd2/system/refcount.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/VDLib/win32/DIBSection.h>
#include <vd2/VDLib/win32/FileMapping.h>
#include <vd2/VDLib/ParameterCurve.h>
#include "VBitmap.h"
#include "FilterSystem.h"
#include "filter.h"
#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "gui.h"

//////////////////

class IVDVideoDisplay;
class IVDPositionControl;
struct VDWaveFormat;
class VDTimeline;
struct VDXWaveFormat;
struct VDXFilterDefinition;

///////////////////

VDXWaveFormat *VDXCopyWaveFormat(const VDXWaveFormat *pFormat);

///////////////////

class FilterDefinitionInstance;

class VFBitmapInternal : public VBitmap {
public:
	VFBitmapInternal();
	VFBitmapInternal(const VFBitmapInternal&);
	~VFBitmapInternal();

	VFBitmapInternal& operator=(const VFBitmapInternal&);

	void Fixup(void *base);
	void ConvertBitmapLayoutToPixmapLayout();
	void ConvertPixmapLayoutToBitmapLayout();
	void ConvertPixmapToBitmap();

public:
	// Must match layout of VFBitmap!
	enum {
		NEEDS_HDC		= 0x00000001L,
	};

	DWORD	dwFlags;
	HDC		hdc;

	uint32	mFrameRateHi;
	uint32	mFrameRateLo;
	sint64	mFrameCount;

	VDXPixmapLayout	*mpPixmapLayout;
	const VDXPixmap	*mpPixmap;			

public:
	VDDIBSectionW32	mDIBSection;
	VDPixmap		mPixmap;
	VDPixmapLayout	mPixmapLayout;
};

class FilterInstanceAutoDeinit;

struct VDFilterThreadContext {
	int					tmp[16];
	void				*ESPsave;
};

class VDFilterActivationImpl {		// clone of VDXFilterActivation
public:
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

	VDFilterActivationImpl(VDXFBitmap& _dst, VDXFBitmap& _src, VDXFBitmap *_last);
	VDFilterActivationImpl(const VDFilterActivationImpl& fa, VDXFBitmap& _dst, VDXFBitmap& _src, VDXFBitmap *_last);
};

class FilterInstance : public ListNode, public VDFilterActivationImpl {
	FilterInstance& operator=(const FilterInstance&);		// outlaw copy assignment

public:
	FilterInstance(const FilterInstance& fi);
	FilterInstance(FilterDefinitionInstance *);
	~FilterInstance();

	FilterInstance *Clone();
	void Destroy();

	bool	IsEnabled() const { return mbEnabled; }
	void	SetEnabled(bool enabled) { mbEnabled = enabled; }

	bool	IsConversionRequired() const { return mbBlitOnEntry; }

	uint32	GetFlags() const { return mFlags; }
	uint32	GetFrameDelay() const { return mFlags >> 16; }

	uint32	Prepare(const VFBitmapInternal& input);

	void	Start(int accumulatedDelay);
	void	Stop();

	void	Run(bool firstFrame, sint64 sourceFrame, sint64 outputFrame, sint64 timelineFrame, sint64 sequenceFrame, sint64 sequenceFrameMS, uint32 flags);

	const VDXFilterActivation *AsVDXFilterActivation() const { return (const VDXFilterActivation *)static_cast<const VDFilterActivationImpl *>(this); }
	VDXFilterActivation *AsVDXFilterActivation() { return (VDXFilterActivation *)static_cast<VDFilterActivationImpl *>(this); }

	sint64	GetSourceFrame(sint64 frame) const;

	uint32		GetSourceFrameWidth()	const { return mRealSrc.w; }
	uint32		GetSourceFrameHeight()	const { return mRealSrc.h; }
	VDFraction	GetSourceFrameRate()	const { return VDFraction(mRealSrc.mFrameRateHi, mRealSrc.mFrameRateLo); }
	sint64		GetSourceFrameCount()	const { return mRealSrc.mFrameCount; }

	uint32		GetOutputFrameWidth()	const { return mRealDst.w; }
	uint32		GetOutputFrameHeight()	const { return mRealDst.h; }
	VDFraction	GetOutputFrameRate()	const { return VDFraction(mRealDst.mFrameRateHi, mRealDst.mFrameRateLo); }
	sint64		GetOutputFrameCount()	const { return mRealDst.mFrameCount; }

	sint64		GetLastSourceFrame()	const { return mfsi.lCurrentSourceFrame; }
	sint64		GetLastOutputFrame()	const { return mfsi.lCurrentFrame; }

	VDParameterCurve *GetAlphaParameterCurve() const { return mpAlphaCurve; }
	void SetAlphaParameterCurve(VDParameterCurve *p) { mpAlphaCurve = p; }

protected:
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
	VDPixmap			mExternalSrc;
	VDPixmap			mExternalDst;

	bool	mbInvalidFormat;
	bool	mbInvalidFormatHandling;
	bool	mbBlitOnEntry;
	int		mBlendBuffer;

	int		mSrcBuf;
	int		mDstBuf;
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
		sint64	mTimelineFrame;
	};

	vdfastvector<DelayInfo>	mFsiDelayRing;
	uint32		mDelayRingPos;
	VDXFilterStateInfo mfsi;

	VDPixmap	mBlendPixmap;
	VDScriptObject	mScriptObj;

protected:
	uint32		mFlags;
	bool		mbEnabled;
	bool		mbStarted;

	VDStringW	mFilterName;

	FilterInstanceAutoDeinit	*mpAutoDeinit;

	std::vector<VDScriptFunctionDef>	mScriptFunc;

	FilterDefinitionInstance *mpFDInst;

	vdrefptr<VDParameterCurve> mpAlphaCurve;

	VDFilterThreadContext	mThreadContext;
};

//////////

extern List			g_listFA;

extern FilterSystem	filters;

VDXFilterDefinition *FilterAdd(VDXFilterModule *fm, VDXFilterDefinition *pfd, int fd_len);
void				FilterAddBuiltin(const VDXFilterDefinition *pfd);
void				FilterRemove(VDXFilterDefinition *fd);

struct FilterBlurb {
	FilterDefinitionInstance	*key;
	VDStringA					name;
	VDStringA					author;
	VDStringA					description;
};

void				FilterEnumerateFilters(std::list<FilterBlurb>& blurbs);


LONG FilterGetSingleValue(HWND hWnd, LONG cVal, LONG lMin, LONG lMax, char *title, IVDXFilterPreview *ifp, void (*pUpdateFunction)(long value, void *data), void *pUpdateFunctionData);

#endif
