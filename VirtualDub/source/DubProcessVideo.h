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

#ifndef f_VD2_DUBPROCESSVIDEO_H
#define f_VD2_DUBPROCESSVIDEO_H

#include <vd2/Kasumi/blitter.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/videocodec.h>

class DubOptions;
struct VDRenderVideoPipeFrameInfo;
class VDRenderOutputBuffer;
class IVDAsyncBlitter;
class IDubStatusHandler;
class DubVideoStreamInfo;
class VDDubFrameRequestQueue;
class VDRenderFrameMap;
class VDThreadedVideoCompressor;
class IVDMediaOutputStream;
class VDRTProfileChannel;
class VDLoopThrottle;
class VDRenderOutputBufferTracker;
class IVDVideoDisplay;
class IVDVideoSource;
struct VDPixmapLayout;
class VDFilterFrameManualSource;
class IVDVideoCompressor;
class FilterSystem;
class AVIPipe;
class VDFilterFrameRequest;
class IVDFilterFrameClientRequest;

class IVDDubVideoProcessorCallback {
public:
	virtual void OnFirstFrameWritten() = 0;
	virtual void OnVideoStreamEnded() = 0;
};

class VDDubVideoProcessor {
	VDDubVideoProcessor(const VDDubVideoProcessor&);
	VDDubVideoProcessor& operator=(const VDDubVideoProcessor&);
public:
	enum VideoWriteResult {
		kVideoWriteOK,							// Frame was processed and written
		kVideoWriteDelayed,						// Codec received intermediate frame; no output.
		kVideoWriteNoOutput,					// No output.
		kVideoWriteDiscarded,					// Frame was discarded by preview QC.
		kVideoWritePullOnly						// Codec produced frame and didn't take input.
	};

	VDDubVideoProcessor();
	~VDDubVideoProcessor();

	void SetCallback(IVDDubVideoProcessorCallback *cb);
	void SetStatusHandler(IDubStatusHandler *handler);
	void SetOptions(const DubOptions *opts);
	void SetThreadSignals(VDAtomicInt *flag, const char *volatile *pStatus, VDRTProfileChannel *pProfileChannel, VDLoopThrottle *pLoopThrottle);
	void SetVideoStreamInfo(DubVideoStreamInfo *vinfo);
	void SetPreview(bool preview);
	void SetInputDisplay(IVDVideoDisplay *pVideoDisplay);
	void SetOutputDisplay(IVDVideoDisplay *pVideoDisplay);
	void SetVideoFilterOutput(const VDPixmapLayout& layout);
	void SetBlitter(IVDAsyncBlitter *blitter);
	void SetVideoSources(IVDVideoSource *const *pVideoSources, uint32 count);
	void SetVideoFrameSource(VDFilterFrameManualSource *fs);
	void SetVideoCompressor(IVDVideoCompressor *pCompressor, int threadCount);
	void SetVideoFrameMap(const VDRenderFrameMap *frameMap);
	void SetVideoRequestQueue(VDDubFrameRequestQueue *q);
	void SetVideoFilterSystem(FilterSystem *fs);
	void SetVideoPipe(AVIPipe *pipe);
	void SetVideoOutput(IVDMediaOutputStream *out);

	void Init();
	void Shutdown();

	bool IsCompleted() const;

	void UpdateFrames();

	bool WriteVideo();
	void CheckForDecompressorSwitch();

protected:
	void NotifyDroppedFrame(int exdata);
	void NotifyCompletedFrame(uint32 size, bool isKey);

	bool RequestNextVideoFrame();
	bool DoVideoFrameDropTest(const VDRenderVideoPipeFrameInfo& frameInfo);
	VideoWriteResult ReadVideoFrame(const VDRenderVideoPipeFrameInfo& frameInfo);
	VideoWriteResult DecodeVideoFrame(const VDRenderVideoPipeFrameInfo& frameInfo);
	VideoWriteResult GetVideoOutputBuffer(VDRenderOutputBuffer **ppBuffer);
	VideoWriteResult ProcessVideoFrame();
	VideoWriteResult WriteFinishedVideoFrame(VDRenderOutputBuffer *pBuffer, bool holdFrame);
	void WriteNullVideoFrame();
	void WriteFinishedVideoFrame(const void *data, uint32 size, bool isKey, bool renderEnabled, VDRenderOutputBuffer *pBuffer);

