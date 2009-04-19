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

#define INITGUID
#include <windows.h>
#include <mmsystem.h>
#include <cguid.h>
#include <dsound.h>

#include <vd2/system/math.h>
#include <vd2/system/text.h>
#include <vd2/system/VDString.h>
#include <vd2/Riza/audioout.h>

extern HINSTANCE g_hInst;

class VDAudioOutputWaveOutW32 : public IVDAudioOutput {
public:
	VDAudioOutputWaveOutW32();
	~VDAudioOutputWaveOutW32();

	bool	Init(uint32 bufsize, uint32 bufcount, const tWAVEFORMATEX *wf, const wchar_t *preferredDevice);
	void	Shutdown();
	void	GoSilent();

	bool	IsSilent();
	bool	IsFrozen();
	uint32	GetAvailSpace();
	uint32	GetBufferLevel();
	sint32	GetPosition();
	sint32	GetPositionBytes();
	double	GetPositionTime();

	bool	Start();
	bool	Stop();
	bool	Flush();

	bool	Write(const void *data, uint32 len);
	bool	Finalize(uint32 timeout);

private:
	bool	CheckBuffers();
	bool	WaitBuffers(uint32 timeout);

	uint32	mBlockHead;
	uint32	mBlockTail;
	uint32	mBlockWriteOffset;
	uint32	mBlocksPending;
	uint32	mBlockSize;
	uint32	mBlockCount;
	vdblock<char> mBuffer;
	vdblock<WAVEHDR> mHeaders;

	HWAVEOUT__ *mhWaveOut;
	void *	mhWaveEvent;
	uint32	mSamplesPerSec;
	uint32	mAvgBytesPerSec;
	VDCriticalSection	mcsWaveDevice;

	enum InitState {
		kStateNone		= 0,
		kStateOpened	= 1,
		kStatePlaying	= 2,
		kStateSilent	= 10,
	} mCurState;
};

IVDAudioOutput *VDCreateAudioOutputWaveOutW32() {
	return new VDAudioOutputWaveOutW32;
}

VDAudioOutputWaveOutW32::VDAudioOutputWaveOutW32()
	: mBlockHead(0)
	, mBlockTail(0)
	, mBlockWriteOffset(0)
	, mBlocksPending(0)
	, mBlockSize(0)
	, mBlockCount(0)
	, mhWaveOut(NULL)
	, mhWaveEvent(NULL)
	, mSamplesPerSec(0)
	, mAvgBytesPerSec(0)
	, mCurState(kStateNone)
{
}

VDAudioOutputWaveOutW32::~VDAudioOutputWaveOutW32() {
	Shutdown();
}

bool VDAudioOutputWaveOutW32::Init(uint32 bufsize, uint32 bufcount, const WAVEFORMATEX *wf, const wchar_t *preferredDevice) {
	UINT deviceID = WAVE_MAPPER;

	if (preferredDevice && *preferredDevice) {
		UINT numDevices = waveOutGetNumDevs();

		for(UINT i=0; i<numDevices; ++i) {
			WAVEOUTCAPSA caps = {0};

			if (MMSYSERR_NOERROR == waveOutGetDevCapsA(i, &caps, sizeof(caps))) {
				const VDStringW key(VDTextAToW(caps.szPname).c_str());

				if (key == preferredDevice) {
					deviceID = i;
					break;
				}
			}
		}
	}

	mBuffer.resize(bufsize * bufcount);
	mBlockHead = 0;
	mBlockTail = 0;
	mBlockWriteOffset = 0;
	mBlocksPending = 0;
	mBlockSize = bufsize;
	mBlockCount = bufcount;

	if (!mhWaveEvent) {
		mhWaveEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		if (!mhWaveEvent)
			return false;
	}

	MMRESULT res = waveOutOpen(&mhWaveOut, deviceID, wf, (DWORD_PTR)mhWaveEvent, 0, CALLBACK_EVENT);
	if (MMSYSERR_NOERROR != res) {
		Shutdown();
		return false;
	}

	mCurState = kStateOpened;
	mSamplesPerSec = wf->nSamplesPerSec;
	mAvgBytesPerSec = wf->nAvgBytesPerSec;

	// Hmmm... we can't allocate buffers while the wave device
	// is active...
	mHeaders.resize(bufcount);
	memset(mHeaders.data(), 0, bufcount * sizeof mHeaders[0]);

	for(uint32 i=0; i<bufcount; ++i) {
		WAVEHDR& hdr = mHeaders[i];

		hdr.dwBufferLength	= bufsize;
		hdr.dwBytesRecorded	= 0;
		hdr.dwFlags			= 0;
		hdr.dwLoops			= 0;
		hdr.dwUser			= 0;
		hdr.lpData			= mBuffer.data() + bufsize * i;

		res = waveOutPrepareHeader(mhWaveOut, &hdr, sizeof hdr);
		if (MMSYSERR_NOERROR != res) {
			Shutdown();
			return false;
		}
	}

	waveOutPause(mhWaveOut);
	return true;
}

