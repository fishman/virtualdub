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

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "VideoSource.h"
#include <vd2/system/error.h>
#include <vd2/system/list.h>
#include <vd2/system/registry.h>
#include <vd2/system/strutil.h>
#include <vd2/Dita/services.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDLib/Dialog.h>

#include "plugins.h"
#include "resource.h"
#include "oshelper.h"
#include "PositionControl.h"
#include "ClippingControl.h"
#include "gui.h"
#include "FilterPreview.h"

#include "filtdlg.h"
#include "filters.h"
#include "FilterInstance.h"
#include "FilterFrameVideoSource.h"

extern HINSTANCE g_hInst;
extern const char g_szError[];
extern VDXFilterFunctions g_VDFilterCallbacks;

extern vdrefptr<IVDVideoSource> inputVideo;

enum {
	kFileDialog_LoadPlugin		= 'plug',
};

//////////////////////////////

bool VDShowFilterClippingDialog(VDGUIHandle hParent, FilterInstance *pFiltInst, List *pFilterList);
void FilterLoadFilter(HWND hWnd);

///////////////////////////////////////////////////////////////////////////
//
//	add filter dialog
//
///////////////////////////////////////////////////////////////////////////


class VDDialogFilterListW32 : public VDDialogBaseW32 {
public:
	inline VDDialogFilterListW32() : VDDialogBaseW32(IDD_FILTER_LIST) {}

	FilterDefinitionInstance *Activate(VDGUIHandle hParent);

protected:
	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void ReinitDialog();

	HWND						mhwndList;
	FilterDefinitionInstance	*mpFilterDefInst;
	std::list<FilterBlurb>		mFilterList;
};

FilterDefinitionInstance *VDDialogFilterListW32::Activate(VDGUIHandle hParent) {
	return ActivateDialog(hParent) ? mpFilterDefInst : NULL;
}

void VDDialogFilterListW32::ReinitDialog() {
	static INT tabs[]={ 175 };

	mhwndList = GetDlgItem(mhdlg, IDC_FILTER_LIST);

	mFilterList.clear();
	FilterEnumerateFilters(mFilterList);

	int index;

	SendMessage(mhwndList, LB_SETTABSTOPS, 1, (LPARAM)tabs);
	SendMessage(mhwndList, LB_RESETCONTENT, 0, 0);

	for(std::list<FilterBlurb>::const_iterator it(mFilterList.begin()), itEnd(mFilterList.end()); it!=itEnd; ++it) {
		const FilterBlurb& fb = *it;
		char buf[256];

		wsprintf(buf,"%s\t%s", fb.name.c_str(), fb.author.c_str());
		index = SendMessage(mhwndList, LB_ADDSTRING, 0, (LPARAM)buf);
		SendMessage(mhwndList, LB_SETITEMDATA, (WPARAM)index, (LPARAM)&fb);
	}

	SendMessage(mhdlg, WM_NEXTDLGCTL, (WPARAM)mhwndList, TRUE);
}

INT_PTR VDDialogFilterListW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			ReinitDialog();
            return FALSE;

        case WM_COMMAND:
			switch(HIWORD(wParam)) {
			case LBN_SELCANCEL:
				SendMessage(GetDlgItem(mhdlg, IDC_FILTER_INFO), WM_SETTEXT, 0, (LPARAM)"");
				break;
			case LBN_SELCHANGE:
				{
					int index;

					if (LB_ERR != (index = SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0))) {
						const FilterBlurb& fb = *(FilterBlurb *)SendMessage((HWND)lParam, LB_GETITEMDATA, (WPARAM)index, 0);

						SendMessage(GetDlgItem(mhdlg, IDC_FILTER_INFO), WM_SETTEXT, 0, (LPARAM)fb.description.c_str());
					}
				}
				break;
			case LBN_DBLCLK:
				SendMessage(mhdlg, WM_COMMAND, MAKELONG(IDOK, BN_CLICKED), (LPARAM)GetDlgItem(mhdlg, IDOK));
				break;
			default:
				switch(LOWORD(wParam)) {
				case IDOK:
					{
						int index;

						if (LB_ERR != (index = SendMessage(mhwndList, LB_GETCURSEL, 0, 0))) {
							const FilterBlurb& fb = *(FilterBlurb *)SendMessage(mhwndList, LB_GETITEMDATA, (WPARAM)index, 0);

							mpFilterDefInst = fb.key;

							End(true);
						}
					}
					return TRUE;
				case IDCANCEL:
					End(false);
					return TRUE;
				case IDC_LOAD:
					FilterLoadFilter(mhdlg);
					ReinitDialog();
					return TRUE;
				}
			}
            break;
    }
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////
//
//	Filter list dialog
//
///////////////////////////////////////////////////////////////////////////

