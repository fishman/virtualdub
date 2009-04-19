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
#include <vd2/system/log.h>
#include <vd2/system/profile.h>
#include <vd2/system/time.h>
#include <vd2/Dita/resources.h>
#include <vd2/Riza/bitmap.h>
#include "AsyncBlitter.h"
#include "AVIPipe.h"
#include "AVIOutput.h"
#include "Dub.h"
#include "DubIO.h"
#include "DubProcessVideo.h"
#include "DubStatus.h"
#include "FilterFrameRequest.h"
#include "FilterFrameManualSource.h"
#include "FilterSystem.h"
#include "prefs.h"
#include "ThreadedVideoCompressor.h"
#include "VideoSource.h"

using namespace nsVDDub;

bool VDPreferencesIsPreferInternalVideoDecodersEnabled();
IVDVideoDecompressor *VDFindVideoDecompressorEx(uint32 fccHandler, const VDAVIBitmapInfoHeader *hdr, uint32 hdrlen, bool preferInternal);

namespace {
	enum { kVDST_Dub = 1 };

	enum {
		kVDM_SegmentOverflowOccurred,
		kVDM_BeginningNextSegment,
		kVDM_IOThreadLivelock,
		kVDM_ProcessingThreadLivelock,
		kVDM_CodecLoopingDuringDelayedFlush,
		kVDM_FastRecompressUsingFormat,
		kVDM_SlowRecompressUsingFormat,
		kVDM_FullUsingInputFormat,
		kVDM_FullUsingOutputFormat
	};

	enum {
		BUFFERID_INPUT = 1,
		BUFFERID_OUTPUT = 2
	};

	enum {
		// This is to work around an XviD decode bug (see VideoSource.h).
		kDecodeOverflowWorkaroundSize = 16,

		kReasonableBFrameBufferLimit = 100
	};
}

///////////////////////////////////////////////////////////////////////////

namespace {
	bool AsyncUpdateCallback(int pass, void *pDisplayAsVoid, void *pInterlaced, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;
		int nFieldMode = *(int *)pInterlaced;

		uint32 baseFlags = IVDVideoDisplay::kVisibleOnly;

		if (g_prefs.fDisplay & Preferences::kDisplayEnableVSync)
			baseFlags |= IVDVideoDisplay::kVSync;

		if (nFieldMode) {
			if ((nFieldMode - 1) & 1) {
				if (pass)
					pVideoDisplay->Update(IVDVideoDisplay::kEvenFieldOnly | baseFlags);
				else
					pVideoDisplay->Update(IVDVideoDisplay::kOddFieldOnly | IVDVideoDisplay::kFirstField | baseFlags);
			} else {
				if (pass)
					pVideoDisplay->Update(IVDVideoDisplay::kOddFieldOnly | baseFlags);
				else
					pVideoDisplay->Update(IVDVideoDisplay::kEvenFieldOnly | IVDVideoDisplay::kFirstField | baseFlags);
			}

			return !pass;
		} else {
			pVideoDisplay->Update(IVDVideoDisplay::kAllFields | baseFlags);
			return false;
		}
	}

	bool AsyncDecompressorFailedCallback(int pass, void *pDisplayAsVoid, void *, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;

		pVideoDisplay->SetSourceMessage(L"Unable to display compressed video: no decompressor is available to decode the compressed video.");
		return false;
	}

	bool AsyncDecompressorSuccessfulCallback(int pass, void *pDisplayAsVoid, void *, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;

		pVideoDisplay->SetSourceMessage(L"Compressed video preview is enabled.\n\nPreview will resume starting with the next key frame.");
		return false;
	}

	bool AsyncDecompressorErrorCallback(int pass, void *pDisplayAsVoid, void *, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;

		pVideoDisplay->SetSourceMessage(L"Unable to display compressed video: An error has occurred during decompression.");
		return false;
	}

	bool AsyncDecompressorUpdateCallback(int pass, void *pDisplayAsVoid, void *pPixmapAsVoid, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;
		VDPixmap *pPixmap = (VDPixmap *)pPixmapAsVoid;

		pVideoDisplay->SetSource(true, *pPixmap);
		return false;
	}
}

VDDubVideoProcessor::VDDubVideoProcessor()
	: mpOptions(NULL)
	, mpStatusHandler(NULL)
	, mpVInfo(NULL)
	, mbVideoPushEnded(false)
	, mbVideoEnded(false)
	, mbPreview(false)
	, mbFirstFrame(true)
	, mbInputLocked(false)
	, mpAbort(NULL)
	, mppCurrentAction(NULL)
	, mpCB(NULL)
	, mpVideoFrameMap(NULL)
	, mpVideoFrameSource(NULL)
	, mpVideoRequestQueue(NULL)
	, mpVideoPipe(NULL)
	, mpVideoFilters(NULL)
	, mpVideoCompressor(NULL)
	, mpThreadedVideoCompressor(NULL)
	, mVideoFramesDelayed(0)
	, mbFlushingCompressor(false)
	, mpVideoOut(NULL)
	, mpProcessingProfileChannel(NULL)
	, mpLoopThrottle(NULL)
	, mFramesToDrop(0)
	, mpBlitter(NULL)
	, mpInputDisplay(NULL)
	, mpOutputDisplay(NULL)
	, mpFrameBufferTracker(NULL)
	, mpDisplayBufferTracker(NULL)
	, mRefreshFlag(true)
	, mbVideoDecompressorEnabled(false)
	, mbVideoDecompressorPending(false)
	, mbVideoDecompressorErrored(false)
{
}

VDDubVideoProcessor::~VDDubVideoProcessor() {
	if (mpDisplayBufferTracker) {
		mpDisplayBufferTracker->Release();
		mpDisplayBufferTracker = NULL;
	}
}

void VDDubVideoProcessor::SetCallback(IVDDubVideoProcessorCallback *cb) {
	mpCB = cb;
}

