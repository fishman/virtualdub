//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2006 Avery Lee
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

#ifdef _MSC_VER
	#pragma warning(disable: 4786)		// STFU
#endif

#include <vd2/Riza/capdriver.h>
#include <vd2/Riza/opengl.h>
#include <vd2/system/binary.h>
#include <vd2/system/vdstring.h>
#include <vd2/system/error.h>
#include <vd2/system/time.h>
#include <vd2/system/profile.h>
#include <vd2/system/registry.h>
#include <windows.h>
#include <vector>
#include "resource.h"

using namespace nsVDCapture;

extern HINSTANCE g_hInst;

namespace {
	#include "cap_screen_glshaders.inl"

	DWORD AutodetectCaptureBltMode() {
		OSVERSIONINFOA verinfo={sizeof(OSVERSIONINFOA)};

		if (GetVersionEx(&verinfo)) {
			if (verinfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
				// Windows 2000 or newer
				if (verinfo.dwMajorVersion >= 5)
					return SRCCOPY | CAPTUREBLT;
			} else if (verinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
				// Test for Windows 98 or newer
				if (verinfo.dwMajorVersion >= 5 || (verinfo.dwMajorVersion == 4 && verinfo.dwMinorVersion >= 10))
					return SRCCOPY | CAPTUREBLT;
			}

		}

		return SRCCOPY;
	}
}

class VDCaptureDriverScreen : public IVDCaptureDriver, public IVDTimerCallback {
	VDCaptureDriverScreen(const VDCaptureDriverScreen&);
	VDCaptureDriverScreen& operator=(const VDCaptureDriverScreen&);
public:
	VDCaptureDriverScreen();
	~VDCaptureDriverScreen();

	void	*AsInterface(uint32 id) { return NULL; }

	bool	Init(VDGUIHandle hParent);
	void	Shutdown();

	void	SetCallback(IVDCaptureDriverCallback *pCB);

	void	LockUpdates() {}
	void	UnlockUpdates() {}

	bool	IsHardwareDisplayAvailable();

	void	SetDisplayMode(nsVDCapture::DisplayMode m);
	nsVDCapture::DisplayMode		GetDisplayMode();

	void	SetDisplayRect(const vdrect32& r);
	vdrect32	GetDisplayRectAbsolute();
	void	SetDisplayVisibility(bool vis);

	void	SetFramePeriod(sint32 ms);
	sint32	GetFramePeriod();

	uint32	GetPreviewFrameCount();

	bool	GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat);
	bool	SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size);

	bool	SetTunerChannel(int channel) { return false; }
	int		GetTunerChannel() { return -1; }
	bool	GetTunerChannelRange(int& minChannel, int& maxChannel) { return false; }
	uint32	GetTunerFrequencyPrecision() { return 0; }
	uint32	GetTunerExactFrequency() { return 0; }
	bool	SetTunerExactFrequency(uint32 freq) { return false; }
	nsVDCapture::TunerInputMode	GetTunerInputMode() { return kTunerInputUnknown; }
	void	SetTunerInputMode(nsVDCapture::TunerInputMode tunerMode) {}

	int		GetAudioDeviceCount();
	const wchar_t *GetAudioDeviceName(int idx);
	bool	SetAudioDevice(int idx);
	int		GetAudioDeviceIndex();
	bool	IsAudioDeviceIntegrated(int idx) { return false; }

	int		GetVideoSourceCount();
	const wchar_t *GetVideoSourceName(int idx);
	bool	SetVideoSource(int idx);
	int		GetVideoSourceIndex();

	int		GetAudioSourceCount();
	const wchar_t *GetAudioSourceName(int idx);
	bool	SetAudioSource(int idx);
	int		GetAudioSourceIndex();

	int		GetAudioInputCount();
	const wchar_t *GetAudioInputName(int idx);
	bool	SetAudioInput(int idx);
	int		GetAudioInputIndex();

	int		GetAudioSourceForVideoSource(int idx) { return -2; }

	bool	IsAudioCapturePossible();
	bool	IsAudioCaptureEnabled();
	bool	IsAudioPlaybackPossible() { return false; }
	bool	IsAudioPlaybackEnabled() { return false; }
	void	SetAudioCaptureEnabled(bool b);
	void	SetAudioAnalysisEnabled(bool b);
	void	SetAudioPlaybackEnabled(bool b) {}

	void	GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats);

	bool	GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat);
	bool	SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size);

	bool	IsDriverDialogSupported(nsVDCapture::DriverDialog dlg);
	void	DisplayDriverDialog(nsVDCapture::DriverDialog dlg);

	bool	IsPropertySupported(uint32 id) { return false; }
	sint32	GetPropertyInt(uint32 id, bool *pAutomatic) { if (pAutomatic) *pAutomatic = true; return 0; }
	void	SetPropertyInt(uint32 id, sint32 value, bool automatic) {}
	void	GetPropertyInfoInt(uint32 id, sint32& minVal, sint32& maxVal, sint32& step, sint32& defaultVal, bool& automatic, bool& manual) {}

	bool	CaptureStart();
	void	CaptureStop();
	void	CaptureAbort();

protected:
	void	SyncCaptureStop();
	void	SyncCaptureAbort();
	void	InitMixerSupport();
	void	ShutdownMixerSupport();
	bool	InitWaveAnalysis();
	void	ShutdownWaveAnalysis();
	bool	InitVideoBuffer();
	void	ShutdownVideoBuffer();
	void	FlushFrameQueue();
	sint64	ComputeGlobalTime();
	void	DoFrame();
	void	DispatchFrame(const void *data, uint32 size, sint64 timestamp);

	void	TimerCallback();

	void	ConvertToYV12_GL_NV1x(int w, int h, float u, float v);
	void	ConvertToYV12_GL_NV2x(int w, int h, float u, float v);
	void	ConvertToYV12_GL_ATIFS(int w, int h, float u, float v);
	void	ConvertToYUY2_GL_NV1x(int w, int h, float u, float v);
	void	ConvertToYUY2_GL_NV2x_ATIFS(int w, int h, float u, float v, bool atifs);
	GLuint	GetOcclusionQueryPixelCountSafe(GLuint query);

	void	LoadSettings();
	void	SaveSettings();

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK StaticWndProcGL(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK VideoSourceDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

	enum { kPreviewTimerID = 100 };

	HWND	mhwndParent;
	HWND	mhwnd;
	HWND	mhwndGL;
	HWND	mhwndGLDraw;

	bool	mbCapBuffersInited;
	HDC		mhdcOffscreen;
	HBITMAP	mhbmOffscreen;
	void	*mpOffscreenData;
	uint32	mOffscreenSize;

	VDOpenGLBinding		mGL;
	bool	mbOpenGLMode;
	GLuint	mGLBuffers[2];
	GLuint	mGLShaderBase;
	GLuint	mGLTextures[2];
	int		mGLTextureW;
	int		mGLTextureH;
	vdblock<uint32> mGLReadBuffer;
	GLuint	mGLOcclusionQueries[2];
	bool	mbFrameValid[2];
	bool	mbGLOcclusionValid[2];
	bool	mbGLOcclusionPrevFrameValid;
	sint64	mTimestampQueue[4];
	int		mTimestampDelay;
	int		mTimestampIndex;

	GLuint	mGLCursorCacheTexture;
	float	mGLCursorCacheTextureInvW;
	float	mGLCursorCacheTextureInvH;
	HCURSOR	mCachedCursor;
	int		mCachedCursorWidth;
	int		mCachedCursorHeight;
	int		mCachedCursorHotspotX;
	int		mCachedCursorHotspotY;
	bool	mbCachedCursorXORMode;

	HDC		mhdcCursorBuffer;
	HBITMAP	mhbmCursorBuffer;
	HGDIOBJ	mhbmCursorBufferOld;
	uint32	*mpCursorBuffer;

	VDAtomicInt	mbCaptureFramePending;
	bool	mbCapturing;
	bool	mbCaptureSetup;
	bool	mbVisible;
	bool	mbAudioHardwarePresent;
	bool	mbAudioHardwareEnabled;
	bool	mbAudioCaptureEnabled;
	bool	mbAudioAnalysisEnabled;
	bool	mbAudioAnalysisActive;

	bool	mbTrackCursor;
	bool	mbTrackActiveWindow;
	bool	mbTrackActiveWindowClient;
	int		mTrackX;
	int		mTrackY;
	int		mTrackOffsetX;
	int		mTrackOffsetY;

	bool	mbRescaleImage;
	int		mRescaleW;
	int		mRescaleH;

	bool	mbDrawMousePointer;
	bool	mbRemoveDuplicates;

	uint32	mFramePeriod;

	vdstructex<BITMAPINFOHEADER>	mVideoFormat;
	vdstructex<WAVEFORMATEX>		mAudioFormat;

	IVDCaptureDriverCallback	*mpCB;
	DisplayMode			mDisplayMode;
	uint32	mGlobalTimeBase;

	UINT	mPreviewFrameTimer;
	VDAtomicInt	mPreviewFrameCount;

	VDCallbackTimer	mCaptureTimer;

	HMIXER	mhMixer;
	int		mMixerInput;
	MIXERCONTROL	mMixerInputControl;
	typedef std::vector<VDStringW>	MixerInputs;
	MixerInputs	mMixerInputs;

	HWAVEIN mhWaveIn;
	WAVEHDR	mWaveBufHdrs[2];
	vdblock<char>	mWaveBuffer;

	MyError	mCaptureError;

	VDRTProfileChannel mProfileChannel;

	static ATOM sWndClass;
	static ATOM sWndClassGL;
};

ATOM VDCaptureDriverScreen::sWndClass;
ATOM VDCaptureDriverScreen::sWndClassGL;

VDCaptureDriverScreen::VDCaptureDriverScreen()
	: mhwndParent(NULL)
	, mhwnd(NULL)
	, mhwndGL(NULL)
	, mhwndGLDraw(NULL)
	, mbCapBuffersInited(false)
	, mhdcOffscreen(NULL)
	, mhbmOffscreen(NULL)
	, mpOffscreenData(NULL)
	, mOffscreenSize(0)
	, mbOpenGLMode(true)
	, mGLShaderBase(0)
	, mGLTextureW(1)
	, mGLTextureH(1)
	, mGLCursorCacheTexture(0)
	, mCachedCursor(NULL)
	, mCachedCursorHotspotX(0)
	, mCachedCursorHotspotY(0)
	, mhdcCursorBuffer(NULL)
	, mhbmCursorBuffer(NULL)
	, mhbmCursorBufferOld(NULL)
	, mpCursorBuffer(NULL)
	, mbCaptureFramePending(false)
	, mbCapturing(false)
	, mbCaptureSetup(false)
	, mbVisible(false)
	, mbAudioAnalysisEnabled(false)
	, mbAudioAnalysisActive(false)
	, mbTrackCursor(false)
	, mbTrackActiveWindow(false)
	, mbTrackActiveWindowClient(false)
	, mTrackX(0)
	, mTrackY(0)
	, mTrackOffsetX(0)
	, mTrackOffsetY(0)
	, mbRescaleImage(false)
	, mRescaleW(GetSystemMetrics(SM_CXSCREEN))
	, mRescaleH(GetSystemMetrics(SM_CYSCREEN))
	, mbDrawMousePointer(true)
	, mbRemoveDuplicates(true)
	, mFramePeriod(10000000 / 30)
	, mpCB(NULL)
	, mDisplayMode(kDisplayNone)
	, mPreviewFrameTimer(0)
	, mhMixer(NULL)
	, mhWaveIn(NULL)
	, mProfileChannel("Capture driver")
{
	memset(mWaveBufHdrs, 0, sizeof mWaveBufHdrs);

	mVideoFormat.resize(sizeof(BITMAPINFOHEADER));
	mVideoFormat->biSize		= sizeof(BITMAPINFOHEADER);
	mVideoFormat->biWidth		= 320;
	mVideoFormat->biHeight		= 240;
	mVideoFormat->biPlanes		= 1;
	mVideoFormat->biCompression	= BI_RGB;
	mVideoFormat->biBitCount	= 32;
	mVideoFormat->biSizeImage	= 320*240*4;
	mVideoFormat->biXPelsPerMeter	= 0;
	mVideoFormat->biYPelsPerMeter	= 0;
	mVideoFormat->biClrUsed			= 0;
	mVideoFormat->biClrImportant	= 0;

	mAudioFormat.resize(sizeof(WAVEFORMATEX));
	mAudioFormat->wFormatTag		= WAVE_FORMAT_PCM;
	mAudioFormat->nSamplesPerSec	= 44100;
	mAudioFormat->nAvgBytesPerSec	= 44100*4;
	mAudioFormat->nChannels			= 2;
	mAudioFormat->nBlockAlign		= 4;
	mAudioFormat->wBitsPerSample	= 16;
	mAudioFormat->cbSize			= 0;

	mOffscreenSize = mVideoFormat->biSizeImage;

	mGLOcclusionQueries[0] = 0;
	mGLOcclusionQueries[1] = 0;
	mbGLOcclusionValid[0] = false;
	mbGLOcclusionValid[1] = false;
	mbGLOcclusionPrevFrameValid = false;

	memset(mGLTextures, 0, sizeof mGLTextures);
	memset(mGLBuffers, 0, sizeof mGLBuffers);
	memset(mbFrameValid, 0, sizeof mbFrameValid);
}

