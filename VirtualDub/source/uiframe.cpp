//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
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
#include <windows.h>
#include "oshelper.h"
#include "uiframe.h"
#include "resource.h"
#include "gui.h"

#if defined(_MSC_VER) && _MSC_VER < 1400
	#pragma optimize("a", off)		// Needs to be off or compiler thinks 'this' can't be aliased through mhwnd
#endif

extern HINSTANCE g_hInst;

static const char szAppName[]="VirtualDub";
static const wchar_t szAppNameW[]=L"VirtualDub";

bool VDRegisterUIFrameWindow() {
	return !!VDUIFrame::Register();
}

ATOM VDUIFrame::sClass;
vdlist<VDUIFrame>	VDUIFrame::sFrameList;

VDUIFrame::VDUIFrame(HWND hwnd)
	: mpClient(NULL)
	, mhAccel(NULL)
	, mNestCount(0)
	, mbDetachClient(false)
	, mbDetachEngine(false)
	, mbDestroy(false)
	, mhwnd(hwnd)
	, mRefCount(1)
	, mpRegistryName(NULL)
{
	VDASSERT(sFrameList.find(this) == sFrameList.end());
	sFrameList.push_back(this);
}

VDUIFrame::~VDUIFrame() {
}

void VDUIFrame::Attach(IVDUIFrameClient *p) {
	mpClient = p;
	mNextMode = 0;
}

void VDUIFrame::Detach() {
	if (mpClient) {
		mbDetachClient = true;
		if (!mNestCount)
			DetachNow(mhwnd, mbDetachClient, mbDetachEngine);
	}
}

void VDUIFrame::AttachEngine(IVDUIFrameEngine *p) {
	mpEngine = p;
}

void VDUIFrame::DetachEngine() {
	if (mpEngine) {
		mbDetachEngine = true;
		if (!mNestCount)
			DetachNow(mhwnd, mbDetachClient, mbDetachEngine);
	}
}

void VDUIFrame::Destroy() {
	mbDestroy = true;
	mbDetachClient = true;
	mbDetachEngine = true;
	if (!mNestCount)
		DetachNow(mhwnd, true, true);
}

void VDUIFrame::SetNextMode(int nextMode) {
	mNextMode = nextMode;

	if (!mNestCount)
		DetachNow(mhwnd, true, true);
}

void VDUIFrame::SetAccelTable(HACCEL hAccel) {
	mhAccel = hAccel;
}

void VDUIFrame::SetRegistryName(const char *name) {
	mpRegistryName = name;
}

void VDUIFrame::SavePlacement() {
	if (mpRegistryName)
		VDUISaveWindowPlacementW32(mhwnd, mpRegistryName);
}

void VDUIFrame::RestorePlacement(int nCmdShow) {
	if (mpRegistryName)
		VDUIRestoreWindowPlacementW32(mhwnd, mpRegistryName, nCmdShow);
}

LPARAM VDUIFrame::DefProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDASSERT(mNestCount);

	// We decrement the nesting count here to allow DefWindowProc() to call DestroyWindow()
	// without triggering our asserts. Otherwise, we think it is an unsafe detach.

	--mNestCount;
	LRESULT r = mpDefWindowProc(hwnd, msg, wParam, lParam);
	++mNestCount;

	return r;
}

void VDUIFrame::DestroyAll() {
	while(!sFrameList.empty()) {
		VDUIFrame *p = sFrameList.back();

		DestroyWindow(p->mhwnd);
	}
}

ATOM VDUIFrame::Register() {
	union {
		WNDCLASSA a;
		WNDCLASSW w;
	} wc;

    wc.a.style			= 0;
    wc.a.lpfnWndProc	= VDUIFrame::StaticWndProc;
    wc.a.cbClsExtra		= 0;
    wc.a.cbWndExtra		= sizeof(void *)*2;
    wc.a.hInstance		= g_hInst;
    wc.a.hIcon			= LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_VIRTUALDUB));
    wc.a.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wc.a.hbrBackground	= (HBRUSH)(COLOR_3DFACE+1); //GetStockObject(LTGRAY_BRUSH); 

	if (GetVersion() < 0x80000000) {
	    wc.w.lpszMenuName	= MAKEINTRESOURCEW(IDR_MAIN_MENU);
		wc.w.lpszClassName	= szAppNameW;

		sClass = RegisterClassW(&wc.w);
	} else {
	    wc.a.lpszMenuName	= MAKEINTRESOURCEA(IDR_MAIN_MENU);
		wc.a.lpszClassName	= szAppName;

		sClass = RegisterClassA(&wc.a);
	}

	return !!sClass;
}

