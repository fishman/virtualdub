//	VDShader - Custom shader video filter for VirtualDub
//	Copyright (C) 2007 Avery Lee
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
#include "f_base.h"

VDVFilterDialog::VDVFilterDialog()
	: mhdlg(NULL)
{
}

LRESULT VDVFilterDialog::Show(HINSTANCE hInst, LPCTSTR templName, HWND parent) {
	return DialogBoxParam(hInst, templName, parent, StaticDlgProc, (LPARAM)this);
}

INT_PTR CALLBACK VDVFilterDialog::StaticDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDVFilterDialog *pThis;

	if (msg == WM_INITDIALOG) {
		pThis = (VDVFilterDialog *)lParam;
		SetWindowLongPtr(hdlg, DWLP_USER, (LONG_PTR)pThis);
		pThis->mhdlg = hdlg;
	} else
		pThis = (VDVFilterDialog *)GetWindowLongPtr(hdlg, DWLP_USER);

	return pThis ? pThis->DlgProc(msg, wParam, lParam) : FALSE;
}

INT_PTR VDVFilterDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////

VDVFilterBase::VDVFilterBase() {
}

VDVFilterBase::~VDVFilterBase() {
}

void VDVFilterBase::SetHooks(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	this->fa = fa;
	this->ff = ff;
}

///////////////////////////////////////////////////////////////////////////

bool VDVFilterBase::Init() {
	return true;
}

void VDVFilterBase::Start() {
}

void VDVFilterBase::End() {
}

bool VDVFilterBase::Configure(VDXHWND hwnd) {
	return hwnd != NULL;
}

void VDVFilterBase::GetSettingString(char *buf, int maxlen) {
}

void VDVFilterBase::GetScriptString(char *buf, int maxlen) {
}

int VDVFilterBase::Serialize(char *buf, int maxbuf) {
	return 0;
}

int VDVFilterBase::Deserialize(const char *buf, int maxbuf) {
	return 0;
}

///////////////////////////////////////////////////////////////////////////

void __cdecl VDVFilterBase::FilterDeinit   (VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	reinterpret_cast<VDVFilterBase *>(fa->filter_data)->~VDVFilterBase();
}

int  __cdecl VDVFilterBase::FilterRun      (const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDVFilterBase *pThis = reinterpret_cast<VDVFilterBase *>(fa->filter_data);

	pThis->fa		= const_cast<VDXFilterActivation *>(fa);

	return !pThis->Run();
}

long __cdecl VDVFilterBase::FilterParam    (VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDVFilterBase *pThis = reinterpret_cast<VDVFilterBase *>(fa->filter_data);

	pThis->fa		= fa;

	return pThis->GetParams();
}

int  __cdecl VDVFilterBase::FilterConfig   (VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hwnd) {
	VDVFilterBase *pThis = reinterpret_cast<VDVFilterBase *>(fa->filter_data);

	pThis->fa		= fa;

	return !pThis->Configure(hwnd);
}

int  __cdecl VDVFilterBase::FilterStart    (VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDVFilterBase *pThis = reinterpret_cast<VDVFilterBase *>(fa->filter_data);

	pThis->fa		= fa;

	pThis->Start();
	return 0;
}

int  __cdecl VDVFilterBase::FilterEnd      (VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDVFilterBase *pThis = reinterpret_cast<VDVFilterBase *>(fa->filter_data);

	pThis->fa		= fa;

	pThis->End();
	return 0;
}

bool __cdecl VDVFilterBase::FilterScriptStr(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen) {
	VDVFilterBase *pThis = reinterpret_cast<VDVFilterBase *>(fa->filter_data);

	pThis->fa		= fa;

	pThis->GetScriptString(buf, buflen);

	return true;
}

void __cdecl VDVFilterBase::FilterString2  (const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxlen) {
	VDVFilterBase *pThis = reinterpret_cast<VDVFilterBase *>(fa->filter_data);

	pThis->fa		= const_cast<VDXFilterActivation *>(fa);

	pThis->GetSettingString(buf, maxlen);
}

int  __cdecl VDVFilterBase::FilterSerialize    (VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxbuf) {
	VDVFilterBase *pThis = reinterpret_cast<VDVFilterBase *>(fa->filter_data);

	pThis->fa		= fa;

	return pThis->Serialize(buf, maxbuf);
}

void __cdecl VDVFilterBase::FilterDeserialize  (VDXFilterActivation *fa, const VDXFilterFunctions *ff, const char *buf, int maxbuf) {
	VDVFilterBase *pThis = reinterpret_cast<VDVFilterBase *>(fa->filter_data);

	pThis->fa		= fa;

	pThis->Deserialize(buf, maxbuf);
}