VDCaptureDriverScreen::~VDCaptureDriverScreen() {
	Shutdown();
}

void VDCaptureDriverScreen::SetCallback(IVDCaptureDriverCallback *pCB) {
	mpCB = pCB;
}

bool VDCaptureDriverScreen::Init(VDGUIHandle hParent) {
	mhwndParent = (HWND)hParent;

	if (!sWndClass) {
		WNDCLASS wc = { 0, StaticWndProc, 0, sizeof(VDCaptureDriverScreen *), g_hInst, NULL, NULL, NULL, NULL, "Riza screencap window" };

		sWndClass = RegisterClass(&wc);

		if (!sWndClass)
			return false;

		WNDCLASS wcgl = { CS_OWNDC, StaticWndProcGL, 0, sizeof(VDCaptureDriverScreen *), g_hInst, NULL, NULL, NULL, NULL, "Riza screencap GL window" };

		sWndClassGL = RegisterClass(&wcgl);

		if (!sWndClassGL)
			return false;
	}

	// Attempt to open mixer device. It is OK for this to fail. Note that we
	// have a bit of a problem in that (a) the mixer API doesn't take
	// WAVE_MAPPER, and (b) we can't get access to the handle that the
	// capture window creates. For now, we sort of fake it.
	InitMixerSupport();

	// Create message sink.
	if (!(mhwnd = CreateWindow((LPCTSTR)sWndClass, "", WS_CHILD, 0, 0, 0, 0, mhwndParent, NULL, g_hInst, this))) {
		Shutdown();
		return false;
	}

	mPreviewFrameCount = 0;

	mbAudioHardwarePresent = false;
	mbAudioHardwarePresent = true;
	mbAudioHardwareEnabled = mbAudioHardwarePresent;
	mbAudioCaptureEnabled = true;

	LoadSettings();

	InitVideoBuffer();

	return true;
}

void VDCaptureDriverScreen::Shutdown() {
	ShutdownWaveAnalysis();

	if (mPreviewFrameTimer) {
		KillTimer(mhwnd, mPreviewFrameTimer);
		mPreviewFrameTimer = 0;
	}

	ShutdownVideoBuffer();

	SaveSettings();

	if (mhwnd) {
		DestroyWindow(mhwnd);
		mhwnd = NULL;
	}

	if (mhMixer) {
		mixerClose(mhMixer);
		mhMixer = NULL;
	}
}

bool VDCaptureDriverScreen::IsHardwareDisplayAvailable() {
	return true;
}

void VDCaptureDriverScreen::SetDisplayMode(DisplayMode mode) {
	if (mode == mDisplayMode)
		return;

	if (mDisplayMode == kDisplayAnalyze) {
		if (mPreviewFrameTimer) {
			KillTimer(mhwnd, mPreviewFrameTimer);
			mPreviewFrameTimer = 0;
		}
	}

	mDisplayMode = mode;

	if (mPreviewFrameTimer) {
		KillTimer(mhwnd, mPreviewFrameTimer);
		mPreviewFrameTimer = 0;
	}

	switch(mode) {
	case kDisplayNone:
		ShowWindow(mhwnd, SW_HIDE);
		break;
	case kDisplayHardware:
		mPreviewFrameTimer = SetTimer(mhwnd, kPreviewTimerID, mFramePeriod / 10000, NULL);

		if (mbVisible)
			ShowWindow(mhwnd, SW_SHOWNA);
		break;
	case kDisplaySoftware:
		mPreviewFrameTimer = SetTimer(mhwnd, kPreviewTimerID, mFramePeriod / 10000, NULL);

		if (mbVisible)
			ShowWindow(mhwnd, SW_SHOWNA);
		break;
	case kDisplayAnalyze:
		mPreviewFrameTimer = SetTimer(mhwnd, kPreviewTimerID, mFramePeriod / 10000, NULL);

		if (mbVisible)
			ShowWindow(mhwnd, SW_HIDE);
		break;
	}
}

DisplayMode VDCaptureDriverScreen::GetDisplayMode() {
	return mDisplayMode;
}

void VDCaptureDriverScreen::SetDisplayRect(const vdrect32& r) {
	SetWindowPos(mhwnd, NULL, r.left, r.top, r.width(), r.height(), SWP_NOZORDER|SWP_NOACTIVATE);
}

vdrect32 VDCaptureDriverScreen::GetDisplayRectAbsolute() {
	RECT r;
	GetWindowRect(mhwnd, &r);
	MapWindowPoints(GetParent(mhwnd), NULL, (LPPOINT)&r, 2);
	return vdrect32(r.left, r.top, r.right, r.bottom);
}

void VDCaptureDriverScreen::SetDisplayVisibility(bool vis) {
	if (vis == mbVisible)
		return;

	mbVisible = vis;
	ShowWindow(mhwnd, vis && mDisplayMode != kDisplayAnalyze ? SW_SHOWNA : SW_HIDE);
}

void VDCaptureDriverScreen::SetFramePeriod(sint32 ms) {
	mFramePeriod = ms;

	if (mpCB)
		mpCB->CapEvent(kEventVideoFrameRateChanged, 0);
}

sint32 VDCaptureDriverScreen::GetFramePeriod() {
	return mFramePeriod;
}

uint32 VDCaptureDriverScreen::GetPreviewFrameCount() {
	if (mDisplayMode == kDisplaySoftware || mDisplayMode == kDisplayAnalyze)
		return mPreviewFrameCount;

	return 0;
}

bool VDCaptureDriverScreen::GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat) {
	vformat = mVideoFormat;
	return true;
}

bool VDCaptureDriverScreen::SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size) {
	bool success = false;

	if (pbih->biCompression == VDMAKEFOURCC('Y', 'U', 'Y', '2')) {
		if (!mbOpenGLMode)
			return false;

		if ((pbih->biWidth & 1))
			return false;

		mOffscreenSize = pbih->biWidth * abs(pbih->biHeight) * 2;
	} else if (pbih->biCompression == VDMAKEFOURCC('Y', 'V', '1', '2')) {
		if (!mbOpenGLMode)
			return false;

		if ((pbih->biWidth & 7) || (pbih->biHeight & 1))
			return false;
		mOffscreenSize = pbih->biWidth * abs(pbih->biHeight) * 3 / 2;
	} else if (pbih->biCompression == BI_RGB) {
		if (pbih->biBitCount != 32)
			return false;

		mOffscreenSize = pbih->biWidth * abs(pbih->biHeight) * 4;
	} else {
		return false;
	}

	ShutdownVideoBuffer();
	mVideoFormat.assign(pbih, size);
	if (mpCB)
		mpCB->CapEvent(kEventVideoFormatChanged, 0);
	InitVideoBuffer();
	success = true;
	return success;
}

int VDCaptureDriverScreen::GetAudioDeviceCount() {
	return mbAudioHardwarePresent ? 1 : 0;
}

const wchar_t *VDCaptureDriverScreen::GetAudioDeviceName(int idx) {
	if (idx || !mbAudioHardwarePresent)
		return NULL;

	return L"Wave Mapper";
}

bool VDCaptureDriverScreen::SetAudioDevice(int idx) {
	if (idx < -1 || idx >= 1)
		return false;
	
	if (!idx && !mbAudioHardwarePresent)
		return false;

	bool enable = !idx;

	if (enable == mbAudioHardwareEnabled)
		return true;

	ShutdownWaveAnalysis();
	mbAudioHardwareEnabled = enable;
	InitWaveAnalysis();

	return true;
}

int VDCaptureDriverScreen::GetAudioDeviceIndex() {
	return mbAudioHardwareEnabled ? 0 : -1;
}

int VDCaptureDriverScreen::GetVideoSourceCount() {
	return 0;
}

const wchar_t *VDCaptureDriverScreen::GetVideoSourceName(int idx) {
	return NULL;
}

bool VDCaptureDriverScreen::SetVideoSource(int idx) {
	return idx == -1;
}

int VDCaptureDriverScreen::GetVideoSourceIndex() {
	return -1;
}

int VDCaptureDriverScreen::GetAudioSourceCount() {
	return 0;
}

const wchar_t *VDCaptureDriverScreen::GetAudioSourceName(int idx) {
	return NULL;
}

bool VDCaptureDriverScreen::SetAudioSource(int idx) {
	return idx == -1;
}

int VDCaptureDriverScreen::GetAudioSourceIndex() {
	return -1;
}

int VDCaptureDriverScreen::GetAudioInputCount() {
	return mbAudioHardwareEnabled ? mMixerInputs.size() : 0;
}

const wchar_t *VDCaptureDriverScreen::GetAudioInputName(int idx) {
	if (!mbAudioHardwareEnabled || (unsigned)idx >= mMixerInputs.size())
		return NULL;

	MixerInputs::const_iterator it(mMixerInputs.begin());

	std::advance(it, idx);

	return (*it).c_str();
}

bool VDCaptureDriverScreen::SetAudioInput(int idx) {
	if (!mbAudioHardwareEnabled || !mhMixer)
		return idx == -1;

	VDASSERT(mMixerInputs.size() == mMixerInputControl.cMultipleItems);

	if (idx != -1 && (unsigned)idx >= mMixerInputControl.cMultipleItems)
		return false;

	// attempt to set the appropriate mixer input

	vdblock<MIXERCONTROLDETAILS_BOOLEAN> vals(mMixerInputControl.cMultipleItems);

	for(unsigned i=0; i<mMixerInputControl.cMultipleItems; ++i)
		vals[i].fValue = (i == idx);

	MIXERCONTROLDETAILS details = {sizeof(MIXERCONTROLDETAILS)};

	details.dwControlID		= mMixerInputControl.dwControlID;
	details.cChannels		= 1;
	details.cMultipleItems	= mMixerInputControl.cMultipleItems;
	details.cbDetails		= sizeof(MIXERCONTROLDETAILS_BOOLEAN);
	details.paDetails		= vals.data();

	if (MMSYSERR_NOERROR != mixerSetControlDetails((HMIXEROBJ)mhMixer, &details, MIXER_SETCONTROLDETAILSF_VALUE))
		return false;

	mMixerInput = idx;

	return true;
}

int VDCaptureDriverScreen::GetAudioInputIndex() {
	return mbAudioHardwareEnabled ? mMixerInput : -1;
}

bool VDCaptureDriverScreen::IsAudioCapturePossible() {
	return mbAudioHardwareEnabled;
}

bool VDCaptureDriverScreen::IsAudioCaptureEnabled() {
	return mbAudioHardwareEnabled && mbAudioCaptureEnabled;
}

void VDCaptureDriverScreen::SetAudioCaptureEnabled(bool b) {
	mbAudioCaptureEnabled = b;
}

void VDCaptureDriverScreen::SetAudioAnalysisEnabled(bool b) {
	if (mbAudioAnalysisEnabled == b)
		return;

	mbAudioAnalysisEnabled = b;

	if (mbAudioAnalysisEnabled)
		InitWaveAnalysis();
	else
		ShutdownWaveAnalysis();
}

