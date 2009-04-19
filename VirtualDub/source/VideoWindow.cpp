//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2002 Avery Lee
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

#define f_VIDEOWINDOW_CPP

#include "stdafx.h"

#include <algorithm>

#include <windows.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/w32assist.h>
#include <vd2/Riza/display.h>

#include "VideoWindow.h"
#include "prefs.h"			// for display preferences

#include "resource.h"

extern HINSTANCE g_hInst;

static LRESULT APIENTRY VideoWindowWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

////////////////////////////

extern const char g_szVideoWindowClass[]="phaeronVideoWindow";

////////////////////////////

class VDVideoWindow : public IVDVideoWindow {
public:
	VDVideoWindow(HWND hwnd);
	~VDVideoWindow();

	static ATOM RegisterControl();

	void SetSourceSize(int w, int h);
	void GetFrameSize(int& w, int& h);
	void Resize();
	void SetChild(HWND hwnd);
	void SetDisplay(IVDVideoDisplay *pDisplay);

private:
	HWND mhwnd;
	HWND mhwndChild;
	HMENU mhmenu;
	int mSourceWidth;
	int mSourceHeight;
	double mSourceAspectRatio;
	double mZoom;
	double mAspectRatio;
	double mFreeAspectRatio;
	bool mbAspectIsFrameBased;
	bool mbResizing;

	IVDVideoDisplay *mpDisplay;

	LRESULT mLastHitTest;

	static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void NCPaint(HRGN hrgn);
	void RecalcClientArea(RECT& rc);
	LRESULT RecalcClientArea(NCCALCSIZE_PARAMS& params);
	LRESULT HitTest(int x, int y);
	void OnCommand(int cmd);
	void OnContextMenu(int x, int y);

	void SetAspectRatio(double ar, bool bFrame);
	void SetZoom(double zoom);
};

////////////////////////////

ATOM RegisterVideoWindow() {
	return VDVideoWindow::RegisterControl();
}

IVDVideoWindow *VDGetIVideoWindow(HWND hwnd) {
	return (IVDVideoWindow *)(VDVideoWindow *)GetWindowLongPtr(hwnd, 0);
}

VDVideoWindow::VDVideoWindow(HWND hwnd)
	: mhwnd(hwnd)
	, mhwndChild(NULL)
	, mhmenu(LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_DISPLAY_MENU)))
	, mSourceWidth(0)
	, mSourceHeight(0)
	, mSourceAspectRatio(1.0)
	, mZoom(1.0f)
	, mAspectRatio(-1)
	, mFreeAspectRatio(1.0)
	, mLastHitTest(HTNOWHERE)
	, mbResizing(false)
	, mpDisplay(NULL)
{
	SetWindowLongPtr(mhwnd, 0, (LONG_PTR)this);
}

VDVideoWindow::~VDVideoWindow() {
	if (mhmenu)
		DestroyMenu(mhmenu);
}

ATOM VDVideoWindow::RegisterControl() {
	WNDCLASS wc;

	wc.style			= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= VDVideoWindow::WndProcStatic;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= sizeof(VDVideoWindow *);
	wc.hInstance		= g_hInst;
	wc.hIcon			= NULL;
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground	= NULL; //(HBRUSH)(COLOR_3DFACE+1);
	wc.lpszMenuName		= NULL;
	wc.lpszClassName	= g_szVideoWindowClass;

	return RegisterClass(&wc);
}

void VDVideoWindow::SetSourceSize(int w, int h) {
	mSourceWidth		= w;
	mSourceHeight		= h;
	mSourceAspectRatio	= 1.0;

	if (h)
		mSourceAspectRatio = (double)w / (double)h;

	Resize();
}

void VDVideoWindow::GetFrameSize(int& w, int& h) {
	RECT r;
	GetClientRect(mhwnd, &r);
	w = r.right;
	h = r.bottom;
}

