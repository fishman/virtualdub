#include "stdafx.h"
#include <hash_map>
#include <shellapi.h>
#include <commctrl.h>
#include <vd2/system/filesys.h>
#include <vd2/system/hash.h>
#include <vd2/system/registry.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/w32assist.h>
#include <vd2/Dita/services.h>
#include <vd2/VDLib/Dialog.h>
#include "resource.h"
#include "gui.h"
#include "job.h"

namespace {
	enum {
		kFileDialog_BatchOutputDir	= 'bout'
	};
}

extern DubOptions g_dubOpts;
extern HINSTANCE g_hInst;
extern const char g_szError[];
extern const char g_szWarning[];

///////////////////////////////////////////////////////////////////////////////

class VDUIProxyControl : public vdlist_node {
public:
	VDUIProxyControl();

	VDZHWND GetHandle() const { return mhwnd; }

	void Attach(VDZHWND hwnd);

	virtual VDZLRESULT On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam);

protected:
	HWND	mhwnd;
};

VDUIProxyControl::VDUIProxyControl()
	: mhwnd(NULL)
{
}

void VDUIProxyControl::Attach(VDZHWND hwnd) {
	VDASSERT(IsWindow(hwnd));
	mhwnd = hwnd;
}

VDZLRESULT VDUIProxyControl::On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) {
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

class VDUIProxyMessageDispatcherW32 {
public:
	void AddControl(VDUIProxyControl *control);
	void RemoveControl(VDZHWND hwnd);

	VDZLRESULT Dispatch_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam);

protected:
	size_t Hash(VDZHWND hwnd) const;
	VDUIProxyControl *GetControl(VDZHWND hwnd);

	enum { kHashTableSize = 31 };

	typedef vdlist<VDUIProxyControl> HashChain;
	HashChain mHashTable[kHashTableSize];
};

void VDUIProxyMessageDispatcherW32::AddControl(VDUIProxyControl *control) {
	VDZHWND hwnd = control->GetHandle();
	size_t hc = Hash(hwnd);

	mHashTable[hc].push_back(control);
}

void VDUIProxyMessageDispatcherW32::RemoveControl(VDZHWND hwnd) {
	size_t hc = Hash(hwnd);
	HashChain& hchain = mHashTable[hc];

	HashChain::iterator it(hchain.begin()), itEnd(hchain.end());
	for(; it == itEnd; ++it) {
		VDUIProxyControl *control = *it;

		if (control->GetHandle() == hwnd) {
			hchain.erase(control);
			break;
		}
	}

}

VDZLRESULT VDUIProxyMessageDispatcherW32::Dispatch_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) {
	const NMHDR *hdr = (const NMHDR *)lParam;
	VDUIProxyControl *control = GetControl(hdr->hwndFrom);

	if (control)
		return control->On_WM_NOTIFY(wParam, lParam);

	return 0;
}

size_t VDUIProxyMessageDispatcherW32::Hash(VDZHWND hwnd) const {
	return (size_t)hwnd % (size_t)kHashTableSize;
}