void VDCaptureDriverScreen::GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats) {
	static const int kSamplingRates[]={
		8000,
		11025,
		12000,
		16000,
		22050,
		24000,
		32000,
		44100,
		48000,
		96000,
		192000
	};

	static const int kChannelCounts[]={
		1,
		2
	};

	static const int kSampleDepths[]={
		8,
		16
	};

	for(int sridx=0; sridx < sizeof kSamplingRates / sizeof kSamplingRates[0]; ++sridx)
		for(int chidx=0; chidx < sizeof kChannelCounts / sizeof kChannelCounts[0]; ++chidx)
			for(int sdidx=0; sdidx < sizeof kSampleDepths / sizeof kSampleDepths[0]; ++sdidx) {
				WAVEFORMATEX wfex={
					WAVE_FORMAT_PCM,
					kChannelCounts[chidx],
					kSamplingRates[sridx],
					0,
					0,
					kSampleDepths[sdidx],
					0
				};

				wfex.nBlockAlign = wfex.nChannels * (wfex.wBitsPerSample >> 3);
				wfex.nAvgBytesPerSec = wfex.nBlockAlign * wfex.nSamplesPerSec;

				if (MMSYSERR_NOERROR ==waveInOpen(NULL, WAVE_MAPPER, &wfex, 0, 0, WAVE_FORMAT_QUERY | WAVE_FORMAT_DIRECT)) {
					aformats.push_back(vdstructex<WAVEFORMATEX>(&wfex, sizeof wfex));
				}
			}
}

bool VDCaptureDriverScreen::GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat) {
	aformat = mAudioFormat;
	return true;
}

bool VDCaptureDriverScreen::SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size) {
	if (!mbAudioHardwareEnabled)
		return false;

	ShutdownWaveAnalysis();
	mAudioFormat.assign(pwfex, size);
	if (mbAudioAnalysisEnabled)
		InitWaveAnalysis();
	return true;
}

bool VDCaptureDriverScreen::IsDriverDialogSupported(DriverDialog dlg) {
	if (dlg == kDialogVideoSource)
		return true;

	return false;
}

void VDCaptureDriverScreen::DisplayDriverDialog(DriverDialog dlg) {
	VDASSERT(IsDriverDialogSupported(dlg));

	switch(dlg) {
	case kDialogVideoFormat:
		break;
	case kDialogVideoSource:
		ShutdownVideoBuffer();
		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_SCREENCAP_OPTS), mhwnd, VideoSourceDlgProc, (LPARAM)this);

		SaveSettings();

		// without OpenGL, we can only do 32-bit RGB
		if (!mbOpenGLMode && mVideoFormat->biCompression != BI_RGB) {
			mVideoFormat->biCompression = BI_RGB;
			mVideoFormat->biSizeImage = mVideoFormat->biWidth * abs(mVideoFormat->biHeight) * 4;
			mVideoFormat->biBitCount = 32;
			mpCB->CapEvent(kEventVideoFormatChanged, 0);
		}

		InitVideoBuffer();
		break;
	case kDialogVideoDisplay:
		break;
	}
}

bool VDCaptureDriverScreen::CaptureStart() {
	ShutdownWaveAnalysis();

	if (!VDINLINEASSERTFALSE(mbCapturing)) {
		if (mbOpenGLMode) {
			HDC hdc = GetDC(mhwndGL);
			if (hdc) {
				if (mGL.Begin(hdc)) {
					FlushFrameQueue();
					mGL.End();
				}

				ReleaseDC(mhwndGL, hdc);
			}
		}

		if (mpCB->CapEvent(kEventPreroll, 0)) {
			mpCB->CapBegin(0);
			if (mbAudioCaptureEnabled)
				InitWaveAnalysis();

			mGlobalTimeBase = VDGetAccurateTick();
			mbCapturing = true;
			mbCaptureSetup = true;
			mCaptureTimer.Init2(this, mFramePeriod);
		} else {
			if (mbAudioAnalysisEnabled)
				InitWaveAnalysis();
		}
	}

	return mbCapturing;
}

void VDCaptureDriverScreen::CaptureStop() {
	SendMessage(mhwnd, WM_APP+16, 0, 0);
}

void VDCaptureDriverScreen::CaptureAbort() {
	SendMessage(mhwnd, WM_APP+17, 0, 0);
	mbCapturing = false;
}

void VDCaptureDriverScreen::SyncCaptureStop() {
	if (VDINLINEASSERT(mbCaptureSetup)) {
		mCaptureTimer.Shutdown();
		mbCapturing = false;
		mbCaptureSetup = false;

		if (mCaptureError.gets()) {
			mpCB->CapEnd(&mCaptureError);
			mCaptureError.discard();
		} else
			mpCB->CapEnd(NULL);

		if (mbAudioCaptureEnabled)
			ShutdownWaveAnalysis();

		if (mbAudioAnalysisEnabled)
			InitWaveAnalysis();
	}
}

void VDCaptureDriverScreen::SyncCaptureAbort() {
	SyncCaptureStop();
}

void VDCaptureDriverScreen::InitMixerSupport() {
	WAVEINCAPS wcaps={0};
	if (MMSYSERR_NOERROR == waveInGetDevCaps(WAVE_MAPPER, &wcaps, sizeof wcaps) && wcaps.dwFormats) {
		WAVEFORMATEX wfex;

		// create lowest-common denominator format for device
		wfex.wFormatTag			= WAVE_FORMAT_PCM;

		if (wcaps.dwFormats & (WAVE_FORMAT_4M08 | WAVE_FORMAT_4M16 | WAVE_FORMAT_4S08 | WAVE_FORMAT_4S16))
			wfex.nSamplesPerSec = 11025;
		else if (wcaps.dwFormats & (WAVE_FORMAT_2M08 | WAVE_FORMAT_2M16 | WAVE_FORMAT_2S08 | WAVE_FORMAT_2S16))
			wfex.nSamplesPerSec = 22050;
		else
			wfex.nSamplesPerSec = 44100;

		if (wcaps.dwFormats & (WAVE_FORMAT_1M08 | WAVE_FORMAT_1M16 | WAVE_FORMAT_2M08 | WAVE_FORMAT_2M16 | WAVE_FORMAT_4M08 | WAVE_FORMAT_4M16))
			wfex.nChannels = 1;
		else
			wfex.nChannels = 2;

		if (wcaps.dwFormats & (WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 | WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_4M08 | WAVE_FORMAT_4S08))
			wfex.wBitsPerSample = 8;
		else
			wfex.wBitsPerSample = 16;

		wfex.nBlockAlign		= wfex.wBitsPerSample >> 3;
		wfex.nAvgBytesPerSec	= wfex.nSamplesPerSec * wfex.nBlockAlign;
		wfex.cbSize				= 0;

		// create the device
		HWAVEIN hwi;
		if (MMSYSERR_NOERROR == waveInOpen(&hwi, WAVE_MAPPER, &wfex, 0, 0, CALLBACK_NULL)) {
			// create mixer based on device

			if (MMSYSERR_NOERROR == mixerOpen(&mhMixer, (UINT)hwi, 0, 0, MIXER_OBJECTF_HWAVEIN)) {
				MIXERLINE mixerLine = {sizeof(MIXERLINE)};

				mixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_WAVEIN;

				if (MMSYSERR_NOERROR == mixerGetLineInfo((HMIXEROBJ)mhMixer, &mixerLine, MIXER_GETLINEINFOF_COMPONENTTYPE)) {

					// Try to find a MIXER or MUX control
					MIXERLINECONTROLS lineControls = {sizeof(MIXERLINECONTROLS)};

					mMixerInputControl.cbStruct = sizeof(MIXERCONTROL);
					mMixerInputControl.dwControlType = 0;

					lineControls.dwLineID = mixerLine.dwLineID;
					lineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_MUX;
					lineControls.cControls = 1;
					lineControls.pamxctrl = &mMixerInputControl;
					lineControls.cbmxctrl = sizeof(MIXERCONTROL);

					MMRESULT res;

					res = mixerGetLineControls((HMIXEROBJ)mhMixer, &lineControls, MIXER_GETLINECONTROLSF_ONEBYTYPE);

					if (MMSYSERR_NOERROR != res) {
						lineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_MIXER;

						res = mixerGetLineControls((HMIXEROBJ)mhMixer, &lineControls, MIXER_GETLINECONTROLSF_ONEBYTYPE);
					}

					// The mux/mixer control must be of MULTIPLE type; otherwise, we reject it.
					if (!(mMixerInputControl.fdwControl & MIXERCONTROL_CONTROLF_MULTIPLE))
						res = MMSYSERR_ERROR;

					// If we were successful, then enumerate all source lines and push them into the map.
					if (MMSYSERR_NOERROR != res) {
						mixerClose(mhMixer);
						mhMixer = NULL;
					} else {
						// Enumerate control inputs and populate the name array
						vdblock<MIXERCONTROLDETAILS_LISTTEXT> names(mMixerInputControl.cMultipleItems);

						MIXERCONTROLDETAILS details = {sizeof(MIXERCONTROLDETAILS)};

						details.dwControlID		= mMixerInputControl.dwControlID;
						details.cChannels		= 1;
						details.cMultipleItems	= mMixerInputControl.cMultipleItems;
						details.cbDetails		= sizeof(MIXERCONTROLDETAILS_LISTTEXT);
						details.paDetails		= names.data();

						mMixerInput = -1;

						if (MMSYSERR_NOERROR == mixerGetControlDetails((HMIXEROBJ)mhMixer, &details, MIXER_GETCONTROLDETAILSF_LISTTEXT)) {
							mMixerInputs.reserve(details.cMultipleItems);

							for(unsigned i=0; i<details.cMultipleItems; ++i)
								mMixerInputs.push_back(MixerInputs::value_type(VDTextAToW(names[i].szName)));

							vdblock<MIXERCONTROLDETAILS_BOOLEAN> vals(mMixerInputControl.cMultipleItems);

							details.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
							details.paDetails = vals.data();

							if (MMSYSERR_NOERROR == mixerGetControlDetails((HMIXEROBJ)mhMixer, &details, MIXER_GETCONTROLDETAILSF_VALUE)) {
								// Attempt to find a mixer input that is set. Note that for
								// a multiple-select type (MIXER) this will pick the first
								// enabled input.
								for(unsigned i=0; i<details.cMultipleItems; ++i)
									if (vals[i].fValue) {
										mMixerInput = i;
										break;
									}
							}
						}
					}
				}

				// We don't close the mixer here; it is left open while we have the
				// capture device opened.
			}

			waveInClose(hwi);
		}
	}
}

void VDCaptureDriverScreen::ShutdownMixerSupport() {
	if (mhMixer) {
		mixerClose(mhMixer);
		mhMixer = NULL;
	}
}

bool VDCaptureDriverScreen::InitWaveAnalysis() {
	if (!mbAudioHardwareEnabled)
		return false;

	vdstructex<WAVEFORMATEX> aformat;

	if (!GetAudioFormat(aformat))
		return false;

	uint32	blockSize = (aformat->nAvgBytesPerSec + 9) / 10 + aformat->nBlockAlign - 1;
	blockSize -= blockSize % aformat->nBlockAlign;

	mWaveBuffer.resize(blockSize*2);

	if (MMSYSERR_NOERROR != waveInOpen(&mhWaveIn, WAVE_MAPPER, aformat.data(), (DWORD_PTR)mhwnd, 0, CALLBACK_WINDOW | WAVE_FORMAT_DIRECT))
		return false;

	mbAudioAnalysisActive = true;
	for(int i=0; i<2; ++i) {
		WAVEHDR& hdr = mWaveBufHdrs[i];

		hdr.lpData			= &mWaveBuffer[blockSize*i];
		hdr.dwBufferLength	= blockSize;
		hdr.dwBytesRecorded	= 0;
		hdr.dwFlags			= 0;
		hdr.dwLoops			= 0;
		if (MMSYSERR_NOERROR != waveInPrepareHeader(mhWaveIn, &hdr, sizeof(WAVEHDR))) {
			ShutdownWaveAnalysis();
			return false;
		}

		if (MMSYSERR_NOERROR != waveInAddBuffer(mhWaveIn, &hdr, sizeof(WAVEHDR))) {
			ShutdownWaveAnalysis();
			return false;
		}
	}

	if (MMSYSERR_NOERROR != waveInStart(mhWaveIn)) {
		ShutdownWaveAnalysis();
		return false;
	}

	return true;
}

