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
#include <vector>
#include <algorithm>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/VDString.h>
#include <vd2/system/w32assist.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>

#include "VideoDisplay.h"
#include "VideoDisplayDrivers.h"

#include "prefs.h"			// for display preferences

extern HINSTANCE g_hInst;

extern const char g_szVideoDisplayControlName[] = "phaeronVideoDisplay";

extern void VDMemcpyRect(void *dst, ptrdiff_t dststride, const void *src, ptrdiff_t srcstride, size_t w, size_t h);

extern IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverD3DFX();

///////////////////////////////////////////////////////////////////////////

namespace {
	bool VDIsForegroundTask() {
		HWND hwndFore = GetForegroundWindow();

		if (!hwndFore)
			return false;

		DWORD dwProcessId = 0;
		GetWindowThreadProcessId(hwndFore, &dwProcessId);

		return dwProcessId == GetCurrentProcessId();
	}

	bool VDIsTerminalServicesClient() {
		if ((sint32)(GetVersion() & 0x000000FF) >= 0x00000005) {
			return GetSystemMetrics(SM_REMOTESESSION) != 0;		// Requires Windows NT SP4 or later.
		}

		return false;	// Ignore Windows 95/98/98SE/ME/NT3/NT4.  (Broken on NT4 Terminal Server, but oh well.)
	}
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayWindow : public IVDVideoDisplay {
public:
	static ATOM Register();

protected:
	VDVideoDisplayWindow(HWND hwnd);
	~VDVideoDisplayWindow();

	void SetSourceMessage(const wchar_t *msg);
	void SetSourcePalette(const uint32 *palette, int count);
	bool SetSource(bool bAutoUpdate, const VDPixmap& src, void *pSharedObject, ptrdiff_t sharedOffset, bool bAllowConversion, bool bInterlaced);
	bool SetSourcePersistent(bool bAutoUpdate, const VDPixmap& src, bool bAllowConversion, bool bInterlaced);
	void SetSourceSubrect(const vdrect32 *r);
	void Update(int);
	void PostUpdate(int);
	void Reset();
	void Cache();
	void SetCallback(IVDVideoDisplayCallback *pcb);
	void LockAcceleration(bool locked);
	FilterMode GetFilterMode();
	void SetFilterMode(FilterMode mode);

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void OnPaint();
	void SyncSetSourceMessage(const wchar_t *);
	bool SyncSetSource(bool bAutoUpdate, const VDVideoDisplaySourceInfo& params);
	void SyncReset();
	bool SyncInit(bool bAutoRefresh);
	void SyncUpdate(int);
	void SyncCache();
	void SyncDisplayChange();
	void SyncForegroundChange(bool bForeground);
	void SyncRealizePalette();
	void VerifyDriverResult(bool result);
	static void StaticRemapPalette();
	static bool StaticCheckPaletted();
	static void StaticCreatePalette();

	static LRESULT CALLBACK StaticLookoutWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	enum {
		kReinitDisplayTimerId = 500
	};

	HWND		mhwnd;
	HPALETTE	mhOldPalette;

	VDVideoDisplaySourceInfo	mSource;

	IVDVideoDisplayMinidriver *mpMiniDriver;
	UINT	mReinitDisplayTimer;

	IVDVideoDisplayCallback		*mpCB;
	int		mInhibitRefresh;

	FilterMode	mFilterMode;
	bool	mbLockAcceleration;

	bool		mbUseSubrect;
	vdrect32	mSourceSubrect;
	VDStringW	mMessage;

	VDPixmapBuffer		mCachedImage;

	uint32	mSourcePalette[256];

	typedef std::vector<VDVideoDisplayWindow *> tDisplayWindows;
	static tDisplayWindows	sDisplayWindows;
	static HWND				shwndLookout;
	static HPALETTE			shPalette;
	static uint8			sLogicalPalette[256];
};

VDVideoDisplayWindow::tDisplayWindows	VDVideoDisplayWindow::sDisplayWindows;
HWND									VDVideoDisplayWindow::shwndLookout;
HPALETTE								VDVideoDisplayWindow::shPalette;
uint8									VDVideoDisplayWindow::sLogicalPalette[256];

///////////////////////////////////////////////////////////////////////////

ATOM VDVideoDisplayWindow::Register() {
	WNDCLASS wc;

	wc.style			= 0;
	wc.lpfnWndProc		= StaticLookoutWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= g_hInst;
	wc.hIcon			= 0;
	wc.hCursor			= 0;
	wc.hbrBackground	= 0;
	wc.lpszMenuName		= 0;
	wc.lpszClassName	= "phaeronVideoDisplayLookout";

	if (!RegisterClass(&wc))
		return NULL;

	wc.style			= CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= StaticWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= sizeof(VDVideoDisplayWindow *);
	wc.hInstance		= g_hInst;
	wc.hIcon			= 0;
	wc.hCursor			= 0;
	wc.hbrBackground	= 0;
	wc.lpszMenuName		= 0;
	wc.lpszClassName	= g_szVideoDisplayControlName;

	return RegisterClass(&wc);
}

IVDVideoDisplay *VDGetIVideoDisplay(HWND hwnd) {
	return static_cast<IVDVideoDisplay *>(reinterpret_cast<VDVideoDisplayWindow*>(GetWindowLongPtr(hwnd, 0)));
}

bool VDRegisterVideoDisplayControl() {
	return 0 != VDVideoDisplayWindow::Register();
}

///////////////////////////////////////////////////////////////////////////

VDVideoDisplayWindow::VDVideoDisplayWindow(HWND hwnd)
	: mhwnd(hwnd)
	, mhOldPalette(0)
	, mpMiniDriver(0)
	, mReinitDisplayTimer(0)
	, mpCB(0)
	, mInhibitRefresh(0)
	, mFilterMode(kFilterAnySuitable)
	, mbLockAcceleration(false)
	, mbUseSubrect(false)
{
	mSource.pixmap.data = 0;

	sDisplayWindows.push_back(this);
	if (!shwndLookout)
		shwndLookout = CreateWindow("phaeronVideoDisplayLookout", "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, g_hInst, 0);
}

VDVideoDisplayWindow::~VDVideoDisplayWindow() {
	tDisplayWindows::iterator it(std::find(sDisplayWindows.begin(), sDisplayWindows.end(), this));

	if (it != sDisplayWindows.end()) {
		*it = sDisplayWindows.back();
		sDisplayWindows.pop_back();

		if (sDisplayWindows.empty()) {
			if (shPalette) {
				DeleteObject(shPalette);
				shPalette = 0;
			}

			VDASSERT(shwndLookout);
			DestroyWindow(shwndLookout);
			shwndLookout = NULL;
		}
	} else {
		VDASSERT(false);
	}
}

///////////////////////////////////////////////////////////////////////////

#define MYWM_SETSOURCE		(WM_USER + 0x100)
#define MYWM_UPDATE			(WM_USER + 0x101)
#define MYWM_CACHE			(WM_USER + 0x102)
#define MYWM_RESET			(WM_USER + 0x103)
#define MYWM_SETSOURCEMSG	(WM_USER + 0x104)

void VDVideoDisplayWindow::SetSourceMessage(const wchar_t *msg) {
	SendMessage(mhwnd, MYWM_SETSOURCEMSG, 0, (LPARAM)msg);
}

void VDVideoDisplayWindow::SetSourcePalette(const uint32 *palette, int count) {
	memcpy(mSourcePalette, palette, 4*std::min<int>(count, 256));
}

bool VDVideoDisplayWindow::SetSource(bool bAutoUpdate, const VDPixmap& src, void *pObject, ptrdiff_t offset, bool bAllowConversion, bool bInterlaced) {
	// We do allow data to be NULL for set-without-load.
	if (src.data)
		VDAssertValidPixmap(src);

	VDVideoDisplaySourceInfo params;

	params.pixmap			= src;
	params.pSharedObject	= pObject;
	params.sharedOffset		= offset;
	params.bAllowConversion	= bAllowConversion;
	params.bPersistent		= pObject != 0;
	params.bInterlaced		= bInterlaced;

	const VDPixmapFormatInfo& info = VDPixmapGetInfo(src.format);
	params.bpp = info.qsize >> info.qhbits;
	params.bpr = (((src.w-1) >> info.qwbits)+1) * info.qsize;

	return 0 != SendMessage(mhwnd, MYWM_SETSOURCE, bAutoUpdate, (LPARAM)&params);
}

bool VDVideoDisplayWindow::SetSourcePersistent(bool bAutoUpdate, const VDPixmap& src, bool bAllowConversion, bool bInterlaced) {
	// We do allow data to be NULL for set-without-load.
	if (src.data)
		VDAssertValidPixmap(src);

	VDVideoDisplaySourceInfo params;

	params.pixmap			= src;
	params.pSharedObject	= NULL;
	params.sharedOffset		= 0;
	params.bAllowConversion	= bAllowConversion;
	params.bPersistent		= true;
	params.bInterlaced		= bInterlaced;

	const VDPixmapFormatInfo& info = VDPixmapGetInfo(src.format);
	params.bpp = info.qsize >> info.qhbits;
	params.bpr = (((src.w-1) >> info.qwbits)+1) * info.qsize;

	return 0 != SendMessage(mhwnd, MYWM_SETSOURCE, bAutoUpdate, (LPARAM)&params);
}

void VDVideoDisplayWindow::SetSourceSubrect(const vdrect32 *r) {
	if (r) {
		mbUseSubrect = true;
		mSourceSubrect = *r;
	} else
		mbUseSubrect = false;

	if (mpMiniDriver) {
		if (!mpMiniDriver->SetSubrect(r))
			SyncReset();
	}
}

void VDVideoDisplayWindow::Update(int fieldmode) {
	SendMessage(mhwnd, MYWM_UPDATE, fieldmode, 0);
}

void VDVideoDisplayWindow::PostUpdate(int fieldmode) {
	PostMessage(mhwnd, MYWM_UPDATE, fieldmode, 0);
}

void VDVideoDisplayWindow::Cache() {
	SendMessage(mhwnd, MYWM_CACHE, 0, 0);
}

void VDVideoDisplayWindow::Reset() {
	SendMessage(mhwnd, MYWM_RESET, 0, 0);
}

void VDVideoDisplayWindow::SetCallback(IVDVideoDisplayCallback *pCB) {
	mpCB = pCB;
}

void VDVideoDisplayWindow::LockAcceleration(bool locked) {
	mbLockAcceleration = locked;
}

IVDVideoDisplay::FilterMode VDVideoDisplayWindow::GetFilterMode() {
	return mFilterMode;
}

void VDVideoDisplayWindow::SetFilterMode(FilterMode mode) {
	if (mFilterMode != mode) {
		mFilterMode = mode;

		if (mpMiniDriver) {
			mpMiniDriver->SetFilterMode((IVDVideoDisplayMinidriver::FilterMode)mode);
			InvalidateRect(mhwnd, NULL, FALSE);
		}
	}
}

///////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK VDVideoDisplayWindow::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDVideoDisplayWindow *pThis = (VDVideoDisplayWindow *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		pThis = new VDVideoDisplayWindow(hwnd);
		SetWindowLongPtr(hwnd, 0, (DWORD_PTR)pThis);
		break;
	case WM_NCDESTROY:
		if (pThis)
			pThis->SyncReset();
		delete pThis;
		pThis = NULL;
		SetWindowLongPtr(hwnd, 0, 0);
		break;
	}

