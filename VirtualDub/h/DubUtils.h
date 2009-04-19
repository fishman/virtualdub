#ifndef f_DUBUTILS_H
#define f_DUBUTILS_H

#include <vector>

#include <vd2/system/vdtypes.h>
#include <vd2/system/fraction.h>
#include <vd2/system/thread.h>
#include <vd2/system/VDRingBuffer.h>
#include <vd2/system/vdstl.h>

class IVDStreamSource;
class IVDVideoSource;
class FrameSubset;
class FilterSystem;


///////////////////////////////////////////////////////////////////////////

class VDDubAutoThreadLocation {
public:
	VDDubAutoThreadLocation(const char *volatile& locationVar, const char *location)
		: mLocationVar(locationVar)
	{
		mpOldLocation = mLocationVar;
		mLocationVar = location;
	}

	~VDDubAutoThreadLocation() {
		mLocationVar = mpOldLocation;
	}

	const char *volatile& mLocationVar;
	const char *mpOldLocation;
};

///////////////////////////////////////////////////////////////////////////
//
//	VDStreamInterleaver
//
//	VDStreamInterleaver tells the engine when and where to place blocks
//	from various output streams into the final output file.  It is
//	capable of handing any number of streams of both CBR and VBR
//	orientation, and it also handles file slicing with preemptive
//	ahead, i.e. it cuts before rather than after the limit.
//
///////////////////////////////////////////////////////////////////////////

class VDStreamInterleaver {
public:
	enum Action {
		kActionNone,
		kActionWrite,
		kActionFinish
	};

	void Init(int streams);
	void EnableInterleaving(bool bEnableInterleave) { mbInterleavingEnabled = bEnableInterleave; }
	void InitStream(int stream, uint32 nSampleSize, sint32 nPreload, double nSamplesPerFrame, double nInterval, sint32 nMaxPush);
	void EndStream(int stream);
	void AdjustStreamRate(int stream, double samplesPerFrame);

	Action GetNextAction(int& streamID, sint32& samples);

protected:
	struct Stream {
		sint64		mSamplesWrittenToSegment;
		sint32		mSamplesToWrite;
		sint32		mMaxSampleSize;
		double		mSamplesPerFrame;
		double		mSamplesPerFramePending;
		sint32		mIntervalMicroFrames;
		sint32		mPreloadMicroFrames;
		sint32		mMaxPush;
		bool		mbActive;
	};


	Action PushStreams();


	std::vector<Stream>	mStreams;

	int		mNonIntStream;			// Current stream being worked on in a non-interleaved scenario
	int		mNextStream;
	int		mActiveStreams;
	sint32	mFrames;

	bool	mbInterleavingEnabled;
};

///////////////////////////////////////////////////////////////////////////
//
//	VDRenderFrameMap
//
//	A render frame map holds a mapping from frames to render (timeline)
//	to display frames in the source.  The main benefit to the render frame
//	map is that it automatically handles frame reordering that must occur
//	due to direct stream copy dependencies.
//
///////////////////////////////////////////////////////////////////////////

class VDRenderFrameMap {
public:
	struct FrameEntry {
		VDPosition	mTimelineFrame;
		VDPosition	mDisplayFrame;
		VDPosition	mOrigDisplayFrame;
		int			mSrcIndex;
	};

	void		Init(const vdfastvector<IVDVideoSource *>& videoSources, VDPosition nSrcStart, VDFraction srcStep, const FrameSubset *pSubset, VDPosition nFrameCount, bool bDirect, const FilterSystem *pRemapperFS);

	VDPosition	size() const { return mFrameMap.size(); }

	const FrameEntry& operator[](VDPosition outFrame) const {
		return outFrame>=0 && outFrame<mMaxFrame ? mFrameMap[(tFrameMap::size_type)outFrame] : mInvalidEntry;
	}

protected:
	typedef vdfastvector<FrameEntry> tFrameMap;
	tFrameMap mFrameMap;
	VDPosition mMaxFrame;
	FrameEntry	mInvalidEntry;
};

///////////////////////////////////////////////////////////////////////////
//
//	VDRenderFrameIterator
//
//	Render frame iterators sequentially walk down a frame render map,
//	returning a list of display frames to be processed, and the stream
//	frames that must be fetched to decode them.
//
///////////////////////////////////////////////////////////////////////////