void VDDubVideoProcessor::SetStatusHandler(IDubStatusHandler *handler) {
	mpStatusHandler = handler;
}

void VDDubVideoProcessor::SetOptions(const DubOptions *opts) {
	mpOptions = opts;
}

void VDDubVideoProcessor::SetThreadSignals(VDAtomicInt *flag, const char *volatile *pStatus, VDRTProfileChannel *pProfileChannel, VDLoopThrottle *pLoopThrottle) {
	mpAbort = flag;
	mppCurrentAction = pStatus;
	mpProcessingProfileChannel = pProfileChannel;
	mpLoopThrottle = pLoopThrottle;
}

void VDDubVideoProcessor::SetVideoStreamInfo(DubVideoStreamInfo *vinfo) {
	mpVInfo = vinfo;
}

void VDDubVideoProcessor::SetPreview(bool preview) {
	mbPreview = preview;
}

void VDDubVideoProcessor::SetInputDisplay(IVDVideoDisplay *pVideoDisplay) {
	mpInputDisplay = pVideoDisplay;
}

void VDDubVideoProcessor::SetOutputDisplay(IVDVideoDisplay *pVideoDisplay) {
	mpOutputDisplay = pVideoDisplay;
}

void VDDubVideoProcessor::SetVideoFilterOutput(const VDPixmapLayout& layout) {
	VDASSERT(!mpDisplayBufferTracker);
	mpDisplayBufferTracker = new VDRenderOutputBufferTracker;
	mpDisplayBufferTracker->AddRef();

	mpDisplayBufferTracker->Init(3, layout);
}

void VDDubVideoProcessor::SetBlitter(IVDAsyncBlitter *blitter) {
	mpBlitter = blitter;
}

void VDDubVideoProcessor::SetVideoSources(IVDVideoSource *const *pVideoSources, uint32 count) {
	mVideoSources.assign(pVideoSources, pVideoSources + count);
}

void VDDubVideoProcessor::SetVideoFrameSource(VDFilterFrameManualSource *fs) {
	mpVideoFrameSource = fs;
}

void VDDubVideoProcessor::SetVideoCompressor(IVDVideoCompressor *pCompressor, int threadCount) {
	VDASSERT(!mpThreadedVideoCompressor);

	mpVideoCompressor = pCompressor;

	if (pCompressor) {
		if (threadCount > 1)
			threadCount = 1;

		mpThreadedVideoCompressor = new VDThreadedVideoCompressor;
		mpThreadedVideoCompressor->Init(threadCount, pCompressor);
	}
}

void VDDubVideoProcessor::SetVideoRequestQueue(VDDubFrameRequestQueue *q) {
	mpVideoRequestQueue = q;
}

void VDDubVideoProcessor::SetVideoFrameMap(const VDRenderFrameMap *frameMap) {
	mpVideoFrameMap = frameMap;
}

void VDDubVideoProcessor::SetVideoFilterSystem(FilterSystem *fs) {
	mpVideoFilters = fs;
}

void VDDubVideoProcessor::SetVideoPipe(AVIPipe *pipe) {
	mpVideoPipe = pipe;
}

void VDDubVideoProcessor::SetVideoOutput(IVDMediaOutputStream *out) {
	mpVideoOut = out;
}

void VDDubVideoProcessor::Init() {
	if (!mpDisplayBufferTracker && !mpFrameBufferTracker) {
		mpFrameBufferTracker = new VDRenderOutputBufferTracker;
		mpFrameBufferTracker->AddRef();

		IVDVideoSource *vs = mVideoSources[0];
		mpFrameBufferTracker->Init((void *)vs->getFrameBuffer(), vs->getTargetFormat());
	}
}

void VDDubVideoProcessor::Shutdown() {
	while(!mPendingSourceFrames.empty()) {
		SourceFrameEntry& srcEnt = mPendingSourceFrames.front();

		if (srcEnt.mpRequest)
			srcEnt.mpRequest->Release();

		mPendingSourceFrames.pop_front();
	}

	while(!mPendingOutputFrames.empty()) {
		OutputFrameEntry& outEnt = mPendingOutputFrames.front();

		if (outEnt.mpRequest)
			outEnt.mpRequest->Release();

		mPendingOutputFrames.pop_front();
	}

	if (mpThreadedVideoCompressor) {
		mpThreadedVideoCompressor->Shutdown();
		delete mpThreadedVideoCompressor;
		mpThreadedVideoCompressor = NULL;
	}

	if (mpDisplayBufferTracker) {
		mpDisplayBufferTracker->Shutdown();
		mpDisplayBufferTracker->Release();
		mpDisplayBufferTracker = NULL;
	}

	if (mpFrameBufferTracker) {
		mpFrameBufferTracker->Shutdown();
		mpFrameBufferTracker->Release();
		mpFrameBufferTracker = NULL;
	}
}

bool VDDubVideoProcessor::IsCompleted() const {
	return mbVideoEnded;
}

void VDDubVideoProcessor::UpdateFrames() {
	mRefreshFlag = 1;
}