void VDVideoWindow::SetAspectRatio(double ar, bool bFrame) {
	mAspectRatio = ar;
	mbAspectIsFrameBased = bFrame;

	if (ar > 0) {
		if (bFrame)
			mFreeAspectRatio = ar / mSourceAspectRatio;
		else
			mFreeAspectRatio = ar;
	}

	Resize();
}

void VDVideoWindow::SetZoom(double zoom) {
	mZoom = zoom;
	Resize();
}

void VDVideoWindow::Resize() {
	if (mSourceWidth > 0 && mSourceHeight > 0) {
		int w, h;

		if (mAspectRatio < 0) {
			w = VDRoundToInt(mSourceHeight * mSourceAspectRatio * mFreeAspectRatio * mZoom);
		} else {
			if (mbAspectIsFrameBased)
				w = VDRoundToInt(mSourceHeight * mAspectRatio * mZoom);
			else
				w = VDRoundToInt(mSourceWidth * mAspectRatio * mZoom);
		}
		h = VDRoundToInt(mSourceHeight * mZoom);
		SetWindowPos(mhwnd, NULL, 0, 0, w+8, h+8, SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
	}
}

void VDVideoWindow::SetChild(HWND hwnd) {
	mhwndChild = hwnd;
}

void VDVideoWindow::SetDisplay(IVDVideoDisplay *pDisplay) {
	mpDisplay = pDisplay;
}

LRESULT CALLBACK VDVideoWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_NCCREATE) {
		VDVideoWindow *pvw = new VDVideoWindow(hwnd);

		if (!pvw)
			return FALSE;
	} else if (msg == WM_NCDESTROY) {
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return reinterpret_cast<VDVideoWindow *>(GetWindowLongPtr(hwnd, 0))->WndProc(msg, wParam, lParam);
}

