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

#include "stdafx.h"
#include <process.h>

#include <windows.h>
#include <vfw.h>
#include <commdlg.h>

#include "InputFile.h"
#include "InputFileAVI.h"
#include "AudioSource.h"
#include "AudioSourceAVI.h"
#include "VideoSource.h"
#include "VideoSourceAVI.h"
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/Dita/resources.h>
#include <vd2/Dita/services.h>
#include <vd2/Riza/audioformat.h>
#include "AVIStripeSystem.h"
#include "AVIReadHandler.h"

#include "gui.h"
#include "oshelper.h"
#include "prefs.h"
#include "misc.h"

#include "resource.h"

extern HINSTANCE g_hInst;
extern const wchar_t fileFiltersAppend[];
extern HWND g_hWnd;

bool VDPreferencesIsPreferInternalVideoDecodersEnabled();

namespace {
	enum { kVDST_InputFileAVI = 4 };

	enum {
		kVDM_OpeningFile,			// AVI: Opening file "%ls"
		kVDM_RekeyNotSpecified,		// AVI: Keyframe flag reconstruction was not specified in open options and the video stream is not a known keyframe-only type.  Seeking in the video stream may be extremely slow.
		kVDM_Type1DVNoSound,		// AVI: Type-1 DV file detected -- VirtualDub cannot extract audio from this type of interleaved stream.
		kVDM_InvalidBlockAlign		// AVI: An invalid nBlockAlign value of zero in the audio stream format was fixed.
	};
}

/////////////////////////////////////////////////////////////////////

VDAVIStreamSource::VDAVIStreamSource(InputFileAVI *pParent)
	: mpParent(pParent)
{
	mpParent->Attach(this);
}

VDAVIStreamSource::VDAVIStreamSource(const VDAVIStreamSource& src)
	: mpParent(src.mpParent)
{
	mpParent->Attach(this);
}

VDAVIStreamSource::~VDAVIStreamSource() {
	mpParent->Detach(this);
}

/////////////////////////////////////////////////////////////////////

class VDInputDriverAVI2 : public vdrefcounted<IVDInputDriver> {
public:
	const wchar_t *GetSignatureName() { return L"Audio/video interleave input driver (internal)"; }

	int GetDefaultPriority() {
		return 0;
	}

	uint32 GetFlags() { return kF_Video | kF_Audio; }

	const wchar_t *GetFilenamePattern() {
		return L"Audio/video interleave (*.avi,*.divx)\0*.avi;*.divx\0";
	}

	bool DetectByFilename(const wchar_t *pszFilename) {
		return false;
	}

	int DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
		if (nHeaderSize >= 12) {
			if (!memcmp(pHeader, "RIFF", 4) && !memcmp((char*)pHeader+8, "AVI ", 4))
				return 1;
		}

		return -1;
	}

	InputFile *CreateInputFile(uint32 flags) {
		InputFileAVI *pf = new_nothrow InputFileAVI;

		if (!pf)
			throw MyMemoryError();

		if (flags & kOF_Quiet)
			pf->setAutomated(true);

		if (flags & kOF_AutoSegmentScan)
			pf->EnableSegmentAutoscan();

		return pf;
	}
};

class VDInputDriverAVI1 : public vdrefcounted<IVDInputDriver> {
public:
	const wchar_t *GetSignatureName() { return L"AVIFile/Avisynth input driver (internal)"; }

	int GetDefaultPriority() {
		return -4;
	}

	uint32 GetFlags() { return kF_Video | kF_Audio; }

	const wchar_t *GetFilenamePattern() {
		return L"AVIFile input driver (compat.) (*.avs,*.vdr)\0*.avs;*.vdr\0";
	}

	bool DetectByFilename(const wchar_t *pszFilename) {
		size_t l = wcslen(pszFilename);
		if (l > 4) {
			if (!_wcsicmp(pszFilename + l - 4, L".avs"))
				return true;
			if (!_wcsicmp(pszFilename + l - 4, L".vdr"))
				return true;
		}

		return false;
	}

	int DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
		if (nHeaderSize >= 12) {
			if (!memcmp(pHeader, "RIFF", 4) && !memcmp((char*)pHeader+8, "VDRM", 4))
				return 1;
		}

		return -1;
	}

	InputFile *CreateInputFile(uint32 flags) {
		InputFileAVI *pf = new_nothrow InputFileAVI;

		if (!pf)
			throw MyMemoryError();

		pf->ForceCompatibility();

		if (flags & kOF_Quiet)
			pf->setAutomated(true);

		if (flags & kOF_AutoSegmentScan)
			pf->EnableSegmentAutoscan();

		return pf;
	}
};

extern IVDInputDriver *VDCreateInputDriverAVI1() { return new VDInputDriverAVI1; }
extern IVDInputDriver *VDCreateInputDriverAVI2() { return new VDInputDriverAVI2; }