bool VDDubVideoProcessor::WriteVideo() {
	// We cannot wrap the entire loop with a profiling event because typically
	// involves a nice wait in preview mode.
	for(;;) {
		if (*mpAbort)
			return false;

		// Try to pull out a video frame.
		VideoWriteResult result = ProcessVideoFrame();

		if (result == kVideoWriteDelayed) {
			++mVideoFramesDelayed;
			continue;
		}

		if (result == kVideoWriteDiscarded || result == kVideoWriteOK || result == kVideoWritePullOnly) {
			if (result == kVideoWritePullOnly) {
				VDVERIFY((sint32)--mVideoFramesDelayed >= 0);

				if (mbFlushingCompressor && !mVideoFramesDelayed) {
					mbFlushingCompressor = false;
					if (mpThreadedVideoCompressor)
						mpThreadedVideoCompressor->SetFlush(false);
				}
			}
			break;
		}

		// Try to request new frames.
		const uint32 vpsize = mpVideoPipe->size();
		while(mpVideoRequestQueue->GetQueueLength() < vpsize && mPendingOutputFrames.size() < vpsize) {
			if (!RequestNextVideoFrame())
				break;
		}

		// Drop out frames if we're previewing, behind, and dropping is enabled.
		if (mbPreview && mpOptions->perf.fDropFrames) {
			PendingOutputFrames::iterator it(mPendingOutputFrames.begin()), itEnd(mPendingOutputFrames.end());
			if (it != itEnd) {
				++it;

				uint32 clock = mpBlitter->getPulseClock();
				for(; it != itEnd; ++it) {
					OutputFrameEntry& ofe = *it;

					if (!ofe.mbNullFrame) {
						sint32 delta = (sint32)((uint32)ofe.mTimelineFrame*2 - clock);

						if (delta < 0) {
							ofe.mbNullFrame = true;
							ofe.mbHoldFrame = true;

							if (ofe.mpRequest) {
								ofe.mpRequest->Release();
								ofe.mpRequest = NULL;
							}
						}
					}
				}
			}
		}

		// Try to push in a source frame.
		const VDRenderVideoPipeFrameInfo *pFrameInfo = NULL;
		if (mbFlushingCompressor) {
			result = WriteFinishedVideoFrame(mpHeldCompressionInputBuffer, true);

			if (result == kVideoWriteDelayed) {
				// DivX 5.0.5 seems to have a bug where in the second pass of a multipass operation
				// it outputs an endless number of delay frames at the end!  This causes us to loop
				// infinitely trying to flush a codec delay that never ends.  Unfortunately, there is
				// one case where such a string of delay frames is valid: when the length of video
				// being compressed is shorter than the B-frame delay.  We attempt to detect when
				// this situation occurs and avert the loop.

				VDThreadedVideoCompressor::FlushStatus flushStatus = mpThreadedVideoCompressor->GetFlushStatus();

				if (flushStatus & VDThreadedVideoCompressor::kFlushStatusLoopingDetected) {
					int count = kReasonableBFrameBufferLimit;
					VDLogAppMessage(kVDLogWarning, kVDST_Dub, kVDM_CodecLoopingDuringDelayedFlush, 1, &count);
					result = kVideoWriteOK;

					// terminate loop
					mVideoFramesDelayed = 1;
				}
			}

			if (result == kVideoWriteDiscarded || result == kVideoWriteOK || result == kVideoWritePullOnly) {
				--mVideoFramesDelayed;

				if (!mVideoFramesDelayed) {
					mbFlushingCompressor = false;
					if (mpThreadedVideoCompressor)
						mpThreadedVideoCompressor->SetFlush(false);
				} else {
					VDASSERT((sint32)mVideoFramesDelayed > 0);
				}
			}
		} else {
			bool endOfInput = false;

			{
				VDDubAutoThreadLocation loc(*mppCurrentAction, "waiting for video frame from I/O thread");

				mpLoopThrottle->BeginWait();
				pFrameInfo = mpVideoPipe->getReadBuffer();
				mpLoopThrottle->EndWait();

				if (!pFrameInfo)
					endOfInput = true;
			}

			// Check if we have frames buffered in the codec due to B-frame encoding. If we do,
			// we can't immediately switch from compression to direct copy for smart rendering
			// purposes -- we have to flush the B-frames first.

			if (pFrameInfo && (pFrameInfo->mFlags & kBufferFlagDirectWrite)) {
				if (mVideoFramesDelayed > 0)
					pFrameInfo = NULL;
			}

			if (!pFrameInfo) {
				if (mVideoFramesDelayed > 0) {
					mbFlushingCompressor = true;
					if (mpThreadedVideoCompressor)
						mpThreadedVideoCompressor->SetFlush(true);
					continue;
				} else if (endOfInput) {
					mbVideoEnded = true;
					if (mpCB)
						mpCB->OnVideoStreamEnded();
					break;
				}
			}

			result = ReadVideoFrame(*pFrameInfo);

			// If we pushed a frame to empty the video codec's B-frame queue, decrement the count here.
			if (result == kVideoWritePullOnly) {
				VDVERIFY((sint32)--mVideoFramesDelayed >= 0);
			}

			if (result == kVideoWriteDelayed) {
				//VDDEBUG("[VideoPath] Frame %lld was buffered.\n", pFrameInfo->mDisplayFrame);
				++mVideoFramesDelayed;
			} else {
				//VDDEBUG("[VideoPath] Frame %lld was processed (delay queue length = %d).\n", pFrameInfo->mDisplayFrame, mVideoFramesDelayed);
			}

			if (mbPreview && result == kVideoWriteOK) {
				if (mbFirstFrame) {
					mbFirstFrame = false;
					if (mpCB)
						mpCB->OnFirstFrameWritten();
				}
			}
		}

		if (result == kVideoWritePullOnly)
			break;

		if (pFrameInfo)
			mpVideoPipe->releaseBuffer();
		
		if (result == kVideoWriteOK || result == kVideoWriteDiscarded)
			break;
	}

	++mpVInfo->cur_proc_dst;

	return true;
}