LRESULT VDVideoWindow::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_CREATE:
		{
			RECT rc;

			GetClientRect(mhwnd, &rc);
		}
		return TRUE;
	case WM_DESTROY:
		{
			volatile HWND hwnd = mhwnd;

			delete this;

			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
	case WM_NCHITTEST:
		return mLastHitTest = HitTest((SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
	case WM_NCCALCSIZE:
		if (wParam)
			return RecalcClientArea(*(NCCALCSIZE_PARAMS *)lParam);

		RecalcClientArea(*(RECT *)lParam);
		return 0;
	case WM_NCPAINT:
		NCPaint((HRGN)wParam);
		return 0;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			if (HDC hdc = BeginPaint(mhwnd, &ps)) {
				NMHDR hdr;

				hdr.hwndFrom = mhwnd;
				hdr.idFrom = GetWindowLong(mhwnd, GWL_ID);
				hdr.code = VWN_REQUPDATE;

				SendMessage(GetParent(mhwnd), WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);
				EndPaint(mhwnd, &ps);
			}
		}
		return 0;

	case WM_MOVE:
		break;
	case WM_SIZE:
		break;

	case WM_ENTERSIZEMOVE:
		mbResizing = true;
		break;
	case WM_EXITSIZEMOVE:
		mbResizing = false;
		break;

	case WM_WINDOWPOSCHANGING:
		if (mbResizing) {
			WINDOWPOS *pwp = ((WINDOWPOS *)lParam);
			pwp->flags |= SWP_NOZORDER;

			if (mAspectRatio > 0) {
				double ar = mAspectRatio;

				if (!mbAspectIsFrameBased)
					ar *= (double)mSourceWidth / (double)mSourceHeight;

				bool bXMajor = pwp->cx > pwp->cy * ar;

				if (mLastHitTest == HTBOTTOM)
					bXMajor = false;
				else if (mLastHitTest == HTRIGHT)
					bXMajor = true;

				if (bXMajor)
					pwp->cy = VDRoundToInt(pwp->cx / ar);
				else
					pwp->cx = VDRoundToInt(pwp->cy * ar);
			}
		}
		break;
	case WM_WINDOWPOSCHANGED:
		{
			NMHDR hdr;
			RECT r;

			GetClientRect(mhwnd, &r);

			if (mSourceHeight > 0) {
				mZoom = (double)r.bottom / mSourceHeight;

				if (mAspectRatio < 0)
					mFreeAspectRatio = r.right / (r.bottom * mSourceAspectRatio);
			}

			hdr.hwndFrom = mhwnd;
			hdr.idFrom = GetWindowLong(mhwnd, GWL_ID);
			hdr.code = VWN_RESIZED;

			if (mhwndChild)
				SetWindowPos(mhwndChild, NULL, 0, 0, r.right, r.bottom, SWP_NOZORDER);
			SendMessage(GetParent(mhwnd), WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);
		}
		break;		// explicitly pass this through to DefWindowProc for WM_SIZE and WM_MOVE

	case WM_CONTEXTMENU:
		OnContextMenu((sint16)LOWORD(lParam), (sint16)HIWORD(lParam));
		break;

	case WM_COMMAND:
		OnCommand(LOWORD(wParam));
		break;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDVideoWindow::NCPaint(HRGN hrgn) {
	HDC hdc;
	
	// MSDN docs are in error -- if you do not include 0x10000, GetDCEx() will
	// return NULL but still won't set an error flag.  Feng Yuan's Windows
	// Graphics book calls this the "undocumented flag that makes GetDCEx()
	// succeed" flag. ^^;
	//
	// WINE's source code documents it as DCX_USESTYLE, which makes more sense,
	// and that is the way WINE's DefWindowProc() handles WM_NCPAINT.  This is
	// a cleaner solution than using DCX_CACHE, which also works but overrides
	// CS_OWNDC and CS_PARENTDC.  I'm not going to argue with years of reverse
	// engineering.
	//
	// NOTE: Using DCX_INTERSECTRGN as recommended by MSDN causes Windows 98
	//       GDI to intermittently crash!

	hdc = GetDCEx(mhwnd, NULL, DCX_WINDOW|0x10000);

	if (hdc) {
		RECT rc;
		GetWindowRect(mhwnd, &rc);

		if ((WPARAM)hrgn > 1) {
			OffsetClipRgn(hdc, rc.left, rc.top);
			ExtSelectClipRgn(hdc, hrgn, RGN_AND);
			OffsetClipRgn(hdc, -rc.left, -rc.top);
		}

		OffsetRect(&rc, -rc.left, -rc.top);

		DrawEdge(hdc, &rc, BDR_RAISEDOUTER|BDR_RAISEDINNER, BF_RECT);
		rc.left		+= 2;
		rc.right	-= 2;
		rc.top		+= 2;
		rc.bottom	-= 2;
		DrawEdge(hdc, &rc, BDR_SUNKENOUTER|BDR_SUNKENINNER, BF_RECT);

		ReleaseDC(mhwnd, hdc);
	}
}

void VDVideoWindow::RecalcClientArea(RECT& rc) {
	rc.left += 4;
	rc.right -= 4;
	rc.top += 4;
	rc.bottom -= 4;
}

LRESULT VDVideoWindow::RecalcClientArea(NCCALCSIZE_PARAMS& params) {
	// Win32 docs don't say you need to do this, but you do.

	params.rgrc[0].left += 4;
	params.rgrc[0].right -= 4;
	params.rgrc[0].top += 4;
	params.rgrc[0].bottom -= 4;

	return 0;//WVR_ALIGNTOP|WVR_ALIGNLEFT;
}

LRESULT VDVideoWindow::HitTest(int x, int y) {
	POINT pt = { x, y };
	RECT rc;

	GetClientRect(mhwnd, &rc);
	ScreenToClient(mhwnd, &pt);

	if (pt.x >= 4 && pt.y >= 4 && pt.x < rc.right-4 && pt.y < rc.bottom-4)
		return HTCLIENT; //HTCAPTION;
	else {
		int xseg = std::min<int>(16, rc.right/3);
		int yseg = std::min<int>(16, rc.bottom/3);
		int id = 0;

		if (pt.x >= xseg)
			++id;

		if (pt.x >= rc.right - xseg)
			++id;

		if (pt.y >= yseg)
			id += 3;

		if (pt.y >= rc.bottom - yseg)
			id += 3;

		static const LRESULT sHitRegions[9]={
			HTNOWHERE,//HTTOPLEFT,
			HTNOWHERE,//HTTOP,
			HTNOWHERE,//HTTOPRIGHT,
			HTNOWHERE,//HTLEFT,
			HTCLIENT,		// should never be hit
			HTRIGHT,
			HTNOWHERE,//HTBOTTOMLEFT,
			HTBOTTOM,
			HTBOTTOMRIGHT
		};

		return sHitRegions[id];
	}
}

void VDVideoWindow::OnCommand(int cmd) {
	switch(cmd) {
	case ID_DISPLAY_ZOOM_25:		SetZoom(0.25); break;
	case ID_DISPLAY_ZOOM_33:		SetZoom(1.0/3.0); break;
	case ID_DISPLAY_ZOOM_50:		SetZoom(0.5); break;
	case ID_DISPLAY_ZOOM_66:		SetZoom(2.0/3.0); break;
	case ID_DISPLAY_ZOOM_75:		SetZoom(3.0/4.0); break;
	case ID_DISPLAY_ZOOM_100:		SetZoom(1.0); break;
	case ID_DISPLAY_ZOOM_150:		SetZoom(1.5); break;
	case ID_DISPLAY_ZOOM_200:		SetZoom(2.0); break;
	case ID_DISPLAY_ZOOM_300:		SetZoom(3.0); break;
	case ID_DISPLAY_ZOOM_400:		SetZoom(4.0); break;
	case ID_DISPLAY_ZOOM_EXACT:
		mZoom = 1.0;
		SetAspectRatio(1.0, false);
		break;
	case ID_DISPLAY_AR_FREE:		SetAspectRatio(-1, false); break;
	case ID_DISPLAY_AR_PIXEL_0909:  SetAspectRatio( 10.0/11.0, false); break;
	case ID_DISPLAY_AR_PIXEL_1000:	SetAspectRatio(  1.0     , false); break;
	case ID_DISPLAY_AR_PIXEL_1093:  SetAspectRatio( 59.0/54.0, false); break;
	case ID_DISPLAY_AR_PIXEL_1212:  SetAspectRatio( 40.0/33.0, false); break;
	case ID_DISPLAY_AR_PIXEL_1364:  SetAspectRatio( 15.0/11.0, false); break;
	case ID_DISPLAY_AR_PIXEL_1457:  SetAspectRatio(118.0/81.0, false); break;
	case ID_DISPLAY_AR_PIXEL_1639:  SetAspectRatio( 59.0/36.0, false); break;
	case ID_DISPLAY_AR_PIXEL_1818:  SetAspectRatio( 20.0/11.0, false); break;
	case ID_DISPLAY_AR_PIXEL_2185:  SetAspectRatio( 59.0/27.0, false); break;
	case ID_DISPLAY_AR_FRAME_1333:	SetAspectRatio(4.0/3.0, true); break;
	case ID_DISPLAY_AR_FRAME_1364:	SetAspectRatio(15.0/11.0, true); break;
	case ID_DISPLAY_AR_FRAME_1777:	SetAspectRatio(16.0/9.0, true); break;
	case ID_DISPLAY_FILTER_POINT:
		if (mpDisplay)
			mpDisplay->SetFilterMode(IVDVideoDisplay::kFilterPoint);
		break;
	case ID_DISPLAY_FILTER_BILINEAR:
		if (mpDisplay)
			mpDisplay->SetFilterMode(IVDVideoDisplay::kFilterBilinear);
		break;
	case ID_DISPLAY_FILTER_BICUBIC:
		if (mpDisplay)
			mpDisplay->SetFilterMode(IVDVideoDisplay::kFilterBicubic);
		break;
	case ID_DISPLAY_FILTER_ANY:
		if (mpDisplay)
			mpDisplay->SetFilterMode(IVDVideoDisplay::kFilterAnySuitable);
		break;
	}
}

void VDVideoWindow::OnContextMenu(int x, int y) {
	HMENU hmenu = GetSubMenu(mhmenu, 0);

	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_25, fabs(mZoom - 0.25) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_33, fabs(mZoom - 1.0/3.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_50, fabs(mZoom - 0.5) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_66, fabs(mZoom - 2.0/3.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_75, fabs(mZoom - 3.0/4.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_100, fabs(mZoom - 1.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_150, fabs(mZoom - 1.5) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_200, fabs(mZoom - 2.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_300, fabs(mZoom - 3.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_400, fabs(mZoom - 4.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_FREE, mAspectRatio < 0);

	// The aspect ratio values below come from "mir DMG: Aspect Ratios and Frame Sizes",
	// http://www.mir.com/DMG/aspect.html, as seen on 09/02/2004.
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_0909, !mbAspectIsFrameBased && fabs(mAspectRatio - 10.0/11.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1000, !mbAspectIsFrameBased && fabs(mAspectRatio -  1.0     ) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1093, !mbAspectIsFrameBased && fabs(mAspectRatio - 59.0/54.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1212, !mbAspectIsFrameBased && fabs(mAspectRatio - 40.0/33.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1364, !mbAspectIsFrameBased && fabs(mAspectRatio - 15.0/11.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1457, !mbAspectIsFrameBased && fabs(mAspectRatio -118.0/81.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1639, !mbAspectIsFrameBased && fabs(mAspectRatio - 59.0/36.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1818, !mbAspectIsFrameBased && fabs(mAspectRatio - 20.0/11.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_2185, !mbAspectIsFrameBased && fabs(mAspectRatio - 59.0/27.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_FRAME_1333,  mbAspectIsFrameBased && fabs(mAspectRatio -  4.0/ 3.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_FRAME_1364,  mbAspectIsFrameBased && fabs(mAspectRatio - 15.0/11.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_FRAME_1777,  mbAspectIsFrameBased && fabs(mAspectRatio - 16.0/ 9.0) < 1e-5);

	DWORD dwEnabled1 = MF_BYCOMMAND | MF_GRAYED;
	DWORD dwEnabled2 = MF_BYCOMMAND | MF_GRAYED;
	if (mpDisplay && !(g_prefs.fDisplay & Preferences::kDisplayDisableDX)) {
		if (g_prefs.fDisplay & (Preferences::kDisplayEnableD3D | Preferences::kDisplayEnableOpenGL))
			dwEnabled1 = MF_BYCOMMAND | MF_ENABLED;

		if (g_prefs.fDisplay & Preferences::kDisplayEnableD3D)
			dwEnabled2 = MF_BYCOMMAND | MF_ENABLED;
	}
	EnableMenuItem(hmenu, ID_DISPLAY_FILTER_POINT, dwEnabled1);
	EnableMenuItem(hmenu, ID_DISPLAY_FILTER_BILINEAR, dwEnabled1);
	EnableMenuItem(hmenu, ID_DISPLAY_FILTER_BICUBIC, dwEnabled2);
	EnableMenuItem(hmenu, ID_DISPLAY_FILTER_ANY, dwEnabled1);

	if (mpDisplay) {
		IVDVideoDisplay::FilterMode mode = mpDisplay->GetFilterMode();

		CheckMenuItem(hmenu, ID_DISPLAY_FILTER_POINT, mode == IVDVideoDisplay::kFilterPoint ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hmenu, ID_DISPLAY_FILTER_BILINEAR, mode == IVDVideoDisplay::kFilterBilinear ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hmenu, ID_DISPLAY_FILTER_BICUBIC, mode == IVDVideoDisplay::kFilterBicubic ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hmenu, ID_DISPLAY_FILTER_ANY, mode == IVDVideoDisplay::kFilterAnySuitable ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	}

	TrackPopupMenu(hmenu, TPM_LEFTALIGN|TPM_TOPALIGN|TPM_LEFTBUTTON, x, y, 0, mhwnd, NULL);
}