void VDAudioOutputWaveOutW32::Shutdown() {
	if (mCurState == kStateSilent)
		return;

	Stop();

	if (!mHeaders.empty()) {
		for(int i=mHeaders.size()-1; i>=0; --i) {
			WAVEHDR& hdr = mHeaders[i];

			if (hdr.dwFlags & WHDR_PREPARED)
				waveOutUnprepareHeader(mhWaveOut, &hdr, sizeof hdr);
		}
	}

	mHeaders.clear();
	mBuffer.clear();
	mBlocksPending = 0;
	mBlockCount = 0;
	mBlockSize = 0;

	if (mhWaveOut) {
		waveOutClose(mhWaveOut);
		mhWaveOut = NULL;
	}

	if (mhWaveEvent) {
		CloseHandle(mhWaveEvent);
		mhWaveEvent = NULL;
	}

	mCurState = kStateNone;
}

void VDAudioOutputWaveOutW32::GoSilent() {
	mCurState = kStateSilent;
}

bool VDAudioOutputWaveOutW32::IsSilent() {
	return mCurState == kStateSilent;
}

bool VDAudioOutputWaveOutW32::Start() {
	if (mCurState == kStateSilent)
		return true;

	if (mCurState < kStateOpened)
		return false;

	if (MMSYSERR_NOERROR != waveOutRestart(mhWaveOut))
		return false;

	mCurState = kStatePlaying;

	return true;
}

bool VDAudioOutputWaveOutW32::Stop() {
	if (mCurState == kStateSilent) return true;

	if (mCurState >= kStateOpened) {
		if (MMSYSERR_NOERROR != waveOutReset(mhWaveOut))
			return false;

		mCurState = kStateOpened;

		CheckBuffers();
	}

	return true;
}

bool VDAudioOutputWaveOutW32::CheckBuffers() {
	if (mCurState == kStateSilent) return true;

	if (!mBlocksPending)
		return false;

	WAVEHDR& hdr = mHeaders[mBlockHead];

	if (!(hdr.dwFlags & WHDR_DONE))
		return false;

	++mBlockHead;
	if (mBlockHead >= mBlockCount)
		mBlockHead = 0;
	--mBlocksPending;
	VDASSERT(mBlocksPending >= 0);
	return true;
}

bool VDAudioOutputWaveOutW32::WaitBuffers(uint32 timeout) {
	if (mCurState == kStateSilent) return true;

	if (mhWaveOut && timeout) {
		for(;;) {
			if (WAIT_OBJECT_0 != WaitForSingleObject(mhWaveEvent, timeout))
				return false;

			if (CheckBuffers())
				return true;
		}
	}

	return CheckBuffers();
}

uint32 VDAudioOutputWaveOutW32::GetAvailSpace() {
	CheckBuffers();
	return (mBlockCount - mBlocksPending) * mBlockSize - mBlockWriteOffset;
}

uint32 VDAudioOutputWaveOutW32::GetBufferLevel() {
	CheckBuffers();

	uint32 level = mBlocksPending * mBlockSize;
	if (mBlockWriteOffset) {
		level -= mBlockSize;
		level += mBlockWriteOffset;
	}

	return level;
}

bool VDAudioOutputWaveOutW32::Write(const void *data, uint32 len) {
	if (mCurState == kStateSilent)
		return true;

	CheckBuffers();

	while(len) {
		if (mBlocksPending >= mBlockCount) {
			if (mCurState == kStateOpened) {
				if (!Start())
					return false;
			}

			if (!WaitBuffers(0)) {
				if (!WaitBuffers(INFINITE)) {
					return false;
				}
				continue;
			}
			break;
		}

		WAVEHDR& hdr = mHeaders[mBlockTail];

		uint32 tc = mBlockSize - mBlockWriteOffset;
		if (tc > len)
			tc = len;

		if (tc) {
			if (data) {
				memcpy((char *)hdr.lpData + mBlockWriteOffset, data, tc);
				data = (const char *)data + tc;
			} else
				memset((char *)hdr.lpData + mBlockWriteOffset, 0, tc);

			mBlockWriteOffset += tc;
			len -= tc;
		}

		if (mBlockWriteOffset >= mBlockSize) {
			if (!Flush())
				return false;
		}
	}

	return true;
}

