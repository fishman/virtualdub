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

#include "stdafx.h"

#include <ddraw.h>

#include <vd2/system/error.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/time.h>
#include <vector>

#include "DubUtils.h"
#include "VideoSource.h"
#include "FrameSubset.h"
#include "vbitmap.h"
#include "FilterSystem.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDStreamInterleaver
//
///////////////////////////////////////////////////////////////////////////

void VDStreamInterleaver::Init(int streams) {
	mStreams.resize(streams);
	mNextStream = 0;
	mFrames = 0;

	mbInterleavingEnabled	= true;
	mNonIntStream		= 0;
	mActiveStreams		= 0;
}

void VDStreamInterleaver::InitStream(int stream, uint32 nSampleSize, sint32 nPreload, double nSamplesPerFrame, double nInterval, sint32 nMaxPush) {
	VDASSERT(stream>=0 && stream<mStreams.size());

	Stream& streaminfo = mStreams[stream];

	streaminfo.mSamplesWrittenToSegment = 0;
	streaminfo.mMaxSampleSize		= nSampleSize;
	streaminfo.mPreloadMicroFrames	= (sint32)((double)nPreload / nSamplesPerFrame * 65536);
	streaminfo.mSamplesPerFrame		= nSamplesPerFrame;
	streaminfo.mSamplesPerFramePending	= -1;
	streaminfo.mIntervalMicroFrames	= (sint32)(65536.0 / nInterval);
	streaminfo.mbActive				= true;
	streaminfo.mMaxPush				= nMaxPush;

	++mActiveStreams;
}

void VDStreamInterleaver::EndStream(int stream) {
	Stream& streaminfo = mStreams[stream];

	if (streaminfo.mbActive) {
		streaminfo.mbActive		= false;
		streaminfo.mSamplesToWrite	= 0;
		--mActiveStreams;

		while(mNonIntStream < mStreams.size() && !mStreams[mNonIntStream].mbActive)
			++mNonIntStream;
	}
}

void VDStreamInterleaver::AdjustStreamRate(int stream, double samplesPerFrame) {
	VDASSERT(stream >= 0 && stream < mStreams.size());
	Stream& streaminfo = mStreams[stream];

	streaminfo.mSamplesPerFramePending = samplesPerFrame;
}

VDStreamInterleaver::Action VDStreamInterleaver::GetNextAction(int& streamID, sint32& samples) {
	const int nStreams = mStreams.size();

	if (!mActiveStreams)
		return kActionFinish;

	for(;;) {
		if (!mNextStream) {
			Action act = PushStreams();

			if (act != kActionWrite)
				return act;
		}

		for(; mNextStream<nStreams; ++mNextStream) {
			Stream& streaminfo = mStreams[mNextStream];

			if (!mbInterleavingEnabled && mNextStream > mNonIntStream)
				break;

			if (streaminfo.mSamplesToWrite > 0) {
				samples = streaminfo.mSamplesToWrite;
				if (samples > streaminfo.mMaxPush)
					samples = streaminfo.mMaxPush;
				streaminfo.mSamplesToWrite -= samples;
				streamID = mNextStream;
				VDASSERT(samples < 2147483647);
				streaminfo.mSamplesWrittenToSegment += samples;
				return kActionWrite;
			}
		}

		mNextStream = 0;
	}
}