void VDCaptureDriverScreen::ShutdownWaveAnalysis() {
	if (mhWaveIn) {
		mbAudioAnalysisActive = false;
		waveInReset(mhWaveIn);

		for(int i=0; i<2; ++i) {
			if (mWaveBufHdrs[i].dwFlags & WHDR_PREPARED)
				waveInUnprepareHeader(mhWaveIn, &mWaveBufHdrs[i], sizeof(WAVEHDR));
		}

		waveInClose(mhWaveIn);
		mhWaveIn = NULL;
	}

	mWaveBuffer.clear();
}

bool VDCaptureDriverScreen::InitVideoBuffer() {
	ShutdownVideoBuffer();

	if (!mbOpenGLMode) {
		HDC hdc = GetDC(NULL);
		if (!hdc)
			return false;

		mhdcOffscreen = CreateCompatibleDC(hdc);
		mhbmOffscreen = CreateDIBSection(hdc, (const BITMAPINFO *)mVideoFormat.data(), DIB_PAL_COLORS, &mpOffscreenData, NULL, 0);
		ReleaseDC(NULL, hdc);

		if (!mhbmOffscreen || !mhdcOffscreen) {
			ShutdownVideoBuffer();
			return false;
		}

		DeleteObject(SelectObject(mhdcOffscreen, mhbmOffscreen));

		mOffscreenSize = mVideoFormat->biSizeImage;
	} else {
		if (!mGL.Init()) {
			mbOpenGLMode = false;
			return InitVideoBuffer();
		}

		if (!(mhwndGL = CreateWindow((LPCTSTR)sWndClassGL, "VirtualDub OpenGL support", WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, g_hInst, this))) {
			mbOpenGLMode = false;
			return InitVideoBuffer();
		}

		RECT r;
		GetClientRect(mhwnd, &r);
		if (!(mhwndGLDraw = CreateWindow((LPCTSTR)sWndClassGL, "VirtualDub OpenGL support", WS_CHILD|WS_VISIBLE, 0, 0, r.right, r.bottom, mhwnd, NULL, g_hInst, this))) {
			mbOpenGLMode = false;
			return InitVideoBuffer();
		}

		HDC hdc = GetDC(mhwndGL);
		if (!mGL.Attach(hdc, 24, 8, 0, 0, true)) {
			ReleaseDC(mhwndGL, hdc);
			mbOpenGLMode = false;
			return InitVideoBuffer();
		}

		HDC hdc2 = GetDC(mhwndGLDraw);
		mGL.AttachAux(hdc2, 24, 8, 0, 0, true);
		ReleaseDC(mhwndGLDraw, hdc2);

		int buffersize = mVideoFormat->biWidth * abs(mVideoFormat->biHeight) * 4;

		mGL.Begin(hdc);

		if (mGL.EXT_pixel_buffer_object) {
			mGL.glGenBuffersARB(2, mGLBuffers);
			mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
			mGL.glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, buffersize, NULL, GL_STREAM_READ_ARB);
			mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[1]);
			mGL.glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, buffersize, NULL, GL_STREAM_READ_ARB);
		} else {
			mGLReadBuffer.resize((buffersize + 3) >> 2);
		}

		VDASSERT(!mGL.glGetError());

		mGL.glGenTextures(2, mGLTextures);
		mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);

		int srcw = mVideoFormat->biWidth;
		int srch = abs(mVideoFormat->biHeight);

		if (mbRescaleImage) {
			if (srcw < mRescaleW)
				srcw = mRescaleW;
			if (srch < mRescaleH)
				srch = mRescaleH;
		}

		int w = srcw * 2 - 1;
		int h = srch * 2 - 1;

		while(int t = w & (w-1))
			w = t;

		while(int t = h & (h-1))
			h = t;

		mGLTextureW = w;
		mGLTextureH = h;

		mGL.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
		VDASSERT(!mGL.glGetError());

		mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[1]);
		mGL.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
		VDASSERT(!mGL.glGetError());

		int cxcursor = GetSystemMetrics(SM_CXCURSOR);
		int cycursor = GetSystemMetrics(SM_CYCURSOR);

		mCachedCursorWidth = cxcursor;
		mCachedCursorHeight = cycursor;

		HDC hdcScreen = GetDC(NULL);
		mhdcCursorBuffer = CreateCompatibleDC(hdcScreen);
		const BITMAPINFOHEADER bihCursor={
			sizeof(BITMAPINFOHEADER),
			cxcursor,
			cycursor * 2,
			1,
			32,
			BI_RGB,
			0,
			0,
			0,
			0,
			0
		};
		ReleaseDC(NULL, hdcScreen);

		mhbmCursorBuffer = CreateDIBSection(mhdcCursorBuffer, (const BITMAPINFO *)&bihCursor, DIB_RGB_COLORS, (void **)&mpCursorBuffer, NULL, 0);
		mhbmCursorBufferOld = SelectObject(mhdcCursorBuffer, mhbmCursorBuffer);

		cxcursor = cxcursor * 2 - 1;
		while(int t = cxcursor & (cxcursor - 1))
			cxcursor = t;

		cycursor = cycursor * 2 - 1;
		while(int t = cycursor & (cycursor - 1))
			cycursor = t;

		mCachedCursor = NULL;
		mGLCursorCacheTextureInvW = 1.0f / (float)cxcursor;
		mGLCursorCacheTextureInvH = 1.0f / (float)cycursor;

		mGL.glGenTextures(1, &mGLCursorCacheTexture);
		mGL.glBindTexture(GL_TEXTURE_2D, mGLCursorCacheTexture);
		mGL.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cxcursor, cycursor, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);

		if (mGL.NV_occlusion_query && mGL.ARB_multitexture && mbRemoveDuplicates)
			mGL.glGenOcclusionQueriesNV(2, mGLOcclusionQueries);

		// initialize shaders
		mGLShaderBase = mGL.InitTechniques(g_techniques, sizeof g_techniques / sizeof g_techniques[0]);

		VDASSERT(!mGL.glGetError());

		mbOpenGLMode = true;

		FlushFrameQueue();

		mGL.End();

		ReleaseDC(mhwndGL, hdc);

		mTimestampIndex = 0;
		mTimestampDelay = 0;
		if (mGL.EXT_pixel_buffer_object)
			++mTimestampDelay;
		if (mGL.NV_occlusion_query && mGL.ARB_multitexture && mbRemoveDuplicates)
			++mTimestampDelay;
	}

	mbCapBuffersInited = true;
	return true;
}

void VDCaptureDriverScreen::ShutdownVideoBuffer() {
	mbCapBuffersInited = false;

	if (mhbmCursorBufferOld) {
		SelectObject(mhdcCursorBuffer, mhbmCursorBufferOld);
		mhbmCursorBufferOld = NULL;
	}

	if (mhbmCursorBuffer) {
		DeleteObject(mhbmCursorBuffer);
		mhbmCursorBuffer = NULL;
	}

	if (mhdcCursorBuffer) {
		DeleteDC(mhdcCursorBuffer);
		mhdcCursorBuffer = NULL;
	}

	if (mGL.IsInited()) {
		if (HDC hdc = GetDC(mhwndGL)) {
			if (mGL.Begin(hdc)) {
				if (mGL.NV_occlusion_query) {
					for(int i=0; i<2; ++i) {
						if (mbGLOcclusionValid[i]) {
							GetOcclusionQueryPixelCountSafe(mGLOcclusionQueries[i]);
							mbGLOcclusionValid[i] = false;
						}
					}
					mGL.glDeleteOcclusionQueriesNV(2, mGLOcclusionQueries);
				}
				mGL.End();
			}
			ReleaseDC(mhwndGL, hdc);
		}

		mGL.glDeleteLists(mGLShaderBase, sizeof g_techniques / sizeof g_techniques[0]);

		if (mGLTextures[0]) {
			mGL.glDeleteTextures(2, mGLTextures);
			mGLTextures[0] = 0;
		}

		if (mGLCursorCacheTexture) {
			mGL.glDeleteTextures(1, &mGLCursorCacheTexture);
			mGLCursorCacheTexture = 0;
			mCachedCursor = NULL;
		}

		if (mGL.EXT_pixel_buffer_object)
			mGL.glDeleteBuffersARB(2, mGLBuffers);

		mGL.Shutdown();
	}

	if (mhwndGLDraw) {
		DestroyWindow(mhwndGLDraw);
		mhwndGLDraw = NULL;
	}

	if (mhwndGL) {
		DestroyWindow(mhwndGL);
		mhwndGL = NULL;
	}

	if (mhdcOffscreen) {
		DeleteDC(mhdcOffscreen);
		mhdcOffscreen = NULL;
	}

	if (mhbmOffscreen) {
		DeleteObject(mhbmOffscreen);
		mhbmOffscreen = NULL;
	}
}

void VDCaptureDriverScreen::FlushFrameQueue() {
	if (mbOpenGLMode) {
		for(int i=0; i<2; ++i) {
			if (mbGLOcclusionValid[i]) {
				GetOcclusionQueryPixelCountSafe(mGLOcclusionQueries[i]);
				mbGLOcclusionValid[i] = false;
			}
		}

		for(int i=0; i<sizeof(mTimestampQueue)/sizeof(mTimestampQueue[0]); ++i)
			mTimestampQueue[i] = -1;

		mbFrameValid[0] = mbFrameValid[1] = false;
		mbGLOcclusionValid[0] = mbGLOcclusionValid[1] = false;
		mbGLOcclusionPrevFrameValid = false;
	}
}

sint64 VDCaptureDriverScreen::ComputeGlobalTime() {
	sint64 tickDelta = (sint64)VDGetAccurateTick() - (sint64)mGlobalTimeBase;
	if (tickDelta < 0)
		tickDelta = 0;
	return (sint64)((uint64)tickDelta * 1000);
}