	return pThis ? pThis->WndProc(msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT VDVideoDisplayWindow::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_DESTROY:
		if (mReinitDisplayTimer) {
			KillTimer(mhwnd, mReinitDisplayTimer);
			mReinitDisplayTimer = 0;
		}

		if (mhOldPalette) {
			DeleteObject(mhOldPalette);
			mhOldPalette = 0;
		}

		break;
	case WM_PAINT:
		OnPaint();
		return 0;
	case MYWM_SETSOURCE:
		return SyncSetSource(wParam != 0, *(const VDVideoDisplaySourceInfo *)lParam);
	case MYWM_UPDATE:
		SyncUpdate((FieldMode)wParam);
		return 0;
	case MYWM_RESET:
		SyncReset();
		mSource.pixmap.data = NULL;
		return 0;
	case MYWM_SETSOURCEMSG:
		SyncSetSourceMessage((const wchar_t *)lParam);
		return 0;
	case WM_SIZE:
		if (mpMiniDriver)
			VerifyDriverResult(mpMiniDriver->Resize());
		break;
	case WM_TIMER:
		if (wParam == mReinitDisplayTimer) {
			SyncInit(true);
			return 0;
		} else {
			if (mpMiniDriver)
				VerifyDriverResult(mpMiniDriver->Tick((int)wParam));
		}
		break;
	case WM_NCHITTEST:
		return HTTRANSPARENT;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDVideoDisplayWindow::OnPaint() {

	++mInhibitRefresh;

	bool bDisplayOK = false;

	if (mpMiniDriver) {
		if (mpMiniDriver->IsValid())
			bDisplayOK = true;
		else if (mSource.pixmap.data && mSource.bPersistent && !mpMiniDriver->Update(IVDVideoDisplayMinidriver::kModeAllFields))
			bDisplayOK = true;
	}

	if (!bDisplayOK) {
		--mInhibitRefresh;
		if (mpCB)
			mpCB->DisplayRequestUpdate(this);
		++mInhibitRefresh;
	}

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhwnd, &ps);

	if (hdc) {
		RECT r;

		GetClientRect(mhwnd, &r);

		if (mpMiniDriver && mpMiniDriver->IsValid())
			VerifyDriverResult(mpMiniDriver->Paint(hdc, r, IVDVideoDisplayMinidriver::kModeAllFields));
		else {
			FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE + 1));
			if (!mMessage.empty()) {
				HGDIOBJ hgo = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
				SetBkMode(hdc, TRANSPARENT);
				VDDrawTextW32(hdc, mMessage.data(), mMessage.size(), &r, DT_CENTER | DT_VCENTER | DT_NOPREFIX | DT_WORDBREAK);
				SelectObject(hdc, hgo);
			}
		}