void VDDubVideoProcessor::CheckForDecompressorSwitch() {
	if (!mpOutputDisplay)
		return;

	if (!mpVideoCompressor)
		return;
	
	if (mbVideoDecompressorEnabled == mpOptions->video.fShowDecompressedFrame)
		return;

	mbVideoDecompressorEnabled = mpOptions->video.fShowDecompressedFrame;

	if (mbVideoDecompressorEnabled) {
		mbVideoDecompressorErrored = false;
		mbVideoDecompressorPending = true;

		const VDAVIBitmapInfoHeader *pbih = (const VDAVIBitmapInfoHeader *)mpVideoCompressor->GetOutputFormat();
		mpVideoDecompressor = VDFindVideoDecompressorEx(0, pbih, mpVideoCompressor->GetOutputFormatSize(), VDPreferencesIsPreferInternalVideoDecodersEnabled());

		if (mpVideoDecompressor) {
			if (!mpVideoDecompressor->SetTargetFormat(0))
				mpVideoDecompressor = NULL;
			else {
				try {
					mpVideoDecompressor->Start();

					mpLoopThrottle->BeginWait();
					mpBlitter->lock(BUFFERID_OUTPUT);
					mpLoopThrottle->EndWait();
					mpBlitter->postAPC(BUFFERID_OUTPUT, AsyncDecompressorSuccessfulCallback, mpOutputDisplay, NULL);					

					int format = mpVideoDecompressor->GetTargetFormat();
					int variant = mpVideoDecompressor->GetTargetFormatVariant();

					VDPixmapLayout layout;
					VDMakeBitmapCompatiblePixmapLayout(layout, abs(pbih->biWidth), abs(pbih->biHeight), format, variant, mpVideoDecompressor->GetTargetFormatPalette());

					mVideoDecompBuffer.init(layout);
				} catch(const MyError&) {
					mpVideoDecompressor = NULL;
				}
			}
		}

		if (!mpVideoDecompressor) {
			mpLoopThrottle->BeginWait();
			mpBlitter->lock(BUFFERID_OUTPUT);
			mpLoopThrottle->EndWait();
			mpBlitter->postAPC(BUFFERID_OUTPUT, AsyncDecompressorFailedCallback, mpOutputDisplay, NULL);
		}
	} else {
		if (mpVideoDecompressor) {
			mpLoopThrottle->BeginWait();
			mpBlitter->lock(BUFFERID_OUTPUT);
			mpLoopThrottle->EndWait();
			mpBlitter->unlock(BUFFERID_OUTPUT);
			mpVideoDecompressor->Stop();
			mpVideoDecompressor = NULL;
		}
		mpBlitter->postAPC(0, AsyncReinitDisplayCallback, this, NULL);
	}
}

void VDDubVideoProcessor::NotifyDroppedFrame(int exdata) {
	if (!(exdata & kBufferFlagPreload)) {
		mpBlitter->nextFrame(2);

		if (mFramesToDrop)
			--mFramesToDrop;

		if (mpStatusHandler)
			mpStatusHandler->NotifyNewFrame(0);
	}
}

void VDDubVideoProcessor::NotifyCompletedFrame(uint32 bytes, bool isKey) {
	mpVInfo->lastProcessedTimestamp = VDGetCurrentTick();
	++mpVInfo->processed;

	mpBlitter->nextFrame(2);

	if (mpStatusHandler)
		mpStatusHandler->NotifyNewFrame(isKey ? bytes : bytes | 0x80000000);
}

bool VDDubVideoProcessor::RequestNextVideoFrame() {
	if (mpVInfo->cur_dst >= mpVInfo->end_dst) {
		if (!mbVideoPushEnded) {
			mbVideoPushEnded = true;

			VDDubFrameRequest req;
			req.mbDirect = false;
			req.mSrcFrame = -1;

			mpVideoRequestQueue->AddRequest(req);
		}
		return false;
	}

	VDPosition timelinePos = mpVInfo->cur_dst++;
	bool drop = false;

	if (mbPreview && mpOptions->perf.fDropFrames && !mPendingOutputFrames.empty()) {
		uint32 clock = mpBlitter->getPulseClock();

		if ((sint32)(clock - (uint32)timelinePos*2) > 0) {
			drop = true;

			mPendingOutputFrames.back().mbHoldFrame = true;
		}
	}

	const VDRenderFrameMap::FrameEntry& frameEntry = (*mpVideoFrameMap)[timelinePos];

	if (frameEntry.mSourceFrame < 0 || drop) {
		OutputFrameEntry outputEntry;
		outputEntry.mTimelineFrame = frameEntry.mTimelineFrame;
		outputEntry.mpRequest = NULL;
		outputEntry.mbHoldFrame = true;
		outputEntry.mbNullFrame = true;

		mPendingOutputFrames.push_back(outputEntry);
		return true;
	}

	vdrefptr<IVDFilterFrameClientRequest> outputReq;
	VDDubFrameRequest req;
	if (frameEntry.mbDirect) {
		req.mbDirect = true;
		req.mSrcFrame = frameEntry.mSourceFrame;

		mpVideoRequestQueue->AddRequest(req);
	} else if (mpVideoFilters) {
		if (mpOptions->video.mode == DubVideoOptions::M_FULL)
			mpVideoFilters->RequestFrame(frameEntry.mSourceFrame, ~outputReq);
		else
			mpVideoFrameSource->CreateRequest(frameEntry.mSourceFrame, false, ~outputReq);

		vdrefptr<VDFilterFrameRequest> freq;
		while(mpVideoFrameSource->GetNextRequest(~freq)) {
			const VDFilterFrameRequestTiming& timing = freq->GetTiming();

			req.mSrcFrame = timing.mOutputFrame;
			req.mbDirect = false;
			mpVideoRequestQueue->AddRequest(req);

			SourceFrameEntry srcEnt;
			srcEnt.mpRequest = freq;
			srcEnt.mSourceFrame = req.mSrcFrame;
			mPendingSourceFrames.push_back(srcEnt);
			freq.release();
		}
	} else {
		req.mbDirect = false;
		req.mSrcFrame = frameEntry.mSourceFrame;

		mpVideoRequestQueue->AddRequest(req);
	}

	OutputFrameEntry outputEntry;
	outputEntry.mTimelineFrame = frameEntry.mTimelineFrame;
	outputEntry.mpRequest = outputReq;
	outputEntry.mbHoldFrame = frameEntry.mbHoldFrame;
	outputEntry.mbNullFrame = false;

	mPendingOutputFrames.push_back(outputEntry);
	outputReq.release();

	return true;
}