void VDCaptureDriverScreen::DoFrame() {
	if (!mbCapBuffersInited)
		return;

	sint64 globalTime;

	int w = mVideoFormat->biWidth;
	int h = abs(mVideoFormat->biHeight);
	int srcw = w;
	int srch = h;

	if (mbRescaleImage) {
		srcw = mRescaleW;
		srch = mRescaleH;
	}

	if (mbTrackCursor) {
		POINT pt;

		if (GetCursorPos(&pt)) {
			mTrackX = pt.x - ((w+1) >> 1);
			mTrackY = pt.y - ((h+1) >> 1);
		}
	} else {
		mTrackX = mTrackOffsetX;
		mTrackY = mTrackOffsetY;
	}

	if (mbTrackActiveWindow) {
		HWND hwndFore = GetForegroundWindow();

		if (hwndFore) {
			RECT r;
			bool success = false;

			if (mbTrackActiveWindowClient) {
				if (GetClientRect(hwndFore, &r)) {
					if (MapWindowPoints(hwndFore, NULL, (LPPOINT)&r, 2))
						success = true;
				}
			} else {
				if (GetWindowRect(hwndFore, &r))
					success = true;
			}

			if (success) {
				if (!mbTrackCursor) {
					mTrackX = r.left + mTrackOffsetX;
					mTrackY = r.left + mTrackOffsetY;
				}

				if (mTrackX > r.right - srcw)
					mTrackX = r.right - srcw;
				if (mTrackX < r.left)
					mTrackX = r.left;
				if (mTrackY > r.bottom - srch)
					mTrackY = r.bottom - srch;
				if (mTrackY < r.top)
					mTrackY = r.top;
			}
		}
	}

	globalTime = ComputeGlobalTime();
	if (mbCapturing || mDisplayMode) {
		// Check for cursor update.
		CURSORINFO ci = {sizeof(CURSORINFO)};
		bool cursorImageUpdated = false;

		if (mbDrawMousePointer) {
			if (!::GetCursorInfo(&ci)) {
				ci.hCursor = NULL;
			}

			if (ci.hCursor) {
				if (mCachedCursor != ci.hCursor) {
					mCachedCursor = ci.hCursor;

					ICONINFO ii;
					if (::GetIconInfo(ci.hCursor, &ii)) {
						mCachedCursorHotspotX = ii.xHotspot;
						mCachedCursorHotspotY = ii.yHotspot;

						if (mbOpenGLMode) {
							bool mergeMask = false;

							HDC hdc = GetDC(NULL);
							if (hdc) {
								mbCachedCursorXORMode = false;

								if (!ii.hbmColor) {
									mbCachedCursorXORMode = true;

									// Query bitmap format.
									BITMAPINFOHEADER maskFormat = {sizeof(BITMAPINFOHEADER)};
									if (::GetDIBits(hdc, ii.hbmMask, 0, 0, NULL, (LPBITMAPINFO)&maskFormat, DIB_RGB_COLORS)) {
										// Validate cursor size. This shouldn't change since SM_CXCURSOR and SM_CYCURSOR are constant.
										if (maskFormat.biWidth == mCachedCursorWidth && maskFormat.biHeight == mCachedCursorHeight * 2) {
											// Retrieve bitmap bits.
											BITMAPINFOHEADER hdr = {};
											hdr.biSize			= sizeof(BITMAPINFOHEADER);
											hdr.biWidth			= maskFormat.biWidth;
											hdr.biHeight		= maskFormat.biHeight;
											hdr.biPlanes		= 1;
											hdr.biBitCount		= 32;
											hdr.biCompression	= BI_RGB;
											hdr.biSizeImage		= maskFormat.biWidth * maskFormat.biHeight * 4;
											hdr.biXPelsPerMeter	= 0;
											hdr.biYPelsPerMeter	= 0;
											hdr.biClrUsed		= 0;
											hdr.biClrImportant	= 0;

											::GetDIBits(hdc, ii.hbmMask, 0, maskFormat.biHeight, mpCursorBuffer, (LPBITMAPINFO)&hdr, DIB_RGB_COLORS);
										}
									}

									uint32 numPixels = mCachedCursorWidth * mCachedCursorHeight;
									uint32 *pXORMask = mpCursorBuffer;
									uint32 *pANDMask = pXORMask + numPixels;

									for(uint32 i=0; i<numPixels; ++i)
										pXORMask[i] = (pXORMask[i] & 0xFFFFFF) + (~pANDMask[i] << 24);
								} else {
									RECT r1 = {0, 0, mCachedCursorWidth, mCachedCursorHeight};
									RECT r2 = {0, mCachedCursorHeight, mCachedCursorWidth, mCachedCursorHeight*2};
									FillRect(mhdcCursorBuffer, &r1, (HBRUSH)GetStockObject(BLACK_BRUSH));
									FillRect(mhdcCursorBuffer, &r2, (HBRUSH)GetStockObject(WHITE_BRUSH));
									DrawIcon(mhdcCursorBuffer, 0, 0, ci.hCursor);
									DrawIcon(mhdcCursorBuffer, 0, mCachedCursorHeight, ci.hCursor);
									GdiFlush();

									uint32 numPixels = mCachedCursorWidth * mCachedCursorHeight;
									uint32 *pWhiteMask = mpCursorBuffer;
									uint32 *pBlackMask = pWhiteMask + numPixels;

									for(uint32 i=0; i<numPixels; ++i) {
										uint32 pixelOnWhite = pWhiteMask[i];
										uint32 pixelOnBlack = pBlackMask[i];
										int alpha = 255 - (int)(pWhiteMask[i] & 255) + (int)(pBlackMask[i] & 255);
										if ((unsigned)alpha >= 256)
											alpha = ~alpha >> 31;
										pWhiteMask[i] = (pBlackMask[i] & 0xffffff) + (alpha << 24);
									}
								}

								cursorImageUpdated = true;
								ReleaseDC(NULL, hdc);
							}
						}

						if (ii.hbmColor)
							VDVERIFY(::DeleteObject(ii.hbmColor));
						if (ii.hbmMask)
							VDVERIFY(::DeleteObject(ii.hbmMask));
					}
				}

				ci.ptScreenPos.x -= mCachedCursorHotspotX;
				ci.ptScreenPos.y -= mCachedCursorHotspotY;
			}
		}

		if (mbOpenGLMode) {
			RECT r = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};

			GetWindowRect(mhwndGL, &r);

			if (HDC hdc = GetDC(mhwndGL)) {
				if (mGL.Begin(hdc)) {
					// init state
					VDASSERT(!mGL.glGetError());
					mGL.glDrawBuffer(GL_BACK);
					mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

					// update cursor if necessary
					if (cursorImageUpdated) {
						mGL.glBindTexture(GL_TEXTURE_2D, mGLCursorCacheTexture);
						mGL.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mCachedCursorWidth, mCachedCursorHeight, GL_BGRA_EXT, GL_UNSIGNED_BYTE, mpCursorBuffer);
					}

					// read screen into texture
					if (mGL.ARB_multitexture)
						mGL.glActiveTextureARB(GL_TEXTURE0_ARB);
					mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);

					int srcx = mTrackX - r.left;
					int srcy = r.bottom - (mTrackY + srch);

					if (srcx > r.right - r.left - srcw)
						srcx = r.right - r.left - srcw;
					if (srcy > r.bottom - r.top - srch)
						srcy = r.bottom - r.top - srch;
					if (srcx < 0)
						srcx = 0;
					if (srcy < 0)
						srcy = 0;

					// Enable depth test. We don't actually want the depth test, but we need it
					// on for occlusion query to work on ATI.
					mGL.glEnable(GL_DEPTH_TEST);
					mGL.glDepthFunc(GL_ALWAYS);
					mGL.glDepthMask(GL_FALSE);

					// shrink image
					VDASSERT(!mGL.glGetError());
					mGL.glDisable(GL_LIGHTING);
					mGL.glDisable(GL_CULL_FACE);
					mGL.glDisable(GL_BLEND);
					mGL.glDisable(GL_ALPHA_TEST);
					mGL.glDisable(GL_STENCIL_TEST);
					mGL.glDisable(GL_SCISSOR_TEST);
					mGL.glEnable(GL_TEXTURE_2D);

					mGL.DisableFragmentShaders();

					int fbw = srcw < w ? w : srcw;
					int fbh = srch < h ? h : srch;

					mGL.glViewport(0, 0, fbw, fbh);
					mGL.glMatrixMode(GL_MODELVIEW);
					mGL.glLoadIdentity();
					mGL.glMatrixMode(GL_PROJECTION);
					mGL.glLoadIdentity();
					mGL.glOrtho(0, fbw, 0, fbh, -1.0f, 1.0f);

					VDASSERT(!mGL.glGetError());
					mGL.glReadBuffer(GL_FRONT);

					mProfileChannel.Begin(0xd0e0f0, "GL:ReadScreen");
					if (!mbRescaleImage || mbDrawMousePointer) {
						mGL.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, srcx, srcy, srcw, srch);
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

						float u = (float)srcw / (float)mGLTextureW;
						float v = (float)srch / (float)mGLTextureH;

						mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
						mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
						mGL.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
						mGL.glBegin(GL_QUADS);
							mGL.glTexCoord2f(0.0f, 0.0f);
							mGL.glVertex2f(0, 0);
							mGL.glTexCoord2f(u, 0.0f);
							mGL.glVertex2f((float)srcw, 0);
							mGL.glTexCoord2f(u, v);
							mGL.glVertex2f((float)srcw, (float)srch);
							mGL.glTexCoord2f(0.0f, v);
							mGL.glVertex2f(0, (float)srch);
						mGL.glEnd();
						mGL.glReadBuffer(GL_BACK);

						if (ci.hCursor) {
							mGL.glBindTexture(GL_TEXTURE_2D, mGLCursorCacheTexture);
							mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
							mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
							mGL.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

							int curx = ci.ptScreenPos.x - srcx;
							int cury = (r.bottom - (ci.ptScreenPos.y + mCachedCursorHeight)) - srcy;
							float curu = (float)mCachedCursorWidth * mGLCursorCacheTextureInvW;
							float curv = (float)mCachedCursorHeight * mGLCursorCacheTextureInvH;
							mGL.glEnable(GL_BLEND);

							if (mbCachedCursorXORMode && mGL.EXT_texture_env_combine) {
								mGL.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
								mGL.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
								mGL.glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
								mGL.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_ONE_MINUS_SRC_ALPHA);
								mGL.glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
								mGL.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
								mGL.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);
								mGL.glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
								mGL.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_EXT, GL_SRC_ALPHA);
								mGL.glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1);
								mGL.glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);
								mGL.glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR);
								mGL.glBegin(GL_QUADS);
									mGL.glTexCoord2f(0.0f, 0.0f);
									mGL.glVertex2i(curx, cury);
									mGL.glTexCoord2f(curu, 0.0f);
									mGL.glVertex2i(curx + mCachedCursorWidth, cury);
									mGL.glTexCoord2f(curu, curv);
									mGL.glVertex2i(curx + mCachedCursorWidth, cury + mCachedCursorHeight);
									mGL.glTexCoord2f(0.0f, curv);
									mGL.glVertex2i(curx, cury + mCachedCursorHeight);
								mGL.glEnd();
								mGL.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
								mGL.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
							} else
								mGL.glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

							mGL.glBegin(GL_QUADS);
								mGL.glTexCoord2f(0.0f, 0.0f);
								mGL.glVertex2i(curx, cury);
								mGL.glTexCoord2f(curu, 0.0f);
								mGL.glVertex2i(curx + mCachedCursorWidth, cury);
								mGL.glTexCoord2f(curu, curv);
								mGL.glVertex2i(curx + mCachedCursorWidth, cury + mCachedCursorHeight);
								mGL.glTexCoord2f(0.0f, curv);
								mGL.glVertex2i(curx, cury + mCachedCursorHeight);
							mGL.glEnd();
							mGL.glDisable(GL_BLEND);
							mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
						}

						srcx = 0;
						srcy = 0;
					}

					if (mbRescaleImage) {
						do {
							int dstw = std::max<int>(w, (srcw+1) >> 1);
							int dsth = std::max<int>(h, (srch+1) >> 1);

							mGL.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, srcx, srcy, srcw, srch);
							srcx = 0;
							srcy = 0;

							mGL.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

							float u = (float)srcw / (float)mGLTextureW;
							float v = (float)srch / (float)mGLTextureH;

							mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
							mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
							mGL.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
							mGL.glBegin(GL_QUADS);
								mGL.glTexCoord2f(0.0f, 0.0f);
								mGL.glVertex2f(0, 0);
								mGL.glTexCoord2f(u, 0.0f);
								mGL.glVertex2f((float)dstw, 0);
								mGL.glTexCoord2f(u, v);
								mGL.glVertex2f((float)dstw, (float)dsth);
								mGL.glTexCoord2f(0.0f, v);
								mGL.glVertex2f(0, (float)dsth);
							mGL.glEnd();

							mGL.glReadBuffer(GL_BACK);
							srcw = dstw;
							srch = dsth;
						} while(srcw != w || srch != h);
					}
					mProfileChannel.End();

					VDASSERT(!mGL.glGetError());

					// compute texturing parameters
					float u = (float)w / (float)mGLTextureW;
					float v = (float)h / (float)mGLTextureH;

					bool removeDuplicates = mGL.NV_occlusion_query && mGL.ARB_multitexture && mbRemoveDuplicates;
					if (mVideoFormat->biCompression || removeDuplicates || mDisplayMode == kDisplaySoftware || mDisplayMode == kDisplayHardware) {
						mGL.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);

						if (removeDuplicates) {
							if (!mbGLOcclusionPrevFrameValid) {
								mbGLOcclusionPrevFrameValid = true;
								mbFrameValid[0] = true;
							} else {
								mProfileChannel.Begin(0xa0c0f0, "GL:OcclusionQuery");
								mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
								mGL.glEnable(GL_TEXTURE_2D);
								mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[1]);

								mGL.glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
								mGL.glEnable(GL_ALPHA_TEST);
								mGL.glAlphaFunc(GL_GREATER, 0.0f);
								
								mGL.glBeginOcclusionQueryNV(mGLOcclusionQueries[0]);
								if (mGL.NV_register_combiners)
									mGL.glCallList(mGLShaderBase + kVDOpenGLTechIndex_difference_NV1x);
								else if (mGL.ATI_fragment_shader)
									mGL.glCallList(mGLShaderBase + kVDOpenGLTechIndex_difference_ATIFS);

								mGL.glBegin(GL_QUADS);
									mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0.0f, 0.0f);
									mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0.0f, 0.0f);
									mGL.glVertex2f(0, 0);
									mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u, 0.0f);
									mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u, 0.0f);
									mGL.glVertex2f((float)w, 0);
									mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u, v);
									mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u, v);
									mGL.glVertex2f((float)w, (float)h);
									mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0.0f, v);
									mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0.0f, v);
									mGL.glVertex2f(0, (float)h);
								mGL.glEnd();
								mGL.glEndOcclusionQueryNV();
								mGL.glFlush();

								mbGLOcclusionValid[0] = true;

								mGL.glDisable(GL_ALPHA_TEST);
								mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

								std::swap(mGLTextures[0], mGLTextures[1]);
								std::swap(mGLOcclusionQueries[0], mGLOcclusionQueries[1]);
								std::swap(mbGLOcclusionValid[0], mbGLOcclusionValid[1]);
								mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
								mGL.glDisable(GL_TEXTURE_2D);
								mGL.glActiveTextureARB(GL_TEXTURE0_ARB);
								mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);

								GLint pixelCount = 0;

								if (mbGLOcclusionValid[0]) {
									pixelCount = GetOcclusionQueryPixelCountSafe(mGLOcclusionQueries[0]);
									mbGLOcclusionValid[0] = false;
								}

								mbFrameValid[0] = (pixelCount > 0);

								VDASSERT(!mGL.glGetError());
								std::swap(mbFrameValid[0], mbFrameValid[1]);
								mProfileChannel.End();
							}
						} else
							mbFrameValid[0] = mbFrameValid[1] = true;
					} else
						mbFrameValid[0] = mbFrameValid[1] = true;

					mProfileChannel.Begin(0xa0ffa0, "GL:ConvertAndRead");
					switch(mVideoFormat->biCompression) {
					case VDMAKEFOURCC('Y', 'V', '1', '2'):
						if (mGL.ATI_fragment_shader)
							ConvertToYV12_GL_ATIFS(w, h, u, v);
						else if (mGL.NV_register_combiners) {
							if (mGL.NV_register_combiners2)
								ConvertToYV12_GL_NV2x(w, h, u, v);
							else
								ConvertToYV12_GL_NV1x(w, h, u, v);
						}
						break;
					case VDMAKEFOURCC('Y', 'U', 'Y', '2'):
						if (mGL.ATI_fragment_shader)
							ConvertToYUY2_GL_NV2x_ATIFS(w, h, u, v, true);
						else if (mGL.NV_register_combiners) {
							if (mGL.NV_register_combiners2)
								ConvertToYUY2_GL_NV2x_ATIFS(w, h, u, v, false);
							else
								ConvertToYUY2_GL_NV1x(w, h, u, v);
						}
						break;
					default:
						mGL.glReadBuffer(GL_BACK);
						if (mGL.EXT_pixel_buffer_object) {
							mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
							mGL.glReadPixels(0, 0, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)0);
						} else
							mGL.glReadPixels(0, 0, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)mGLReadBuffer.data());

						break;
					}
					mProfileChannel.End();

					// readback!

					mTimestampQueue[mTimestampIndex & 3] = globalTime;
					globalTime = mTimestampQueue[(mTimestampIndex + mTimestampDelay) & 3];
					++mTimestampIndex;

					if (globalTime >= 0) {
						if (mGL.EXT_pixel_buffer_object) {
							if (mbFrameValid[0]) {
								mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[1]);
								mProfileChannel.Begin(0xa0a0ff, "GL:MapBuffer");
								void *p = mGL.glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
								mProfileChannel.End();
								DispatchFrame(p, mOffscreenSize, globalTime);
								mGL.glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
							} else
								DispatchFrame(NULL, 0, globalTime);

							std::swap(mGLBuffers[0], mGLBuffers[1]);
						} else {
							if (mbFrameValid[0])
								DispatchFrame(mGLReadBuffer.data(), mOffscreenSize, globalTime);
							else
								DispatchFrame(NULL, 0, globalTime);
						}
					}

					VDASSERT(!mGL.glGetError());

					// necessary for ATI to work
					if (mGL.ATI_fragment_shader) {
						if (mGL.EXT_swap_control)
							mGL.wglSwapIntervalEXT(0);
						mGL.wglSwapBuffers(hdc);
					}

					mGL.End();
				}
				ReleaseDC(mhwndGL, hdc);
			}
		} else {
			mProfileChannel.Begin(0xf0d0d0, "Capture (GDI)");
			if (HDC hdc = GetDC(NULL)) {
				static DWORD sBitBltMode = AutodetectCaptureBltMode();

				int srcx = mTrackX;
				int srcy = mTrackY;

				int limitx = GetSystemMetrics(SM_CXSCREEN) - w;
				int limity = GetSystemMetrics(SM_CYSCREEN) - h;

				if (srcx > limitx)
					srcx = limitx;

				if (srcx < 0)
					srcx = 0;

				if (srcy > limity)
					srcy = limity;

				if (srcy < 0)
					srcy = 0;

				BitBlt(mhdcOffscreen, 0, 0, w, h, hdc, srcx, srcy, sBitBltMode);
				if (ci.hCursor)
					DrawIcon(mhdcOffscreen, ci.ptScreenPos.x - srcx, ci.ptScreenPos.y - srcy, ci.hCursor);
				ReleaseDC(NULL, hdc);
			}
			mProfileChannel.End();

			if (mDisplayMode == kDisplaySoftware || mDisplayMode == kDisplayAnalyze) {
				mProfileChannel.Begin(0xe0e0e0, "Preview (GDI)");
				if (HDC hdc = GetDC(mhwnd)) {
					BitBlt(hdc, 0, 0, w, h, mhdcOffscreen, 0, 0, SRCCOPY);
					ReleaseDC(mhwnd, hdc);
				}
				mProfileChannel.End();
			}

			GdiFlush();
			DispatchFrame(mpOffscreenData, mOffscreenSize, globalTime);
		}
	}
	
	if (mbVisible) {
		if (mbOpenGLMode) {
			if (mDisplayMode == kDisplayHardware || mDisplayMode == kDisplaySoftware) {
				RECT rdraw;
				GetClientRect(mhwndGLDraw, &rdraw);

				if (rdraw.right && rdraw.bottom) {
					mProfileChannel.Begin(0xe0e0e0, "Overlay (OpenGL)");
					if (HDC hdcDraw = GetDC(mhwndGLDraw)) {
						if (mGL.Begin(hdcDraw)) {
							VDASSERT(!mGL.glGetError());
							mGL.glDisable(GL_LIGHTING);
							mGL.glDisable(GL_CULL_FACE);
							mGL.glDisable(GL_BLEND);
							mGL.glDisable(GL_ALPHA_TEST);
							mGL.glDisable(GL_DEPTH_TEST);
							mGL.glDisable(GL_STENCIL_TEST);
							mGL.glDisable(GL_SCISSOR_TEST);
							mGL.glEnable(GL_TEXTURE_2D);

							VDASSERT(!mGL.glGetError());
							mGL.DisableFragmentShaders();

							float dstw = (float)rdraw.right;
							float dsth = (float)rdraw.bottom;

							VDASSERT(!mGL.glGetError());
							mGL.glViewport(0, 0, rdraw.right, rdraw.bottom);
							mGL.glMatrixMode(GL_MODELVIEW);
							mGL.glLoadIdentity();
							mGL.glMatrixMode(GL_PROJECTION);
							mGL.glLoadIdentity();
							mGL.glOrtho(0, dstw, 0, dsth, -1.0f, 1.0f);

							mGL.glDrawBuffer(GL_BACK);
							mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
							mGL.glClearColor(0.5f, 0.0f, 0.0f, 0.0f);
							mGL.glClear(GL_COLOR_BUFFER_BIT);
							VDASSERT(!mGL.glGetError());

							mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
							VDASSERT(!mGL.glGetError());

							mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
							mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
							mGL.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
							mGL.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

							float u = (float)srcw / (float)mGLTextureW;
							float v = (float)srch / (float)mGLTextureH;

							VDASSERT(!mGL.glGetError());
							mGL.glBegin(GL_QUADS);
								mGL.glTexCoord2f(0.0f, 0.0f);
								mGL.glVertex2f(0, 0);
								mGL.glTexCoord2f(u, 0.0f);
								mGL.glVertex2f(dstw, 0);
								mGL.glTexCoord2f(u, v);
								mGL.glVertex2f(dstw, dsth);
								mGL.glTexCoord2f(0.0f, v);
								mGL.glVertex2f(0, dsth);
							mGL.glEnd();
							VDASSERT(!mGL.glGetError());

							if (mGL.EXT_swap_control)
								mGL.wglSwapIntervalEXT(0);

							mGL.wglSwapBuffers(hdcDraw);
							mGL.End();
						}
						ReleaseDC(mhwndGLDraw, hdcDraw);
					}
					mProfileChannel.End();
				}
			}
		} else {
			if (mDisplayMode == kDisplayHardware) {
				mProfileChannel.Begin(0xe0e0e0, "Overlay (GDI)");
				if (HDC hdcScreen = GetDC(NULL)) {
					if (HDC hdc = GetDC(mhwnd)) {
						int srcx = mTrackX;
						int srcy = mTrackY;

						int limitx = GetSystemMetrics(SM_CXSCREEN) - w;
						int limity = GetSystemMetrics(SM_CYSCREEN) - h;

						if (srcx > limitx)
							srcx = limitx;

						if (srcx < 0)
							srcx = 0;

						if (srcy > limity)
							srcy = limity;

						if (srcy < 0)
							srcy = 0;

						BitBlt(hdc, 0, 0, w, h, hdcScreen, srcx, srcy, SRCCOPY);
						ReleaseDC(mhwnd, hdc);
					}
					ReleaseDC(NULL, hdcScreen);
				}
				mProfileChannel.End();
			}
		}
	}

	if (mDisplayMode)
		++mPreviewFrameCount;
}