VDUIProxyControl *VDUIProxyMessageDispatcherW32::GetControl(VDZHWND hwnd) {
	size_t hc = Hash(hwnd);
	HashChain& hchain = mHashTable[hc];

	HashChain::iterator it(hchain.begin()), itEnd(hchain.end());
	for(; it != itEnd; ++it) {
		VDUIProxyControl *control = *it;

		if (control->GetHandle() == hwnd)
			return control;
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////

class IVDUIListViewVirtualItem : public IVDRefCount {
public:
	virtual void GetText(int subItem, VDStringW& s) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

class VDUIProxyListView : public VDUIProxyControl {
public:
	VDUIProxyListView();

	void AutoSizeColumns();
	void Clear();
	void DeleteItem(int index);
	int GetColumnCount() const;
	int GetItemCount() const;
	IVDUIListViewVirtualItem *GetVirtualItem(int index) const;
	void InsertColumn(int index, const wchar_t *label, int width);
	int InsertItem(int item, const wchar_t *text);
	int InsertVirtualItem(int item, IVDUIListViewVirtualItem *lvvi);
	void RefreshItem(int item);
	void SetItemText(int item, int subitem, const wchar_t *text);

protected:
	VDZLRESULT On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam);

	int			mNextTextIndex;
	VDStringW	mTextW[3];
	VDStringA	mTextA[3];
};

VDUIProxyListView::VDUIProxyListView()
	: mNextTextIndex(0)
{
}

void VDUIProxyListView::AutoSizeColumns() {
	const int colCount = GetColumnCount();

	for(int col=0; col<colCount; ++col) {
		SendMessage(mhwnd, LVM_SETCOLUMNWIDTH, col, LVSCW_AUTOSIZE);
	}
}

void VDUIProxyListView::Clear() {
	SendMessage(mhwnd, LVM_DELETEALLITEMS, 0, 0);
}

void VDUIProxyListView::DeleteItem(int index) {
	SendMessage(mhwnd, LVM_DELETEITEM, index, 0);
}

int VDUIProxyListView::GetColumnCount() const {
	HWND hwndHeader = (HWND)SendMessage(mhwnd, LVM_GETHEADER, 0, 0);
	if (!hwndHeader)
		return 0;

	return (int)SendMessage(hwndHeader, HDM_GETITEMCOUNT, 0, 0);
}

int VDUIProxyListView::GetItemCount() const {
	return (int)SendMessage(mhwnd, LVM_GETITEMCOUNT, 0, 0);
}

IVDUIListViewVirtualItem *VDUIProxyListView::GetVirtualItem(int index) const {
	if (index < 0)
		return NULL;

	if (VDIsWindowsNT()) {
		LVITEMW itemw={};
		itemw.mask = LVIF_PARAM;
		itemw.iItem = index;
		itemw.iSubItem = 0;
		if (SendMessage(mhwnd, LVM_GETITEMA, 0, (LPARAM)&itemw))
			return (IVDUIListViewVirtualItem *)itemw.lParam;
	} else {
		LVITEMA itema={};
		itema.mask = LVIF_PARAM;
		itema.iItem = index;
		itema.iSubItem = 0;
		if (SendMessage(mhwnd, LVM_GETITEMW, 0, (LPARAM)&itema))
			return (IVDUIListViewVirtualItem *)itema.lParam;
	}

	return NULL;
}

void VDUIProxyListView::InsertColumn(int index, const wchar_t *label, int width) {
	if (VDIsWindowsNT()) {
		LVCOLUMNW colw = {};

		colw.mask		= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		colw.fmt		= LVCFMT_LEFT;
		colw.cx			= width;
		colw.pszText	= (LPWSTR)label;

		SendMessageW(mhwnd, LVM_INSERTCOLUMNW, (WPARAM)index, (LPARAM)&colw);
	} else {
		LVCOLUMNA cola = {};
		VDStringA labela(VDTextWToA(label));

		cola.mask		= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		cola.fmt		= LVCFMT_LEFT;
		cola.cx			= width;
		cola.pszText	= (LPSTR)labela.c_str();

		SendMessageA(mhwnd, LVM_INSERTCOLUMNA, (WPARAM)index, (LPARAM)&cola);
	}
}

int VDUIProxyListView::InsertItem(int item, const wchar_t *text) {
	if (VDIsWindowsNT()) {
		LVITEMW itemw = {};

		itemw.mask		= LVIF_TEXT;
		itemw.pszText	= (LPWSTR)text;

		return (int)SendMessageW(mhwnd, LVM_INSERTITEMW, 0, (LPARAM)&itemw);
	} else {
		LVITEMA itema = {};
		VDStringA texta(VDTextWToA(text));

		itema.mask		= LVIF_TEXT;
		itema.pszText	= (LPSTR)texta.c_str();

		return (int)SendMessageA(mhwnd, LVM_INSERTITEMA, 0, (LPARAM)&itema);
	}
}

int VDUIProxyListView::InsertVirtualItem(int item, IVDUIListViewVirtualItem *lvvi) {
	int index;

	if (VDIsWindowsNT()) {
		LVITEMW itemw = {};

		itemw.mask		= LVIF_TEXT | LVIF_PARAM;
		itemw.pszText	= LPSTR_TEXTCALLBACKW;
		itemw.lParam	= (LPARAM)lvvi;

		index = (int)SendMessageW(mhwnd, LVM_INSERTITEMW, 0, (LPARAM)&itemw);
	} else {
		LVITEMA itema = {};

		itema.mask		= LVIF_TEXT | LVIF_PARAM;
		itema.pszText	= LPSTR_TEXTCALLBACKA;
		itema.lParam	= (LPARAM)lvvi;

		index = (int)SendMessageA(mhwnd, LVM_INSERTITEMA, 0, (LPARAM)&itema);
	}

	if (index >= 0)
		lvvi->AddRef();

	return index;
}

void VDUIProxyListView::RefreshItem(int item) {
	SendMessage(mhwnd, LVM_REDRAWITEMS, item, item);
}

void VDUIProxyListView::SetItemText(int item, int subitem, const wchar_t *text) {
	if (VDIsWindowsNT()) {
		LVITEMW itemw = {};

		itemw.mask		= LVIF_TEXT;
		itemw.iItem		= item;
		itemw.iSubItem	= subitem;
		itemw.pszText	= (LPWSTR)text;

		SendMessageW(mhwnd, LVM_SETITEMW, 0, (LPARAM)&itemw);
	} else {
		LVITEMA itema = {};
		VDStringA texta(VDTextWToA(text));

		itema.mask		= LVIF_TEXT;
		itema.iItem		= item;
		itema.iSubItem	= subitem;
		itema.pszText	= (LPSTR)texta.c_str();

		SendMessageA(mhwnd, LVM_SETITEMA, 0, (LPARAM)&itema);
	}
}

VDZLRESULT VDUIProxyListView::On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) {
	const NMHDR *hdr = (const NMHDR *)lParam;

	switch(hdr->code) {
		case LVN_GETDISPINFOA:
			{
				NMLVDISPINFOA *dispa = (NMLVDISPINFOA *)hdr;
				IVDUIListViewVirtualItem *lvvi = (IVDUIListViewVirtualItem *)dispa->item.lParam;

				mTextW[0].clear();
				lvvi->GetText(dispa->item.iSubItem, mTextW[0]);
				mTextA[mNextTextIndex] = VDTextWToA(mTextW[0]);
				dispa->item.pszText = (LPSTR)mTextA[mNextTextIndex].c_str();

				if (++mNextTextIndex >= 3)
					mNextTextIndex = 0;
			}
			break;

		case LVN_GETDISPINFOW:
			{
				NMLVDISPINFOW *dispw = (NMLVDISPINFOW *)hdr;
				IVDUIListViewVirtualItem *lvvi = (IVDUIListViewVirtualItem *)dispw->item.lParam;

				mTextW[mNextTextIndex].clear();
				lvvi->GetText(dispw->item.iSubItem, mTextW[mNextTextIndex]);
				dispw->item.pszText = (LPWSTR)mTextW[mNextTextIndex].c_str();

				if (++mNextTextIndex >= 3)
					mNextTextIndex = 0;
			}
			break;

		case LVN_DELETEITEM:
			{
				const NMLISTVIEW *nmlv = (const NMLISTVIEW *)hdr;
				IVDUIListViewVirtualItem *lvvi = (IVDUIListViewVirtualItem *)nmlv->lParam;

				if (lvvi)
					lvvi->Release();
			}
			break;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

class VDUIBatchWizardNameFilter : public VDDialogFrameW32 {
public:
	VDUIBatchWizardNameFilter();
	~VDUIBatchWizardNameFilter();

	bool IsMatchCaseEnabled() const { return mbCaseSensitive; }

	const wchar_t *GetSearchString() const { return mSearchString.c_str(); }
	const wchar_t *GetReplaceString() const { return mReplaceString.c_str(); }

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);

	VDStringW	mSearchString;
	VDStringW	mReplaceString;
	bool		mbCaseSensitive;
};

VDUIBatchWizardNameFilter::VDUIBatchWizardNameFilter()
	: VDDialogFrameW32(IDD_BATCH_WIZARD_NAMEFILTER)
	, mbCaseSensitive(false)
{
	VDRegistryAppKey key("Persistance");

	mbCaseSensitive = key.getBool("Batch Wizard: Match case", false);
}

VDUIBatchWizardNameFilter::~VDUIBatchWizardNameFilter() {
}

bool VDUIBatchWizardNameFilter::OnLoaded() {
	SetFocusToControl(IDC_SEARCHSTR);
	return true;
}

void VDUIBatchWizardNameFilter::OnDataExchange(bool write) {
	ExchangeControlValueString(write, IDC_SEARCHSTR, mSearchString);
	ExchangeControlValueString(write, IDC_REPLACESTR, mReplaceString);
	ExchangeControlValueBoolCheckbox(write, IDC_MATCHCASE, mbCaseSensitive);

	if (write) {
		VDRegistryAppKey key("Persistance");

		key.setBool("Batch Wizard: Match case", mbCaseSensitive);
	}
}

///////////////////////////////////////////////////////////////////////////////

class VDUIBatchWizard : public VDDialogFrameW32 {
public:
	VDUIBatchWizard();
	~VDUIBatchWizard();

protected:
	VDZINT_PTR DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);
	bool OnLoaded();
	void OnSize();
	bool OnCommand(uint32 id, uint32 extcode);
	void OnDropFiles(IVDUIDropFileList *dropFileList);
	bool CheckAndConfirmConflicts();

	bool mbOutputRelative;

	VDDialogResizerW32	mResizer;
	VDUIProxyListView	mList;

	VDUIProxyMessageDispatcherW32	mMsgDispatcher;

	HMENU	mhmenuPopups;
};

VDUIBatchWizard::VDUIBatchWizard()
	: VDDialogFrameW32(IDD_BATCH_WIZARD)
	, mhmenuPopups(LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_BATCHWIZARD_MENU)))
{
}