bool VDAudioOutputWaveOutW32::Flush() {
	if (mCurState == kStateOpened) {
		if (!Start())
			return false;
	}

	if (mBlockWriteOffset <= 0)
		return true;

	WAVEHDR& hdr = mHeaders[mBlockTail];

	hdr.dwBufferLength = mBlockWriteOffset;
	hdr.dwFlags &= ~WHDR_DONE;
	MMRESULT res = waveOutWrite(mhWaveOut, &hdr, sizeof hdr);
	mBlockWriteOffset = 0;

	if (res != MMSYSERR_NOERROR)
		return false;

	++mBlockTail;
	if (mBlockTail >= mBlockCount)
		mBlockTail = 0;
	++mBlocksPending;
	VDASSERT(mBlocksPending <= mBlockCount);
	return true;
}

bool VDAudioOutputWaveOutW32::Finalize(uint32 timeout) {
	if (mCurState == kStateSilent) return true;

	Flush();

	while(CheckBuffers(), mBlocksPending) {
		if (WAIT_OBJECT_0 != WaitForSingleObject(mhWaveEvent, timeout))
			return false;
	}

	return true;
}

sint32 VDAudioOutputWaveOutW32::GetPosition() {
	MMTIME mmtime;

	if (mCurState != kStatePlaying) return -1;

	mmtime.wType = TIME_SAMPLES;

	MMRESULT res;

	vdsynchronized(mcsWaveDevice) {
		res = waveOutGetPosition(mhWaveOut, &mmtime, sizeof mmtime);
	}

	if (MMSYSERR_NOERROR != res)
		return -1;

	switch(mmtime.wType) {
	case TIME_BYTES:
		return MulDiv(mmtime.u.cb, 1000, mAvgBytesPerSec);
	case TIME_MS:
		return mmtime.u.ms;
	case TIME_SAMPLES:
		return MulDiv(mmtime.u.sample, 1000, mSamplesPerSec);
	}

	return -1;
}

sint32 VDAudioOutputWaveOutW32::GetPositionBytes() {
	MMTIME mmtime;

	if (mCurState != kStatePlaying) return -1;

	mmtime.wType = TIME_BYTES;

	MMRESULT res;

	vdsynchronized(mcsWaveDevice) {
		res = waveOutGetPosition(mhWaveOut, &mmtime, sizeof mmtime);
	}

	if (MMSYSERR_NOERROR != res)
		return -1;

	switch(mmtime.wType) {
	case TIME_BYTES:
		return mmtime.u.cb;
	case TIME_MS:
		return MulDiv(mmtime.u.ms, mAvgBytesPerSec, 1000);
	case TIME_SAMPLES:
		return MulDiv(mmtime.u.sample, mAvgBytesPerSec, mSamplesPerSec);
	}

	return -1;
}

double VDAudioOutputWaveOutW32::GetPositionTime() {
	MMTIME mmtime;

	if (mCurState != kStatePlaying) return -1;

	mmtime.wType = TIME_MS;

	MMRESULT res;

	vdsynchronized(mcsWaveDevice) {
		res = waveOutGetPosition(mhWaveOut, &mmtime, sizeof mmtime);
	}

	if (MMSYSERR_NOERROR != res)
		return -1;

	switch(mmtime.wType) {
	case TIME_BYTES:
		return (double)mmtime.u.cb / (double)mAvgBytesPerSec;
	case TIME_MS:
		return (double)mmtime.u.ms / 1000.0;
	case TIME_SAMPLES:
		return (double)mmtime.u.sample / (double)mSamplesPerSec;
	}

	return -1;
}

bool VDAudioOutputWaveOutW32::IsFrozen() {
	if (mCurState != kStatePlaying)
		return true;

	CheckBuffers();

	return !mBlocksPending;
}

///////////////////////////////////////////////////////////////////////////

class VDAudioOutputDirectSoundW32 : public IVDAudioOutput {
public:
	VDAudioOutputDirectSoundW32();
	~VDAudioOutputDirectSoundW32();

	bool	Init(uint32 bufsize, uint32 bufcount, const tWAVEFORMATEX *wf, const wchar_t *preferredDevice);
	void	Shutdown();
	void	GoSilent();

	bool	IsSilent();
	bool	IsFrozen();
	uint32	GetAvailSpace();
	uint32	GetBufferLevel();
	sint32	GetPosition();
	sint32	GetPositionBytes();
	double	GetPositionTime();

	bool	Start();
	bool	Stop();
	bool	Flush();

