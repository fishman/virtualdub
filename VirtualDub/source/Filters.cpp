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

#include <stdarg.h>
#include <malloc.h>
#include <windows.h>

#include <ctype.h>

#ifdef _MSC_VER
	#include <intrin.h>
#endif

#include "resource.h"
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/filesys.h>
#include <vd2/system/protscope.h>
#include <vd2/system/refcount.h>
#include <vd2/system/VDString.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDLib/Dialog.h>
#include "plugins.h"

#include "filter.h"
#include "filters.h"
#include <vd2/plugin/vdplugin.h>

List			g_listFA;
FilterSystem	filters;

///////////////////////////////////////////////////////////////////////////
//
//	FilterDefinitionInstance
//
///////////////////////////////////////////////////////////////////////////

FilterDefinitionInstance::FilterDefinitionInstance(VDExternalModule *pfm)
	: mpExtModule(pfm)
	, mRefCount(0)
{
}

FilterDefinitionInstance::~FilterDefinitionInstance() {
	VDASSERT(mRefCount==0);
}

void FilterDefinitionInstance::Assign(const FilterDefinition& def, int len) {
	memset(&mDef, 0, sizeof mDef);
	memcpy(&mDef, &def, std::min<size_t>(sizeof mDef, len));

	mName			= def.name;
	mAuthor			= def.maker ? def.maker : "(internal)";
	mDescription	= def.desc;

	mDef._module	= const_cast<VDXFilterModule *>(&mpExtModule->GetFilterModuleInfo());
}

const FilterDefinition& FilterDefinitionInstance::Attach() {
	if (mpExtModule)
		mpExtModule->Lock();

	++mRefCount;

	return mDef;
}

void FilterDefinitionInstance::Detach() {
	VDASSERT(mRefCount.dec() >= 0);

	if (mpExtModule)
		mpExtModule->Unlock();
}

///////////////////////////////////////////////////////////////////////////
//
//	Filter global functions
//
///////////////////////////////////////////////////////////////////////////

static ListAlloc<FilterDefinitionInstance>	g_filterDefs;

FilterDefinition *FilterAdd(VDXFilterModule *fm, FilterDefinition *pfd, int fd_len) {
	VDExternalModule *pExtModule = VDGetExternalModuleByFilterModule(fm);

	if (pExtModule) {
		List2<FilterDefinitionInstance>::fwit it2(g_filterDefs.begin());

		for(; it2; ++it2) {
			FilterDefinitionInstance& fdi = *it2;

			if (fdi.GetModule() == pExtModule && fdi.GetName() == pfd->name) {
				fdi.Assign(*pfd, fd_len);
				return const_cast<FilterDefinition *>(&fdi.GetDef());
			}
		}

		vdautoptr<FilterDefinitionInstance> pfdi(new FilterDefinitionInstance(pExtModule));
		pfdi->Assign(*pfd, fd_len);

		const FilterDefinition *pfdi2 = &pfdi->GetDef();
		g_filterDefs.AddTail(pfdi.release());

		return const_cast<FilterDefinition *>(pfdi2);
	}

	return NULL;
}

void FilterAddBuiltin(const FilterDefinition *pfd) {
	vdautoptr<FilterDefinitionInstance> fdi(new FilterDefinitionInstance(NULL));
	fdi->Assign(*pfd, sizeof(FilterDefinition));

	g_filterDefs.AddTail(fdi.release());
}

void FilterRemove(FilterDefinition *fd) {
#if 0
	List2<FilterDefinitionInstance>::fwit it(g_filterDefs.begin());

	for(; it; ++it) {
		FilterDefinitionInstance& fdi = *it;

		if (&fdi.GetDef() == fd) {
			fdi.Remove();
			delete &fdi;
			return;
		}
	}
#endif
}