bool VDDubVideoProcessor::DoVideoFrameDropTest(const VDRenderVideoPipeFrameInfo& frameInfo) {
#if 1
	return false;
#else
	const int			exdata				= frameInfo.mFlags;
	const int			droptype			= frameInfo.mDroptype;
	const uint32		lastSize			= frameInfo.mLength;
	const VDPosition	sample_num			= frameInfo.mRawFrame;
	const VDPosition	display_num			= frameInfo.mDisplayFrame;
	const int			srcIndex			= frameInfo.mSrcIndex;
	IVDVideoSource *vsrc = mVideoSources[srcIndex];

	bool bDrop = false;

	if (mpOptions->perf.fDropFrames) {
		// If audio is frozen, force frames to be dropped.
		bDrop = !vsrc->isDecodable(sample_num);

		if (mbAudioFrozen && mbAudioFrozenValid) {
			mFramesToDrop = 1;
		}

		if (mFramesToDrop) {
			long lCurrentDelta = mpBlitter->getFrameDelta();
			if (mpOptions->video.previewFieldMode)
				lCurrentDelta >>= 1;
			if (mFramesToDrop > lCurrentDelta) {
				mFramesToDrop = lCurrentDelta;
				if (mFramesToDrop < 0)
					mFramesToDrop = 0;
			}
		}

		if (mFramesToDrop && !bDrop) {

			// Attempt to drop a frame before the decoder.  Droppable frames (zero-byte
			// or B-frames) can be dropped without any problem without question.  Dependant
			// (P-frames or delta frames) and independent frames (I-frames or keyframes)
			// should only be dropped if there is a reasonable expectation that another
			// independent frame will arrive around the time that we want to stop dropping
			// frames, since we'll basically kill decoding until then.

			if (droptype == AVIPipe::kDroppable) {
				bDrop = true;
			} else {
				int total, indep;

				mpVideoPipe->getDropDistances(total, indep);

				// Do a blind drop if we know a keyframe will arrive within two frames.

				if (indep == 0x3FFFFFFF && vsrc->nearestKey(display_num + mpOptions->video.frameRateDecimation*2) > display_num)
					indep = 0;

				if (indep > 0 && indep < mFramesToDrop)
					return true;
			}
		}
	}

	// Zero-byte drop frame? Just nuke it now.
	if (mpOptions->video.mbPreserveEmptyFrames || (mpOptions->video.mode == DubVideoOptions::M_FULL && mpVideoFilters->isEmpty())) {
		if (!lastSize && (!(exdata & kBufferFlagInternalDecode) || (exdata & kBufferFlagSameAsLast)))
			return true;
	}

	// Don't drop this frame.
	return false;
#endif
}

VDDubVideoProcessor::VideoWriteResult VDDubVideoProcessor::ReadVideoFrame(const VDRenderVideoPipeFrameInfo& frameInfo) {
	const void			*buffer				= frameInfo.mpData;
	const int			exdata				= frameInfo.mFlags;
	const uint32		lastSize			= frameInfo.mLength;
	const int			srcIndex			= frameInfo.mSrcIndex;

	IVDVideoSource *vsrc = mVideoSources[srcIndex];

	if (mbPreview) {
		if (DoVideoFrameDropTest(frameInfo)) {
			NotifyDroppedFrame(exdata);
			return kVideoWriteDiscarded;
		}
	}

	// If:
	//
	//	- we're in direct mode, or
	//	- we've got an empty frame and "Preserve Empty Frames" is enabled
	//
	// ...write it directly to the output and skip the rest.

	bool isDirectWrite = (exdata & kBufferFlagDirectWrite) != 0;
	if (isDirectWrite) {
		VDASSERT(!mPendingOutputFrames.empty());

		const OutputFrameEntry& nextOutputFrame = mPendingOutputFrames.front();
		VDASSERT(!nextOutputFrame.mpRequest);

		mpVInfo->cur_proc_src = nextOutputFrame.mTimelineFrame;

		mPendingOutputFrames.pop_front();

		uint32 flags = (exdata & kBufferFlagDelta) || !(exdata & kBufferFlagDirectWrite) ? 0 : AVIOutputStream::kFlagKeyFrame;
		mpVideoOut->write(flags, (char *)buffer, lastSize, 1);

		mpVInfo->total_size += lastSize + 24;
		mpVInfo->lastProcessedTimestamp = VDGetCurrentTick();
		++mpVInfo->processed;
		if (mpStatusHandler)
			mpStatusHandler->NotifyNewFrame(lastSize | (exdata&1 ? 0x80000000 : 0));

		if (mpThreadedVideoCompressor) {
			mpThreadedVideoCompressor->SkipFrame();

			if (exdata & kBufferFlagDirectWrite)
				mpThreadedVideoCompressor->Restart();
		}

		return kVideoWriteOK;
	}

	// Fast Repack: Decompress data and send to compressor (possibly non-RGB).
	// Slow Repack: Decompress data and send to compressor.
	// Full:		Decompress, process, filter, convert, send to compressor.

	VDDubVideoProcessor::VideoWriteResult decodeResult = DecodeVideoFrame(frameInfo);

	if (!(exdata & kBufferFlagPreload)) {
		if (!mPendingSourceFrames.empty()) {
			const SourceFrameEntry& sfe = mPendingSourceFrames.front();

			VDASSERT(sfe.mSourceFrame == frameInfo.mDisplayFrame);
			if (sfe.mpRequest) {
				mpVideoFrameSource->AllocateRequestBuffer(sfe.mpRequest);
				const VDFilterFrameBuffer *buf = sfe.mpRequest->GetResultBuffer();
				const VDPixmap pxdst(VDPixmapFromLayout(mpVideoFilters->GetInputLayout(), (void *)buf->GetBasePointer()));
				const VDPixmap& pxsrc = vsrc->getTargetFormat();

				if (!mpInputBlitter)
					mpInputBlitter = VDPixmapCreateBlitter(pxdst, pxsrc);

				mpInputBlitter->Blit(pxdst, pxsrc);

				sfe.mpRequest->MarkComplete(true);
				mpVideoFrameSource->CompleteRequest(sfe.mpRequest, true);
				sfe.mpRequest->Release();
			}

			mPendingSourceFrames.pop_front();
		}
	}

	if (decodeResult != kVideoWriteOK)
		return decodeResult;

	// drop frames if the drop counter is >0
	if (mFramesToDrop && mbPreview) {
		if (mbInputLocked) {
			mbInputLocked = false;
			mpBlitter->unlock(BUFFERID_INPUT);
		}

		NotifyDroppedFrame(exdata);
		return kVideoWriteDiscarded;
	}

	if (mpVideoFilters)
		return kVideoWriteNoOutput;

	vdrefptr<VDRenderOutputBuffer> pBuffer;
	VDDubVideoProcessor::VideoWriteResult getOutputResult = GetVideoOutputBuffer(~pBuffer);
	if (getOutputResult != kVideoWriteOK)
		return getOutputResult;

	VDASSERT(!mPendingOutputFrames.empty());

	const OutputFrameEntry& nextOutputFrame = mPendingOutputFrames.front();
	VDASSERT(!nextOutputFrame.mpRequest);

	mpProcessingProfileChannel->Begin(0xffc0c0, "V-BltOut");
	const VDPixmap& pxsrc = vsrc->getTargetFormat();
	if (!mpOutputBlitter)
		mpOutputBlitter = VDPixmapCreateBlitter(pBuffer->mPixmap, pxsrc);

	mpOutputBlitter->Blit(pBuffer->mPixmap, pxsrc);
	mpProcessingProfileChannel->End();

	mpVInfo->cur_proc_src = nextOutputFrame.mTimelineFrame;

	bool holdFrame = nextOutputFrame.mbHoldFrame;
	mPendingOutputFrames.pop_front();

	return WriteFinishedVideoFrame(pBuffer, holdFrame);
}