/////////////////////////////////////////////////////////////////////

char InputFileAVI::szME[]="AVI Import Filter";

InputFileAVI::InputFileAVI() {
	fAutomated	= false;

	fAcceptPartial = false;
	fInternalDecoder = VDPreferencesIsPreferInternalVideoDecodersEnabled();
	fDisableFastIO = false;
	iMJPEGMode = 0;
	fccForceVideo = 0;
	fccForceVideoHandler = 0;
	lForceAudioHz = 0;

	pAVIFile = NULL;

	fCompatibilityMode = fRedoKeyFlags = false;

	fAutoscanSegments = false;
}

InputFileAVI::~InputFileAVI() {
	if (pAVIFile)
		pAVIFile->Release();
}

///////////////////////////////////////////////

class InputFileAVIOptions : public InputFileOptions {
public:
	struct InputFileAVIOpts {
		int len;
		int iMJPEGMode;
		FOURCC fccForceVideo;
		FOURCC fccForceVideoHandler;
		long lForceAudioHz;

		bool fCompatibilityMode;
		bool fAcceptPartial;
		bool fRedoKeyFlags;
		bool fInternalDecoder;
		bool fDisableFastIO;
	} opts;
		
	~InputFileAVIOptions();

	bool read(const char *buf);
	int write(char *buf, int buflen) const;

	static INT_PTR APIENTRY SetupDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
};

InputFileAVIOptions::~InputFileAVIOptions() {
}

bool InputFileAVIOptions::read(const char *buf) {
	const InputFileAVIOpts *pp = (const InputFileAVIOpts *)buf;

	if (pp->len != sizeof(InputFileAVIOpts))
		return false;

	opts = *pp;

	return true;
}

int InputFileAVIOptions::write(char *buf, int buflen) const {
	if (buf) {
		InputFileAVIOpts *pp = (InputFileAVIOpts *)buf;

		if (buflen<sizeof(InputFileAVIOpts))
			return 0;

		*pp = opts;
		pp->len = sizeof(InputFileAVIOpts);
	}

	return sizeof(InputFileAVIOpts);
}

///////

INT_PTR APIENTRY InputFileAVIOptions::SetupDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
 	InputFileAVIOptions *thisPtr = (InputFileAVIOptions *)GetWindowLongPtr(hDlg, DWLP_USER);

	switch(message) {
		case WM_INITDIALOG:
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			SendDlgItemMessage(hDlg, IDC_FORCE_FOURCC, EM_LIMITTEXT, 4, 0);
			CheckDlgButton(hDlg, IDC_IF_NORMAL, BST_CHECKED);
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDCANCEL:
				if (GetDlgItem(hDlg, IDC_ACCEPTPARTIAL))
					thisPtr->opts.fAcceptPartial = !!IsDlgButtonChecked(hDlg, IDC_ACCEPTPARTIAL);

				thisPtr->opts.fCompatibilityMode = !!IsDlgButtonChecked(hDlg, IDC_AVI_COMPATIBILITYMODE);
				thisPtr->opts.fRedoKeyFlags = !!IsDlgButtonChecked(hDlg, IDC_AVI_REKEY);
				thisPtr->opts.fInternalDecoder = !!IsDlgButtonChecked(hDlg, IDC_AVI_INTERNALDECODER);
				thisPtr->opts.fDisableFastIO = !!IsDlgButtonChecked(hDlg, IDC_AVI_DISABLEOPTIMIZEDIO);

				if (IsDlgButtonChecked(hDlg, IDC_IF_NORMAL))
					thisPtr->opts.iMJPEGMode = VideoSourceAVI::IFMODE_NORMAL;
				else if (IsDlgButtonChecked(hDlg, IDC_IF_SWAP))
					thisPtr->opts.iMJPEGMode = VideoSourceAVI::IFMODE_SWAP;
				else if (IsDlgButtonChecked(hDlg, IDC_IF_SPLITNOSWAP))
					thisPtr->opts.iMJPEGMode = VideoSourceAVI::IFMODE_SPLIT1;
				else if (IsDlgButtonChecked(hDlg, IDC_IF_SPLITSWAP))
					thisPtr->opts.iMJPEGMode = VideoSourceAVI::IFMODE_SPLIT2;
				else if (IsDlgButtonChecked(hDlg, IDC_IF_DISCARDFIRST))
					thisPtr->opts.iMJPEGMode = VideoSourceAVI::IFMODE_DISCARD1;
				else if (IsDlgButtonChecked(hDlg, IDC_IF_DISCARDSECOND))
					thisPtr->opts.iMJPEGMode = VideoSourceAVI::IFMODE_DISCARD2;

				if (IsDlgButtonChecked(hDlg, IDC_FORCE_FOURCC)) {
					union {
						char c[5];
						FOURCC fccType;
					};
					int i;

					i = SendDlgItemMessage(hDlg, IDC_FOURCC, WM_GETTEXT, sizeof c, (LPARAM)c);

					memset(c+i, ' ', 5-i);

					if (fccType == 0x20202020)
						fccType = ' BID';		// force nothing to DIB, since 0 means no force

					thisPtr->opts.fccForceVideo = fccType;
				} else
					thisPtr->opts.fccForceVideo = 0;

				if (IsDlgButtonChecked(hDlg, IDC_FORCE_HANDLER)) {
					union {
						char c[5];
						FOURCC fccType;
					};
					int i;

					i = SendDlgItemMessage(hDlg, IDC_FOURCC2, WM_GETTEXT, sizeof c, (LPARAM)c);

					memset(c+i, ' ', 5-i);

					if (fccType == 0x20202020)
						fccType = ' BID';		// force nothing to DIB, since 0 means no force

					thisPtr->opts.fccForceVideoHandler = fccType;
				} else
					thisPtr->opts.fccForceVideoHandler = 0;

				if (IsDlgButtonChecked(hDlg, IDC_FORCE_SAMPRATE))
					thisPtr->opts.lForceAudioHz = GetDlgItemInt(hDlg, IDC_AUDIORATE, NULL, FALSE);
				else
					thisPtr->opts.lForceAudioHz = 0;
				
				EndDialog(hDlg, 0);
				return TRUE;

			case IDC_FORCE_FOURCC:
				EnableWindow(GetDlgItem(hDlg, IDC_FOURCC), IsDlgButtonChecked(hDlg, IDC_FORCE_FOURCC));
				return TRUE;

			case IDC_FORCE_HANDLER:
				EnableWindow(GetDlgItem(hDlg, IDC_FOURCC2), IsDlgButtonChecked(hDlg, IDC_FORCE_HANDLER));
				return TRUE;

			case IDC_FORCE_SAMPRATE:
				EnableWindow(GetDlgItem(hDlg, IDC_AUDIORATE), IsDlgButtonChecked(hDlg, IDC_FORCE_SAMPRATE));
				return TRUE;
			}
			break;
	}

	return FALSE;
}