VDStreamInterleaver::Action VDStreamInterleaver::PushStreams() {
	const int nStreams = mStreams.size();

	for(;;) {
		int nFeeding = 0;
		sint64 microFrames = (sint64)mFrames << 16;

		for(int i=mNonIntStream; i<nStreams; ++i) {
			Stream& streaminfo = mStreams[i];

			if (!streaminfo.mbActive)
				continue;

			sint64 microFrameOffset = microFrames;
			
			if (streaminfo.mIntervalMicroFrames != 65536) {
				microFrameOffset += streaminfo.mIntervalMicroFrames - 1;
				microFrameOffset -= microFrameOffset % streaminfo.mIntervalMicroFrames;
			}

			microFrameOffset += streaminfo.mPreloadMicroFrames;

			double frame = microFrameOffset / 65536.0;

			sint64 target = (sint64)ceil(streaminfo.mSamplesPerFrame * frame);
			sint64 toread = target - streaminfo.mSamplesWrittenToSegment;

			if (toread < 0)
				toread = 0;
		
			VDASSERT((sint32)toread == toread);
			streaminfo.mSamplesToWrite = (sint32)toread;

//			VDDEBUG("Dub/Interleaver: Stream #%d: feeding %d samples (%4I64x, %I64d + %I64d -> %I64d)\n", i, (int)toread, microFrameOffset, streaminfo.mSamplesWrittenToSegment, toread, target);
			if (toread > 0)
				++nFeeding;

			if (!mbInterleavingEnabled)
				break;
		}

		if (nFeeding > 0)
			break;

		// If no streams are feeding and we have no cut point, bump the frame target by 1 frame and
		// try again.

		++mFrames;

		for(int i=0; i<nStreams; ++i) {
			Stream& streaminfo = mStreams[i];

			if (streaminfo.mSamplesPerFramePending >= 0) {
				streaminfo.mSamplesPerFrame = streaminfo.mSamplesPerFramePending;
				streaminfo.mSamplesPerFramePending = -1.0;
			}
		}
	}

	if (!mbInterleavingEnabled)
		mNextStream = mNonIntStream;

	return kActionWrite;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDRenderFrameMap
//
///////////////////////////////////////////////////////////////////////////

void VDRenderFrameMap::Init(const vdfastvector<IVDVideoSource *>& videoSources, VDPosition nSrcStart, VDFraction srcStep, const FrameSubset *pSubset, VDPosition nFrameCount, bool bDirect, const FilterSystem *pRemapperFS) {
	VDPosition directLast = -1;
	int sourceLast = -1;
	IVDVideoSource *pVS = NULL;
	VDPosition len = 0;

	mFrameMap.reserve((size_t)nFrameCount);
	for(VDPosition frame = 0; frame < nFrameCount; ++frame) {
		VDPosition timelineFrame = nSrcStart + srcStep.scale64t(frame);
		VDPosition srcFrame = timelineFrame;
		int source = 0;

		if (pSubset) {
			bool masked;
			srcFrame = pSubset->lookupFrame((int)srcFrame, masked, source);
			if (srcFrame < 0)
				break;
		} else {
			if (srcFrame < 0 || srcFrame >= len)
				break;
		}

		if (sourceLast != source) {
			sourceLast = source;
			pVS = videoSources[source];
			len = pVS->asStream()->getLength();
			directLast = -1;
		}

		// we need to preserve this so filter timing isn't screwed up across drop frames
		VDPosition origSrcFrame = srcFrame;

		if (pRemapperFS)
			srcFrame = pRemapperFS->GetSourceFrame(srcFrame);

		srcFrame = pVS->getRealDisplayFrame(srcFrame);

		if (bDirect) {
			VDPosition key = pVS->nearestKey((LONG)srcFrame);

			if (directLast < key)
				directLast = key;
			else if (directLast > srcFrame)
				directLast = key;
			else {
				if (directLast < srcFrame) {
					for(;;) {
						++directLast;
						if (directLast >= srcFrame)
							break;

						if (pVS->getDropType(directLast) != VideoSource::kDroppable)
							break;
					}
				}
			}

			srcFrame = directLast;
		}

		FrameEntry ent;
		ent.mSrcIndex = source;
		ent.mTimelineFrame = timelineFrame;
		ent.mDisplayFrame = srcFrame;
		ent.mOrigDisplayFrame = origSrcFrame;

		mFrameMap.push_back(ent);
	}

	mMaxFrame = mFrameMap.size();

	mInvalidEntry.mSrcIndex = -1;
	mInvalidEntry.mTimelineFrame = -1;
	mInvalidEntry.mDisplayFrame = -1;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDRenderFrameIterator
//
///////////////////////////////////////////////////////////////////////////

void VDRenderFrameIterator::Init(const vdfastvector<IVDVideoSource *>& videoSources, bool bDirect, bool bSmart, const FilterSystem *filtsys) {
	mVideoSources = videoSources;
	mpFilterSystem	= filtsys;
	mbDirect		= bDirect || bSmart;
	mbSmart			= bSmart;
	mDstFrame		= 0;
	mSrcOrigDisplayFrame	= -1;
	mSrcDisplayFrame	= -1;
	mSrcTargetSample	= -1;
	mSequenceFrame = 0;
	mLastSrcDisplayFrame = -1;
	mSrcIndex = -1;
	mLastSrcIndex = -1;
	mpVideoSource = NULL;
	mbFinished		= false;
	mbSameAsLast	= false;

	Reload();
}

VDRenderFrameStep VDRenderFrameIterator::Next() {
	VDRenderFrameStep step;

	while(!mbFinished) {
		bool b;
		VDPosition f = -1;

		if (mSrcDisplayFrame >= 0) {
			f = mpVideoSource->streamGetNextRequiredFrame(b);
			step.mbIsPreroll = (b!=0) && !mbDirect;
		} else {
			f = -1;
			step.mbIsPreroll = false;
		}

		if (f!=-1 || mbFirstSourceFrame) {
			mbFirstSourceFrame = false;

			VDASSERT(mSrcIndex >= 0);

			step.mSourceFrame	= f;
			step.mTargetSample	= mSrcTargetSample;
			step.mOrigDisplayFrame = mSrcOrigDisplayFrame;
			step.mDisplayFrame	= mSrcDisplayFrame;
			step.mTimelineFrame	= mSrcTimelineFrame;
			step.mSequenceFrame	= mSequenceFrame;
			step.mSrcIndex		= mSrcIndex;
			step.mbDirect		= mbDirect;
			step.mbSameAsLast	= mbSameAsLast;

			if (mbDirect) {
				if (!Reload())
					mbFinished = true;
			}

			return step;
		}

		if (!Reload())
			break;
	}

	step.mSourceFrame	= -1;
	step.mTargetSample	= mSrcTargetSample;
	step.mSrcIndex		= mSrcIndex;
	step.mTimelineFrame	= mSrcTimelineFrame;
	step.mOrigDisplayFrame	= mSrcOrigDisplayFrame;
	step.mDisplayFrame	= mSrcDisplayFrame;
	step.mSequenceFrame	= mSequenceFrame;
	step.mbIsPreroll	= false;
	step.mbSameAsLast	= true;
	step.mbDirect		= mbDirect;

	mbFinished = true;

	return step;
}

bool VDRenderFrameIterator::Reload() {
	const VDRenderFrameMap::FrameEntry& ent = mFrameMap[mDstFrame];

	if (ent.mSrcIndex < 0)
		return false;

	mSrcIndex = ent.mSrcIndex;

	if (mLastSrcIndex != mSrcIndex) {
		mLastSrcIndex = mSrcIndex;
		mLastSrcDisplayFrame = -1;
	}

	mpVideoSource = mVideoSources[mSrcIndex];

	mSrcTimelineFrame	= ent.mTimelineFrame;
	VDPosition nextOrigDisplay = ent.mOrigDisplayFrame;
	VDPosition nextDisplay = ent.mDisplayFrame;

	if (mbSmart) {
		bool isFiltered = mpFilterSystem && mpFilterSystem->IsFiltered(ent.mTimelineFrame);

		if (mbDirect) {
			mpVideoSource->streamSetDesiredFrame(nextDisplay);
			if (isFiltered || mpVideoSource->streamGetRequiredCount(NULL) != 1) {
				mpVideoSource->streamRestart();
				mbDirect = false;
			}
		} else {
			if (!isFiltered && mpVideoSource->isKey(nextDisplay)) {
				mpVideoSource->streamRestart();
				mbDirect = true;
			}
		}
	}

	mbSameAsLast = (nextDisplay == mLastSrcDisplayFrame);

	if (mbDirect && mbSameAsLast) {
		nextOrigDisplay = -1;
		nextDisplay = -1;
	} else {
		mpVideoSource->streamSetDesiredFrame(nextDisplay);
		mLastSrcDisplayFrame = nextDisplay;
	}

	mSrcOrigDisplayFrame = nextOrigDisplay;
	mSrcDisplayFrame = nextDisplay;
	mSrcTargetSample = mpVideoSource->displayToStreamOrder(nextDisplay);
	mSequenceFrame = mDstFrame;
	++mDstFrame;

	mbFirstSourceFrame = true;
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAudioPipeline
//
///////////////////////////////////////////////////////////////////////////

VDAudioPipeline::VDAudioPipeline() {
}

VDAudioPipeline::~VDAudioPipeline() {
}

void VDAudioPipeline::Init(uint32 bytes, uint32 sampleSize, bool vbrModeEnabled) {
	mbVBRModeEnabled = vbrModeEnabled;
	mbInputClosed = false;
	mbOutputClosed = false;
	mSampleSize = sampleSize;

	mBuffer.Init(bytes);
}

void VDAudioPipeline::Shutdown() {
	mBuffer.Shutdown();
}

int VDAudioPipeline::ReadPartial(void *pBuffer, int bytes) {
	int actual = mBuffer.Read((char *)pBuffer, bytes);

	if (actual)
		msigRead.signal();

	return actual;
}

bool VDAudioPipeline::Write(const void *data, int bytes, const VDAtomicInt *abortFlag) {
	int actual;
	while(bytes > 0) {
		void *dst = BeginWrite(bytes, actual);

		if (!actual) {
			if (abortFlag && *abortFlag)
				return false;

			msigRead.wait();
			continue;
		}

		memcpy(dst, data, actual);

		EndWrite(actual);

		data = (const char *)data + actual;
		bytes -= actual;
	}

	VDASSERT(bytes == 0);

	return true;
}

void *VDAudioPipeline::BeginWrite(int requested, int& actual) {
	return mBuffer.LockWrite(requested, actual);
}

void VDAudioPipeline::EndWrite(int actual) {
	if (actual) {
		mBuffer.UnlockWrite(actual);
		msigWrite.signal();
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	VDLoopThrottle
//
///////////////////////////////////////////////////////////////////////////

VDLoopThrottle::VDLoopThrottle()
	: mThrottleFactor(1.0f)
	, mWaitDepth(0)
	, mWaitTime(0)
	, mLastTime(0)
	, mbLastTimeValid(false)
	, mWindowIndex(0)
	, mWaitTimeWindowSum(0)
	, mActiveTimeWindowSum(0)
{
	memset(mWaitTimeWindow, 0, sizeof mWaitTimeWindow);
	memset(mActiveTimeWindow, 0, sizeof mActiveTimeWindow);
}

VDLoopThrottle::~VDLoopThrottle() {
}

bool VDLoopThrottle::Delay() {
	float desiredRatio = mThrottleFactor;

	if (desiredRatio >= 1.0f)
		return true;

	if (desiredRatio <= 0.0f) {
		::Sleep(100);
		return false;
	}

	uint32 total = mActiveTimeWindowSum + mWaitTimeWindowSum;

	if (total > 0) {
		float delta = mActiveTimeWindowSum - total * mThrottleFactor;

		mWaitTime += delta * (0.1f / 16.0f);
	}

	if (mWaitTime > 0) {
		int delayTime = VDRoundToInt(mWaitTime);

		if (delayTime > 1000)
			delayTime = 1000;

		BeginWait();
		::Sleep(delayTime);
		EndWait();
	}

	return true;
}

void VDLoopThrottle::BeginWait() {
	if (!mWaitDepth++) {	// transitioning active -> wait
		uint32 currentTime = VDGetAccurateTick();

		if (mbLastTimeValid) {
			sint32 delta = currentTime - mLastTime;

			// Time shouldn't ever go backwards, but clocks on Windows occasionally have
			// the habit of doing so due to time adjustments, broken RDTSC, etc.
			if (delta < 0)
				delta = 0;

			mActiveTimeWindowSum -= mActiveTimeWindow[mWindowIndex];
			mActiveTimeWindow[mWindowIndex] = delta;
			mActiveTimeWindowSum += delta;
		}

		mLastTime = currentTime;
		mbLastTimeValid = true;
	}
}

void VDLoopThrottle::EndWait() {
	if (!--mWaitDepth) {	// transitioning wait -> active
		uint32 currentTime = VDGetAccurateTick();

		if (mbLastTimeValid) {
			sint32 delta = currentTime - mLastTime;

			// Time shouldn't ever go backwards, but clocks on Windows occasionally have
			// the habit of doing so due to time adjustments, broken RDTSC, etc.
			if (delta < 0)
				delta = 0;

			mWaitTimeWindowSum -= mWaitTimeWindow[mWindowIndex];
			mWaitTimeWindow[mWindowIndex] = delta;
			mWaitTimeWindowSum += delta;

			mWindowIndex = (mWindowIndex + 1) & 15;
		}

		mLastTime = currentTime;
		mbLastTimeValid = true;
	}
}
