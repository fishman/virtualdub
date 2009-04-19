#include "stdafx.h"
#include <windows.h>
#include <vd2/system/w32assist.h>
#include <vd2/VDLib/Dialog.h>

extern HINSTANCE g_hInst;

VDDialogFrameW32::VDDialogFrameW32(uint32 dlgid)
	: mpDialogResourceName(MAKEINTRESOURCE(dlgid))
	, mbIsModal(false)
	, mhdlg(NULL)
{
}

bool VDDialogFrameW32::Create(VDGUIHandle parent) {
	if (!mhdlg) {
		mbIsModal = false;

		if (VDIsWindowsNT())
			CreateDialogParamW(g_hInst, IS_INTRESOURCE(mpDialogResourceName) ? (LPCWSTR)mpDialogResourceName : VDTextAToW(mpDialogResourceName).c_str(), (HWND)parent, StaticDlgProc, (LPARAM)this);
		else
			CreateDialogParamA(g_hInst, mpDialogResourceName, (HWND)parent, StaticDlgProc, (LPARAM)this);
	}

	return mhdlg != NULL;
}

void VDDialogFrameW32::Destroy() {
	if (mhdlg)
		DestroyWindow(mhdlg);
}

sintptr VDDialogFrameW32::ShowDialog(VDGUIHandle parent) {
	mbIsModal = true;
	if (VDIsWindowsNT())
		return DialogBoxParamW(g_hInst, IS_INTRESOURCE(mpDialogResourceName) ? (LPCWSTR)mpDialogResourceName : VDTextAToW(mpDialogResourceName).c_str(), (HWND)parent, StaticDlgProc, (LPARAM)this);
	else
		return DialogBoxParamA(g_hInst, mpDialogResourceName, (HWND)parent, StaticDlgProc, (LPARAM)this);
}

void VDDialogFrameW32::Show() {
	if (mhdlg)
		ShowWindow(mhdlg, SW_SHOWNA);
}

void VDDialogFrameW32::Hide() {
	if (mhdlg)
		ShowWindow(mhdlg, SW_HIDE);
}

void VDDialogFrameW32::End(sintptr result) {
	if (!mhdlg)
		return;

	if (mbIsModal)
		EndDialog(mhdlg, result);
	else
		DestroyWindow(mhdlg);
}

void VDDialogFrameW32::SetFocusToControl(uint32 id) {
	if (!mhdlg)
		return;

	HWND hwnd = GetDlgItem(mhdlg, id);
	if (hwnd)
		SendMessage(mhdlg, WM_NEXTDLGCTL, (WPARAM)hwnd, TRUE);
}

void VDDialogFrameW32::EnableControl(uint32 id, bool enabled) {
	if (!mhdlg)
		return;

	HWND hwnd = GetDlgItem(mhdlg, id);
	if (hwnd)
		EnableWindow(hwnd, enabled);
}

void VDDialogFrameW32::SetControlText(uint32 id, const wchar_t *s) {
	if (!mhdlg)
		return;

	HWND hwnd = GetDlgItem(mhdlg, id);
	if (hwnd)
		VDSetWindowTextW32(hwnd, s);
}

void VDDialogFrameW32::SetControlTextF(uint32 id, const wchar_t *format, ...) {
	if (!mhdlg)
		return;

	HWND hwnd = GetDlgItem(mhdlg, id);
	if (hwnd) {
		VDStringW s;
		va_list val;

		va_start(val, format);
		s.append_vsprintf(format, val);
		va_end(val);

		VDSetWindowTextW32(hwnd, s.c_str());
	}
}

uint32 VDDialogFrameW32::GetControlValueUint32(uint32 id) {
	if (!mhdlg) {
		FailValidation(id);
		return 0;
	}

	HWND hwnd = GetDlgItem(mhdlg, id);
	if (!hwnd) {
		FailValidation(id);
		return 0;
	}

	VDStringW s(VDGetWindowTextW32(hwnd));
	unsigned val;
	wchar_t tmp;
	if (1 != swscanf(s.c_str(), L" %u %c", &val, &tmp)) {
		FailValidation(id);
		return 0;
	}

	return val;
}