bool VDUIFrame::TranslateAcceleratorMessage(MSG& msg) {
	if (!msg.hwnd || (msg.message != WM_KEYDOWN && msg.message != WM_SYSKEYDOWN && msg.message != WM_KEYUP && msg.message != WM_SYSKEYUP && msg.message != WM_CHAR))
		return false;

	HWND hwnd = VDGetAncestorW32(msg.hwnd, GA_ROOT);

	ATOM a = (ATOM)GetClassLongPtr(hwnd, GCW_ATOM);

	if (a != sClass)
		return false;

	VDUIFrame *p = (VDUIFrame *)GetWindowLongPtr(hwnd, 0);

	if (p->mhAccel && TranslateAccelerator(hwnd, p->mhAccel, &msg))
		return true;

	// check for frame-specific intercept
	if (p->mpClient) {
		switch(msg.message) {
		case WM_CHAR:		return p->mpClient->Intercept_WM_CHAR(msg.wParam, msg.lParam);
		case WM_KEYDOWN:	return p->mpClient->Intercept_WM_KEYDOWN(msg.wParam, msg.lParam);
		case WM_KEYUP:		return p->mpClient->Intercept_WM_KEYUP(msg.wParam, msg.lParam);
		case WM_SYSKEYDOWN:	return p->mpClient->Intercept_WM_SYSKEYDOWN(msg.wParam, msg.lParam);
		case WM_SYSKEYUP:	return p->mpClient->Intercept_WM_SYSKEYUP(msg.wParam, msg.lParam);
		}
	}

	return false;
}

LRESULT CALLBACK VDUIFrame::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDUIFrame *p;

	if (msg == WM_NCCREATE) {
		p = new_nothrow VDUIFrame(hwnd);
		if (!p)
			return FALSE;
		SetWindowLongPtr(hwnd, 0, (LONG_PTR)p);
		p->mpDefWindowProc = IsWindowUnicode(hwnd) ? DefWindowProcW : DefWindowProcA;
	} else if (p = (VDUIFrame *)GetWindowLongPtr(hwnd, 0)) {
		switch (msg) {
		case WM_DESTROY:
			p->SavePlacement();
			p->Detach();
			p->DetachEngine();
			break;
		case WM_NCDESTROY:
			p->DetachNow(hwnd, true, true);
			VDASSERT(sFrameList.find(p) != sFrameList.end());
			sFrameList.erase(p);
			p->mhwnd = NULL;
			SetWindowLongPtr(hwnd, 0, NULL);
			if (!--p->mRefCount)
				delete p;
			p = NULL;

			if (sFrameList.empty())
				PostQuitMessage(0);
			break;
		case VDWM_ENGINE_EVENT:
			if (p->mpEngine)
				return p->mpEngine->OnEngineEvent(wParam, lParam);

			return 0;
		}
	}

	if (!p)
		return (IsWindowUnicode(hwnd) ? DefWindowProcW : DefWindowProcA)(hwnd, msg, wParam, lParam);

	if (p->mpClient) {
		// This refcount prevents the object from going away if a destroy occurs from
		// DefWindowProc(), so that the outgoing refcount adjustments in DefProc() don't
		// trash memory.
		++p->mRefCount;

		// The nesting count here is essentially a local refcount on the client/engine pair;
		// otherwise, the engine and client can get detached while running!
		++p->mNestCount;
		LRESULT r = p->mpClient->WndProc(hwnd, msg, wParam, lParam);
		--p->mNestCount;
		if (!--p->mRefCount) {
			delete p;
			p = NULL;
		} else if (!p->mNestCount) {
			if (p->mbDetachClient || p->mbDetachEngine)
				p->DetachNow(hwnd, p->mbDetachClient, p->mbDetachEngine);
		}

		return r;
	}
	return p->mpDefWindowProc(hwnd, msg, wParam, lParam);
}

void VDUIFrame::DetachNow(HWND hwnd, bool bClient, bool bEngine) {
	VDASSERT((!mpClient && !mpEngine) || (!mNestCount && (bClient || bEngine)));

	if (bClient && mpClient) {
		vdrefptr<IVDUIFrameClient> pClient;
		pClient.from(mpClient);		// prevents recursion

		mhAccel = NULL;
		mbDetachClient = false;

		pClient->Detach();
	}

	if (bEngine && mpEngine) {
		vdrefptr<IVDUIFrameEngine> pEngine;
		pEngine.from(mpEngine);		// prevents recursion

		mbDetachEngine = false;

		pEngine->Detach();
	}

	if (mbDestroy) {
		mbDestroy = false;
		DestroyWindow(mhwnd);
		return;
	}

	if (const int nextMode = mNextMode) {
		mNextMode = 0;		// prevents recursion
		// HACK
		extern void VDSwitchUIFrameMode(HWND hwnd, int nextMode);
		VDSwitchUIFrameMode(hwnd, nextMode);
	}
}