void FilterEnumerateFilters(std::list<FilterBlurb>& blurbs) {
	List2<FilterDefinitionInstance>::fwit it(g_filterDefs.begin());

	for(; it; ++it) {
		FilterDefinitionInstance& fd = *it;

		blurbs.push_back(FilterBlurb());
		FilterBlurb& fb = blurbs.back();

		fb.key			= &fd;
		fb.name			= fd.GetName();
		fb.author		= fd.GetAuthor();
		fb.description	= fd.GetDescription();
	}
}

//////////////////

class VDUIDialogFilterSingleValue : public VDDialogFrameW32 {
public:
	VDUIDialogFilterSingleValue(sint32 value, sint32 minValue, sint32 maxValue, IVDXFilterPreview2 *ifp2, const wchar_t *title, void (*cb)(long, void *), void *cbdata);

	sint32 GetValue() const { return mValue; }

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	void OnHScroll(uint32 id, int code);
	bool OnCommand(uint32 id, uint32 extcode);

	void UpdateSettingsString();

	sint32 mValue;
	sint32 mMinValue;
	sint32 mMaxValue;
	IVDXFilterPreview2 *mifp2;

	void (*mpUpdateFunction)(long value, void *data);
	void *mpUpdateFunctionData;

	const wchar_t *mpTitle;
};

VDUIDialogFilterSingleValue::VDUIDialogFilterSingleValue(sint32 value, sint32 minValue, sint32 maxValue, IVDXFilterPreview2 *ifp2, const wchar_t *title, void (*cb)(long, void *), void *cbdata)
	: VDDialogFrameW32(IDD_FILTER_SINGVAR)
	, mValue(value)
	, mMinValue(minValue)
	, mMaxValue(maxValue)
	, mifp2(ifp2)
	, mpUpdateFunction(cb)
	, mpUpdateFunctionData(cbdata)
	, mpTitle(title)
{
}

bool VDUIDialogFilterSingleValue::OnLoaded() {
	TBSetRange(IDC_SLIDER, mMinValue, mMaxValue);
	UpdateSettingsString();

	if (mifp2) {
		VDZHWND hwndPreviewButton = GetControl(IDC_PREVIEW);

		if (hwndPreviewButton)
			mifp2->InitButton((VDXHWND)hwndPreviewButton);
	}

	return VDDialogFrameW32::OnLoaded();
}

void VDUIDialogFilterSingleValue::OnDataExchange(bool write) {
	if (!write)
		TBSetValue(IDC_SLIDER, mValue);
}

void VDUIDialogFilterSingleValue::OnHScroll(uint32 id, int code) {
	if (id == IDC_SLIDER) {
		int v = TBGetValue(id);

		if (v != mValue) {
			mValue = v;

			UpdateSettingsString();

			if (mpUpdateFunction)
				mpUpdateFunction(mValue, mpUpdateFunctionData);

			if (mifp2)
				mifp2->RedoFrame();
		}
	}
}

bool VDUIDialogFilterSingleValue::OnCommand(uint32 id, uint32 extcode) {
	if (id == IDC_PREVIEW) {
		if (mifp2)
			mifp2->Toggle((VDXHWND)mhdlg);
		return true;
	}

	return false;
}

void VDUIDialogFilterSingleValue::UpdateSettingsString() {
	SetControlTextF(IDC_SETTING, L"%d", mValue);
}

LONG FilterGetSingleValue(HWND hWnd, LONG cVal, LONG lMin, LONG lMax, char *title, IVDXFilterPreview2 *ifp2, void (*pUpdateFunction)(long value, void *data), void *pUpdateFunctionData) {
	VDStringW tbuf;
	tbuf.sprintf(L"Filter: %hs", title);

	VDUIDialogFilterSingleValue dlg(cVal, lMin, lMax, ifp2, tbuf.c_str(), pUpdateFunction, pUpdateFunctionData);

	if (dlg.ShowDialog((VDGUIHandle)hWnd))
		return dlg.GetValue();

	return cVal;
}