double VDDialogFrameW32::GetControlValueDouble(uint32 id) {
	if (!mhdlg) {
		FailValidation(id);
		return 0;
	}

	HWND hwnd = GetDlgItem(mhdlg, id);
	if (!hwnd) {
		FailValidation(id);
		return 0;
	}

	VDStringW s(VDGetWindowTextW32(hwnd));
	double val;
	wchar_t tmp;
	if (1 != swscanf(s.c_str(), L" %lg %c", &val, &tmp)) {
		FailValidation(id);
		return 0;
	}

	return val;
}

void VDDialogFrameW32::ExchangeControlValueDouble(bool write, uint32 id, const wchar_t *format, double& val, double minVal, double maxVal) {
	if (write) {
		val = GetControlValueDouble(id);
		if (val < minVal || val > maxVal)
			FailValidation(id);
	} else {
		SetControlTextF(id, format, val);
	}
}

void VDDialogFrameW32::CheckButton(uint32 id, bool checked) {
	CheckDlgButton(mhdlg, id, checked ? BST_CHECKED : BST_UNCHECKED);
}

bool VDDialogFrameW32::IsButtonChecked(uint32 id) {
	return IsDlgButtonChecked(mhdlg, id) != 0;
}

void VDDialogFrameW32::BeginValidation() {
	mbValidationFailed = false;
}

bool VDDialogFrameW32::EndValidation() {
	if (mbValidationFailed) {
		SignalFailedValidation(mFailedId);
		return false;
	}

	return true;
}

void VDDialogFrameW32::FailValidation(uint32 id) {
	mbValidationFailed = true;
	mFailedId = id;
}

void VDDialogFrameW32::SignalFailedValidation(uint32 id) {
	if (!mhdlg)
		return;

	HWND hwnd = GetDlgItem(mhdlg, id);

	MessageBeep(MB_ICONEXCLAMATION);
	if (hwnd)
		SetFocus(hwnd);
}

sint32 VDDialogFrameW32::LBGetSelectedIndex(uint32 id) {
	return SendDlgItemMessage(mhdlg, id, LB_GETCURSEL, 0, 0);
}

void VDDialogFrameW32::LBSetSelectedIndex(uint32 id, sint32 idx) {
	SendDlgItemMessage(mhdlg, id, LB_SETCURSEL, idx, 0);
}

void VDDialogFrameW32::LBAddString(uint32 id, const wchar_t *s) {
	if (VDIsWindowsNT()) {
		SendDlgItemMessageW(mhdlg, id, LB_ADDSTRING, 0, (LPARAM)s);
	} else {
		SendDlgItemMessageA(mhdlg, id, LB_ADDSTRING, 0, (LPARAM)VDTextWToA(s).c_str());		
	}
}

void VDDialogFrameW32::LBAddStringF(uint32 id, const wchar_t *format, ...) {
	VDStringW s;
	va_list val;

	va_start(val, format);
	s.append_vsprintf(format, val);
	va_end(val);

	LBAddString(id, s.c_str());
}

void VDDialogFrameW32::OnDataExchange(bool write) {
}

bool VDDialogFrameW32::OnLoaded() {
	OnDataExchange(false);
	return false;
}

bool VDDialogFrameW32::OnOK() {
	BeginValidation();
	OnDataExchange(true);
	return !EndValidation();
}

bool VDDialogFrameW32::OnCancel() {
	return false;
}

void VDDialogFrameW32::OnSize() {
}

void VDDialogFrameW32::OnDestroy() {
}

bool VDDialogFrameW32::OnTimer(uint32 id) {
	return false;
}

bool VDDialogFrameW32::OnCommand(uint32 id, uint32 extcode) {
	return false;
}

bool VDDialogFrameW32::PreNCDestroy() {
	return false;
}

