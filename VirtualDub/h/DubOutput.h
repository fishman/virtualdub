//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#ifndef f_DUBOUTPUT_H
#define f_DUBOUTPUT_H

#include <vector>
#include <vd2/system/VDString.h>
#include <vd2/system/vdalloc.h>
#include "AVIStripeSystem.h"
#include "fixes.h"

class IVDMediaOutput;

class VDINTERFACE IVDDubberOutputSystem {
public:
	virtual IVDMediaOutput *CreateSegment() = 0;
	virtual void CloseSegment(IVDMediaOutput *pSegment, bool bLast) = 0;
	virtual void SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) = 0;
	virtual void SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr) = 0;
	virtual bool AcceptsVideo() = 0;
	virtual bool AcceptsAudio() = 0;
	virtual bool IsRealTime() = 0;
};

class VDAVIOutputFileSystem : public IVDDubberOutputSystem {
public:
	VDAVIOutputFileSystem();
	~VDAVIOutputFileSystem();

	void SetCaching(bool bAllowOSCaching);
	void SetIndexing(bool bAllowHierarchicalExtensions);
	void Set1GBLimit(bool bUse1GBLimit);
	void SetBuffer(int bufferSize);
	void SetTextInfo(const std::list<std::pair<uint32, VDStringA> >& info);

	void SetFilename(const wchar_t *pszFilename);
	void SetFilenamePattern(const wchar_t *pszSegmentPrefix, const wchar_t *pszExt, int nMinimumDigits);

	IVDMediaOutput *CreateSegment();
	void CloseSegment(IVDMediaOutput *pSegment, bool bLast);
	void SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat);
	void SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr);
	bool AcceptsVideo();
	bool AcceptsAudio();
	bool IsRealTime() { return false; }

private:
	VDStringW	mSegmentBaseName;
	VDStringW	mSegmentExt;
	int			mSegmentDigits;
	int			mCurrentSegment;
	int			mBufferSize;
	int			mAlignment;
	bool		mbInterleaved;
	bool		mbAllowCaching;
	bool		mbAllowIndexing;
	bool		mbUse1GBLimit;

	typedef std::list<std::pair<uint32, VDStringA> > tTextInfo;
	tTextInfo	mTextInfo;

	AVIStreamHeader_fixed	mVideoStreamInfo;
	std::vector<char>		mVideoFormat;
	AVIStreamHeader_fixed	mAudioStreamInfo;
	std::vector<char>		mAudioFormat;
};

class VDAVIOutputStripedSystem : public IVDDubberOutputSystem {
public:
	VDAVIOutputStripedSystem(const wchar_t *pszFilename);
	~VDAVIOutputStripedSystem();

	void Set1GBLimit(bool bUse1GBLimit);

	IVDMediaOutput *CreateSegment();
	void CloseSegment(IVDMediaOutput *pSegment, bool bLast);
	void SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat);
	void SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr);
	bool AcceptsVideo();
	bool AcceptsAudio();
	bool IsRealTime() { return false; }

private:
	bool		mbUse1GBLimit;

	AVIStreamHeader_fixed	mVideoStreamInfo;
	std::vector<char>		mVideoFormat;
	AVIStreamHeader_fixed	mAudioStreamInfo;
	std::vector<char>		mAudioFormat;

	vdautoptr<AVIStripeSystem>	mpStripeSystem;
};

class VDAVIOutputWAVSystem : public IVDDubberOutputSystem {
public:
	VDAVIOutputWAVSystem(const wchar_t *pszFilename);
	~VDAVIOutputWAVSystem();

	void SetBuffer(int size);
	IVDMediaOutput *CreateSegment();
	void CloseSegment(IVDMediaOutput *pSegment, bool bLast);
	void SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat);
	void SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr);
	bool AcceptsVideo();
	bool AcceptsAudio();
	bool IsRealTime() { return false; }

private:
	VDStringW	mFilename;
	int			mBufferSize;

	AVIStreamHeader_fixed	mAudioStreamInfo;
	std::vector<char>		mAudioFormat;
};

class VDAVIOutputRawSystem : public IVDDubberOutputSystem {
public:
	VDAVIOutputRawSystem(const wchar_t *pszFilename);
	~VDAVIOutputRawSystem();

	void SetBuffer(int size);
	IVDMediaOutput *CreateSegment();
	void CloseSegment(IVDMediaOutput *pSegment, bool bLast);
	void SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat);
	void SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr);
	bool AcceptsVideo();
	bool AcceptsAudio();
	bool IsRealTime() { return false; }

private:
	VDStringW	mFilename;
	int			mBufferSize;

	AVIStreamHeader_fixed	mAudioStreamInfo;
	std::vector<char>		mAudioFormat;
};

class VDAVIOutputImagesSystem : public IVDDubberOutputSystem {
public:
	VDAVIOutputImagesSystem();
	~VDAVIOutputImagesSystem();