		EndPaint(mhwnd, &ps);
	}

	--mInhibitRefresh;
}

bool VDVideoDisplayWindow::SyncSetSource(bool bAutoUpdate, const VDVideoDisplaySourceInfo& params) {
	mCachedImage.clear();

	mSource = params;
	mMessage.clear();

	if (mpMiniDriver && mpMiniDriver->ModifySource(mSource)) {
		mSource.bAllowConversion = true;

		if (bAutoUpdate)
			SyncUpdate(kAllFields);
		return true;
	}

	SyncReset();
	if (!SyncInit(bAutoUpdate))
		return false;

	mSource.bAllowConversion = true;
	return true;
}

void VDVideoDisplayWindow::SyncReset() {
	if (mpMiniDriver) {
		mpMiniDriver->Shutdown();
		delete mpMiniDriver;
		mpMiniDriver = 0;
	}
}

void VDVideoDisplayWindow::SyncSetSourceMessage(const wchar_t *msg) {
	if (!mpMiniDriver && mMessage == msg)
		return;

	SyncReset();
	mSource.pixmap.format = 0;
	mMessage = msg;
	InvalidateRect(mhwnd, NULL, TRUE);
}

bool VDVideoDisplayWindow::SyncInit(bool bAutoRefresh) {
	if (!mSource.pixmap.data || !mSource.pixmap.format)
		return true;

	VDASSERT(!mpMiniDriver);

	bool bIsForeground = VDIsForegroundTask();

	do {
		if ((g_prefs.fDisplay & Preferences::kDisplayUseDXWithTS) || !VDIsTerminalServicesClient()) {
			if (mbLockAcceleration || !mSource.bAllowConversion || bIsForeground) {
				// The 3D drivers don't currently support subrects.
				if (!(g_prefs.fDisplay & Preferences::kDisplayDisableDX)) {
					if (!mbUseSubrect) {
						if (g_prefs.fDisplay & Preferences::kDisplayEnableOpenGL) {
							mpMiniDriver = VDCreateVideoDisplayMinidriverOpenGL();
							if (mpMiniDriver->Init(mhwnd, mSource))
								break;
							SyncReset();
						}

						if (g_prefs.fDisplay & Preferences::kDisplayEnableD3D) {
							if (g_prefs.fDisplay & Preferences::kDisplayEnableD3DFX)
								mpMiniDriver = VDCreateVideoDisplayMinidriverD3DFX();
							else
								mpMiniDriver = VDCreateVideoDisplayMinidriverDX9();
							if (mpMiniDriver->Init(mhwnd, mSource))
								break;
							SyncReset();
						}
					}

					mpMiniDriver = VDCreateVideoDisplayMinidriverDirectDraw();
					if (mpMiniDriver->Init(mhwnd, mSource))
						break;
					SyncReset();
				}

			} else {
				VDDEBUG("VideoDisplay: Application in background -- disabling accelerated preview.\n");
			}
		}

		mpMiniDriver = VDCreateVideoDisplayMinidriverGDI();
		if (mpMiniDriver->Init(mhwnd, mSource))
			break;

		VDDEBUG("VideoDisplay: No driver was able to handle the requested format! (%d)\n", mSource.pixmap.format);
		SyncReset();
	} while(false);

	if (mpMiniDriver) {
		mpMiniDriver->SetLogicalPalette(sLogicalPalette);
		mpMiniDriver->SetFilterMode((IVDVideoDisplayMinidriver::FilterMode)mFilterMode);
		mpMiniDriver->SetSubrect(mbUseSubrect ? &mSourceSubrect : NULL);

		if (mReinitDisplayTimer)
			KillTimer(mhwnd, mReinitDisplayTimer);

		if (StaticCheckPaletted()) {
			if (!shPalette)
				StaticCreatePalette();

			SyncRealizePalette();
		}

		if (bAutoRefresh) {
			if (mSource.bPersistent)
				SyncUpdate(kAllFields);
			else if (mpCB)
				mpCB->DisplayRequestUpdate(this);
		}
	}

	return mpMiniDriver != 0;
}