VDDubVideoProcessor::VideoWriteResult VDDubVideoProcessor::GetVideoOutputBuffer(VDRenderOutputBuffer **ppBuffer) {
	vdrefptr<VDRenderOutputBuffer> pBuffer;

	mpProcessingProfileChannel->Begin(0xe0e0e0, "V-Lock2");
	mpLoopThrottle->BeginWait();
	for(;;) {
		VDVideoDisplayFrame *tmp;
		if (mpOutputDisplay && mpOutputDisplay->RevokeBuffer(false, &tmp)) {
			pBuffer.set(static_cast<VDRenderOutputBuffer *>(tmp));
			break;
		}

		bool successful = mpDisplayBufferTracker ? mpDisplayBufferTracker->AllocFrame(100, ~pBuffer) : mpFrameBufferTracker->AllocFrame(100, ~pBuffer);

		if (*mpAbort)
			break;

		if (successful)
			break;
	}
	mpLoopThrottle->EndWait();
	mpProcessingProfileChannel->End();

	if (!pBuffer)
		return kVideoWriteDiscarded;

	*ppBuffer = pBuffer.release();
	return kVideoWriteOK;
}

VDDubVideoProcessor::VideoWriteResult VDDubVideoProcessor::ProcessVideoFrame() {
	vdrefptr<VDRenderOutputBuffer> pBuffer;

	if (mpThreadedVideoCompressor) {
		vdrefptr<VDRenderPostCompressionBuffer> pOutBuffer;
		if (mpThreadedVideoCompressor->ExchangeBuffer(NULL, ~pOutBuffer)) {
			WriteFinishedVideoFrame(pOutBuffer->mOutputBuffer.data(), pOutBuffer->mOutputSize, pOutBuffer->mbOutputIsKey, false, NULL);
			return kVideoWritePullOnly;
		}
	}

	// Process frame to backbuffer for Full video mode.  Do not process if we are
	// running in Repack mode only!

	if (mpOptions->video.mode == DubVideoOptions::M_FULL) {
		// process frame
		mpProcessingProfileChannel->Begin(0x008000, "V-Filter");
		mpVideoFilters->RunToCompletion();
		mpProcessingProfileChannel->End();
	}

	if (mPendingOutputFrames.empty())
		return kVideoWriteNoOutput;

	const OutputFrameEntry& nextOutputFrame = mPendingOutputFrames.front();

	if (nextOutputFrame.mbNullFrame) {
		WriteNullVideoFrame();
		mPendingOutputFrames.pop_front();
		return kVideoWriteOK;
	}

	IVDFilterFrameClientRequest *pOutputReq = nextOutputFrame.mpRequest;

	if (!pOutputReq || !pOutputReq->IsCompleted()) {
		if (mbInputLocked) {
			mbInputLocked = false;
			mpBlitter->unlock(BUFFERID_INPUT);
		}

		return kVideoWriteNoOutput;
	}

	if (!pOutputReq->IsSuccessful()) {
		const VDFilterFrameRequestError *err = pOutputReq->GetError();

		if (err)
			throw MyError("%s", err->mError.c_str());
		else
			throw MyError("An unknown error occurred during video filtering.");
	}

	VDDubVideoProcessor::VideoWriteResult getOutputResult = GetVideoOutputBuffer(~pBuffer);
	if (getOutputResult != kVideoWriteOK)
		return getOutputResult;

	mpProcessingProfileChannel->Begin(0xffc0c0, "V-BltOut");
	const VDPixmapLayout& layout = (mpOptions->video.mode == DubVideoOptions::M_FULL) ? mpVideoFilters->GetOutputLayout() : mpVideoFilters->GetInputLayout();
	const VDPixmap& pxsrc = VDPixmapFromLayout(layout, pOutputReq->GetResultBuffer()->GetBasePointer());
	if (!mpOutputBlitter)
		mpOutputBlitter = VDPixmapCreateBlitter(pBuffer->mPixmap, pxsrc);

	mpOutputBlitter->Blit(pBuffer->mPixmap, pxsrc);
	mpProcessingProfileChannel->End();

	mpVInfo->cur_proc_src = nextOutputFrame.mTimelineFrame;

	pOutputReq->Release();

	bool holdFrame = nextOutputFrame.mbHoldFrame;
	mPendingOutputFrames.pop_front();

	return WriteFinishedVideoFrame(pBuffer, holdFrame);
}