	bool	Write(const void *data, uint32 len);
	bool	Finalize(uint32 timeout);

private:
	bool	Init2(uint32 bufsize, uint32 bufcount, const tWAVEFORMATEX *wf);

	static ATOM sWndClass;

	HWND				mhwnd;
	HMODULE				mhmodDS;
	IDirectSound8		*mpDS8;
	IDirectSoundBuffer8	*mpDSBuffer;

	uint32	mBufferSize;
	uint32	mTailCursor;

	double	mMillisecsPerByte;

	enum InitState {
		kStateNone		= 0,
		kStateOpened	= 1,
		kStatePlaying	= 2,
		kStateSilent	= 10,
	} mCurState;
};

ATOM VDAudioOutputDirectSoundW32::sWndClass;

IVDAudioOutput *VDCreateAudioOutputDirectSoundW32() {
	return new VDAudioOutputDirectSoundW32;
}

VDAudioOutputDirectSoundW32::VDAudioOutputDirectSoundW32()
	: mhwnd(NULL)
	, mhmodDS(NULL)
	, mpDS8(NULL)
	, mpDSBuffer(NULL)
{
}

VDAudioOutputDirectSoundW32::~VDAudioOutputDirectSoundW32() {
}

bool VDAudioOutputDirectSoundW32::Init(uint32 bufsize, uint32 bufcount, const tWAVEFORMATEX *wf, const wchar_t *preferredDevice) {
	if (!Init2(bufsize, bufcount, wf)) {
		Shutdown();
		return false;
	}
	return true;
}

bool VDAudioOutputDirectSoundW32::Init2(uint32 bufsize, uint32 bufcount, const tWAVEFORMATEX *wf) {
	mBufferSize = bufsize * bufcount;
	mMillisecsPerByte = 1000.0 * (double)wf->nBlockAlign / (double)wf->nAvgBytesPerSec;

	// attempt to load DirectSound library
	mhmodDS = LoadLibraryA("dsound");
	if (!mhmodDS)
		return false;

	typedef HRESULT (WINAPI *tpDirectSoundCreate8)(LPCGUID, LPDIRECTSOUND8 *, LPUNKNOWN);
	tpDirectSoundCreate8 pDirectSoundCreate8 = (tpDirectSoundCreate8)GetProcAddress(mhmodDS, "DirectSoundCreate8");
	if (!pDirectSoundCreate8) {
		VDDEBUG("VDAudioOutputDirectSound: Cannot find DirectSoundCreate8 entry point!\n");
		return false;
	}

	// attempt to create DirectSound object
	HRESULT hr = pDirectSoundCreate8(NULL, &mpDS8, NULL);
	if (FAILED(hr)) {
		VDDEBUG("VDAudioOutputDirectSound: Failed to create DirectSound object! hr=%08x\n", hr);
		return false;
	}

	// register window class
	if (!sWndClass) {
		WNDCLASS wc = {0};
		wc.lpfnWndProc = DefWindowProc;
		wc.lpszClassName = "VirtualDub DirectSound window";
		wc.hInstance = g_hInst;
		sWndClass = RegisterClass(&wc);
		if (!sWndClass)
			return false;
	}

	// create window
	mhwnd = CreateWindowA((LPCTSTR)sWndClass, "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, g_hInst, NULL);
	if (!mhwnd) {
		VDDEBUG("VDAudioOutputDirectSound: Failed to create window!\n");
		return false;
	}

	// Set cooperative level.
	//
	// From microsoft.public.win32.programmer.directx.audio, by an SDE on the Windows AV team:
	//
	// "I can't speak for all DirectX components but DirectSound does not
	//  subclass the window procedure.  It simply uses the window handle to
	//  determine (every 1/2 second, in a seperate thread) if the window that
	//  corresponds to the handle has the focus (Actually, it is slightly more
	//  complicated than that, but that is close enough for this discussion). 
	//  You can feel free to use the desktop window or console window for the
	//  window handle if you are going to create GLOBAL_FOCUS buffers."
	//
	// Alright, you guys said we could do it!
	//
	hr = mpDS8->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);
	if (FAILED(hr)) {
		VDDEBUG("VDAudioOutputDirectSound: Failed to set cooperative level! hr=%08x\n", hr);
		return false;
	}

	// create looping secondary buffer
	DSBUFFERDESC dsd={sizeof(DSBUFFERDESC)};
	dsd.dwFlags			= DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
	dsd.dwBufferBytes	= bufsize * bufcount;
	dsd.lpwfxFormat		= (WAVEFORMATEX *)wf;
	dsd.guid3DAlgorithm	= DS3DALG_DEFAULT;

	IDirectSoundBuffer *pDSB;
	hr = mpDS8->CreateSoundBuffer(&dsd, &pDSB, NULL);
	if (FAILED(hr)) {
		VDDEBUG("VDAudioOutputDirectSound: Failed to create secondary buffer! hr=%08x\n", hr);
		return false;
	}

	// query to IDirectSoundBuffer8
	hr = pDSB->QueryInterface(IID_IDirectSoundBuffer8, (void **)&mpDSBuffer);
	pDSB->Release();
	if (FAILED(hr)) {
		VDDEBUG("VDAudioOutputDirectSound: Failed to obtain IDirectSoundBuffer8 interface! hr=%08x\n", hr);
		return false;
	}

	// all done!
	mTailCursor = 0;
	return true;
}