void VDVideoDisplayWindow::SyncUpdate(int mode) {
	if (mSource.pixmap.data && !mpMiniDriver) {
		SyncInit(true);
		return;
	}

	if (mpMiniDriver) {
		if (mode & kVisibleOnly) {
			bool bVisible = true;

			if (HDC hdc = GetDC(mhwnd)) {
				RECT r;
				GetClientRect(mhwnd, &r);
				bVisible = 0 != RectVisible(hdc, &r);
				ReleaseDC(mhwnd, hdc);
			}

			mode = (FieldMode)(mode & ~kVisibleOnly);

			if (!bVisible)
				return;
		}

		if (mpMiniDriver->Update((IVDVideoDisplayMinidriver::UpdateMode)mode)) {
			if (!mInhibitRefresh)
				mpMiniDriver->Refresh((IVDVideoDisplayMinidriver::UpdateMode)mode);
		}
	}
}

void VDVideoDisplayWindow::SyncCache() {
	if (mSource.pixmap.data && mSource.pixmap.data != mCachedImage.data) {
		mCachedImage.assign(mSource.pixmap);

		mSource.pixmap		= mCachedImage;
		mSource.bPersistent	= true;
	}

	if (mSource.pixmap.data && !mpMiniDriver)
		SyncInit(true);
}