	static bool AsyncReinitDisplayCallback(int pass, void *pThisAsVoid, void *, bool aborting);
	static bool StaticAsyncUpdateOutputCallback(int pass, void *pThisAsVoid, void *pBuffer, bool aborting);
	bool AsyncUpdateOutputCallback(int pass, VDRenderOutputBuffer *pBuffer, bool aborting);

	// status
	const DubOptions	*mpOptions;
	IDubStatusHandler	*mpStatusHandler;
	DubVideoStreamInfo	*mpVInfo;
	bool				mbVideoPushEnded;
	bool				mbVideoEnded;
	bool				mbPreview;
	bool				mbFirstFrame;
	bool				mbInputLocked;
	VDAtomicInt			*mpAbort;
	const char			*volatile *mppCurrentAction;
	IVDDubVideoProcessorCallback	*mpCB;

	// video sourcing
	const VDRenderFrameMap	*mpVideoFrameMap;
	VDFilterFrameManualSource	*mpVideoFrameSource;
	VDDubFrameRequestQueue	*mpVideoRequestQueue;
	AVIPipe					*mpVideoPipe;
	vdautoptr<IVDPixmapBlitter>	mpInputBlitter;

	// video decompression
	typedef vdfastvector<IVDVideoSource *> VideoSources;
	VideoSources		mVideoSources;

	// video filtering
	FilterSystem		*mpVideoFilters;

	// video output conversion
	vdautoptr<IVDPixmapBlitter>	mpOutputBlitter;

	// video compression
	vdrefptr<VDRenderOutputBuffer> mpHeldCompressionInputBuffer;
	IVDVideoCompressor	*mpVideoCompressor;
	VDThreadedVideoCompressor *mpThreadedVideoCompressor;
	uint32				mVideoFramesDelayed;
	bool				mbFlushingCompressor;

	// video output
	IVDMediaOutputStream	*mpVideoOut;			// alias: AVIout->videoOut

	struct SourceFrameEntry {
		VDFilterFrameRequest *mpRequest;
		VDPosition mSourceFrame;
	};

	typedef vdfastdeque<SourceFrameEntry> PendingSourceFrames;
	PendingSourceFrames	mPendingSourceFrames;

	struct OutputFrameEntry {
		IVDFilterFrameClientRequest *mpRequest;
		VDPosition mTimelineFrame;
		bool mbHoldFrame;
		bool mbNullFrame;
	};

	typedef vdfastdeque<OutputFrameEntry> PendingOutputFrames;
	PendingOutputFrames	mPendingOutputFrames;

	VDRTProfileChannel	*mpProcessingProfileChannel;
	VDLoopThrottle		*mpLoopThrottle;

	// DISPLAY
	uint32				mFramesToDrop;
	IVDAsyncBlitter		*mpBlitter;
	IVDVideoDisplay		*mpInputDisplay;
	IVDVideoDisplay		*mpOutputDisplay;
	VDRenderOutputBufferTracker *mpFrameBufferTracker;
	VDRenderOutputBufferTracker *mpDisplayBufferTracker;
	VDAtomicInt			mRefreshFlag;

	// DECOMPRESSION PREVIEW
	vdautoptr<IVDVideoDecompressor>	mpVideoDecompressor;
	bool				mbVideoDecompressorEnabled;
	bool				mbVideoDecompressorPending;
	bool				mbVideoDecompressorErrored;
	VDPixmapBuffer		mVideoDecompBuffer;
};

#endif	// f_VD2_DUBPROCESSVIDEO_H