VDZINT_PTR VDZCALLBACK VDDialogFrameW32::StaticDlgProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	VDDialogFrameW32 *pThis = (VDDialogFrameW32 *)GetWindowLongPtr(hwnd, DWLP_USER);

	if (msg == WM_INITDIALOG) {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		pThis = (VDDialogFrameW32 *)lParam;
		pThis->mhdlg = hwnd;
	} else if (msg == WM_NCDESTROY) {
		if (pThis) {
			bool deleteMe = pThis->PreNCDestroy();

			pThis->mhdlg = NULL;
			SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)(void *)NULL);

			if (deleteMe)
				delete pThis;

			pThis = NULL;
			return FALSE;
		}
	}

	return pThis ? pThis->DlgProc(msg, wParam, lParam) : FALSE;
}

VDZINT_PTR VDDialogFrameW32::DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			return !OnLoaded();

		case WM_COMMAND:
			{
				uint32 id = LOWORD(wParam);

				if (id == IDOK) {
					if (!OnOK())
						End(true);

					return TRUE;
				} else if (id == IDCANCEL) {
					if (!OnCancel())
						End(false);

					return TRUE;
				} else {
					if (OnCommand(id, HIWORD(wParam)))
						return TRUE;
				}
			}

			break;

		case WM_DESTROY:
			OnDestroy();
			break;

		case WM_SIZE:
			OnSize();
			return FALSE;

		case WM_TIMER:
			return OnTimer((uint32)wParam);
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////

VDDialogResizerW32::VDDialogResizerW32() {
}

VDDialogResizerW32::~VDDialogResizerW32() {
}

void VDDialogResizerW32::Init(HWND hwnd) {
	mhwndBase = hwnd;
	mWidth = 1;
	mHeight = 1;

	RECT r;
	if (GetClientRect(hwnd, &r)) {
		mWidth = r.right;
		mHeight = r.bottom;
	}
}

void VDDialogResizerW32::Relayout() {
	RECT r;

	if (GetClientRect(mhwndBase, &r))
		Relayout(r.right, r.bottom);
}

void VDDialogResizerW32::Relayout(int width, int height) {
	HDWP hdwp = BeginDeferWindowPos(mControls.size());

	mWidth = width;
	mHeight = height;

	Controls::const_iterator it(mControls.begin()), itEnd(mControls.end());
	for(; it!=itEnd; ++it) {
		const ControlEntry& ent = *it;
		uint32 flags = SWP_NOZORDER|SWP_NOACTIVATE;

		if (!(ent.mAlignment & (kAnchorX | kAnchorY)))
			flags |= SWP_NOMOVE;

		if (!(ent.mAlignment & (kAnchorW | kAnchorH)))
			flags |= SWP_NOSIZE;

		int x = ent.mX;
		int y = ent.mY;
		int w = ent.mW;
		int h = ent.mH;

		if (ent.mAlignment & kAnchorX)
			x += mWidth;

		if (ent.mAlignment & kAnchorW)
			w += mWidth;

		if (ent.mAlignment & kAnchorY)
			y += mHeight;

		if (ent.mAlignment & kAnchorH)
			h += mHeight;

		if (hdwp) {
			HDWP hdwp2 = DeferWindowPos(hdwp, ent.mhwnd, NULL, x, y, w, h, flags);

			if (hdwp2) {
				hdwp = hdwp2;
				continue;
			}
		}

		SetWindowPos(ent.mhwnd, NULL, x, y, w, h, flags);
	}

	if (hdwp)
		EndDeferWindowPos(hdwp);
}

void VDDialogResizerW32::Add(uint32 id, int alignment) {
	HWND hwndControl = GetDlgItem(mhwndBase, id);
	if (!hwndControl)
		return;

	RECT r;
	if (!GetWindowRect(hwndControl, &r))
		return;

	if (!MapWindowPoints(NULL, mhwndBase, (LPPOINT)&r, 2))
		return;

	ControlEntry& ce = mControls.push_back();

	ce.mhwnd		= hwndControl;
	ce.mAlignment	= alignment;
	ce.mX			= r.left;
	ce.mY			= r.top;
	ce.mW			= r.right - r.left;
	ce.mH			= r.bottom - r.top;

	if (alignment & kAnchorX)
		ce.mX -= mWidth;

	if (alignment & kAnchorW)
		ce.mW -= mWidth;

	if (alignment & kAnchorY)
		ce.mY -= mHeight;

	if (alignment & kAnchorH)
		ce.mH -= mHeight;
}