void VDVideoDisplayWindow::SyncDisplayChange() {
	if (mhOldPalette && !shPalette) {
		if (HDC hdc = GetDC(mhwnd)) {
			SelectPalette(hdc, mhOldPalette, FALSE);
			mhOldPalette = 0;
			ReleaseDC(mhwnd, hdc);
		}
	}
	if (!mhOldPalette && shPalette) {
		if (HDC hdc = GetDC(mhwnd)) {
			mhOldPalette = SelectPalette(hdc, shPalette, FALSE);
			ReleaseDC(mhwnd, hdc);
		}
	}
	if (!mReinitDisplayTimer) {
		SyncReset();
		if (!SyncInit(true))
			mReinitDisplayTimer = SetTimer(mhwnd, kReinitDisplayTimerId, 500, NULL);
	}
}

void VDVideoDisplayWindow::SyncForegroundChange(bool bForeground) {
	if (!mbLockAcceleration)
		SyncReset();

	SyncRealizePalette();
}

void VDVideoDisplayWindow::SyncRealizePalette() {
	if (HDC hdc = GetDC(mhwnd)) {
		HPALETTE pal = SelectPalette(hdc, shPalette, FALSE);
		if (!mhOldPalette)
			mhOldPalette = pal;
		RealizePalette(hdc);
		StaticRemapPalette();

		if (mpMiniDriver) {
			mpMiniDriver->SetLogicalPalette(sLogicalPalette);
			if (mSource.bPersistent)
				SyncUpdate(kAllFields);
			else if (mpCB)
				mpCB->DisplayRequestUpdate(this);
		}

		ReleaseDC(mhwnd, hdc);
	}
}