VDDubVideoProcessor::VideoWriteResult VDDubVideoProcessor::WriteFinishedVideoFrame(VDRenderOutputBuffer *pBuffer, bool holdBuffer) {
	// write it to the file	
	const void *frameBuffer = pBuffer->mpBase;

	vdrefptr<VDRenderPostCompressionBuffer> pNewBuffer;
	uint32 dwBytes;
	bool isKey;

	if (mpVideoCompressor) {
		bool gotFrame;

		if (holdBuffer)
			mpHeldCompressionInputBuffer = pBuffer;

		mpProcessingProfileChannel->Begin(0x80c080, "V-Compress");
		{
			VDDubAutoThreadLocation loc(*mppCurrentAction, "compressing video frame");
			gotFrame = mpThreadedVideoCompressor->ExchangeBuffer(pBuffer, ~pNewBuffer);
		}
		mpProcessingProfileChannel->End();

		// Check if codec buffered a frame.
		if (!gotFrame)
			return kVideoWriteDelayed;

		frameBuffer = pNewBuffer->mOutputBuffer.data();
		dwBytes = pNewBuffer->mOutputSize;
		isKey = pNewBuffer->mbOutputIsKey;

	} else {

		dwBytes = ((const VDAVIBitmapInfoHeader *)mpVideoOut->getFormat())->biSizeImage;
		isKey = true;
	}

	WriteFinishedVideoFrame(frameBuffer, dwBytes, isKey, true, pBuffer);
	return kVideoWriteOK;
}

void VDDubVideoProcessor::WriteNullVideoFrame() {
	WriteFinishedVideoFrame(NULL, 0, false, false, NULL);
}

void VDDubVideoProcessor::WriteFinishedVideoFrame(const void *data, uint32 size, bool isKey, bool renderEnabled, VDRenderOutputBuffer *pBuffer) {
	if (mpVideoCompressor) {
		if (mpVideoDecompressor && !mbVideoDecompressorErrored) {
			mpProcessingProfileChannel->Begin(0x80c080, "V-Decompress");

			if (mbVideoDecompressorPending && isKey) {
				mbVideoDecompressorPending = false;
			}

			if (!mbVideoDecompressorPending && size) {
				try {
					memset((char *)data + size, 0xA5, kDecodeOverflowWorkaroundSize);
					mpVideoDecompressor->DecompressFrame(mVideoDecompBuffer.base(), (char *)data, size, isKey, false);
				} catch(const MyError&) {
					mpBlitter->postAPC(0, AsyncDecompressorErrorCallback, mpOutputDisplay, NULL);
					mbVideoDecompressorErrored = true;
				}
			}

			mpProcessingProfileChannel->End();
		}
	}

	////// WRITE VIDEO FRAME TO DISK
	{
		VDDubAutoThreadLocation loc(*mppCurrentAction, "writing video frame to disk");
		mpVideoOut->write(isKey ? AVIOutputStream::kFlagKeyFrame : 0, (char *)data, size, 1);
	}
	mpVInfo->total_size += size + 24;

	////// RENDERING

	if (renderEnabled) {
		bool renderFrame = (mbPreview || mRefreshFlag.xchg(0));
		bool renderInputFrame = renderFrame && mpInputDisplay && mpOptions->video.fShowInputFrame;
		bool renderOutputFrame = (renderFrame || mbVideoDecompressorEnabled) && mpOutputDisplay && mpOptions->video.mode == DubVideoOptions::M_FULL && mpOptions->video.fShowOutputFrame && size;

		if (renderInputFrame) {
			mpBlitter->postAPC(BUFFERID_INPUT, AsyncUpdateCallback, mpInputDisplay, (void *)&mpOptions->video.previewFieldMode);
		} else
			mpBlitter->unlock(BUFFERID_INPUT);

		mbInputLocked = false;

		if (renderOutputFrame) {
			if (mbVideoDecompressorEnabled) {
				if (mpVideoDecompressor && !mbVideoDecompressorErrored && !mbVideoDecompressorPending) {
					mpBlitter->lock(BUFFERID_OUTPUT);
					mpBlitter->postAPC(BUFFERID_OUTPUT, AsyncDecompressorUpdateCallback, mpOutputDisplay, &mVideoDecompBuffer);
				}
			} else {
				mpLoopThrottle->BeginWait();
				mpBlitter->lock(BUFFERID_OUTPUT);
				mpLoopThrottle->EndWait();
				pBuffer->AddRef();
				mpBlitter->postAPC(BUFFERID_OUTPUT, StaticAsyncUpdateOutputCallback, this, pBuffer);
			}
		} else
			mpBlitter->unlock(BUFFERID_OUTPUT);
	}

	if (mpOptions->perf.fDropFrames && mbPreview) {
		long lFrameDelta;

		lFrameDelta = mpBlitter->getFrameDelta() / 2;

		if (lFrameDelta < 0) lFrameDelta = 0;
		
		if (lFrameDelta > 0) {
			mFramesToDrop = lFrameDelta;
		}
	}

	NotifyCompletedFrame(size, isKey);
}