class VDVideoFiltersDialog : public VDDialogBaseW32 {
public:
	VDVideoFiltersDialog();
	
	void Init(IVDVideoSource *pVS, VDPosition initialTime);
	void Init(int w, int h, int format, const VDFraction& rate, sint64 length, VDPosition initialTime);

	VDVideoFiltersDialogResult GetResult() const { return mResult; }

protected:
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void OnInit();
	void OnLVGetDispInfo(NMLVDISPINFO& dispInfo);
	void OnLVItemChanged(const NMLISTVIEW& nmlv);

	void MakeFilterList(List& list);
	void EnableConfigureBox(HWND hdlg, int index = -1);
	void RedoFilters();
	void RelayoutFilterList();

	VDFraction	mOldFrameRate;
	sint64		mOldFrameCount;
	int			mInputWidth;
	int			mInputHeight;
	int			mInputFormat;
	VDFraction	mInputRate;
	VDFraction	mInputPixelAspect;
	sint64		mInputLength;
	VDTime		mInitialTimeUS;
	IVDVideoSource	*mpVS;

	bool		mbShowFormats;
	bool		mbShowAspectRatios;
	bool		mbShowFrameRates;

	int			mFilterEnablesUpdateLock;

	HWND		mhwndList;

	VDStringA	mTempStr;

	typedef vdfastvector<FilterInstance *> Filters; 
	Filters	mFilters;

	VDVideoFiltersDialogResult mResult;
};

VDVideoFiltersDialog::VDVideoFiltersDialog()
	: VDDialogBaseW32(IDD_FILTERS)
	, mInputWidth(320)
	, mInputHeight(240)
	, mInputFormat(nsVDPixmap::kPixFormat_XRGB8888)
	, mInputRate(30, 1)
	, mInputPixelAspect(1, 1)
	, mInputLength(100)
	, mInitialTimeUS(-1)
	, mpVS(NULL)
	, mbShowFormats(false)
	, mbShowAspectRatios(false)
	, mbShowFrameRates(false)
	, mFilterEnablesUpdateLock(0)
{
	mResult.mbDialogAccepted = false;
	mResult.mbChangeDetected = false;
	mResult.mbRescaleRequested = false;
}

void VDVideoFiltersDialog::Init(IVDVideoSource *pVS, VDPosition initialTime) {
	IVDStreamSource *pSS = pVS->asStream();
	const VDPixmap& px = pVS->getTargetFormat();

	mpVS			= pVS;
	mInputWidth		= px.w;
	mInputHeight	= px.h;
	mInputFormat	= px.format;
	mInputRate		= pSS->getRate();
	mInputPixelAspect = pVS->getPixelAspectRatio();
	mInputLength	= pSS->getLength();
	mInitialTimeUS	= initialTime;
}

void VDVideoFiltersDialog::Init(int w, int h, int format, const VDFraction& rate, sint64 length, VDPosition initialTime) {
	mpVS			= NULL;
	mInputWidth		= w;
	mInputHeight	= h;
	mInputFormat	= format;
	mInputRate		= rate;
	mInputLength	= length;
	mInitialTimeUS	= initialTime;
}

