#include "stdafx.h"
#include <vd2/system/profile.h>
#include <vd2/Riza/videocodec.h>
#include "ThreadedVideoCompressor.h"

enum {
	// This is to work around an XviD decode bug (see VideoSource.h).
	kDecodeOverflowWorkaroundSize = 16,

	kReasonableBFrameBufferLimit = 100
};

///////////////////////////////////////////////////////////////////////////

void VDRenderOutputBufferTracker::Init(void *base, const VDPixmap& px) {
	vdrefptr<VDRenderOutputBuffer> buf(new VDRenderOutputBuffer(this));

	buf->Init(base, px);
}

void VDRenderOutputBufferTracker::Init(int count, const VDPixmapLayout& layout) {
	VDRenderBufferAllocator<VDRenderOutputBuffer>::Init();

	for(int i=0; i<count; ++i) {
		vdrefptr<VDRenderOutputBuffer> buf(new VDRenderOutputBuffer(this));

		buf->Init(layout);
	}
}

///////////////////////////////////////////////////////////////////////////

VDRenderOutputBuffer::VDRenderOutputBuffer(VDRenderOutputBufferTracker *tracker)
	: mpTracker(tracker)
{
}

VDRenderOutputBuffer::~VDRenderOutputBuffer() {
}

int VDRenderOutputBuffer::Release() {
	int rc = --mRefCount;

	if (!rc) {
		if (!mpTracker->FreeFrame(this))
			delete this;
	}

	return rc;
}

void VDRenderOutputBuffer::Init(void *base, const VDPixmap& px) {
	mpBase = base;
	mPixmap = px;
}

void VDRenderOutputBuffer::Init(const VDPixmapLayout& layout) {
	mBuffer.init(layout);
	mPixmap = mBuffer;
	mpBase = mBuffer.base();
}

///////////////////////////////////////////////////////////////////////////

class VDRenderPostCompressionBuffer;

void VDRenderPostCompressionBufferAllocator::Init(int count, uint32 auxsize) {
	VDRenderBufferAllocator<VDRenderPostCompressionBuffer>::Init();

	for(int i=0; i<count; ++i) {
		vdrefptr<VDRenderPostCompressionBuffer> buf(new VDRenderPostCompressionBuffer(this));

		buf->mOutputBuffer.resize(auxsize);
	}
}

VDRenderPostCompressionBuffer::VDRenderPostCompressionBuffer(VDRenderPostCompressionBufferAllocator *tracker)
	: mpTracker(tracker)
{
}

VDRenderPostCompressionBuffer::~VDRenderPostCompressionBuffer() {
}

int VDRenderPostCompressionBuffer::Release() {
	int rc = --mRefCount;

	if (!rc) {
		if (!mpTracker->FreeFrame(this))
			delete this;
	}

	return rc;
}

///////////////////////////////////////////////////////////////////////////

class VDThreadedVideoCompressorSlave : public VDThread {
public:
	VDThreadedVideoCompressorSlave();
	~VDThreadedVideoCompressorSlave();

	void Init(VDThreadedVideoCompressor *parent, IVDVideoCompressor *compressor);
	void Shutdown();

protected:
	void ThreadRun();

	VDThreadedVideoCompressor *mpParent;
	IVDVideoCompressor *mpCompressor;
};

/////////

VDThreadedVideoCompressor::VDThreadedVideoCompressor()
	: mpThreads(NULL)
	, mThreadCount(0)
	, mbClientFlushInProgress(false)
	, mBarrier(0)
	, mFrameSkipCounter(0)
	, mpAllocator(new VDRenderPostCompressionBufferAllocator)
	, mFramesSubmitted(0)
	, mFramesProcessed(0)
	, mFramesBufferedInFlush(0)
	, mbFlushInProgress(false)
	, mbLoopDetectedDuringFlush(false)
	, mInputBufferCount(0)
{
}

VDThreadedVideoCompressor::~VDThreadedVideoCompressor() {
}

VDThreadedVideoCompressor::FlushStatus VDThreadedVideoCompressor::GetFlushStatus() {
	uint32 result = 0;

	vdsynchronized(mMutex) {
		if (mbLoopDetectedDuringFlush)
			result |= kFlushStatusLoopingDetected;
	}

	return (FlushStatus)result;
}

void VDThreadedVideoCompressor::Init(int threads, IVDVideoCompressor *pBaseCompressor) {
	VDASSERT(threads <= 1);

	Shutdown();

	uint32 compsize = pBaseCompressor->GetMaxOutputSize() + kDecodeOverflowWorkaroundSize;

	mpAllocator->Init(threads + 1, compsize);

	mpBaseCompressor = pBaseCompressor;
	mbInErrorState = false;
	mInputBufferCount.Reset(0);
	mThreadCount = threads;
	mpThreads = new VDThreadedVideoCompressorSlave[threads];

	if (threads)
		mpThreads[0].Init(this, pBaseCompressor);
}

void VDThreadedVideoCompressor::Shutdown() {
	vdsynchronized(mMutex) {
		mbInErrorState = true;
	}

	mpAllocator->Shutdown();

	if (mpThreads) {
		for(int i=0; i<mThreadCount; ++i) {
			mInputBufferCount.Post();
		}

		delete[] mpThreads;
	}

	FlushQueues();
}

void VDThreadedVideoCompressor::SetFlush(bool flush) {
	if (mbClientFlushInProgress == flush)
		return;

	mbClientFlushInProgress = flush;
	if (flush) {
		vdsynchronized(mMutex) {
			mbFlushInProgress = true;
		}
	} else {
		// barrier all threads
		for(int i=0; i<mThreadCount; ++i)
			mBarrier.Wait();

		// flush all queues
		FlushQueues();

		// clear the flush flag
		vdsynchronized(mMutex) {
			mbFlushInProgress = false;
			mbLoopDetectedDuringFlush = false;
		}

		// release all threads
		for(int i=0; i<mThreadCount; ++i)
			mBarrier.Post();
	}
}