	void SetFilenamePattern(const wchar_t *pszSegmentPrefix, const wchar_t *pszSegmentSuffix, int nMinimumDigits);
	void SetFormat(int format, int quality);

	IVDMediaOutput *CreateSegment();
	void CloseSegment(IVDMediaOutput *pSegment, bool bLast);
	void SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat);
	void SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr);
	bool AcceptsVideo();
	bool AcceptsAudio();
	bool IsRealTime() { return false; }

private:
	VDStringW	mSegmentPrefix;
	VDStringW	mSegmentSuffix;
	int			mSegmentDigits;
	int			mFormat;			// from AVIOutputImages
	int			mQuality;

	AVIStreamHeader_fixed	mVideoStreamInfo;
	std::vector<char>		mVideoFormat;
	AVIStreamHeader_fixed	mAudioStreamInfo;
	std::vector<char>		mAudioFormat;
};

class VDAVIOutputFilmstripSystem : public IVDDubberOutputSystem {
public:
	VDAVIOutputFilmstripSystem(const wchar_t *filename);
	~VDAVIOutputFilmstripSystem();

	IVDMediaOutput *CreateSegment();
	void CloseSegment(IVDMediaOutput *pSegment, bool bLast);
	void SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat);
	void SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr);
	bool AcceptsVideo();
	bool AcceptsAudio();
	bool IsRealTime() { return false; }

private:
	VDStringW	mFilename;

	AVIStreamHeader_fixed	mVideoStreamInfo;
	std::vector<char>		mVideoFormat;
};

class VDAVIOutputGIFSystem : public IVDDubberOutputSystem {
public:
	VDAVIOutputGIFSystem(const wchar_t *filename);
	~VDAVIOutputGIFSystem();

	IVDMediaOutput *CreateSegment();
	void CloseSegment(IVDMediaOutput *pSegment, bool bLast);
	void SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat);
	void SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr);
	bool AcceptsVideo();
	bool AcceptsAudio();
	bool IsRealTime() { return false; }

	void SetLoopCount(int loopCount) { mLoopCount = loopCount; }

private:
	VDStringW	mFilename;
	int			mLoopCount;

	AVIStreamHeader_fixed	mVideoStreamInfo;
	std::vector<char>		mVideoFormat;
};

class VDAVIOutputPreviewSystem : public IVDDubberOutputSystem {
public:
	VDAVIOutputPreviewSystem();
	~VDAVIOutputPreviewSystem();

	IVDMediaOutput *CreateSegment();
	void CloseSegment(IVDMediaOutput *pSegment, bool bLast);
	void SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat);
	void SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr);
	bool AcceptsVideo();
	bool AcceptsAudio();
	bool IsRealTime() { return true; }

private:
	AVIStreamHeader_fixed	mVideoStreamInfo;
	vdfastvector<char>		mVideoFormat;
	AVIStreamHeader_fixed	mAudioStreamInfo;
	vdfastvector<char>		mAudioFormat;
	bool					mbAudioVBR;
};

class VDAVIOutputNullVideoSystem : public IVDDubberOutputSystem {
public:
	VDAVIOutputNullVideoSystem();
	~VDAVIOutputNullVideoSystem();

	IVDMediaOutput *CreateSegment();
	void CloseSegment(IVDMediaOutput *pSegment, bool bLast);
	void SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat);
	void SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr);
	bool AcceptsVideo();
	bool AcceptsAudio();
	bool IsRealTime() { return false; }

private:
	AVIStreamHeader_fixed	mVideoStreamInfo;
	vdfastvector<char>		mVideoFormat;
	AVIStreamHeader_fixed	mAudioStreamInfo;
	vdfastvector<char>		mAudioFormat;
};

class VDAVIOutputSegmentedSystem : public IVDDubberOutputSystem {
public:
	VDAVIOutputSegmentedSystem(IVDDubberOutputSystem *pChildSystem, bool intervalIsSeconds, double interval, double preloadInSeconds, sint64 max_bytes, sint64 max_frames);
	~VDAVIOutputSegmentedSystem();

	IVDMediaOutput *CreateSegment();
	void CloseSegment(IVDMediaOutput *pSegment, bool bLast);
	void SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat);
	void SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved, bool vbr);
	bool AcceptsVideo();
	bool AcceptsAudio();
	bool IsRealTime() { return false; }

private:
	IVDDubberOutputSystem *mpChildSystem;
	bool					mbIntervalIsSeconds;
	double					mInterval;
	double					mPreload;
	sint64					mMaxBytes;
	sint64					mMaxFrames;

	AVIStreamHeader_fixed	mVideoStreamInfo;
	vdfastvector<char>		mVideoFormat;
	AVIStreamHeader_fixed	mAudioStreamInfo;
	vdfastvector<char>		mAudioFormat;
};

#endif