VDDubVideoProcessor::VideoWriteResult VDDubVideoProcessor::DecodeVideoFrame(const VDRenderVideoPipeFrameInfo& frameInfo) {
	const void *buffer = frameInfo.mpData;
	const int exdata = frameInfo.mFlags;
	const int srcIndex = frameInfo.mSrcIndex;
	const VDPosition sample_num = frameInfo.mStreamFrame;
	const VDPosition target_num = frameInfo.mTargetSample;

	IVDVideoSource *vsrc = mVideoSources[srcIndex];

	mpProcessingProfileChannel->Begin(0xe0e0e0, "V-Lock1");
	bool bLockSuccessful;

	mpLoopThrottle->BeginWait();
	do {
		bLockSuccessful = mpBlitter->lock(BUFFERID_INPUT, mbPreview ? 500 : -1);
	} while(!bLockSuccessful && !*mpAbort);
	mpLoopThrottle->EndWait();
	mpProcessingProfileChannel->End();

	if (!bLockSuccessful)
		return kVideoWriteDiscarded;

	mbInputLocked = true;

	///// DECODE FRAME

	if (exdata & kBufferFlagPreload)
		mpProcessingProfileChannel->Begin(0xfff0f0, "V-Preload");
	else
		mpProcessingProfileChannel->Begin(0xffe0e0, "V-Decode");

	if (!(exdata & kBufferFlagFlushCodec)) {
		VDDubAutoThreadLocation loc(*mppCurrentAction, "decompressing video frame");
		vsrc->streamGetFrame(buffer, frameInfo.mLength, 0 != (exdata & kBufferFlagPreload), sample_num, target_num);
	}

	mpProcessingProfileChannel->End();

	if (exdata & kBufferFlagPreload) {
		mpBlitter->unlock(BUFFERID_INPUT);
		mbInputLocked = false;
		return kVideoWriteNoOutput;
	}

	return kVideoWriteOK;
}


bool VDDubVideoProcessor::AsyncReinitDisplayCallback(int pass, void *pThisAsVoid, void *, bool aborting) {
	if (aborting)
		return false;

	VDDubVideoProcessor *pThis = (VDDubVideoProcessor *)pThisAsVoid;
	pThis->mpOutputDisplay->Reset();
	return false;
}

bool VDDubVideoProcessor::StaticAsyncUpdateOutputCallback(int pass, void *pThisAsVoid, void *pBuffer, bool aborting) {
	return ((VDDubVideoProcessor *)pThisAsVoid)->AsyncUpdateOutputCallback(pass, (VDRenderOutputBuffer *)pBuffer, aborting);
}

bool VDDubVideoProcessor::AsyncUpdateOutputCallback(int pass, VDRenderOutputBuffer *pBuffer, bool aborting) {
	if (aborting) {
		if (pBuffer)
			pBuffer->Release();

		return false;
	}

	IVDVideoDisplay *pVideoDisplay = mpOutputDisplay;
	int nFieldMode = mpOptions->video.previewFieldMode;

	uint32 baseFlags = IVDVideoDisplay::kVisibleOnly | IVDVideoDisplay::kDoNotCache;

	if (g_prefs.fDisplay & Preferences::kDisplayEnableVSync)
		baseFlags |= IVDVideoDisplay::kVSync;

	if (pBuffer) {
		if (nFieldMode) {
			switch(nFieldMode) {
				case DubVideoOptions::kPreviewFieldsWeaveTFF:
					pBuffer->mFlags = baseFlags | IVDVideoDisplay::kEvenFieldOnly | IVDVideoDisplay::kAutoFlipFields;
					break;

				case DubVideoOptions::kPreviewFieldsWeaveBFF:
					pBuffer->mFlags = baseFlags | IVDVideoDisplay::kOddFieldOnly | IVDVideoDisplay::kAutoFlipFields;
					break;

				case DubVideoOptions::kPreviewFieldsBobTFF:
					pBuffer->mFlags = baseFlags | IVDVideoDisplay::kBobEven | IVDVideoDisplay::kAllFields | IVDVideoDisplay::kAutoFlipFields;
					break;

				case DubVideoOptions::kPreviewFieldsBobBFF:
					pBuffer->mFlags = baseFlags | IVDVideoDisplay::kBobOdd | IVDVideoDisplay::kAllFields | IVDVideoDisplay::kAutoFlipFields;
					break;

				case DubVideoOptions::kPreviewFieldsNonIntTFF:
					pBuffer->mFlags = baseFlags | IVDVideoDisplay::kEvenFieldOnly | IVDVideoDisplay::kAutoFlipFields | IVDVideoDisplay::kSequentialFields;
					break;

				case DubVideoOptions::kPreviewFieldsNonIntBFF:
					pBuffer->mFlags = baseFlags | IVDVideoDisplay::kOddFieldOnly | IVDVideoDisplay::kAutoFlipFields | IVDVideoDisplay::kSequentialFields;
					break;
			}

			pBuffer->mbInterlaced = true;
		} else {
			pBuffer->mFlags = IVDVideoDisplay::kAllFields | baseFlags;
			pBuffer->mbInterlaced = false;
		}

		pBuffer->mbAllowConversion = true;

		pVideoDisplay->PostBuffer(pBuffer);
		pBuffer->Release();
		return false;
	}

	if (nFieldMode) {
		if (nFieldMode == 2) {
			if (pass)
				pVideoDisplay->Update(IVDVideoDisplay::kEvenFieldOnly | baseFlags);
			else
				pVideoDisplay->Update(IVDVideoDisplay::kOddFieldOnly | IVDVideoDisplay::kFirstField | baseFlags);
		} else {
			if (pass)
				pVideoDisplay->Update(IVDVideoDisplay::kOddFieldOnly | baseFlags);
			else
				pVideoDisplay->Update(IVDVideoDisplay::kEvenFieldOnly | IVDVideoDisplay::kFirstField | baseFlags);
		}

		return !pass;
	} else {
		pVideoDisplay->Update(IVDVideoDisplay::kAllFields | baseFlags);
		return false;
	}
}