void VDCaptureDriverScreen::DispatchFrame(const void *data, uint32 size, sint64 timestamp) {
	if (!mpCB)
		return;

	if (mDisplayMode == kDisplayAnalyze && size) {
		try {
			mpCB->CapProcessData(-1, data, size, 0, true, 0);
		} catch(MyError&) {
			// Eat preview errors.
		}
	}

	if (mbCapturing) {
		try {
			if (mpCB->CapEvent(kEventCapturing, 0))
				mpCB->CapProcessData(0, data, size, timestamp, true, timestamp);
			else {
				mbCapturing = false;
				CaptureAbort();
			}

		} catch(MyError& e) {
			mCaptureError.TransferFrom(e);
			CaptureAbort();
			mbCapturing = false;
		}
	}
}

void VDCaptureDriverScreen::ConvertToYV12_GL_NV1x(int w, int h, float u, float v) {
	// YV12 conversion - NV_register_combiners path
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	float y = 0.0f;
	float w4 = (float)w * 0.25f;
	for(int phase=0; phase<4; phase += 2) {
		float u0 = ((float)phase - 1.5f) / (float)mGLTextureW;
		float v0 = 0;
		float u1 = u0 + u;
		float v1 = v0 + v;

		float u2 = ((float)phase - 0.5f) / (float)mGLTextureW;
		float u3 = u2 + u;

		mGL.glCallList(mGLShaderBase + (phase ? kVDOpenGLTechIndex_YV12_NV1x_Y_ra : kVDOpenGLTechIndex_YV12_NV1x_Y_gb));
		mGL.glColorMask(GL_TRUE, phase==0, phase==0, GL_TRUE);

		float y0 = y;
		float y1 = y + (float)h;

		mGL.glColor4f(0.0f, 0.0f, 1.0f, 0.0f);

		mGL.glBegin(GL_QUADS);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);	mGL.glVertex2f(0.0f, y0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);	mGL.glVertex2f(w4, y0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);	mGL.glVertex2f(w4, y1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);	mGL.glVertex2f(0.0f, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());
	}

	y = 0.0f;

	float w8 = (float)w * 0.125f;
	int h2 = h >> 1;
	for(int cmode=0; cmode<2; ++cmode) {
		mGL.glCallList(mGLShaderBase + (cmode ? kVDOpenGLTechIndex_YV12_NV1x_Cb : kVDOpenGLTechIndex_YV12_NV1x_Cr));
		for(int phase=0; phase<4; phase += 2) {
			float u0 = (float)(phase * 2 - 3.0f) / (float)mGLTextureW;
			float v0 = 0;
			float u1 = u0 + u;
			float v1 = v0 + v;

			float u2 = (float)(phase * 2 - 1.0f) / (float)mGLTextureW;
			float u3 = u0 + u;

			mGL.glColorMask(GL_TRUE, phase==0, phase==0, GL_TRUE);

			float y0 = y;
			float y1 = y + (float)h2;

			mGL.glBegin(GL_QUADS);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);	mGL.glVertex2f(w4, y0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);	mGL.glVertex2f(w4+w8, y0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);	mGL.glVertex2f(w4+w8, y1);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);	mGL.glVertex2f(w4, y1);
			mGL.glEnd();
			VDASSERT(!mGL.glGetError());
		}

		y += h2;
	}

	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	// readback!
	VDASSERT(!mGL.glGetError());
	mGL.glReadBuffer(GL_BACK);

	if (mGL.EXT_pixel_buffer_object) {
		mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
		mGL.glReadPixels(0, 0, w >> 2, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)0);
		mGL.glReadPixels(w >> 2, 0, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)(w * h));
		mGL.glReadPixels(w >> 2, h >> 1, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)(w * h * 5 / 4));
	} else {
		char *dst = (char *)mGLReadBuffer.data();
		mGL.glReadPixels(0, 0, w >> 2, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst);
		mGL.glReadPixels(w >> 2, 0, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst + (w * h));
		mGL.glReadPixels(w >> 2, h >> 1, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst + (w * h * 5 / 4));
	}
}