struct VDRenderFrameStep {
	VDPosition	mSourceFrame;
	VDPosition	mTargetSample;
	VDPosition	mOrigDisplayFrame;
	VDPosition	mDisplayFrame;
	VDPosition	mTimelineFrame;
	VDPosition	mSequenceFrame;
	bool		mbIsPreroll;
	bool		mbDirect;
	bool		mbSameAsLast;
	int			mSrcIndex;
};

class VDRenderFrameIterator {
public:
	VDRenderFrameIterator(const VDRenderFrameMap& frameMap) : mFrameMap(frameMap) {}

	void		Init(const vdfastvector<IVDVideoSource *>& videoSources, bool bDirect, bool bSmart, const FilterSystem *filtsys);
	VDRenderFrameStep	Next();

protected:
	bool		Reload();
	void		ReloadQueue(sint32 nCount);
	long		ConvertToIdealRawFrame(sint64 frame);

	const VDRenderFrameMap&	mFrameMap;

	int			mSrcIndex;
	int			mLastSrcIndex;
	VDPosition	mSrcTimelineFrame;
	VDPosition	mSrcOrigDisplayFrame;
	VDPosition	mSrcDisplayFrame;
	VDPosition	mSrcTargetSample;
	VDPosition	mLastSrcDisplayFrame;
	VDPosition	mSequenceFrame;
	VDPosition	mDstFrame;
	VDPosition	mDstFrameQueueNext;

	vdfastvector<IVDVideoSource *> mVideoSources;
	IVDVideoSource *mpVideoSource;

	const FilterSystem	*mpFilterSystem;

	bool		mbDirect;
	bool		mbSmart;
	bool		mbSameAsLast;
	bool		mbFirstSourceFrame;
	bool		mbFinished;
};

///////////////////////////////////////////////////////////////////////////
//
//	VDAudioPipeline
//
///////////////////////////////////////////////////////////////////////////

class VDAudioPipeline {
public:
	VDAudioPipeline();
	~VDAudioPipeline();

	void Init(uint32 bytes, uint32 sampleSize, bool vbrModeEnabled);
	void Shutdown();

	bool IsVBRModeEnabled() {
		return mbVBRModeEnabled;
	}

	void Abort() {
		msigRead.signal();
		msigWrite.signal();
	}

	void CloseInput() {
		mbInputClosed = true;
		msigWrite.signal();
	}

	void CloseOutput() {
		mbOutputClosed = true;
		msigRead.signal();
	}

	void WaitUntilOutputClosed() {
		while(!mbOutputClosed)
			msigRead.wait();
	}

	uint32	GetSampleSize() const {
		return mSampleSize;
	}

	bool full() const {
		return mBuffer.full();
	}

	int getLevel() const {
		return mBuffer.getLevel();
	}

	int getSpace() const {
		return mBuffer.getSpace();
	}

	bool isInputClosed() const {
		return mbInputClosed;
	}

	bool isOutputClosed() const {
		return mbOutputClosed;
	}

	VDSignal& getReadSignal() { return msigRead; }
	VDSignal& getWriteSignal() { return msigWrite; }

	int ReadPartial(void *pBuffer, int bytes);
	void ReadWait() {
		msigWrite.wait();
	}

	bool Write(const void *data, int bytes, const VDAtomicInt *abortFlag);
	void *BeginWrite(int requested, int& actual);
	void EndWrite(int actual);

protected:
	uint32				mSampleSize;
	VDRingBuffer<char>	mBuffer;
	VDSignal			msigRead;
	VDSignal			msigWrite;
	bool				mbVBRModeEnabled;
	volatile bool		mbInputClosed;
	volatile bool		mbOutputClosed;
};

///////////////////////////////////////////////////////////////////////////
//
//	VDLoopThrottle
//
///////////////////////////////////////////////////////////////////////////

class VDLoopThrottle {
public:
	VDLoopThrottle();
	~VDLoopThrottle();

	void SetThrottleFactor(float factor) {
		mThrottleFactor = factor;
	}

	bool Delay();

	void BeginWait();
	void EndWait();

protected:
	VDAtomicFloat	mThrottleFactor;
	int		mWaitDepth;
	float	mWaitTime;
	uint32	mLastTime;
	bool	mbLastTimeValid;

	int		mWindowIndex;
	uint32	mWaitTimeWindow[16];
	uint32	mActiveTimeWindow[16];
	uint32	mWaitTimeWindowSum;
	uint32	mActiveTimeWindowSum;
};

#endif