void InputFileAVI::setOptions(InputFileOptions *_ifo) {
	InputFileAVIOptions *ifo = (InputFileAVIOptions *)_ifo;

	fCompatibilityMode	= ifo->opts.fCompatibilityMode;
	fAcceptPartial		= ifo->opts.fAcceptPartial;
	fRedoKeyFlags		= ifo->opts.fRedoKeyFlags;
	fInternalDecoder	= ifo->opts.fInternalDecoder;
	fDisableFastIO		= ifo->opts.fDisableFastIO;
	iMJPEGMode			= ifo->opts.iMJPEGMode;
	fccForceVideo		= ifo->opts.fccForceVideo;
	fccForceVideoHandler= ifo->opts.fccForceVideoHandler;
	lForceAudioHz		= ifo->opts.lForceAudioHz;
}

InputFileOptions *InputFileAVI::createOptions(const void *buf, uint32 len) {
	InputFileAVIOptions *ifo = new InputFileAVIOptions();

	if (!ifo) throw MyMemoryError();

	if (!ifo->read((const char *)buf)) {
		delete ifo;
		return NULL;
	}

	return ifo;
}

InputFileOptions *InputFileAVI::promptForOptions(VDGUIHandle hwnd) {
	InputFileAVIOptions *ifo = new InputFileAVIOptions();

	if (!ifo) throw MyMemoryError();

	DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_EXTOPENOPTS_AVI),
			(HWND)hwnd, InputFileAVIOptions::SetupDlgProc, (LPARAM)ifo);

	// if we were forced to AVIFile mode (possibly due to an Avisynth script),
	// propagate that condition to the options
	ifo->opts.fCompatibilityMode |= fCompatibilityMode;

	return ifo;
}

///////////////////////////////////////////////

void InputFileAVI::EnableSegmentAutoscan() {
	fAutoscanSegments = true;
}

void InputFileAVI::ForceCompatibility() {
	fCompatibilityMode = true;
}

void InputFileAVI::setAutomated(bool fAuto) {
	fAutomated = fAuto;
}

