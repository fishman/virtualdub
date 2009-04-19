#ifndef f_VD2_DUBPROCESS_H
#define f_VD2_DUBPROCESS_H

#include <vd2/system/thread.h>
#include <vd2/system/time.h>
#include <vd2/system/profile.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <deque>

class IVDMediaOutput;
class IVDMediaOutputStream;
class IVDDubberOutputSystem;
class VDAudioPipeline;
class DubOptions;
class IVDVideoSource;
class IVDVideoDisplay;
class AudioStream;
class VideoTelecineRemover;
class VDStreamInterleaver;
class IVDVideoCompressor;
class IVDAsyncBlitter;
class IDubStatusHandler;
struct VDRenderVideoPipeFrameInfo;
class VDRenderOutputBufferTracker;
class VDRenderOutputBuffer;
class VDThreadedVideoCompressor;

class VDDubProcessThread : public VDThread, protected IVDTimerCallback {
public:
	VDDubProcessThread();
	~VDDubProcessThread();

	bool IsCompleted() const { return mbCompleted; }

	void SetParent(IDubberInternal *pParent);
	void SetAbortSignal(VDAtomicInt *pAbort);
	void SetStatusHandler(IDubStatusHandler *pStatusHandler);
	void SetInputDisplay(IVDVideoDisplay *pVideoDisplay);
	void SetOutputDisplay(IVDVideoDisplay *pVideoDisplay);
	void SetVideoFilterOutput(const VDPixmapLayout& layout);
	void SetVideoSources(IVDVideoSource *const *pVideoSources, uint32 count);
	void SetAudioSourcePresent(bool present);
	void SetAudioCorrector(AudioStreamL3Corrector *pCorrector);
	void SetVideoIVTC(VideoTelecineRemover *pIVTC);
	void SetVideoCompressor(IVDVideoCompressor *pCompressor, int maxThreads);

	void Init(const DubOptions& opts, DubVideoStreamInfo *pvsi, IVDDubberOutputSystem *pOutputSystem, AVIPipe *pVideoPipe, VDAudioPipeline *pAudioPipe, VDStreamInterleaver *pStreamInterleaver);
	void Shutdown();

	void Abort();
	void UpdateFrames();

	bool GetError(MyError& e) {
		if (mbError) {
			e.TransferFrom(mError);
			return true;
		}
		return false;
	}

	uint32 GetActivityCounter() {
		return mActivityCounter;
	}

	const char *GetCurrentAction() {
		return mpCurrentAction;
	}

	VDSignal *GetBlitterSignal();

	void SetThrottle(float f);

protected:
	enum VideoWriteResult {
		kVideoWriteOK,							// Frame was processed and written
		kVideoWritePushedPendingEmptyFrame,		// A pending null frame was processed instead of the current frame.
		kVideoWriteBufferedEmptyFrame,			// A pending null frame was buffered to track the codec's internal pipeline.
		kVideoWriteDelayed,						// Codec received intermediate frame; no output.
		kVideoWriteBuffered,					// Codec received display frame; no output.
		kVideoWriteDiscarded,					// Frame was discarded by preview QC.
		kVideoWritePullOnly						// Codec produced frame and didn't take input.
	};

	void NextSegment();

	void NotifyDroppedFrame(int exdata);
	void NotifyCompletedFrame(uint32 size, bool isKey);

	bool DoVideoFrameDropTest(const VDRenderVideoPipeFrameInfo& frameInfo);
	VideoWriteResult WriteVideoFrame(const VDRenderVideoPipeFrameInfo& frameInfo);
	void WriteFinishedVideoFrame(const void *data, uint32 size, bool isKey, bool renderEnabled, VDRenderOutputBuffer *pBuffer);

	void WritePendingEmptyVideoFrame();
	bool WriteAudio(sint32 count);

	void ThreadRun();
	void TimerCallback();
	void UpdateAudioStreamRate();

	static bool AsyncReinitDisplayCallback(int pass, void *pThisAsVoid, void *, bool aborting);
	static bool StaticAsyncUpdateOutputCallback(int pass, void *pThisAsVoid, void *pBuffer, bool aborting);
	bool AsyncUpdateOutputCallback(int pass, VDRenderOutputBuffer *pBuffer, bool aborting);

	const DubOptions		*opt;

	VDStreamInterleaver		*mpInterleaver;
	VDLoopThrottle			mLoopThrottle;
	IDubberInternal			*mpParent;

	// OUTPUT
	IVDMediaOutput			*mpAVIOut;
	IVDMediaOutputStream	*mpAudioOut;			// alias: AVIout->audioOut
	IVDMediaOutputStream	*mpVideoOut;			// alias: AVIout->videoOut
	IVDDubberOutputSystem	*mpOutputSystem;

	// AUDIO SECTION
	VDAudioPipeline			*mpAudioPipe;
	AudioStreamL3Corrector	*mpAudioCorrector;
	bool				mbAudioPresent;
	bool				mbAudioEnded;
	vdfastvector<char>	mAudioBuffer;

	// VIDEO SECTION
	AVIPipe					*mpVideoPipe;
	IVDVideoDisplay			*mpInputDisplay;
	IVDVideoDisplay			*mpOutputDisplay;
	VideoTelecineRemover	*mpInvTelecine;

	DubVideoStreamInfo	*mpVInfo;
	IVDAsyncBlitter		*mpBlitter;
	VDXFilterStateInfo	mfsi;
	IDubStatusHandler	*mpStatusHandler;

	IVDVideoCompressor	*mpVideoCompressor;
	VDThreadedVideoCompressor *mpThreadedVideoCompressor;

	VDRenderOutputBufferTracker *mpFrameBufferTracker;
	VDRenderOutputBufferTracker *mpDisplayBufferTracker;

	typedef vdfastvector<IVDVideoSource *> VideoSources;
	VideoSources		mVideoSources;

	std::deque<uint32>	mVideoNullFrameDelayQueue;		///< This is a queue used to track null frames between non-null frames. It runs parallel to a video codec's internal B-frame delay queue.
	uint32				mPendingNullVideoFrames;

	// PREVIEW
	bool				mbPreview;
	bool				mbFirstPacket;
	bool				mbAudioFrozen;
	bool				mbAudioFrozenValid;
	bool				mbSyncToAudioEvenClock;
	long				lDropFrames;

	// DECOMPRESSION PREVIEW
	vdautoptr<IVDVideoDecompressor>	mpVideoDecompressor;
	bool				mbVideoDecompressorEnabled;
	bool				mbVideoDecompressorPending;
	bool				mbVideoDecompressorErrored;
	VDPixmapBuffer		mVideoDecompBuffer;

	// ERROR HANDLING
	MyError				mError;
	bool				mbError;
	bool				mbCompleted;
	VDAtomicInt			*mpAbort;

	const char			*volatile mpCurrentAction;
	VDAtomicInt			mActivityCounter;
	VDAtomicInt			mRefreshFlag;

	VDRTProfileChannel	mProcessingProfileChannel;
	VDCallbackTimer		mFrameTimer;
};

#endif