void VDCaptureDriverScreen::ConvertToYV12_GL_NV2x(int w, int h, float u, float v) {
	// YV12 conversion - NV_register_combiners2 path
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	float y = 0.0f;
	float w4 = (float)w * 0.25f;
	{
		float u0 = -1.5f / (float)mGLTextureW;
		float v0 = 0;
		float u1 = u0 + u;
		float v1 = v0 + v;

		float u2 = -0.5f / (float)mGLTextureW;
		float u3 = u2 + u;

		float u4 = +0.5f / (float)mGLTextureW;
		float u5 = u4 + u;

		float u6 = +1.5f / (float)mGLTextureW;
		float u7 = u6 + u;

		mGL.glCallList(mGLShaderBase + kVDOpenGLTechIndex_YV12_NV2x_Y);
		mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		float y0 = y;
		float y1 = y + (float)h;

		mGL.glColor4f(0.0f, 0.0f, 1.0f, 0.0f);

		mGL.glBegin(GL_QUADS);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u6, v1);
			mGL.glVertex2f(0.0f, y0);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u7, v1);
			mGL.glVertex2f(w4, y0);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u7, v0);
			mGL.glVertex2f(w4, y1);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u6, v0);
			mGL.glVertex2f(0.0f, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());
	}

	mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
	mGL.glDisable(GL_TEXTURE_2D);

	y = 0.0f;

	float w8 = (float)w * 0.125f;
	int h2 = h >> 1;
	for(int cmode=0; cmode<2; ++cmode) {
		mGL.glCallList(mGLShaderBase + (cmode ? kVDOpenGLTechIndex_YV12_NV2x_Cb : kVDOpenGLTechIndex_YV12_NV2x_Cr));
		for(int phase=0; phase<4; phase += 2) {
			float u0 = (float)(phase * 2 - 4.0f) / (float)mGLTextureW;
			float v0 = 0;
			float u1 = u0 + u;
			float v1 = v0 + v;

			float u2 = (float)(phase * 2 - 2.0f) / (float)mGLTextureW;
			float u3 = u2 + u;

			float u4 = (float)(phase * 2 - 0.0f) / (float)mGLTextureW;
			float u5 = u4 + u;

			mGL.glColorMask(GL_TRUE, phase==0, phase==0, GL_TRUE);

			float y0 = y;
			float y1 = y + (float)h2;

			mGL.glBegin(GL_QUADS);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v1);
				mGL.glVertex2f(w4, y0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v1);
				mGL.glVertex2f(w4+w8, y0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v0);
				mGL.glVertex2f(w4+w8, y1);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v0);
				mGL.glVertex2f(w4, y1);
			mGL.glEnd();
			VDASSERT(!mGL.glGetError());
		}

		y += h2;
	}

	mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	// readback!
	VDASSERT(!mGL.glGetError());
	mGL.glReadBuffer(GL_BACK);

	if (mGL.EXT_pixel_buffer_object) {
		mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
		mGL.glReadPixels(0, 0, w >> 2, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)0);
		mGL.glReadPixels(w >> 2, 0, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)(w * h));
		mGL.glReadPixels(w >> 2, h >> 1, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)(w * h * 5 / 4));
	} else {
		char *dst = (char *)mGLReadBuffer.data();
		mGL.glReadPixels(0, 0, w >> 2, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst);
		mGL.glReadPixels(w >> 2, 0, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst + (w * h));
		mGL.glReadPixels(w >> 2, h >> 1, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst + (w * h * 5 / 4));
	}
}

void VDCaptureDriverScreen::ConvertToYV12_GL_ATIFS(int w, int h, float u, float v) {
	// YV12 conversion - NV_register_combiners2 path
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	float y = 0.0f;
	float w4 = (float)w * 0.25f;
	{
		float u0 = -1.5f / (float)mGLTextureW;
		float v0 = 0;
		float u1 = u0 + u;
		float v1 = v0 + v;

		float u2 = -0.5f / (float)mGLTextureW;
		float u3 = u2 + u;

		float u4 = +0.5f / (float)mGLTextureW;
		float u5 = u4 + u;

		float u6 = +1.5f / (float)mGLTextureW;
		float u7 = u6 + u;

		mGL.glCallList(mGLShaderBase + kVDOpenGLTechIndex_YV12_ATIFS_Y);
		mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		float y0 = y;
		float y1 = y + (float)h;

		mGL.glColor4f(0.0f, 0.0f, 1.0f, 0.0f);

		mGL.glBegin(GL_QUADS);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u6, v1);
			mGL.glVertex2f(0.0f, y0);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u7, v1);
			mGL.glVertex2f(w4, y0);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u7, v0);
			mGL.glVertex2f(w4, y1);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u6, v0);
			mGL.glVertex2f(0.0f, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());
	}

	mGL.glActiveTextureARB(GL_TEXTURE4_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);

	y = 0.0f;

	float w8 = (float)w * 0.125f;
	int h2 = h >> 1;
	mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	for(int cmode=0; cmode<2; ++cmode) {
		mGL.glCallList(mGLShaderBase + (cmode ? kVDOpenGLTechIndex_YV12_ATIFS_Cb : kVDOpenGLTechIndex_YV12_ATIFS_Cr));
		float u0 = -4.5f / (float)mGLTextureW;		float u1 = u0 + u;
		float u2 = -2.5f / (float)mGLTextureW;		float u3 = u2 + u;
		float u4 = -0.5f / (float)mGLTextureW;		float u5 = u4 + u;
		float u6 = +0.5f / (float)mGLTextureW;		float u7 = u6 + u;
		float u8 = +2.5f / (float)mGLTextureW;		float u9 = u8 + u;

		float v0 = 0;
		float v1 = v0 + v;

		float y0 = y;
		float y1 = y + (float)h2;

		mGL.glBegin(GL_QUADS);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u6, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE4_ARB, u8, v1);
			mGL.glVertex2f(w4, y0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u7, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE4_ARB, u9, v1);
			mGL.glVertex2f(w4+w8, y0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u7, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE4_ARB, u9, v0);
			mGL.glVertex2f(w4+w8, y1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u6, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE4_ARB, u8, v0);
			mGL.glVertex2f(w4, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());

		y += h2;
	}

	mGL.glActiveTextureARB(GL_TEXTURE4_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	// readback!
	VDASSERT(!mGL.glGetError());
	mGL.glReadBuffer(GL_BACK);

	if (mGL.EXT_pixel_buffer_object) {
		mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
		mGL.glReadPixels(0, 0, w >> 2, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)0);
		mGL.glReadPixels(w >> 2, 0, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)(w * h));
		mGL.glReadPixels(w >> 2, h >> 1, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)(w * h * 5 / 4));
	} else {
		char *dst = (char *)mGLReadBuffer.data();
		mGL.glReadPixels(0, 0, w >> 2, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst);
		mGL.glReadPixels(w >> 2, 0, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst + (w * h));
		mGL.glReadPixels(w >> 2, h >> 1, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst + (w * h * 5 / 4));
	}
}

void VDCaptureDriverScreen::ConvertToYUY2_GL_NV1x(int w, int h, float u, float v) {
	// YUY2 conversion - NV_register_combiners path
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);
	mGL.glCallList(mGLShaderBase + kVDOpenGLTechIndex_YUY2_NV1x_Y);
	VDASSERT(!mGL.glGetError());

	float y = 0.0f;
	float w2 = (float)w * 0.5f;
	{
		float u0 = -0.5f / (float)mGLTextureW;
		float v0 = 0;
		float u1 = u0 + u;
		float v1 = v0 + v;

		float u2 = +0.5f / (float)mGLTextureW;
		float u3 = u2 + u;

		mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		float y0 = y;
		float y1 = y + (float)h;

		mGL.glColor4f(0.0f, 0.0f, 1.0f, 0.0f);
		mGL.glBegin(GL_QUADS);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);	mGL.glVertex2f(0.0f, y0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);	mGL.glVertex2f(w2, y0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);	mGL.glVertex2f(w2, y1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);	mGL.glVertex2f(0.0f, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());
	}

	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	{
		mGL.glCallList(mGLShaderBase + kVDOpenGLTechIndex_YUY2_NV1x_C);

		float u0 = 0;
		float v0 = 0;
		float u1 = u0 + u;
		float v1 = v0 + v;

		mGL.glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);

		float y0 = 0;
		float y1 = (float)h;

		mGL.glBegin(GL_QUADS);
			mGL.glTexCoord2f(u0, v1);	mGL.glVertex2f(0, y0);
			mGL.glTexCoord2f(u1, v1);	mGL.glVertex2f(w2, y0);
			mGL.glTexCoord2f(u1, v0);	mGL.glVertex2f(w2, y1);
			mGL.glTexCoord2f(u0, v0);	mGL.glVertex2f(0, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());
	}

	VDASSERT(!mGL.glGetError());
	mGL.glReadBuffer(GL_BACK);
	if (mGL.EXT_pixel_buffer_object) {
		mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
		mGL.glReadPixels(0, 0, w >> 1, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)0);
	} else {
		char *dst = (char *)mGLReadBuffer.data();
		mGL.glReadPixels(0, 0, w >> 1, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst);
	}
}

void VDCaptureDriverScreen::ConvertToYUY2_GL_NV2x_ATIFS(int w, int h, float u, float v, bool atifs) {
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);
	mGL.glCallList(mGLShaderBase + (atifs ? kVDOpenGLTechIndex_YUY2_ATIFS : kVDOpenGLTechIndex_YUY2_NV2x));
	VDASSERT(!mGL.glGetError());

	float y = 0.0f;
	float w2 = (float)w * 0.5f;

	mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	{
		float u0 = -0.5f / (float)mGLTextureW;		// Y1
		float u1 = +0.5f / (float)mGLTextureW;		// Y2
		float u2 = -1.0f / (float)mGLTextureW;		// chroma left
		float u3 =  0.0f / (float)mGLTextureW;		// chroma right
		float v0 = 0;
		float v1 = v0 + v;

		float y0 = y;
		float y1 = y + (float)h;

		mGL.glBegin(GL_QUADS);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u1, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u2, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u3, v1);
			mGL.glVertex2f(0.0f, y0);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0+u, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u1+u, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u2+u, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u3+u, v1);
			mGL.glVertex2f(w2, y0);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0+u, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u1+u, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u2+u, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u3+u, v0);
			mGL.glVertex2f(w2, y1);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u1, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u2, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u3, v0);
			mGL.glVertex2f(0.0f, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());
	}

	mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	VDASSERT(!mGL.glGetError());
	mGL.glReadBuffer(GL_BACK);
	if (mGL.EXT_pixel_buffer_object) {
		mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
		mGL.glReadPixels(0, 0, w >> 1, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)0);
	} else {
		char *dst = (char *)mGLReadBuffer.data();
		mGL.glReadPixels(0, 0, w >> 1, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst);
	}
}