VDUIBatchWizard::~VDUIBatchWizard() {
	if (mhmenuPopups)
		DestroyMenu(mhmenuPopups);
}

VDZINT_PTR VDUIBatchWizard::DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	switch(msg) {
		case WM_NOTIFY:
			SetWindowLongPtr(mhdlg, DWLP_MSGRESULT, mMsgDispatcher.Dispatch_WM_NOTIFY(wParam, lParam));
			return TRUE;
	}

	return VDDialogFrameW32::DlgProc(msg, wParam, lParam);
}

bool VDUIBatchWizard::OnLoaded() {
	VDSetDialogDefaultIcons(mhdlg);
	mResizer.Init(mhdlg);
	mResizer.Add(IDC_OUTPUTFOLDER, VDDialogResizerW32::kTC);
	mResizer.Add(IDC_BROWSEOUTPUTFOLDER, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_LIST, VDDialogResizerW32::kMC);
	mResizer.Add(IDC_FILTEROUTPUTNAMES, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_RENAMEFILES, VDDialogResizerW32::kBR);
	mResizer.Add(IDC_ADDTOQUEUE, VDDialogResizerW32::kBR);
	mResizer.Add(IDOK, VDDialogResizerW32::kBR);
	DragAcceptFiles(mhdlg, TRUE);

	mList.Attach(GetDlgItem(mhdlg, IDC_LIST));
	mMsgDispatcher.AddControl(&mList);

	mList.InsertColumn(0, L"Source file", 100);
	mList.InsertColumn(1, L"Output name", 100);

	return false;
}