INT_PTR VDVideoFiltersDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg)
    {
        case WM_INITDIALOG:
			OnInit();
            return FALSE;

		case WM_NOTIFY:
			{
				const NMHDR& hdr = *(const NMHDR *)lParam;
				switch(hdr.idFrom) {
				case IDC_FILTER_LIST:
					switch(hdr.code) {
					case LVN_ITEMCHANGED:
						OnLVItemChanged(*(NMLISTVIEW *)lParam);
						EnableConfigureBox(mhdlg);
						return TRUE;
					case LVN_GETDISPINFO:
						OnLVGetDispInfo(*(NMLVDISPINFO *)lParam);
						return TRUE;
					case NM_DBLCLK:
						SendMessage(mhdlg, WM_COMMAND, MAKELONG(IDC_CONFIGURE, BN_CLICKED), (LPARAM)GetDlgItem(mhdlg, IDC_CONFIGURE));
						break;
					}
					break;
				}
			}
			break;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_ADD:
				if (FilterDefinitionInstance *fdi = VDDialogFilterListW32().Activate((VDGUIHandle)mhdlg)) {
					try {
						FilterInstance *fa = new FilterInstance(fdi);
						fa->AddRef();

						mFilters.push_back(fa);

						LVITEM item;
						item.iItem		= mFilters.size();
						item.iSubItem	= 0;
						item.mask		= LVIF_TEXT;
						item.pszText	= LPSTR_TEXTCALLBACK;

						// Note that this call would end up disabling the filter instance if
						// we didn't have an update lock around it.
						++mFilterEnablesUpdateLock;
						int index = ListView_InsertItem(mhwndList, &item);
						--mFilterEnablesUpdateLock;

						item.iItem		= index;
						item.iSubItem	= 1;
						item.mask		= LVIF_TEXT;
						ListView_SetItem(mhwndList, &item);

						item.iSubItem	= 2;
						ListView_SetItem(mhwndList, &item);

						item.iSubItem	= 3;
						ListView_SetItem(mhwndList, &item);

						ListView_SetCheckState(mhwndList, index, TRUE);
						fa->SetEnabled(true);

						RedoFilters();

						if (fa->IsConfigurable()) {
							List list;
							bool fRemove;

							MakeFilterList(list);

							vdrefptr<IVDVideoFilterPreviewDialog> fp;
							if (VDCreateVideoFilterPreviewDialog(mpVS ? &list : NULL, fa, ~fp)) {
								if (mInitialTimeUS >= 0)
									fp->SetInitialTime(mInitialTimeUS);

								fRemove = !fa->Configure((VDXHWND)mhdlg, fp->AsIVDXFilterPreview2());
							}

							fp = NULL;

							if (fRemove) {
								mFilters.pop_back();
								fa->Release();
								ListView_DeleteItem(mhwndList, index);
								break;
							}
						}

						RedoFilters();

						ListView_SetItemState(mhwndList, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

						EnableConfigureBox(mhdlg, index);
					} catch(const MyError& e) {
						e.post(mhdlg, g_szError);
					}
				}
				break;

			case IDC_DELETE:
				{
					int index = ListView_GetNextItem(mhwndList, -1, LVNI_SELECTED);

					if ((unsigned)index < mFilters.size()) {
						FilterInstance *fa = mFilters[index];
						mFilters.erase(mFilters.begin() + index);

						fa->Release();
						
						// We need to disable check updates around the delete to avoid getting the filter
						// enables scrambled.
						++mFilterEnablesUpdateLock;
						ListView_DeleteItem(mhwndList, index);
						--mFilterEnablesUpdateLock;

						if ((unsigned)index < mFilters.size())
							ListView_SetItemState(mhwndList, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

						RedoFilters();
					}
				}
				break;

			case IDC_CONFIGURE:
				{
					int index = ListView_GetNextItem(mhwndList, -1, LVNI_SELECTED);

					if ((unsigned)index < mFilters.size()) {
						FilterInstance *fa = mFilters[index];

						if (fa->IsConfigurable()) {
							List list;

							RedoFilters();
							MakeFilterList(list);

							vdrefptr<IVDVideoFilterPreviewDialog> fp;
							if (VDCreateVideoFilterPreviewDialog(mpVS ? &list : NULL, fa, ~fp)) {
								if (mInitialTimeUS >= 0)
									fp->SetInitialTime(mInitialTimeUS);

								fa->Configure((VDXHWND)mhdlg, fp->AsIVDXFilterPreview2());
							}

							fp = NULL;

							RedoFilters();

							ListView_SetItemState(mhwndList, -1, 0, LVIS_SELECTED);
							if (index >= 0)
								ListView_SetItemState(mhwndList, index, LVIS_SELECTED, LVIS_SELECTED);
						}
					}
				}
				break;

			case IDC_CLIPPING:
				{
					int index = ListView_GetNextItem(mhwndList, -1, LVNI_SELECTED);

					if ((unsigned)index < mFilters.size()) {
						RedoFilters();
						FilterInstance *fa = mFilters[index];

						filters.DeinitFilters();
						filters.DeallocateBuffers();

						List filterList;
						MakeFilterList(filterList);

						if (VDShowFilterClippingDialog((VDGUIHandle)mhdlg, fa, &filterList)) {
							RedoFilters();

							ListView_SetItemState(mhwndList, -1, 0, LVIS_SELECTED);
							if (index >= 0)
								ListView_SetItemState(mhwndList, index, LVIS_SELECTED, LVIS_SELECTED);
						}
					}
				}
				break;

			case IDC_BLENDING:
				{
					int index = ListView_GetNextItem(mhwndList, -1, LVNI_SELECTED);

					if ((unsigned)index < mFilters.size()) {
						FilterInstance *fa = mFilters[index];

						if (fa->GetAlphaParameterCurve()) {
							fa->SetAlphaParameterCurve(NULL);
						} else {
							VDParameterCurve *curve = new_nothrow VDParameterCurve();
							if (curve) {
								curve->SetYRange(0.0f, 1.0f);
								fa->SetAlphaParameterCurve(curve);
							}
						}

						RedoFilters();
					}
				}
				break;

			case IDC_MOVEUP:
				{
					int index = ListView_GetNextItem(mhwndList, -1, LVNI_SELECTED);

					if (index > 0 && (unsigned)index < mFilters.size()) {
						std::swap(mFilters[index - 1], mFilters[index]);

						for(int i=-1; i<=0; ++i)
							ListView_SetCheckState(mhwndList, index + i, mFilters[index + i]->IsEnabled());

						RedoFilters();

						ListView_SetItemState(mhwndList, index - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
					}
				}
				break;

			case IDC_MOVEDOWN:
				{
					int index = ListView_GetNextItem(mhwndList, -1, LVNI_SELECTED);
					int count = mFilters.size();

					if (index >= 0 && index < count - 1) {
						std::swap(mFilters[index], mFilters[index + 1]);

						for(int i=0; i<=1; ++i)
							ListView_SetCheckState(mhwndList, index + i, mFilters[index + i]->IsEnabled());

						RedoFilters();

						ListView_SetItemState(mhwndList, index + 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
					}
				}
				break;

			case IDOK:
				// We must force filters to stop before we muck with the global list... in case
				// the pane refresh restarted them.
				filters.DeinitFilters();
				filters.DeallocateBuffers();

				while(!g_listFA.IsEmpty()) {
					FilterInstance *fi = static_cast<FilterInstance *>(g_listFA.RemoveTail());

					fi->Release();
				}

				MakeFilterList(g_listFA);

				mFilters.clear();

				mResult.mOldFrameRate		= mOldFrameRate;
				mResult.mOldFrameCount		= mOldFrameCount;
				mResult.mNewFrameRate		= filters.GetOutputFrameRate();
				mResult.mNewFrameCount		= filters.GetOutputFrameCount();

				mResult.mbRescaleRequested = false;
				mResult.mbChangeDetected = false;

				if (mResult.mOldFrameRate != mResult.mNewFrameRate || mResult.mOldFrameCount != mResult.mNewFrameCount) {
					mResult.mbChangeDetected = true;
					mResult.mbRescaleRequested = true;
				}

				mResult.mbDialogAccepted = true;
				End(true);
				return TRUE;
			case IDCANCEL:
				// We must force filters to stop before we muck with the global list... in case
				// the pane refresh restarted them.
				filters.DeinitFilters();
				filters.DeallocateBuffers();

				while(!mFilters.empty()) {
					FilterInstance *fi = mFilters.back();
					mFilters.pop_back();

					fi->Release();
				}

				mResult.mbDialogAccepted = true;
				End(false);
				return TRUE;

			case IDC_SHOWIMAGEFORMATS:
				{
					bool selected = 0 != IsDlgButtonChecked(mhdlg, IDC_SHOWIMAGEFORMATS);

					if (mbShowFormats != selected) {
						mbShowFormats = selected;

						VDRegistryAppKey key("Dialogs\\Filters");
						key.setBool("Show formats", mbShowFormats);

						RelayoutFilterList();
					}
				}
				return TRUE;

			case IDC_SHOWASPECTRATIOS:
				{
					bool selected = 0 != IsDlgButtonChecked(mhdlg, IDC_SHOWASPECTRATIOS);

					if (mbShowAspectRatios != selected) {
						mbShowAspectRatios = selected;

						VDRegistryAppKey key("Dialogs\\Filters");
						key.setBool("Show aspect ratios", mbShowAspectRatios);

						RelayoutFilterList();
					}
				}
				return TRUE;

			case IDC_SHOWFRAMERATES:
				{
					bool selected = 0 != IsDlgButtonChecked(mhdlg, IDC_SHOWFRAMERATES);

					if (mbShowFrameRates != selected) {
						mbShowFrameRates = selected;

						VDRegistryAppKey key("Dialogs\\Filters");
						key.setBool("Show frame rates", mbShowFrameRates);

						RelayoutFilterList();
					}
				}
				return TRUE;
			}
            break;
    }
    return FALSE;
}

void VDVideoFiltersDialog::OnInit() {
	{
		VDRegistryAppKey key("Dialogs\\Filters");
		mbShowFormats = key.getBool("Show formats", mbShowFormats);
		mbShowAspectRatios = key.getBool("Show aspect ratios", mbShowAspectRatios);
		mbShowFrameRates = key.getBool("Show frame rates", mbShowFrameRates);
	}

	CheckDlgButton(mhdlg, IDC_SHOWIMAGEFORMATS, mbShowFormats ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_SHOWASPECTRATIOS, mbShowAspectRatios ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_SHOWFRAMERATES, mbShowFrameRates ? BST_CHECKED : BST_UNCHECKED);

	mhwndList = GetDlgItem(mhdlg, IDC_FILTER_LIST);
	mFilterEnablesUpdateLock = 0;

	ListView_SetExtendedListViewStyle(mhwndList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

	LVCOLUMN col;

	col.mask	= LVCF_TEXT | LVCF_WIDTH;
	col.cx		= 25;
	col.pszText	= "";
	ListView_InsertColumn(mhwndList, 0, &col);

	col.cx		= 50;
	col.pszText	= "Input";
	ListView_InsertColumn(mhwndList, 1, &col);

	col.cx		= 50;
	col.pszText	= "Output";
	ListView_InsertColumn(mhwndList, 2, &col);

	col.cx		= 200;
	col.pszText	= "Filter";
	ListView_InsertColumn(mhwndList, 3, &col);

	FilterInstance *fa_list, *fa;

	fa_list = (FilterInstance *)g_listFA.tail.next;

	while(fa_list->next) {
		try {
			fa = fa_list->Clone();
			fa->AddRef();

			mFilters.push_back(fa);

			LVITEM item;
			item.iItem		= mFilters.size();
			item.iSubItem	= 0;
			item.mask		= LVIF_TEXT | LVIF_STATE;
			item.state		= INDEXTOSTATEIMAGEMASK(1);
			item.pszText	= LPSTR_TEXTCALLBACK;
			int index = ListView_InsertItem(mhwndList, &item);

			ListView_SetCheckState(mhwndList, index, fa->IsEnabled());

			item.iSubItem	= 1;
			item.mask		= LVIF_TEXT;
			item.pszText	= LPSTR_TEXTCALLBACK;
			ListView_SetItem(mhwndList, &item);

			item.iSubItem	= 2;
			item.pszText	= LPSTR_TEXTCALLBACK;
			ListView_SetItem(mhwndList, &item);

			item.iSubItem	= 3;
			item.pszText	= LPSTR_TEXTCALLBACK;
			ListView_SetItem(mhwndList, &item);

		} catch(const MyError&) {
			// bleah!  should really do something...
		}

		fa_list = (FilterInstance *)fa_list->next;
	}

	RedoFilters();

	mOldFrameRate	= filters.GetOutputFrameRate();
	mOldFrameCount	= filters.GetOutputFrameCount();

	SetFocus(mhwndList);
}

void VDVideoFiltersDialog::OnLVGetDispInfo(NMLVDISPINFO& dispInfo) {
	LVITEM& item = dispInfo.item;

	if (item.mask & LVIF_TEXT) {
		if ((unsigned)item.iItem < mFilters.size()) {
			const FilterInstance *fi = mFilters[item.iItem];

			switch(item.iSubItem) {
			case 0:
				if (!fi->IsEnabled())
					item.pszText[0] = 0;
				else
					_snprintf(item.pszText, item.cchTextMax, "%s%s"
								,fi->GetAlphaParameterCurve() ? "[B] " : ""
								,fi->IsConversionRequired() ? "[C] " : fi->IsAlignmentRequired() ? "[A]" : ""
						);
				break;
			case 1:
			case 2:
				{
					const VFBitmapInternal& fbm = (item.iSubItem == 2 ? fi->mRealDst : fi->mRealSrc);
					const VDPixmapLayout& layout = fbm.mPixmapLayout;

					if (!fi->IsEnabled()) {
						vdstrlcpy(item.pszText, "-", item.cchTextMax);
					} else {
						mTempStr.sprintf("%ux%u", layout.w, layout.h);

						if (mbShowFormats) {
							const char *const kFormatNames[]={
								"?",
								"P1",
								"P2",
								"P4",
								"P8",
								"RGB15",
								"RGB16",
								"RGB24",
								"RGB32",
								"Y8",
								"UYVY",
								"YUY2",
								"YUV",
								"YV24",
								"YV16",
								"YV12",
								"YUV411",
								"YVU9",
								"YV16C",
								"YV12C",
								"YUV422-16F",
								"V210",
								"HDYC",
								"NV12",
							};

							VDASSERTCT(sizeof(kFormatNames)/sizeof(kFormatNames[0]) == nsVDPixmap::kPixFormat_Max_Standard);

							mTempStr.append_sprintf(" (%s)", kFormatNames[layout.format]);
						}

						if (mbShowAspectRatios && item.iSubItem == 2) {
							if (fbm.mAspectRatioLo) {
								VDFraction reduced = VDFraction(fbm.mAspectRatioHi, fbm.mAspectRatioLo).reduce();

								if (reduced.getLo() < 1000)
									mTempStr.append_sprintf(" (%u:%u)", reduced.getHi(), reduced.getLo());
								else
									mTempStr.append_sprintf(" (%.4g)", reduced.asDouble());
							} else
								mTempStr += " (?)";
						}

						if (mbShowFrameRates && item.iSubItem == 2) {
							mTempStr.append_sprintf(" (%.3g fps)", (double)fbm.mFrameRateHi / (double)fbm.mFrameRateLo);
						}

						vdstrlcpy(item.pszText, mTempStr.c_str(), item.cchTextMax);
					}
				}
				break;
			case 3:
				{
					VDStringA blurb;
					VDStringA settings;

					blurb = fi->GetName();

					if (fi->GetSettingsString(settings))
						blurb += settings;

					vdstrlcpy(item.pszText, blurb.c_str(), item.cchTextMax);
				}
				break;
			}
		}

		if (item.cchTextMax)
			item.pszText[item.cchTextMax - 1] = 0;
	}
}

void VDVideoFiltersDialog::OnLVItemChanged(const NMLISTVIEW& nmlv) {
	if ((unsigned)nmlv.iItem >= mFilters.size())
		return;

	FilterInstance *fi = mFilters[nmlv.iItem];

	if (nmlv.uChanged & LVIF_STATE) {
		if (!mFilterEnablesUpdateLock) {
			// We fetch the item state because uNewState seems hosed when ListView_SetItemState() is called
			// with a partial mask.
			bool enabled = ListView_GetItemState(mhwndList, nmlv.iItem, LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(2);

			if (enabled != fi->IsEnabled()) {
				fi->SetEnabled(enabled);
				RedoFilters();
			}
		}
	}
}

void VDVideoFiltersDialog::MakeFilterList(List& list) {
	VDASSERT(list.IsEmpty());

	// have to do this since the filter list is intrusive
	filters.DeinitFilters();
	filters.DeallocateBuffers();

	for(Filters::const_iterator it(mFilters.begin()), itEnd(mFilters.end()); it != itEnd; ++it) {
		FilterInstance *fi = *it;

		list.AddHead(fi);
	}
}

void VDVideoFiltersDialog::EnableConfigureBox(HWND hdlg, int index) {
	if (index < 0)
		index = ListView_GetNextItem(mhwndList, -1, LVNI_SELECTED);

	if ((unsigned)index < mFilters.size()) {
		FilterInstance *fa = mFilters[index];

		EnableWindow(GetDlgItem(hdlg, IDC_CONFIGURE), fa->IsConfigurable());
		EnableWindow(GetDlgItem(hdlg, IDC_CLIPPING), TRUE);
		EnableWindow(GetDlgItem(hdlg, IDC_BLENDING), TRUE);
	} else {
		EnableWindow(GetDlgItem(hdlg, IDC_CONFIGURE), FALSE);
		EnableWindow(GetDlgItem(hdlg, IDC_CLIPPING), FALSE);
		EnableWindow(GetDlgItem(hdlg, IDC_BLENDING), FALSE);
	}
}

void VDVideoFiltersDialog::RedoFilters() {
	List listFA;

	MakeFilterList(listFA);

	if (mInputFormat) {
		try {
			filters.prepareLinearChain(&listFA, mInputWidth, mInputHeight, mInputFormat, mInputRate, mInputLength, mInputPixelAspect);
		} catch(const MyError&) {
			// eat error
		}
	}

	RelayoutFilterList();
}

void VDVideoFiltersDialog::RelayoutFilterList() {
	if (!mFilters.empty()) {
		for(int i=0; i<4; ++i)
			ListView_SetColumnWidth(mhwndList, i, -2);

		ListView_RedrawItems(mhwndList, 0, mFilters.size() - 1);
	}
}

VDVideoFiltersDialogResult VDShowDialogVideoFilters(VDGUIHandle h, IVDVideoSource *pVS, VDPosition initialTime) {
	VDVideoFiltersDialog dlg;

	if (pVS)
		dlg.Init(pVS, initialTime);

	dlg.ActivateDialog(h);

	return dlg.GetResult();
}

VDVideoFiltersDialogResult VDShowDialogVideoFilters(VDGUIHandle hParent, int w, int h, int format, const VDFraction& rate, sint64 length, VDPosition initialTime) {
	VDVideoFiltersDialog dlg;

	dlg.Init(w, h, format, rate, length, initialTime);
	dlg.ActivateDialog(hParent);

	return dlg.GetResult();
}

///////////////////////////////////////////////////////////////////////////
//
//	filter crop dialog
//
///////////////////////////////////////////////////////////////////////////

class VDFilterClippingDialog : public VDDialogFrameW32 {
public:
	VDFilterClippingDialog(FilterInstance *pFiltInst, List *pFilterList);

protected:
	void OnDataExchange(bool write);
	bool OnLoaded();

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void UpdateFrame(VDPosition pos);

	List			*mpFilterList;
	FilterInstance	*mpFilterInst;
	FilterSystem	mFilterSys;
	IVDClippingControl	*mpClipCtrl;
	IVDPositionControl	*mpPosCtrl;

	double			mFilterFramesToSourceFrames;
	double			mSourceFramesToFilterFrames;

	vdrefptr<VDFilterFrameVideoSource>	mpFrameSource;

	VDDialogResizerW32	mResizer;
};

VDFilterClippingDialog::VDFilterClippingDialog(FilterInstance *pFiltInst, List *pFilterList)
	: VDDialogFrameW32(IDD_FILTER_CLIPPING)
	, mpFilterList(pFilterList)
	, mpFilterInst(pFiltInst)
{
}

void VDFilterClippingDialog::OnDataExchange(bool write) {
	if (write) {
		vdrect32 r;
		mpClipCtrl->GetClipBounds(r);

		mpFilterInst->SetCrop(r.left, r.top, r.right, r.bottom, IsButtonChecked(IDC_CROP_PRECISE));
	} else {
		const vdrect32& r = mpFilterInst->GetCropInsets();
		mpClipCtrl->SetClipBounds(r);

		bool precise = mpFilterInst->IsPreciseCroppingEnabled();
		CheckButton(IDC_CROP_PRECISE, precise);
		CheckButton(IDC_CROP_FAST, !precise);
	}
}

bool VDFilterClippingDialog::OnLoaded()  {
	RECT rw, rc;

	// try to init filters
	mFilterFramesToSourceFrames = 1.0;
	mSourceFramesToFilterFrames = 1.0;

	if (mpFilterList && inputVideo) {
		IVDStreamSource *pVSS = inputVideo->asStream();
		const VDAVIBitmapInfoHeader *pbih2 = inputVideo->getDecompressedFormat();

		try {
			// halt the main filter system
			filters.DeinitFilters();
			filters.DeallocateBuffers();

			const VDPixmap& pxsrc = inputVideo->getTargetFormat();
			mFilterSys.prepareLinearChain(
					mpFilterList,
					pbih2->biWidth,
					abs(pbih2->biHeight),
					pxsrc.format,
					pVSS->getRate(),
					pVSS->getLength(),
					inputVideo->getPixelAspectRatio());

			mpFrameSource = new VDFilterFrameVideoSource;
			mpFrameSource->Init(inputVideo, mFilterSys.GetInputLayout());

			// start private filter system
			mFilterSys.initLinearChain(
					mpFilterList,
					mpFrameSource,
					pbih2->biWidth,
					abs(pbih2->biHeight),
					pxsrc.format,
					pxsrc.palette,
					pVSS->getRate(),
					pVSS->getLength(),
					inputVideo->getPixelAspectRatio());

			mFilterSys.ReadyFilters(VDXFilterStateInfo::kStatePreview);

			double srcRate = pVSS->getRate().asDouble();
			double dstRate = (double)mpFilterInst->mRealSrc.mFrameRateHi / (double)mpFilterInst->mRealSrc.mFrameRateLo;

			mFilterFramesToSourceFrames = srcRate / dstRate;
			mSourceFramesToFilterFrames = dstRate / srcRate;
		} catch(const MyError&) {
			// eat the error
		}
	}

	HWND hwndClipping = GetDlgItem(mhdlg, IDC_BORDERS);
	mpClipCtrl = VDGetIClippingControl((VDGUIHandle)hwndClipping);

	mpClipCtrl->SetBitmapSize(mpFilterInst->GetPreCropWidth(), mpFilterInst->GetPreCropHeight());

	mpPosCtrl = VDGetIPositionControlFromClippingControl((VDGUIHandle)hwndClipping);
	mpPosCtrl->SetAutoStep(true);
	mpPosCtrl->SetRange(0, mpFilterInst->mRealSrc.mFrameCount < 0 ? 1000 : mpFilterInst->mRealSrc.mFrameCount);
	mpPosCtrl->SetFrameRate(VDFraction(mpFilterInst->mRealSrc.mFrameRateHi, mpFilterInst->mRealSrc.mFrameRateLo));

	GetWindowRect(mhdlg, &rw);
	GetWindowRect(hwndClipping, &rc);
	const int origH = (rw.bottom - rw.top);
	int padW = (rw.right - rw.left) - (rc.right - rc.left);
	int padH = origH - (rc.bottom - rc.top);

	mpClipCtrl->AutoSize(padW, padH);

	GetWindowRect(hwndClipping, &rc);
	MapWindowPoints(NULL, mhdlg, (LPPOINT)&rc, 2);

	const int newH = (rc.bottom - rc.top) + padH;

	mResizer.Init(mhdlg);
	mResizer.Add(IDOK, VDDialogResizerW32::kBR);
	mResizer.Add(IDCANCEL, VDDialogResizerW32::kBR);
	mResizer.Add(IDC_STATIC_YCCCROP, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_CROP_PRECISE, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_CROP_FAST, VDDialogResizerW32::kBL);

	SetWindowPos(mhdlg, NULL, 0, 0, (rc.right - rc.left) + padW, newH, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOMOVE);
	SendMessage(mhdlg, DM_REPOSITION, 0, 0);

	mResizer.Relayout();

	// render first frame
	UpdateFrame(mpPosCtrl->GetPosition());

	return VDDialogFrameW32::OnLoaded();
}

INT_PTR VDFilterClippingDialog::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_BORDERS:
				{
					sint64 pos = -1;

					if (inputVideo) {
						switch(HIWORD(wParam)) {
							case PCN_KEYPREV:
								{
									pos = mpPosCtrl->GetPosition();
									sint64 srcPos = VDFloorToInt64(((double)pos + 0.5) * mFilterFramesToSourceFrames);

									for(;;) {
										sint64 newSrcPos = inputVideo->prevKey(srcPos);

										if (newSrcPos < 0) {
											pos = 0;
											break;
										}

										sint64 newPos = VDFloorToInt64(((double)newSrcPos + 0.5) * mSourceFramesToFilterFrames);
										if (pos != newPos) {
											pos = newPos;
											break;
										}

										srcPos = newSrcPos;
									}

									mpPosCtrl->SetPosition(pos);
								}
								break;
							case PCN_KEYNEXT:
								{
									pos = mpPosCtrl->GetPosition();
									sint64 srcPos = VDFloorToInt64(((double)pos + 0.5) * mFilterFramesToSourceFrames);

									for(;;) {
										sint64 newSrcPos = inputVideo->nextKey(srcPos);

										if (newSrcPos < 0) {
											pos = mpPosCtrl->GetRangeEnd();
											break;
										}

										sint64 newPos = VDFloorToInt64(((double)newSrcPos + 0.5) * mSourceFramesToFilterFrames);
										if (pos != newPos) {
											pos = newPos;
											break;
										}

										srcPos = newSrcPos;
									}

									mpPosCtrl->SetPosition(pos);
								}
								break;
						}

						UpdateFrame(mpPosCtrl->GetPosition());
					}
				}
				return TRUE;
			}
            break;

		case WM_NOTIFY:
			if (GetWindowLong(((NMHDR *)lParam)->hwndFrom, GWL_ID) == IDC_BORDERS) {
				VDPosition pos = guiPositionHandleNotify(lParam, mpPosCtrl);

				if (pos >= 0)
					UpdateFrame(pos);
			}
			break;

		case WM_MOUSEWHEEL:
			// Windows forwards all mouse wheel messages down to us, which we then forward
			// to the clipping control.  Obviously for this to be safe the position control
			// MUST eat the message, which it currently does.
			{
				HWND hwndClipping = GetDlgItem(mhdlg, IDC_BORDERS);
				if (hwndClipping)
					return SendMessage(hwndClipping, WM_MOUSEWHEEL, wParam, lParam);
			}
			break;
    }

	return VDDialogFrameW32::DlgProc(message, wParam, lParam);
}

void VDFilterClippingDialog::UpdateFrame(VDPosition pos) {
	if (mFilterSys.isRunning()) {
		bool success = false;

		sint64 frameCount = mFilterSys.GetOutputFrameCount();
		if (pos >= 0 && pos < frameCount) {
			try {
				vdrefptr<IVDFilterFrameClientRequest> req;

				IVDFilterFrameSource *src = mpFilterInst->GetSource();
				const VDPixmapLayout& srcLayout = src->GetOutputLayout();

				if (src->CreateRequest(pos, false, ~req)) {
					do {
						mpFrameSource->Run();
						mFilterSys.RunToCompletion();
					} while(!req->IsCompleted());

					if (req->IsSuccessful()) {
						VDPixmap px(VDPixmapFromLayout(srcLayout, req->GetResultBuffer()->GetBasePointer()));
						mpClipCtrl->BlitFrame(&px);
					} else {
						mpClipCtrl->BlitFrame(NULL);
					}

					success = true;
				}
			} catch(const MyError&) {
				// eat the error
			}
		}

		if (!success)
			mpClipCtrl->BlitFrame(NULL);
	} else
		guiPositionBlit(GetDlgItem(mhdlg, IDC_BORDERS), pos, mpFilterInst->GetPreCropWidth(), mpFilterInst->GetPreCropHeight());
}

bool VDShowFilterClippingDialog(VDGUIHandle hParent, FilterInstance *pFiltInst, List *pFilterList) {
	VDFilterClippingDialog dlg(pFiltInst, pFilterList);

	return 0 != dlg.ShowDialog(hParent);
}

///////////////////////////////////////////////////////////////////////

void FilterLoadFilter(HWND hWnd) {
	const VDStringW filename(VDGetLoadFileName(kFileDialog_LoadPlugin, (VDGUIHandle)hWnd, L"Load external filter", L"VirtualDub filter (*.vdf)\0*.vdf\0Windows Dynamic-Link Library (*.dll)\0*.dll\0All files (*.*)\0*.*\0", NULL, NULL, NULL));

	if (!filename.empty()) {
		try {
			VDAddPluginModule(filename.c_str());
		} catch(const MyError& e) {
			e.post(hWnd, g_szError);
		}
	}
}