void VDAudioOutputDirectSoundW32::Shutdown() {
	if (mpDSBuffer) {
		mpDSBuffer->Release();
		mpDSBuffer = NULL;
	}

	if (mpDS8) {
		mpDS8->Release();
		mpDS8 = NULL;
	}

	if (mhmodDS) {
		FreeLibrary(mhmodDS);
		mhmodDS = NULL;
	}

	if (mhwnd) {
		DestroyWindow(mhwnd);
		mhwnd = NULL;
	}
}

void VDAudioOutputDirectSoundW32::GoSilent() {
}

bool VDAudioOutputDirectSoundW32::IsSilent() {
	return !mpDSBuffer;
}

bool VDAudioOutputDirectSoundW32::IsFrozen() {
	return false;
}

uint32 VDAudioOutputDirectSoundW32::GetAvailSpace() {
	DWORD playCursor, writeCursor;
	HRESULT hr = mpDSBuffer->GetCurrentPosition(&playCursor, &writeCursor);

	sint32 space = playCursor - mTailCursor;
	if (space < 0)
		space += mBufferSize;

	return (uint32)space;
}

uint32 VDAudioOutputDirectSoundW32::GetBufferLevel() {
	return mBufferSize - GetAvailSpace();
}

sint32 VDAudioOutputDirectSoundW32::GetPosition() {
	DWORD playCursor;
	HRESULT hr = mpDSBuffer->GetCurrentPosition(&playCursor, NULL);

	return VDRoundToInt32(playCursor * mMillisecsPerByte);
}

sint32 VDAudioOutputDirectSoundW32::GetPositionBytes() {
	DWORD playCursor;
	HRESULT hr = mpDSBuffer->GetCurrentPosition(&playCursor, NULL);

	return playCursor;
}

double VDAudioOutputDirectSoundW32::GetPositionTime() {
	return GetPosition() / 1000.0;
}

bool VDAudioOutputDirectSoundW32::Start() {
	if (!mpDSBuffer)
		return true;

	HRESULT hr = mpDSBuffer->Play(0, 0, DSBPLAY_LOOPING);

	return SUCCEEDED(hr);
}

bool VDAudioOutputDirectSoundW32::Stop() {
	if (!mpDSBuffer)
		return true;

	HRESULT hr = mpDSBuffer->Stop();
	return SUCCEEDED(hr);
}

bool VDAudioOutputDirectSoundW32::Flush() {
	return true;
}

bool VDAudioOutputDirectSoundW32::Write(const void *data, uint32 len) {
	if (!mpDSBuffer)
		return true;

	while(len > 0) {
		DWORD playCursor, writeCursor;
		HRESULT hr = mpDSBuffer->GetCurrentPosition(&playCursor, &writeCursor);
		if (FAILED(hr)) {
			return false;
		}

		sint32 tc = (sint32)playCursor - mTailCursor;
		if (tc < 0)
			tc += mBufferSize;

		if (!tc) {
			::Sleep(1);
			continue;
		}

		if ((uint32)tc > len)
			tc = len;

		LPVOID p1, p2;
		DWORD tc1, tc2;
		hr = mpDSBuffer->Lock(mTailCursor, tc, &p1, &tc1, &p2, &tc2, 0);
		if (FAILED(hr))
			return false;

		memcpy(p1, data, tc1);
		data = (char *)data + tc1;
		memcpy(p2, data, tc2);
		data = (char *)data + tc2;

		mpDSBuffer->Unlock(p1, tc1, p2, tc2);

		len -= tc;

		mTailCursor += tc;
		if (mTailCursor >= mBufferSize)
			mTailCursor -= mBufferSize;
	}

	return true;
}

bool VDAudioOutputDirectSoundW32::Finalize(uint32 timeout) {
	return true;
}