void VDVideoDisplayWindow::VerifyDriverResult(bool result) {
	if (!result) {
		if (mpMiniDriver) {
			mpMiniDriver->Shutdown();
			delete mpMiniDriver;
			mpMiniDriver = 0;
		}

		if (!mReinitDisplayTimer)
			mReinitDisplayTimer = SetTimer(mhwnd, kReinitDisplayTimerId, 500, NULL);
	}
}

void VDVideoDisplayWindow::StaticRemapPalette() {
	PALETTEENTRY pal[216];
	struct {
		LOGPALETTE hdr;
		PALETTEENTRY palext[255];
	} physpal;

	physpal.hdr.palVersion = 0x0300;
	physpal.hdr.palNumEntries = 256;

	int i;

	for(i=0; i<216; ++i) {
		pal[i].peRed	= (BYTE)((i / 36) * 51);
		pal[i].peGreen	= (BYTE)(((i%36) / 6) * 51);
		pal[i].peBlue	= (BYTE)((i%6) * 51);
	}

	for(i=0; i<256; ++i) {
		physpal.hdr.palPalEntry[i].peRed	= 0;
		physpal.hdr.palPalEntry[i].peGreen	= 0;
		physpal.hdr.palPalEntry[i].peBlue	= (BYTE)i;
		physpal.hdr.palPalEntry[i].peFlags	= PC_EXPLICIT;
	}

	if (HDC hdc = GetDC(0)) {
		GetSystemPaletteEntries(hdc, 0, 256, physpal.hdr.palPalEntry);
		ReleaseDC(0, hdc);
	}

	if (HPALETTE hpal = CreatePalette(&physpal.hdr)) {
		for(i=0; i<252; ++i) {
			sLogicalPalette[i] = (uint8)GetNearestPaletteIndex(hpal, RGB(pal[i].peRed, pal[i].peGreen, pal[i].peBlue));
#if 0
			VDDEBUG("[%3d %3d %3d] -> [%3d %3d %3d] : error(%+3d %+3d %+3d)\n"
					, pal[i].peRed
					, pal[i].peGreen
					, pal[i].peBlue
					, physpal.hdr.palPalEntry[sLogicalPalette[i]].peRed
					, physpal.hdr.palPalEntry[sLogicalPalette[i]].peGreen
					, physpal.hdr.palPalEntry[sLogicalPalette[i]].peBlue
					, pal[i].peRed - physpal.hdr.palPalEntry[sLogicalPalette[i]].peRed
					, pal[i].peGreen - physpal.hdr.palPalEntry[sLogicalPalette[i]].peGreen
					, pal[i].peBlue - physpal.hdr.palPalEntry[sLogicalPalette[i]].peBlue);
#endif
		}

		DeleteObject(hpal);
	}
}