void InputFileAVI::Init(const wchar_t *szFile) {
	VDLogAppMessage(kVDLogMarker, kVDST_InputFileAVI, kVDM_OpeningFile, 1, &szFile);

	HRESULT err;
	PAVIFILE paf;

	AddFilename(szFile);

	if (fCompatibilityMode) {

		{
			VDExternalCodeBracket bracket(L"An AVIFile input handler", __FILE__, __LINE__);

			err = AVIFileOpen(&paf, VDTextWToA(szFile).c_str(), OF_READ, NULL);
		}

		if (err)
			throw MyAVIError(szME, err);

		if (!(pAVIFile = CreateAVIReadHandler(paf))) {
			AVIFileRelease(paf);
			throw MyMemoryError();
		}
	} else {
		if (!(pAVIFile = CreateAVIReadHandler(szFile)))
			throw MyMemoryError();
	}

	if (fDisableFastIO)
		pAVIFile->EnableFastIO(false);

	if (fAutoscanSegments) {
		const wchar_t *pszName = VDFileSplitPath(szFile);
		VDStringW sPathBase(szFile, pszName - szFile);
		VDStringW sPathTail(pszName);

		if (sPathTail.size() >= 7 && !_wcsicmp(sPathTail.c_str() + sPathTail.size() - 7, L".00.avi")) {
			int nSegment = 0;

			sPathTail.resize(sPathTail.size() - 7);

			VDStringW sPathPattern(VDMakePath(sPathBase.c_str(), sPathTail.c_str()));

			try {
				const char *pszPathHint;

				while(pAVIFile->getSegmentHint(&pszPathHint)) {

					++nSegment;

					VDStringW sPath(sPathPattern + VDswprintf(L".%02d.avi", 1, &nSegment));

					if (!VDDoesPathExist(sPath.c_str())) {
						if (pszPathHint && *pszPathHint) {
							sPathPattern = VDTextAToW(pszPathHint) + sPathTail;
						}

						sPath = (sPathPattern + VDswprintf(L".%02d.avi", 1, &nSegment));
					}

					if (!VDDoesPathExist(sPath.c_str())) {
						char szPath[MAX_PATH];
						wchar_t szTitle[MAX_PATH];

						swprintf(szTitle, MAX_PATH, L"Cannot find file %s", sPath.c_str());

						strcpy(szPath, VDTextWToA(sPath).c_str());

						const VDStringW fname(VDGetLoadFileName(VDFSPECKEY_LOADVIDEOFILE, (VDGUIHandle)g_hWnd, szTitle, fileFiltersAppend, L"avi", 0, 0));

						if (fname.empty())
							throw MyUserAbortError();

						if (!Append(fname.c_str()))
							break;

						sPathPattern = VDMakePath(VDFileSplitPathLeft(fname).c_str(), sPathTail.c_str());
					} else if (!Append(sPath.c_str()))
						break;
				}
			} catch(const MyError& e) {
				char err[128];
				sprintf(err, "Cannot load video segment %02d", nSegment);

				e.post(NULL, err);
			}
		}
	}

	if (fRedoKeyFlags) {
		vdrefptr<IVDVideoSource> vs;
		vdfastvector<uint32> keyflags;
		for(int index=0; GetVideoSource(index, ~vs); ++index) {
			keyflags.clear();
			static_cast<VideoSourceAVI *>(&*vs)->redoKeyFlags(keyflags);
			mNewKeyFlags.push_back(keyflags);
		}
	} else if (pAVIFile->isIndexFabricated()) {
		VDLogAppMessage(kVDLogWarning, kVDST_InputFileAVI, kVDM_RekeyNotSpecified);
	}
}

bool InputFileAVI::Append(const wchar_t *szFile) {
	if (fCompatibilityMode)
		return false;

	if (!szFile)
		return true;

	if (pAVIFile->AppendFile(szFile)) {
		for(Streams::iterator it(mStreams.begin()), itEnd(mStreams.end()); it!=itEnd; ++it) {
			VDAVIStreamSource *p = *it;

			p->Reinit();
		}

		AddFilename(szFile);

		return true;
	}

	return false;
}

void InputFileAVI::GetTextInfo(tFileTextInfo& info) {
	pAVIFile->GetTextInfo(info);	
}

bool InputFileAVI::isOptimizedForRealtime() {
	return pAVIFile->isOptimizedForRealtime();
}

bool InputFileAVI::isStreaming() {
	return pAVIFile->isStreaming();
}

bool InputFileAVI::GetVideoSource(int index, IVDVideoSource **ppSrc) {
	if (index < 0)
		return false;

	vdrefptr<VideoSourceAVI> videoSrc;
	const uint32 *key_flags = NULL;

	if ((uint32)index < mNewKeyFlags.size())
		key_flags = mNewKeyFlags[index].data();

	if (!(videoSrc = new VideoSourceAVI(this, pAVIFile, NULL, NULL, fInternalDecoder, iMJPEGMode, fccForceVideo, fccForceVideoHandler, key_flags)))
		throw MyMemoryError();

	if (!videoSrc->Init(index))
		return false;

	*ppSrc = videoSrc.release();
	return true;
}