void VDUIBatchWizard::OnSize() {
	mResizer.Relayout();
}

class VDUIBatchWizardItem : public vdrefcounted<IVDUIListViewVirtualItem> {
public:
	VDUIBatchWizardItem(const wchar_t *fn);

	const wchar_t *GetFileName() const { return mFileName.c_str(); }
	const wchar_t *GetOutputName() const { return mOutputName.c_str(); }

	void SetOutputName(const wchar_t *s) { mOutputName = s; }

	void GetText(int subItem, VDStringW& s) const;

protected:
	VDStringW	mFileName;
	VDStringW	mOutputName;
};

VDUIBatchWizardItem::VDUIBatchWizardItem(const wchar_t *fn)
	: mFileName(fn)
	, mOutputName(VDFileSplitPath(fn))
{
}

void VDUIBatchWizardItem::GetText(int subItem, VDStringW& s) const {
	switch(subItem) {
		case 0:
			s = mFileName;
			break;

		case 1:
			s = mOutputName;
			break;
	}
}

bool VDUIBatchWizard::OnCommand(uint32 id, uint32 extcode) {
	switch(id) {
		case IDC_OUTPUT_RELATIVE:
			if (extcode == BN_CLICKED)
				mbOutputRelative = true;
			break;

		case IDC_OUTPUT_ABSOLUTE:
			if (extcode == BN_CLICKED)
				mbOutputRelative = false;
			break;

		case IDC_BROWSEOUTPUTFOLDER:
			{
				const VDStringW s(VDGetDirectory(kFileDialog_BatchOutputDir, (VDGUIHandle)mhdlg, L"Select output directory"));

				if (!s.empty())
					SetControlText(IDC_OUTPUTFOLDER, s.c_str());
			}
			return true;

		case IDC_RENAMEFILES:
			if (!CheckAndConfirmConflicts())
				return true;
			{
				int n = mList.GetItemCount();
				int failed = 0;
				int succeeded = 0;

				const VDStringW outputPath(GetControlValueString(IDC_OUTPUTFOLDER));

				for(int i=0; i<n;) {
					VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
					if (item) {
						const wchar_t *srcPath = item->GetFileName();
						const wchar_t *srcName = VDFileSplitPath(srcPath);
						const wchar_t *dstName = item->GetOutputName();
						const VDStringW& dstPath = VDMakePath(VDFileSplitPathLeft(VDStringW(srcPath)).c_str(), dstName);

						bool success = true;
						bool noop = false;

						// check if the filenames are actually the same (no-op).
						if (!wcscmp(srcName, dstName))
							noop = true;
						
						if (!noop) {
							VDStringW tempPath;

							// if the names differ only by case, we must rename through an intermediate name
							if (!_wcsicmp(srcName, dstName)) {
								tempPath = dstPath;
								tempPath.append_sprintf(L"-temp%08x", GetCurrentProcessId() ^ GetTickCount());

								success = !VDDoesPathExist(tempPath.c_str());
							} else {
								success = !VDDoesPathExist(dstPath.c_str());
							}

							if (success) {
								if (VDIsWindowsNT()) {
									if (!tempPath.empty()) {
										success = (0 != MoveFileW(srcPath, tempPath.c_str()));
										if (success) {
											success = (0 != MoveFileW(tempPath.c_str(), dstPath.c_str()));

											if (!success)
												MoveFileW(tempPath.c_str(), srcPath);
										}
									} else {
										success = (0 != MoveFileW(srcPath, dstPath.c_str()));
									}
								} else {
									VDStringA srcPathA(VDTextWToA(srcPath));
									VDStringA dstPathA(VDTextWToA(dstPath));

									if (!tempPath.empty()) {
										const VDStringA tempPathA(VDTextWToA(tempPath));

										success = (0 != MoveFileA(srcPathA.c_str(), tempPathA.c_str()));
										if (success) {
											success = (0 != MoveFileA(tempPathA.c_str(), dstPathA.c_str()));
											if (!success)
												MoveFileA(tempPathA.c_str(), srcPathA.c_str());
										}
									} else {
										success = (0 != MoveFileA(srcPathA.c_str(), dstPathA.c_str()));
									}
								}
							}
						}

						if (success) {
							mList.DeleteItem(i);
							--n;
							++succeeded;
						} else {
							++i;
							++failed;
						}
					}
				}

				VDStringA message;

				message.sprintf("%u file(s) renamed.", succeeded);

				if (failed)
					message.append_sprintf("\n\n%u file(s) could not be renamed and have been left in the list.", failed);

				MessageBox(mhdlg, message.c_str(), g_szError, (failed ? MB_ICONWARNING : MB_ICONINFORMATION)|MB_OK);
			}
			return true;

		case IDC_ADDTOQUEUE:
			if (mhmenuPopups) {
				HWND hwndChild = GetDlgItem(mhdlg, IDC_ADDTOQUEUE);
				RECT r;
				if (GetWindowRect(hwndChild, &r))
					TrackPopupMenu(GetSubMenu(mhmenuPopups, 0), TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, mhdlg, NULL);
			}
			return true;

		case IDC_FILTEROUTPUTNAMES:
			{
				VDUIBatchWizardNameFilter nameFilter;
				if (nameFilter.ShowDialog((VDGUIHandle)mhdlg)) {
					const bool matchCase = nameFilter.IsMatchCaseEnabled();
					const wchar_t *searchStr = nameFilter.GetSearchString();
					const wchar_t *replaceStr = nameFilter.GetReplaceString();
					int n = mList.GetItemCount();
					VDStringW s;
					size_t searchLen = wcslen(searchStr);
					size_t replaceLen = wcslen(replaceStr);

					if (searchLen) {
						for(int i=0; i<n; ++i) {
							VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
							if (item) {
								s = item->GetOutputName();

								int pos = 0;
								bool found = false;
								for(;;) {
									const wchar_t *base = s.c_str();
									const wchar_t *t = NULL;
									
									if (matchCase)
										t = wcsstr(base + pos, searchStr);
									else {
										const wchar_t *start = base + pos;
										size_t left = wcslen(start);

										if (left >= searchLen) {
											const wchar_t *limit = start + left - searchLen + 1;
											for(const wchar_t *t2 = start; t2 != limit; ++t2) {
												if (!_wcsnicmp(t2, searchStr, searchLen)) {
													t = t2;
													break;
												}
											}
										}
									}

									if (!t)
										break;

									found = true;

									size_t offset = t - base;
									s.replace(offset, searchLen, replaceStr, replaceLen);

									pos = offset + replaceLen;
								}

								if (found) {
									item->SetOutputName(s.c_str());
									mList.RefreshItem(i);
								}
							}
						}
					}					
				}
			}
			return true;

		case ID_ADDTOQUEUE_RESAVEASAVI:
			if (!CheckAndConfirmConflicts())
				return true;

			{
				int n = mList.GetItemCount();

				const VDStringW outputPath(GetControlValueString(IDC_OUTPUTFOLDER));
				VDStringW outputFileName;

				for(int i=0; i<n; ++i) {
					VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
					if (item) {
						const wchar_t *outputName = item->GetOutputName();

						if (mbOutputRelative)
							outputFileName = VDMakePath(outputPath.c_str(), outputName);
						else
							outputFileName = VDMakePath(VDFileSplitPathLeft(VDStringW(item->GetFileName())).c_str(), outputName);

						JobAddBatchFile(item->GetFileName(), outputFileName.c_str());
					}
				}

				mList.Clear();
			}
			return true;

		case ID_ADDTOQUEUE_EXTRACTAUDIOASWAV:
		case ID_ADDTOQUEUE_EXTRACTRAWAUDIO:
			if (!CheckAndConfirmConflicts())
				return true;

			{
				const bool raw = (id == ID_ADDTOQUEUE_EXTRACTRAWAUDIO);
				const int n = mList.GetItemCount();

				const VDStringW outputPath(GetControlValueString(IDC_OUTPUTFOLDER));
				VDStringW outputFileName;

				for(int i=0; i<n; ++i) {
					VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
					if (item) {
						const wchar_t *outputName = item->GetOutputName();

						if (mbOutputRelative)
							outputFileName = VDMakePath(outputPath.c_str(), outputName);
						else
							outputFileName = VDMakePath(VDFileSplitPathLeft(VDStringW(item->GetFileName())).c_str(), outputName);

						JobAddConfigurationSaveAudio(&g_dubOpts, item->GetFileName(), NULL, NULL, outputFileName.c_str(), raw, false);
					}
				}

				mList.Clear();
			}
			return true;

		case ID_ADDTOQUEUE_RUNVIDEOANALYSISPASS:
			if (!CheckAndConfirmConflicts())
				return true;

			{
				const int n = mList.GetItemCount();

				for(int i=0; i<n; ++i) {
					VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
					if (item) {
						JobAddConfigurationRunVideoAnalysisPass(&g_dubOpts, item->GetFileName(), NULL, NULL, false);
					}
				}

				mList.Clear();
			}
			return true;
	}

	return false;
}