void VDThreadedVideoCompressor::SkipFrame() {
	if (mThreadCount)
		++mFrameSkipCounter;
	else
		mpBaseCompressor->SkipFrame();
}

void VDThreadedVideoCompressor::Restart() {
	// barrier all threads
	for(int i=0; i<mThreadCount; ++i)
		mBarrier.Wait();

	mpBaseCompressor->Restart();

	// release all threads
	for(int i=0; i<mThreadCount; ++i)
		mBarrier.Post();
}

bool VDThreadedVideoCompressor::ExchangeBuffer(VDRenderOutputBuffer *buffer, VDRenderPostCompressionBuffer **ppOutBuffer) {
	bool success = false;

	if (mThreadCount) {
		vdsynchronized(mMutex) {
			if (mbInErrorState)
				throw mError;

			if (buffer) {
				buffer->AddRef();
				mInputBuffer.push_back(buffer);
				mInputBufferCount.Post();

				if (!mbFlushInProgress)
					++mFramesSubmitted;
			}

			if (ppOutBuffer) {
				if (!mOutputBuffer.empty()) {
					*ppOutBuffer = mOutputBuffer.front();
					mOutputBuffer.pop_front();
					success = true;
				}
			}
		}
	} else {
		if (buffer) {
			if (!mbFlushInProgress)
				++mFramesSubmitted;

			if (!ProcessFrame(buffer, mpBaseCompressor, NULL)) {
				if (mbInErrorState)
					throw mError;

				return false;
			}
		}

		if (ppOutBuffer) {
			if (!mOutputBuffer.empty()) {
				*ppOutBuffer = mOutputBuffer.front();
				mOutputBuffer.pop_front();
				success = true;
			}
		}
	}

	return success;
}

void VDThreadedVideoCompressor::RunSlave(IVDVideoCompressor *compressor) {
	VDRTProfileChannel	profchan("VideoCompressor");

	for(;;) {
		mBarrier.Post();
		mInputBufferCount.Wait();
		mBarrier.Wait();

		vdrefptr<VDRenderOutputBuffer> buffer;
		int framesToSkip = 0;

		vdsynchronized(mMutex) {
			if (mbInErrorState)
				break;

			if (!mInputBuffer.empty()) {
				buffer.set(mInputBuffer.front());
				mInputBuffer.pop_front();

				framesToSkip = mFrameSkipCounter.xchg(0);
			}
		}

		// If we are coming off a flush, it's possible that we don't have a frame here.
		if (!buffer) {
			VDASSERT(!framesToSkip);
			continue;
		}

		while(framesToSkip--)
			compressor->SkipFrame();

		if (!ProcessFrame(buffer, compressor, &profchan))
			break;
	}
}

bool VDThreadedVideoCompressor::ProcessFrame(VDRenderOutputBuffer *pBuffer, IVDVideoCompressor *pCompressor, VDRTProfileChannel *pProfileChannel) {
	vdrefptr<VDRenderPostCompressionBuffer> pOutputBuffer;
	if (!mpAllocator->AllocFrame(-1, ~pOutputBuffer)) {
		VDASSERT(mbInErrorState);
		return false;
	}

	bool isKey;
	uint32 packedSize;
	bool valid;

	if (pProfileChannel)
		pProfileChannel->Begin(0xe0e0e0, "V-Compress");

	try {
		valid = pCompressor->CompressFrame(pOutputBuffer->mOutputBuffer.data(), pBuffer->mpBase, isKey, packedSize);

		if (!valid) {
			vdsynchronized(mMutex) {
				if (mbFlushInProgress) {
					if (++mFramesBufferedInFlush >= kReasonableBFrameBufferLimit) {
						mFramesBufferedInFlush = 0;
						mbLoopDetectedDuringFlush = true;
					}
				}
			}
		}

		if (pProfileChannel)
			pProfileChannel->End();
	} catch(MyError& e) {
		if (pProfileChannel)
			pProfileChannel->End();

		vdsynchronized(mMutex) {
			if (!mbInErrorState) {
				mError.TransferFrom(e);
				mbInErrorState = true;
			}
		}
		return false;
	}

	if (valid) {
		pOutputBuffer->mOutputSize = packedSize;
		pOutputBuffer->mbOutputIsKey = isKey;

		vdsynchronized(mMutex) {
			mFramesBufferedInFlush = 0;

			mOutputBuffer.push_back(pOutputBuffer);
		}

		pOutputBuffer.release();
	}

	return true;
}

void VDThreadedVideoCompressor::FlushQueues() {
	vdsynchronized(mMutex) {
		while(!mOutputBuffer.empty()) {
			mOutputBuffer.front()->Release();
			mOutputBuffer.pop_front();
		}

		while(!mInputBuffer.empty()) {
			mInputBuffer.front()->Release();
			mInputBuffer.pop_front();
		}
	}
}

/////////

VDThreadedVideoCompressorSlave::VDThreadedVideoCompressorSlave() {
}

VDThreadedVideoCompressorSlave::~VDThreadedVideoCompressorSlave() {
}

void VDThreadedVideoCompressorSlave::Init(VDThreadedVideoCompressor *parent, IVDVideoCompressor *compressor) {
	mpParent = parent;
	mpCompressor = compressor;

	ThreadStart();
}

void VDThreadedVideoCompressorSlave::ThreadRun() {
	mpParent->RunSlave(mpCompressor);
}