bool VDVideoDisplayWindow::StaticCheckPaletted() {
	bool bPaletted = false;

	if (HDC hdc = GetDC(0)) {
		if (GetDeviceCaps(hdc, BITSPIXEL) <= 8)		// RC_PALETTE doesn't seem to be set if you switch to 8-bit in Win98 without rebooting.
			bPaletted = true;
		ReleaseDC(0, hdc);
	}

	return bPaletted;
}

void VDVideoDisplayWindow::StaticCreatePalette() {
	struct {
		LOGPALETTE hdr;
		PALETTEENTRY palext[255];
	} pal;

	pal.hdr.palVersion = 0x0300;
	pal.hdr.palNumEntries = 216;

	for(int i=0; i<216; ++i) {
		pal.hdr.palPalEntry[i].peRed	= (BYTE)((i / 36) * 51);
		pal.hdr.palPalEntry[i].peGreen	= (BYTE)(((i%36) / 6) * 51);
		pal.hdr.palPalEntry[i].peBlue	= (BYTE)((i%6) * 51);
		pal.hdr.palPalEntry[i].peFlags	= 0;
	}

	shPalette = CreatePalette(&pal.hdr);
}

LRESULT CALLBACK VDVideoDisplayWindow::StaticLookoutWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_DISPLAYCHANGE:
			{
				bool bPaletted = StaticCheckPaletted();

				if (bPaletted && !shPalette) {
					StaticCreatePalette();
				}

				for(tDisplayWindows::const_iterator it(sDisplayWindows.begin()), itEnd(sDisplayWindows.end()); it!=itEnd; ++it) {
					VDVideoDisplayWindow *p = *it;

					p->SyncDisplayChange();
				}

				if (!bPaletted && shPalette) {
					DeleteObject(shPalette);
					shPalette = 0;
				}
			}
			break;
		case WM_ACTIVATEAPP:
			{
				for(tDisplayWindows::const_iterator it(sDisplayWindows.begin()), itEnd(sDisplayWindows.end()); it!=itEnd; ++it) {
					VDVideoDisplayWindow *p = *it;

					p->SyncForegroundChange(wParam != 0);
				}
			}
			break;

		// Yes, believe it or not, we still support palettes, even when DirectDraw is active.
		// Why?  Very occasionally, people still have to run in 8-bit mode, and a program
		// should still display something half-decent in that case.  Besides, it's kind of
		// neat to be able to dither in safe mode.
		case WM_PALETTECHANGED:
			{
				DWORD dwProcess;

				GetWindowThreadProcessId((HWND)wParam, &dwProcess);

				if (dwProcess != GetCurrentProcessId()) {
					for(tDisplayWindows::const_iterator it(sDisplayWindows.begin()), itEnd(sDisplayWindows.end()); it!=itEnd; ++it) {
						VDVideoDisplayWindow *p = *it;

						p->SyncRealizePalette();
					}
				}
			}
			break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}