void VDUIBatchWizard::OnDropFiles(IVDUIDropFileList *dropFileList) {
	VDStringW fileName;
	for(int index = 0; dropFileList->GetFileName(index, fileName); ++index) {
		const wchar_t *fn = fileName.c_str();

		vdrefptr<VDUIBatchWizardItem> item(new VDUIBatchWizardItem(fn));

		mList.InsertVirtualItem(-1, item);
	}

	mList.AutoSizeColumns();
}

bool VDUIBatchWizard::CheckAndConfirmConflicts() {
	typedef stdext::hash_multimap<uint32, const VDUIBatchWizardItem *> ConflictMap;
	ConflictMap conflictMap;

	int conflicts = 0;
	int n = mList.GetItemCount();

	for(int i=0; i<n; ++i) {
		VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
		if (item) {
			const wchar_t *outputName = item->GetOutputName();
			uint32 hash = VDHashString32I(outputName);

			std::pair<ConflictMap::const_iterator, ConflictMap::const_iterator> result(conflictMap.equal_range(hash));
			for(; result.first != result.second; ++result.first) {
				const VDUIBatchWizardItem *item2 = result.first->second;

				if (!_wcsicmp(outputName, item2->GetOutputName())) {
					++conflicts;
					break;
				}
			}

			if (result.first == result.second)
				conflictMap.insert(ConflictMap::value_type(hash, item));
		}
	}

	if (conflicts) {
		VDStringA message;
		message.sprintf("%u file(s) have conflicting output names and will attempt to overwrite the results of other entries. Proceed anyway?", conflicts);
		return IDOK == MessageBox(mhdlg, message.c_str(), g_szWarning, MB_ICONWARNING | MB_OKCANCEL);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void VDUIDisplayBatchWizard(VDGUIHandle hParent) {
	VDUIBatchWizard wiz;
	wiz.ShowDialog(hParent);
}