bool InputFileAVI::GetAudioSource(int index, AudioSource **ppSrc) {
	vdrefptr<AudioSource> as;

	as = new AudioSourceAVI(this, pAVIFile, index, fAutomated);
	if (static_cast<AudioSourceAVI *>(&*as)->init()) {
		WAVEFORMATEX *pwfex = (WAVEFORMATEX *)as->getFormat();

		if (pwfex->wFormatTag == WAVE_FORMAT_MPEGLAYER3 && pwfex->nBlockAlign == 0) {
			VDLogAppMessage(kVDLogWarning, kVDST_InputFileAVI, kVDM_InvalidBlockAlign);
			pwfex->nBlockAlign = 1;
		}

		if (lForceAudioHz) {
			pwfex->nAvgBytesPerSec = MulDiv(pwfex->nAvgBytesPerSec, lForceAudioHz, pwfex->nSamplesPerSec);
			pwfex->nSamplesPerSec = lForceAudioHz;
			static_cast<AudioSourceAVI *>(&*as)->setRate(VDFraction(pwfex->nAvgBytesPerSec, pwfex->nBlockAlign));
		}
		*ppSrc = as.release();
		return true;
	}
	as = NULL;

	IAVIReadStream *pDVStream = pAVIFile->GetStream('svai', index);
	if (pDVStream) {
		as = new AudioSourceDV(this, pDVStream, fAutomated);
		if (static_cast<AudioSourceDV *>(&*as)->init()) {
			*ppSrc = as.release();
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////

namespace {
	struct MyFileInfo {
		InputFileAVI *thisPtr;
		vdrefptr<IVDVideoSource> mpVideo;
		vdrefptr<AudioSource> mpAudio;

		volatile HWND hWndAbort;
		UINT statTimer;
		long	lVideoKFrames;
		long	lVideoKMinSize;
		sint64 i64VideoKTotalSize;
		long	lVideoKMaxSize;
		long	lVideoCFrames;
		long	lVideoCMinSize;
		sint64	i64VideoCTotalSize;
		long	lVideoCMaxSize;

		long	lAudioFrames;
		long	lAudioMinSize;
		sint64	i64AudioTotalSize;
		long	lAudioMaxSize;

		long	lAudioPreload;

		bool	bAudioFramesIndeterminate;
	};
}

void InputFileAVI::_InfoDlgThread(void *pvInfo) {
	MyFileInfo *pInfo = (MyFileInfo *)pvInfo;
	VDPosition i;
	uint32 lActualBytes, lActualSamples;
	VideoSourceAVI *inputVideoAVI = static_cast<VideoSourceAVI *>(&*pInfo->mpVideo);
	AudioSource *inputAudioAVI = pInfo->mpAudio;

	pInfo->lVideoCMinSize = 0x7FFFFFFF;
	pInfo->lVideoKMinSize = 0x7FFFFFFF;

	const VDPosition videoFrameStart	= inputVideoAVI->getStart();
	const VDPosition videoFrameEnd		= inputVideoAVI->getEnd();

	for(i=videoFrameStart; i<videoFrameEnd; ++i) {
		if (inputVideoAVI->isKey(i)) {
			++pInfo->lVideoKFrames;

			if (!inputVideoAVI->read(i, 1, NULL, 0, &lActualBytes, NULL)) {
				pInfo->i64VideoKTotalSize += lActualBytes;
				if (lActualBytes < pInfo->lVideoKMinSize) pInfo->lVideoKMinSize = lActualBytes;
				if (lActualBytes > pInfo->lVideoKMaxSize) pInfo->lVideoKMaxSize = lActualBytes;
			}
		} else {
			++pInfo->lVideoCFrames;

			if (!inputVideoAVI->read(i, 1, NULL, 0, &lActualBytes, NULL)) {
				pInfo->i64VideoCTotalSize += lActualBytes;
				if (lActualBytes < pInfo->lVideoCMinSize) pInfo->lVideoCMinSize = lActualBytes;
				if (lActualBytes > pInfo->lVideoCMaxSize) pInfo->lVideoCMaxSize = lActualBytes;
			}
		}

		if (pInfo->hWndAbort) {
			SendMessage(pInfo->hWndAbort, WM_USER+256, 0, 0);
			return;
		}
	}

	if (inputAudioAVI) {
		const VDPosition audioFrameStart	= inputAudioAVI->getStart();
		const VDPosition audioFrameEnd		= inputAudioAVI->getEnd();

		pInfo->lAudioMinSize = 0x7FFFFFFF;
		pInfo->bAudioFramesIndeterminate = false;
		pInfo->lAudioPreload = static_cast<VDAudioSourceAVISourced *>(inputAudioAVI)->GetPreloadSamples();

		i = audioFrameStart;
		while(i < audioFrameEnd) {
			if (inputAudioAVI->read(i, AVISTREAMREAD_CONVENIENT, NULL, 0, &lActualBytes, &lActualSamples))
				break;

			if (!lActualSamples) {
				pInfo->bAudioFramesIndeterminate = true;
				break;
			}

			++pInfo->lAudioFrames;
			i += lActualSamples;

			pInfo->i64AudioTotalSize += lActualBytes;
			if (lActualBytes < pInfo->lAudioMinSize) pInfo->lAudioMinSize = lActualBytes;
			if (lActualBytes > pInfo->lAudioMaxSize) pInfo->lAudioMaxSize = lActualBytes;

			if (pInfo->hWndAbort) {
				SendMessage(pInfo->hWndAbort, WM_USER+256, 0, 0);
				return;
			}
		}
	}

	pInfo->hWndAbort = (HWND)1;
}

INT_PTR APIENTRY InputFileAVI::_InfoDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	MyFileInfo *pInfo = (MyFileInfo *)GetWindowLongPtr(hDlg, DWLP_USER);
	InputFileAVI *thisPtr;

	if (pInfo)
		thisPtr = pInfo->thisPtr;

    switch (message)
    {
        case WM_INITDIALOG:
			{
				char buf[128];

				SetWindowLongPtr(hDlg, DWLP_USER, lParam);
				pInfo = (MyFileInfo *)lParam;
				thisPtr = pInfo->thisPtr;

				if (pInfo->mpVideo) {
					char *s;
					VideoSourceAVI *pvs = static_cast<VideoSourceAVI *>(&*pInfo->mpVideo);

					sprintf(buf, "%dx%d, %.3f fps (%ld µs)",
								pvs->getImageFormat()->biWidth,
								pvs->getImageFormat()->biHeight,
								pvs->getRate().asDouble(),
								VDRoundToLong(1000000.0 / pvs->getRate().asDouble()));
					SetDlgItemText(hDlg, IDC_VIDEO_FORMAT, buf);

					const sint64 length = pvs->getLength();
					s = buf + sprintf(buf, "%I64d frames (", length);
					DWORD ticks = VDRoundToInt(1000.0*length/pvs->getRate().asDouble());
					ticks_to_str(s, (buf + sizeof(buf)/sizeof(buf[0])) - s, ticks);
					sprintf(s+strlen(s),".%02d)", (ticks/10)%100);
					SetDlgItemText(hDlg, IDC_VIDEO_NUMFRAMES, buf);

					strcpy(buf, "Unknown");

					if (const wchar_t *name = pvs->getDecompressorName()) {
						VDTextWToA(buf, sizeof(buf)-7, name, -1);
						
						char fcc[5];
						*(long *)fcc = pvs->getImageFormat()->biCompression;
						fcc[4] = 0;
						for(int i=0; i<4; ++i)
							if ((uint8)(fcc[i] - 0x20) >= 0x7f)
								fcc[i] = ' ';

						sprintf(buf+strlen(buf), " (%s)", fcc);
					} else {
						const uint32 comp = pvs->getImageFormat()->biCompression;

						if (comp == '2YUY')
							strcpy(buf, "YCbCr 4:2:2 (YUY2)");
						else if (comp == 'YVYU')
							strcpy(buf, "YCbCr 4:2:2 (UYVY)");
						else if (comp == '024I')
							strcpy(buf, "YCbCr 4:2:0 planar (I420)");
						else if (comp == 'VUYI')
							strcpy(buf, "YCbCr 4:2:0 planar (IYUV)");
						else if (comp == '21VY')
							strcpy(buf, "YCbCr 4:2:0 planar (YV12)");
						else if (comp == '61VY')
							strcpy(buf, "YCbCr 4:2:2 planar (YV16)");
						else if (comp == '9UVY')
							strcpy(buf, "YCbCr 4:1:0 planar (YVU9)");
						else if (comp == '  8Y')
							strcpy(buf, "Monochrome (Y8)");
						else if (comp == '008Y')
							strcpy(buf, "Monochrome (Y800)");
						else
							sprintf(buf, "Uncompressed RGB%d", pvs->getImageFormat()->biBitCount);
					}

					SetDlgItemText(hDlg, IDC_VIDEO_COMPRESSION, buf);
				}
				if (pInfo->mpAudio) {
					AudioSourceAVI *pAS = static_cast<AudioSourceAVI *>(&*pInfo->mpAudio);
					const VDWaveFormat *fmt = pAS->getWaveFormat();
					DWORD cbwfxTemp;
					WAVEFORMATEX *pwfxTemp;
					HACMSTREAM has;
					HACMDRIVERID hadid;
					ACMDRIVERDETAILS add;
					bool fSuccessful = false;

					sprintf(buf, "%ldHz", fmt->mSamplingRate);
					SetDlgItemText(hDlg, IDC_AUDIO_SAMPLINGRATE, buf);

					if (fmt->mChannels == 8)
						strcpy(buf, "7.1");
					else if (fmt->mChannels == 6)
						strcpy(buf, "5.1");
					else if (fmt->mChannels > 2)
						sprintf(buf, "%d", fmt->mChannels);
					else
						sprintf(buf, "%d (%s)", fmt->mChannels, fmt->mChannels>1 ? "Stereo" : "Mono");
					SetDlgItemText(hDlg, IDC_AUDIO_CHANNELS, buf);

					if (fmt->mTag == WAVE_FORMAT_PCM) {
						sprintf(buf, "%d-bit", fmt->mSampleBits);
						SetDlgItemText(hDlg, IDC_AUDIO_PRECISION, buf);
					} else
						SetDlgItemText(hDlg, IDC_AUDIO_PRECISION, "N/A");

					sint64 len = pAS->getLength();

					char *s = buf + sprintf(buf, "%I64d samples (", len);
					uint32 ticks = VDRoundToInt32(1000.0 * len * pAS->getRate().AsInverseDouble());
					ticks_to_str(s, (buf + sizeof(buf)/sizeof(buf[0])) - s, ticks);
					sprintf(s+strlen(s),".%02d)", (ticks/10)%100);
					SetDlgItemText(hDlg, IDC_AUDIO_LENGTH, buf);

					////////// Attempt to detect audio compression //////////

					if (fmt->mTag == nsVDWinFormats::kWAVE_FORMAT_EXTENSIBLE) {
						const nsVDWinFormats::WaveFormatExtensible& wfe = *(const nsVDWinFormats::WaveFormatExtensible *)fmt;

						if (wfe.mGuid == nsVDWinFormats::kKSDATAFORMAT_SUBTYPE_PCM) {
							sprintf(buf, "PCM (%d bits real, chmask %x)", wfe.mBitDepth, wfe.mChannelMask);
							SetDlgItemText(hDlg, IDC_AUDIO_COMPRESSION, buf);
						} else {
							SetDlgItemText(hDlg, IDC_AUDIO_COMPRESSION, "Unknown extended format");
						}
					} else if (fmt->mTag != WAVE_FORMAT_PCM) {
						// Retrieve maximum format size.

						acmMetrics(NULL, ACM_METRIC_MAX_SIZE_FORMAT, (LPVOID)&cbwfxTemp);

						// Fill out a destination wave format (PCM).

						if (pwfxTemp = (WAVEFORMATEX *)allocmem(cbwfxTemp)) {
							pwfxTemp->wFormatTag	= WAVE_FORMAT_PCM;

							// Ask ACM to fill out the details.

							if (!acmFormatSuggest(NULL, (WAVEFORMATEX *)fmt, (WAVEFORMATEX *)pwfxTemp, cbwfxTemp, ACM_FORMATSUGGESTF_WFORMATTAG)) {
								if (!acmStreamOpen(&has, NULL, (WAVEFORMATEX *)fmt, pwfxTemp, NULL, NULL, NULL, ACM_STREAMOPENF_NONREALTIME)) {
									if (!acmDriverID((HACMOBJ)has, &hadid, 0)) {
										memset(&add, 0, sizeof add);

										add.cbStruct = sizeof add;

										if (!acmDriverDetails(hadid, &add, 0)) {
											SetDlgItemText(hDlg, IDC_AUDIO_COMPRESSION, add.szLongName);

											fSuccessful = true;
										}
									}

									acmStreamClose(has, 0);
								}
							}

							freemem(pwfxTemp);
						}

						if (!fSuccessful) {
							char buf[32];

							wsprintf(buf, "Unknown (tag %04X)", fmt->mTag);
							SetDlgItemText(hDlg, IDC_AUDIO_COMPRESSION, buf);
						}
					} else {
						// It's a PCM format...

						SetDlgItemText(hDlg, IDC_AUDIO_COMPRESSION, "PCM (Uncompressed)");
					}
				}
			}

			_beginthread(_InfoDlgThread, 10000, pInfo);

			pInfo->statTimer = SetTimer(hDlg, 1, 250, NULL);

            return (TRUE);

        case WM_COMMAND:                      
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
            {
				if (pInfo->hWndAbort == (HWND)1)
					EndDialog(hDlg, TRUE);
				else
					pInfo->hWndAbort = hDlg;
                return TRUE;
            }
            break;

		case WM_DESTROY:
			if (pInfo->statTimer) KillTimer(hDlg, pInfo->statTimer);
			break;

		case WM_TIMER:
			{
				char buf[128];

				sprintf(buf, "%ld", pInfo->lVideoKFrames);
				SetDlgItemText(hDlg, IDC_VIDEO_NUMKEYFRAMES, buf);

				if (pInfo->lVideoKFrames)
					sprintf(buf, "%ld/%I64d/%ld (%I64dK)"
								,pInfo->lVideoKMinSize
								,pInfo->i64VideoKTotalSize/pInfo->lVideoKFrames
								,pInfo->lVideoKMaxSize
								,(pInfo->i64VideoKTotalSize+1023)>>10);
				else
					strcpy(buf,"(no key frames)");
				SetDlgItemText(hDlg, IDC_VIDEO_KEYFRAMESIZES, buf);

				if (pInfo->lVideoCFrames)
					sprintf(buf, "%ld/%I64d/%ld (%I64dK)"
								,pInfo->lVideoCMinSize
								,pInfo->i64VideoCTotalSize/pInfo->lVideoCFrames
								,pInfo->lVideoCMaxSize
								,(pInfo->i64VideoCTotalSize+1023)>>10);
				else
					strcpy(buf,"(no delta frames)");
				SetDlgItemText(hDlg, IDC_VIDEO_NONKEYFRAMESIZES, buf);

				if (pInfo->mpAudio) {
					if (pInfo->bAudioFramesIndeterminate) {
						SetDlgItemText(hDlg, IDC_AUDIO_NUMFRAMES, "(indeterminate)");
						SetDlgItemText(hDlg, IDC_AUDIO_FRAMESIZES, "(indeterminate)");
						SetDlgItemText(hDlg, IDC_AUDIO_PRELOAD, "(indeterminate)");
					} else {

						if (pInfo->lAudioFrames)
							sprintf(buf, "%ld/%I64d/%ld (%I64dK)"
									,pInfo->lAudioMinSize
									,pInfo->i64AudioTotalSize/pInfo->lAudioFrames
									,pInfo->lAudioMaxSize
									,(pInfo->i64AudioTotalSize+1023)>>10);
						else
							strcpy(buf,"(no audio frames)");
						SetDlgItemText(hDlg, IDC_AUDIO_FRAMESIZES, buf);

						const VDWaveFormat *pWaveFormat = pInfo->mpAudio->getWaveFormat();

						sprintf(buf, "%ld chunks (%.2fs preload)", pInfo->lAudioFrames,
							(double)pInfo->lAudioPreload * pInfo->mpAudio->getRate().AsInverseDouble()
								);
						SetDlgItemText(hDlg, IDC_AUDIO_LAYOUT, buf);

						const double audioRate = (double)pWaveFormat->mDataRate * (1.0 / 125.0);
						const double rawOverhead = 24.0 * pInfo->lAudioFrames;
						const double audioOverhead = 100.0 * rawOverhead / (rawOverhead + pInfo->i64AudioTotalSize);
						sprintf(buf, "%.0f kbps (%.2f%% overhead)", audioRate, audioOverhead);
						SetDlgItemText(hDlg, IDC_AUDIO_DATARATE, buf);
					}
				}

				double totalVideoFrames = (double)pInfo->lVideoKFrames + (sint64)pInfo->lVideoCFrames;
				if (totalVideoFrames > 0) {
					VideoSourceAVI *pvs = static_cast<VideoSourceAVI *>(&*pInfo->mpVideo);
					const double seconds = (double)pvs->getLength() / (double)pvs->getRate().asDouble();
					const double rawOverhead = (24.0 * totalVideoFrames);
					const double totalSize = (double)(pInfo->i64VideoKTotalSize + pInfo->i64VideoCTotalSize);
					const double videoRate = (1.0 / 125.0) * totalSize / seconds;
					const double videoOverhead = totalSize > 0 ? 100.0 * rawOverhead / (rawOverhead + totalSize) : 0;
					sprintf(buf, "%.0f kbps (%.2f%% overhead)", videoRate, videoOverhead);
					SetDlgItemText(hDlg, IDC_VIDEO_DATARATE, buf);
				}
			}

			/////////

			if (pInfo->hWndAbort) {
				KillTimer(hDlg, pInfo->statTimer);
				return TRUE;
			}

			break;

		case WM_USER+256:
			EndDialog(hDlg, TRUE);  
			break;
    }
    return FALSE;
}

void InputFileAVI::InfoDialog(VDGUIHandle hwndParent) {
	MyFileInfo mai;

	memset(&mai, 0, sizeof mai);
	mai.thisPtr = this;

	GetVideoSource(0, ~mai.mpVideo);
	GetAudioSource(0, ~mai.mpAudio);

	DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_AVI_INFO), (HWND)hwndParent, _InfoDlgProc, (LPARAM)&mai);
}

void InputFileAVI::Attach(VDAVIStreamSource *p) {
	mStreams.push_back(p);
}

void InputFileAVI::Detach(VDAVIStreamSource *p) {
	mStreams.erase(p);
}