GLuint VDCaptureDriverScreen::GetOcclusionQueryPixelCountSafe(GLuint query) {
	GLint rv;
	mGL.glFlush();

	int iters = 1000;
	for(;;) {
		mGL.glGetOcclusionQueryivNV(query, GL_PIXEL_COUNT_AVAILABLE_NV, &rv);
		if (rv)
			break;

		if (iters)
			--iters;
		else
			::Sleep(1);
	}

	mGL.glGetOcclusionQueryivNV(query, GL_PIXEL_COUNT_NV, &rv);
	return rv;
}

void VDCaptureDriverScreen::LoadSettings() {
	VDRegistryAppKey key("Capture\\Screen capture");

	mbTrackCursor = key.getBool("Track cursor", mbTrackCursor);
	mbTrackActiveWindow = key.getBool("Track active window", mbTrackActiveWindow);
	mbTrackActiveWindowClient = key.getBool("Track active window client", mbTrackActiveWindowClient);
	mbDrawMousePointer = key.getBool("Draw mouse pointer", mbDrawMousePointer);
	mbRescaleImage = key.getBool("Rescale image", mbRescaleImage);
	mbOpenGLMode = key.getBool("OpenGL mode", mbOpenGLMode);
	mbRemoveDuplicates = key.getBool("Remove duplicates", mbRemoveDuplicates);
	mRescaleW = key.getInt("Rescale width", mRescaleW);
	mRescaleH = key.getInt("Rescale height", mRescaleH);

	if (mRescaleW < 1)
		mRescaleW = 1;

	if (mRescaleW > 32768)
		mRescaleW = 32768;

	if (mRescaleH < 1)
		mRescaleH = 1;

	if (mRescaleH > 32768)
		mRescaleH = 32768;

	mTrackOffsetX = key.getInt("Position X", mTrackOffsetX);
	mTrackOffsetY = key.getInt("Position Y", mTrackOffsetY);
}

void VDCaptureDriverScreen::SaveSettings() {
	VDRegistryAppKey key("Capture\\Screen capture");

	key.setBool("Track cursor", mbTrackCursor);
	key.setBool("Track active window", mbTrackActiveWindow);
	key.setBool("Track active window client", mbTrackActiveWindowClient);
	key.setBool("Draw mouse pointer", mbDrawMousePointer);
	key.setBool("Rescale image", mbRescaleImage);
	key.setBool("OpenGL mode", mbOpenGLMode);
	key.setBool("Remove duplicates", mbRemoveDuplicates);
	key.setInt("Rescale width", mRescaleW);
	key.setInt("Rescale height", mRescaleH);
	key.setInt("Position X", mTrackOffsetX);
	key.setInt("Position Y", mTrackOffsetY);
}

void VDCaptureDriverScreen::TimerCallback() {
	mbCaptureFramePending = true;
	PostMessage(mhwnd, WM_APP+18, 0, 0);
}

LRESULT CALLBACK VDCaptureDriverScreen::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_NCCREATE:
			SetWindowLongPtr(hwnd, 0, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
			break;
		case WM_SIZE:
			{
				VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hwnd, 0);
				if (pThis->mhwndGLDraw)
					SetWindowPos(pThis->mhwndGLDraw, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
			}
			break;
		case MM_WIM_DATA:
			{
				VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hwnd, 0);

				if (pThis->mpCB) {
					WAVEHDR& hdr = *(WAVEHDR *)lParam;

					if (pThis->mbCapturing) {
						if (pThis->mbAudioCaptureEnabled) {
							try {
								pThis->mpCB->CapProcessData(1, hdr.lpData, hdr.dwBytesRecorded, -1, false, pThis->ComputeGlobalTime());
							} catch(MyError& e) {
								pThis->mCaptureError.TransferFrom(e);
								pThis->CaptureAbort();
							}

							waveInAddBuffer(pThis->mhWaveIn, &hdr, sizeof(WAVEHDR));
						}
					} else if (pThis->mbAudioAnalysisActive) {
						// For some reason this is sometimes called after reset. Don't know why yet.
						if (hdr.dwBytesRecorded) {
							try {
								pThis->mpCB->CapProcessData(-2, hdr.lpData, hdr.dwBytesRecorded, -1, false, 0);
							} catch(const MyError&) {
								// eat the error
							}
						}

						waveInAddBuffer(pThis->mhWaveIn, &hdr, sizeof(WAVEHDR));
					}
				}
			}
			return 0;
		case WM_APP+16:
			{
				VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hwnd, 0);
				pThis->SyncCaptureStop();
			}
			return 0;
		case WM_APP+17:
			{
				VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hwnd, 0);
				pThis->SyncCaptureAbort();
			}
			return 0;
		case WM_APP+18:
			{
				VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hwnd, 0);
				if (pThis->mbCaptureFramePending && pThis->mbCapturing) {
					pThis->mbCaptureFramePending = false;
					pThis->DoFrame();
				}
			}
			return 0;
		case WM_TIMER:
			{
				VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hwnd, 0);
				if (!pThis->mbCapturing)
					pThis->DoFrame();
			}
			return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK VDCaptureDriverScreen::StaticWndProcGL(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

INT_PTR CALLBACK VDCaptureDriverScreen::VideoSourceDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hdlg, DWLP_USER);

	switch(msg) {
	case WM_INITDIALOG:
		SetWindowLongPtr(hdlg, DWLP_USER, (LONG_PTR)lParam);
		pThis = (VDCaptureDriverScreen *)lParam;
		CheckDlgButton(hdlg, IDC_POSITION_TRACKMOUSE, pThis->mbTrackCursor);
		CheckDlgButton(hdlg, IDC_POSITION_FIXED, !pThis->mbTrackCursor);

		CheckDlgButton(hdlg, IDC_PANNING_DESKTOP, !pThis->mbTrackActiveWindow);
		CheckDlgButton(hdlg, IDC_PANNING_ACTIVEWINDOW, pThis->mbTrackActiveWindow && !pThis->mbTrackActiveWindowClient);
		CheckDlgButton(hdlg, IDC_PANNING_ACTIVECLIENT, pThis->mbTrackActiveWindow && pThis->mbTrackActiveWindowClient);

		CheckDlgButton(hdlg, IDC_DRAW_CURSOR, pThis->mbDrawMousePointer);
		CheckDlgButton(hdlg, IDC_RESCALE_IMAGE, pThis->mbRescaleImage);
		CheckDlgButton(hdlg, IDC_USE_OPENGL, pThis->mbOpenGLMode);
		CheckDlgButton(hdlg, IDC_REMOVE_DUPES, pThis->mbRemoveDuplicates);
		SetDlgItemInt(hdlg, IDC_WIDTH, pThis->mRescaleW, FALSE);
		SetDlgItemInt(hdlg, IDC_HEIGHT, pThis->mRescaleH, FALSE);
		SetDlgItemInt(hdlg, IDC_POSITION_X, pThis->mTrackOffsetX, TRUE);
		SetDlgItemInt(hdlg, IDC_POSITION_Y, pThis->mTrackOffsetY, TRUE);
reenable:
		{
			bool enableOpenGL = !!IsDlgButtonChecked(hdlg, IDC_USE_OPENGL);
			EnableWindow(GetDlgItem(hdlg, IDC_STATIC_OPENGL), enableOpenGL);
			EnableWindow(GetDlgItem(hdlg, IDC_RESCALE_IMAGE), enableOpenGL);
			EnableWindow(GetDlgItem(hdlg, IDC_REMOVE_DUPES), enableOpenGL);

			bool rescale = enableOpenGL && IsDlgButtonChecked(hdlg, IDC_RESCALE_IMAGE);
			EnableWindow(GetDlgItem(hdlg, IDC_STATIC_WIDTH), rescale);
			EnableWindow(GetDlgItem(hdlg, IDC_STATIC_HEIGHT), rescale);
			EnableWindow(GetDlgItem(hdlg, IDC_WIDTH), rescale);
			EnableWindow(GetDlgItem(hdlg, IDC_HEIGHT), rescale);
		}
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			{
				BOOL success;
				UINT w = GetDlgItemInt(hdlg, IDC_WIDTH, &success, FALSE);
				if (w <= 0 || !success) {
					MessageBeep(MB_ICONEXCLAMATION);
					SetFocus(GetDlgItem(hdlg, IDC_WIDTH));
					return TRUE;
				}
				UINT h = GetDlgItemInt(hdlg, IDC_HEIGHT, &success, FALSE);
				if (h <= 0 || !success) {
					MessageBeep(MB_ICONEXCLAMATION);
					SetFocus(GetDlgItem(hdlg, IDC_HEIGHT));
					return TRUE;
				}

				if (!IsDlgButtonChecked(hdlg, IDC_POSITION_TRACKMOUSE)) {
					int x = GetDlgItemInt(hdlg, IDC_POSITION_X, &success, TRUE);
					if (!success) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(GetDlgItem(hdlg, IDC_POSITION_X));
						return TRUE;
					}

					int y = GetDlgItemInt(hdlg, IDC_POSITION_Y, &success, TRUE);
					if (!success) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(GetDlgItem(hdlg, IDC_POSITION_X));
						return TRUE;
					}

					pThis->mTrackOffsetX = x;
					pThis->mTrackOffsetY = y;
					pThis->mbTrackCursor = false;
				} else {
					pThis->mbTrackCursor = true;
				}

				pThis->mRescaleW = w;
				pThis->mRescaleH = h;

				if (IsDlgButtonChecked(hdlg, IDC_PANNING_DESKTOP)) {
					pThis->mbTrackActiveWindow = false;
					pThis->mbTrackActiveWindowClient = false;
				} else if (IsDlgButtonChecked(hdlg, IDC_PANNING_ACTIVEWINDOW)) {
					pThis->mbTrackActiveWindow = true;
					pThis->mbTrackActiveWindowClient = false;
				} else if (IsDlgButtonChecked(hdlg, IDC_PANNING_ACTIVECLIENT)) {
					pThis->mbTrackActiveWindow = true;
					pThis->mbTrackActiveWindowClient = true;
				}

				pThis->mbDrawMousePointer = !!IsDlgButtonChecked(hdlg, IDC_DRAW_CURSOR);
				pThis->mbRescaleImage = !!IsDlgButtonChecked(hdlg, IDC_RESCALE_IMAGE);
				pThis->mbOpenGLMode = !!IsDlgButtonChecked(hdlg, IDC_USE_OPENGL);
				pThis->mbRemoveDuplicates = !!IsDlgButtonChecked(hdlg, IDC_REMOVE_DUPES);
			}
			EndDialog(hdlg, TRUE);
			return TRUE;
		case IDCANCEL:
			EndDialog(hdlg, FALSE);
			return TRUE;
		case IDC_USE_OPENGL:
		case IDC_RESCALE_IMAGE:
			if (HIWORD(wParam) == BN_CLICKED)
				goto reenable;
			break;
		}
		break;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////

class VDCaptureSystemScreen : public IVDCaptureSystem {
public:
	VDCaptureSystemScreen();
	~VDCaptureSystemScreen();

	void EnumerateDrivers();

	int GetDeviceCount();
	const wchar_t *GetDeviceName(int index);

	IVDCaptureDriver *CreateDriver(int deviceIndex);
};

IVDCaptureSystem *VDCreateCaptureSystemScreen() {
	return new VDCaptureSystemScreen;
}

VDCaptureSystemScreen::VDCaptureSystemScreen()
{
}

VDCaptureSystemScreen::~VDCaptureSystemScreen() {
}

void VDCaptureSystemScreen::EnumerateDrivers() {
}

int VDCaptureSystemScreen::GetDeviceCount() {
	return 1;
}

const wchar_t *VDCaptureSystemScreen::GetDeviceName(int index) {
	return !index ? L"Screen capture" : NULL;
}

IVDCaptureDriver *VDCaptureSystemScreen::CreateDriver(int index) {
	if (index)
		return NULL;

	return new VDCaptureDriverScreen();
}
