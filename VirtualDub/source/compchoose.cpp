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
#include <ctype.h>

#include <windows.h>
#include <commctrl.h>
#include <vfw.h>

#include <vd2/system/debug.h>
#include <vd2/system/protscope.h>
#include <vd2/system/text.h>

#include "resource.h"
#include "helpfile.h"

#include "oshelper.h"
#include "misc.h"

extern HINSTANCE g_hInst;

const char g_szNo[]="No";
const char g_szYes[]="Yes";

///////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK ChooseCompressorDlgProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);

///////////////////////////////////////////////////////////////////////////

void FreeCompressor(COMPVARS *pCompVars) {
	if (!(pCompVars->dwFlags & ICMF_COMPVARS_VALID))
		return;

	if (pCompVars->hic) {
		ICClose(pCompVars->hic);
		pCompVars->hic = NULL;
	}

	pCompVars->dwFlags &= ~ICMF_COMPVARS_VALID;
}

///////////////////////////////////////////////////////////////////////////

struct CCInfo {
	ICINFO		*pCompInfo;
	int			nComp;
	COMPVARS	*pCV;
	BITMAPINFOHEADER *bih;
	char		tbuf[128];

	HIC			hic;
	FOURCC		fccSelect;
	ICINFO		*piiCurrent;

	void		*pState;
	int			cbState;
	char		szCurrentCompression[256];
};

HIC ICOpenASV1(DWORD fccType, DWORD fccHandler, DWORD dwMode) {

	// ASUSASV1.DLL 1.0.4.0 causes a crash under Windows NT/2000 due to
	// nasty code that scans the video BIOS directly for "ASUS" by reading
	// 000C0000-000C00FF.  We workaround the problem by mapping a dummy
	// memory block over that address if nothing is there already.  This
	// will always fail under Windows 95/98.  We cannot just check for
	// the ASV1 code because the ASUSASVD.DLL codec does work and uses
	// the same code.
	//
	// Placing "ASUS" in the memory block works, but the codec checks
	// the string often during compression calls and would simply crash
	// on another call.  Also, there are potential licensing issues.
	// Thus, we simply leave the memory block blank, which basically
	// disables the codec but prevents it from crashing.
	//
	// Under Windows 2000, it appears that this region is always reserved.
	// So to be safe, use reserve->commit->ICOpen->decommit->release.

	LPVOID pDummyVideoBIOSRegion = VirtualAlloc((LPVOID)0x000C0000, 4096, MEM_RESERVE, PAGE_READWRITE);
	LPVOID pDummyVideoBIOS = VirtualAlloc((LPVOID)0x000C0000, 4096, MEM_COMMIT, PAGE_READWRITE);
	HIC hic;

	hic = ICOpen(fccType, fccHandler, dwMode);

	if (pDummyVideoBIOS)
		VirtualFree(pDummyVideoBIOS, 4096, MEM_DECOMMIT);

	if (pDummyVideoBIOSRegion)
		VirtualFree(pDummyVideoBIOSRegion, 0, MEM_RELEASE);

	return hic;
}

void ChooseCompressor(HWND hwndParent, COMPVARS *lpCompVars, BITMAPINFOHEADER *bihInput) {
	CCInfo cci;
	ICINFO info = {sizeof(ICINFO)};
	int i;
	int nComp;

	cci.fccSelect	= NULL;
	cci.pState		= NULL;
	cci.cbState		= 0;
	cci.hic			= NULL;
	cci.piiCurrent	= NULL;

	if (lpCompVars->dwFlags & ICMF_COMPVARS_VALID) {
		cci.fccSelect	= lpCompVars->fccHandler;

		if (lpCompVars->hic) {
			cci.cbState		= ICGetStateSize(lpCompVars->hic);

			if (cci.cbState>0) {
				cci.pState = new char[cci.cbState];

				if (!cci.pState)
					return;

				ICGetState(lpCompVars->hic, cci.pState, cci.cbState);
			}
		}
	}

	nComp = 0;
	cci.pCompInfo = NULL;
	cci.nComp = 0;

	if (bihInput && bihInput->biCompression != BI_RGB) {
		union {
			char fccbuf[5];
			FOURCC fcc;
		};

		fcc = bihInput->biCompression;
		fccbuf[4] = 0;

		sprintf(cci.szCurrentCompression, "(No recompression: %s)", fccbuf);
	} else
		strcpy(cci.szCurrentCompression, "(Uncompressed RGB/YCbCr)");

	vdprotected("enumerating video codecs") {
		for(i=0; ICInfo(ICTYPE_VIDEO, i, &info); i++) {
			HIC hic;

			// Use "special" routine for ASV1.

			union {
				FOURCC fcc;
				char buf[5];
			} u = {info.fccHandler};
			u.buf[4] = 0;

			vdprotected1("opening video codec with FOURCC \"%.4s\"", const char *, u.buf) {
				{
					wchar_t buf[64];
					swprintf(buf, 64, L"A video codec with FOURCC '%.4S'", (const char *)&info.fccHandler);
					VDExternalCodeBracket bracket(buf, __FILE__, __LINE__);

					if (isEqualFOURCC(info.fccHandler, '1VSA'))
						hic = ICOpenASV1(info.fccType, info.fccHandler, ICMODE_COMPRESS);
					else	
						hic = ICOpen(info.fccType, info.fccHandler, ICMODE_COMPRESS);
				}

				if (hic) {
					ICINFO ici = { sizeof(ICINFO) };
					char namebuf[64];

					namebuf[0] = 0;

					if (ICGetInfo(hic, &ici, sizeof(ICINFO)))
						VDTextWToA(namebuf, sizeof namebuf, ici.szDescription, -1);

					vdprotected1("querying video codec \"%.64s\"", const char *, namebuf) {
						if (!bihInput || ICERR_OK==ICCompressQuery(hic, bihInput, NULL)) {

							if (cci.nComp+1 > nComp) {
								ICINFO *pNewArray;
								nComp += 8;
								
								pNewArray = new ICINFO[nComp];

								if (!pNewArray) {
									delete cci.pState;
									ICClose(hic);
									return;
								}

								if (cci.nComp)
									memcpy(pNewArray, cci.pCompInfo, cci.nComp*sizeof(ICINFO));

								delete cci.pCompInfo;
								cci.pCompInfo = pNewArray;
							}

							cci.pCompInfo[cci.nComp] = ici;
							cci.pCompInfo[cci.nComp].fccHandler = info.fccHandler;
							++cci.nComp;
						}
						ICClose(hic);
					}
				}
			}
		}
	}

	cci.pCV = lpCompVars;
	cci.bih = bihInput;

	DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_VIDEOCOMPRESSION), hwndParent,
		ChooseCompressorDlgProc, (LPARAM)&cci);

	if (cci.hic)
		ICClose(cci.hic);

	delete cci.pCompInfo;
	delete cci.pState;
}

///////////////////////////////////////////////////////////////////////////

static int g_xres[]={
	160, 176, 320, 352, 640, 720
};

static int g_yres[]={
	120, 144, 240, 288, 480, 576
};

static int g_depths[]={
	16, 24, 32
};

#define		NWIDTHS		(sizeof g_xres / sizeof g_xres[0])
#define		NHEIGHTS	(sizeof g_yres / sizeof g_yres[0])
#define		NDEPTHS		(sizeof g_depths / sizeof g_depths[0])

void ReenableOptions(HWND hdlg, HIC hic, ICINFO *pii) {
	BOOL fSupports;
	ICINFO info = {sizeof(ICINFO)};
	DWORD dwFlags;

	if (hic) {
		// Ask the compressor for its information again, because some
		// compressors change their flags after certain config options
		// are changed... that means you, SERGE ;-)
		//
		// Preserve the existing fccHandler during the copy.  This allows
		// overloaded codecs (i.e. 'MJPG' for miroVideo DRX, 'mjpx' for
		// PICVideo, 'mjpy' for MainConcept, etc.)

		if (ICGetInfo(hic, &info, sizeof info)) {
			FOURCC fccHandler = pii->fccHandler;

			memcpy(pii, &info, sizeof(ICINFO));
			pii->fccHandler = fccHandler;
		}

		// Query compressor for caps and enable buttons as appropriate.

		EnableWindow(GetDlgItem(hdlg, IDC_ABOUT), ICQueryAbout(hic));
		EnableWindow(GetDlgItem(hdlg, IDC_CONFIGURE), ICQueryConfigure(hic));

	} else {
		EnableWindow(GetDlgItem(hdlg, IDC_ABOUT), FALSE);
		EnableWindow(GetDlgItem(hdlg, IDC_CONFIGURE), FALSE);
	}

	if (pii)
		dwFlags = pii->dwFlags;
	else
		dwFlags = 0;

	fSupports = !!(dwFlags & (VIDCF_CRUNCH | VIDCF_QUALITY));		// Strange but true: Windows expects to be able to crunch even if only the quality bit is set.

	EnableWindow(GetDlgItem(hdlg, IDC_USE_DATARATE), fSupports);
	EnableWindow(GetDlgItem(hdlg, IDC_DATARATE), fSupports);
	EnableWindow(GetDlgItem(hdlg, IDC_STATIC_DATARATE), fSupports);

	fSupports = !!(dwFlags & (VIDCF_TEMPORAL | VIDCF_FASTTEMPORALC));

	EnableWindow(GetDlgItem(hdlg, IDC_USE_KEYFRAMES), fSupports);
	EnableWindow(GetDlgItem(hdlg, IDC_KEYRATE), fSupports);
	EnableWindow(GetDlgItem(hdlg, IDC_STATIC_KEYFRAMES), fSupports);

	fSupports = !!(dwFlags & VIDCF_QUALITY);

	EnableWindow(GetDlgItem(hdlg, IDC_EDIT_QUALITY), fSupports);
	EnableWindow(GetDlgItem(hdlg, IDC_QUALITY_SLIDER), fSupports);
	EnableWindow(GetDlgItem(hdlg, IDC_STATIC_QUALITY_LABEL), fSupports);
}

void SelectCompressor(ICINFO *pii, HWND hdlg, CCInfo *pcci) {
	HIC hic;
	BITMAPINFO bi;
	char buf[256];
	HWND hwndReport = GetDlgItem(hdlg, IDC_SIZE_RESTRICTIONS);
	char *s, *slash;
	int i;

	// Clear restrictions box.

	SendMessage(hwndReport, LB_RESETCONTENT, 0, 0);

	if (!pii || !pii->fccHandler) {
		if (pcci->hic) {
			ICClose(pcci->hic);
			pcci->hic = NULL;
		}

		SetDlgItemText(hdlg, IDC_STATIC_DELTA, g_szNo);
		SetDlgItemText(hdlg, IDC_STATIC_FOURCC, "");
		SetDlgItemText(hdlg, IDC_STATIC_DRIVER, "");

		pcci->piiCurrent = pii;
		ReenableOptions(hdlg, NULL, pii);
		return;
	}

	// Show driver caps.

	SetDlgItemText(hdlg, IDC_STATIC_DELTA, (pii->dwFlags & (VIDCF_TEMPORAL|VIDCF_FASTTEMPORALC)) ? g_szYes : g_szNo);

	// Show driver fourCC code.

	for(i=0; i<4; i++) {
		char c = ((char *)&pii->fccHandler)[i];

		if (isprint((unsigned char)c))
			pcci->tbuf[i+1] = c;
		else
			pcci->tbuf[i+1] = ' ';
	}

	pcci->tbuf[0] = pcci->tbuf[5] = '\'';
	pcci->tbuf[6] = 0;
	SetDlgItemText(hdlg, IDC_STATIC_FOURCC, pcci->tbuf);

	WideCharToMultiByte(CP_ACP, 0, pii->szDriver, -1, pcci->tbuf, sizeof pcci->tbuf, NULL, NULL);

	// Set driver name (rip off the path).

	s = pcci->tbuf;
	slash = s;
	while(*s) {
		if (*s == '/' || *s=='\\' || *s==':')
			slash = s+1;

		++s;
	}


	SetDlgItemText(hdlg, IDC_STATIC_DRIVER, slash);

	// Attempt to open the compressor.

	if (pcci->hic) {
		ICClose(pcci->hic);
		pcci->hic = NULL;
	}

	{
		wchar_t buf[64];
		swprintf(buf, 64, L"A video codec with FOURCC '%.4S'", (const char *)&pii->fccHandler);
		VDExternalCodeBracket bracket(buf, __FILE__, __LINE__);

		hic = ICOpen(pii->fccType, pii->fccHandler, ICMODE_COMPRESS);
	}

	if (!hic) {
		SendMessage(hwndReport, LB_ADDSTRING, 0, (LPARAM)"<Unable to open driver>");
		return;
	}

	if (pii->fccHandler == pcci->fccSelect && pcci->pState)
		ICSetState(hic, pcci->pState, pcci->cbState);

	pcci->piiCurrent = pii;
	ReenableOptions(hdlg, hic, pii);

	// Start querying the compressor for what it can handle

	bi.bmiHeader.biSize				= sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biPlanes			= 1;
	bi.bmiHeader.biCompression		= BI_RGB;
	bi.bmiHeader.biXPelsPerMeter	= 80;
	bi.bmiHeader.biYPelsPerMeter	= 72;
	bi.bmiHeader.biClrUsed			= 0;
	bi.bmiHeader.biClrImportant		= 0;

	// Loop until we can find a width, height, and depth that works!

	int j, k;
	int w, h, d;

	for(i=0; i<NWIDTHS; i++) {
		bi.bmiHeader.biWidth = w = g_xres[i];

		for(j=0; j<NHEIGHTS; j++) {
			bi.bmiHeader.biHeight = h = g_yres[j];

			for(k=0; k<NDEPTHS; k++) {
				d = g_depths[k];
				bi.bmiHeader.biBitCount = (WORD)d;
				bi.bmiHeader.biSizeImage = ((w*d+31)/32)*4*h;

				if (ICERR_OK == ICCompressQuery(hic, &bi.bmiHeader, NULL))
					goto pass;
			}
		}
	}

	SendMessage(hwndReport, LB_ADDSTRING, 0, (LPARAM)"Couldn't find compatible format.");
	SendMessage(hwndReport, LB_ADDSTRING, 0, (LPARAM)"Possible reasons:");
	SendMessage(hwndReport, LB_ADDSTRING, 0, (LPARAM)"*  Codec may only support YUV");
	SendMessage(hwndReport, LB_ADDSTRING, 0, (LPARAM)"*  Codec might be locked.");
	SendMessage(hwndReport, LB_ADDSTRING, 0, (LPARAM)"*  Codec might be decompression-only");
	pcci->hic = hic;
	return;

pass:

	int depth_bits = 0;

	// Check all the depths; see if they work

	for(k=0; k<NDEPTHS; k++) {
		bi.bmiHeader.biBitCount		= (WORD)g_depths[k];
		bi.bmiHeader.biSizeImage	= ((w*g_depths[k]+31)/32)*4*h;

		if (ICERR_OK == ICCompressQuery(hic, &bi.bmiHeader, NULL))
			depth_bits |= (1<<k);
	}

	// Look for X alignment

	bi.bmiHeader.biBitCount = (WORD)d;

	for(i=3; i>=0; i--) {
		bi.bmiHeader.biWidth	 = w + (1<<i);
		bi.bmiHeader.biSizeImage = ((bi.bmiHeader.biWidth*d+31)/32)*4*h;

		if (ICERR_OK != ICCompressQuery(hic, &bi.bmiHeader, NULL))
			break;

	}

	bi.bmiHeader.biWidth	 = w + (1<<(i+2));
	bi.bmiHeader.biSizeImage = ((bi.bmiHeader.biWidth*d+31)/32)*4*h;

	if (ICERR_OK != ICCompressQuery(hic, &bi.bmiHeader, NULL))
		i = -2;

	// Look for Y alignment

	bi.bmiHeader.biWidth = w;

	for(j=3; j>=0; j--) {
		bi.bmiHeader.biHeight	 = h + (1<<j);
		bi.bmiHeader.biSizeImage = ((w*d+31)/32)*4*bi.bmiHeader.biHeight;

		if (ICERR_OK != ICCompressQuery(hic, &bi.bmiHeader, NULL))
			break;
	}

	bi.bmiHeader.biHeight	 = h + (1<<(j+2));
	bi.bmiHeader.biSizeImage = ((w*d+31)/32)*4*bi.bmiHeader.biHeight;

	if (ICERR_OK != ICCompressQuery(hic, &bi.bmiHeader, NULL))
		j = -2;

	// Print out results

	if (i>=0) {
		sprintf(buf, "Width must be a multiple of %d", 1<<(i+1));
		SendMessage(hwndReport, LB_ADDSTRING, 0, (LPARAM)buf);
	} else if (i<-1) {
		sprintf(buf, "Width: unknown (%dx%d worked)", w, h);
		SendMessage(hwndReport, LB_ADDSTRING, 0, (LPARAM)buf);
	}

	if (j>=0) {
		sprintf(buf, "Height must be a multiple of %d", 1<<(j+1));
		SendMessage(hwndReport, LB_ADDSTRING, 0, (LPARAM)buf);
	} else if (j<-1) {
		sprintf(buf, "Height: unknown (%dx%d worked)", w, h);
		SendMessage(hwndReport, LB_ADDSTRING, 0, (LPARAM)buf);
	}

	if (depth_bits != 7) {
		strcpy(buf, "Valid depths:");

		for(k=0; k<3; k++)
			if (depth_bits & (1<<k))
				sprintf(buf+strlen(buf), " %d", g_depths[k]);

		SendMessage(hwndReport, LB_ADDSTRING, 0, (LPARAM)buf);
	}

	if (depth_bits==7 && i<0 && j<0)
		SendMessage(hwndReport, LB_ADDSTRING, 0, (LPARAM)"No known restrictions.");

	pcci->hic = hic;
}

///////////////////////////////////////////////////////////////////////////

static const DWORD dwHelpLookup[]={
	IDC_STATIC_DELTA,			IDH_VIDCOMP_DELTAFRAMES,
	IDC_STATIC_FOURCC,			IDH_VIDCOMP_FOURCCCODE,
	IDC_STATIC_DRIVER,			IDH_VIDCOMP_DRIVERNAME,
	IDC_SIZE_RESTRICTIONS,		IDH_VIDCOMP_FORMATRESTRICTIONS,
	IDC_EDIT_QUALITY,			IDH_VIDCOMP_QUALITYSETTING,
	IDC_QUALITY_SLIDER,			IDH_VIDCOMP_QUALITYSETTING,
	IDC_STATIC_QUALITY_LABEL,	IDH_VIDCOMP_QUALITYSETTING,
	IDC_USE_DATARATE,			IDH_VIDCOMP_TARGETDATARATE,
	IDC_DATARATE,				IDH_VIDCOMP_TARGETDATARATE,
	IDC_STATIC_DATARATE,		IDH_VIDCOMP_TARGETDATARATE,
	IDC_USE_KEYFRAMES,			IDH_VIDCOMP_KEYFRAMEINTERVAL,
	IDC_KEYRATE,				IDH_VIDCOMP_KEYFRAMEINTERVAL,
	IDC_STATIC_KEYFRAMES,		IDH_VIDCOMP_KEYFRAMEINTERVAL,
	NULL
};


INT_PTR CALLBACK ChooseCompressorDlgProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
	CCInfo *pcci = (CCInfo *)GetWindowLongPtr(hdlg, DWLP_USER);
	int i, ind, ind_select;
	HWND hwndItem;

	switch(uiMsg) {
		case WM_INITDIALOG:
			SetWindowLongPtr(hdlg, DWLP_USER, lParam);

			pcci = (CCInfo *)lParam;

			hwndItem = GetDlgItem(hdlg, IDC_COMP_LIST);
			ind = SendMessage(hwndItem, LB_ADDSTRING, 0, (LPARAM)pcci->szCurrentCompression);
			if (ind != LB_ERR)
				SendMessage(hwndItem, LB_SETITEMDATA, ind, -1);


			ind_select = 0;

			for(i=0; i<pcci->nComp; i++) {

				WideCharToMultiByte(CP_ACP, 0, pcci->pCompInfo[i].szDescription, -1, pcci->tbuf, sizeof pcci->tbuf, NULL, NULL);

				ind = SendMessage(hwndItem, LB_ADDSTRING, 0, (LPARAM)pcci->tbuf);
				if (ind != LB_ERR) {
					SendMessage(hwndItem, LB_SETITEMDATA, ind, i);
					if (!ind_select && isEqualFOURCC(pcci->pCompInfo[i].fccHandler, pcci->fccSelect)) {
						ind_select = ind;
						SendMessage(hwndItem, LB_SETCURSEL, ind_select, 0);

						SelectCompressor(&pcci->pCompInfo[i], hdlg, pcci);
					}

				}
			}

			if (!pcci->fccSelect) {
				SendMessage(hwndItem, LB_SETCURSEL, 0, 0);
				SelectCompressor(NULL, hdlg, pcci);
			}

			SendDlgItemMessage(hdlg, IDC_QUALITY_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(0, 100));

			if (pcci->pCV->dwFlags & ICMF_COMPVARS_VALID) {
				SendDlgItemMessage(hdlg, IDC_QUALITY_SLIDER, TBM_SETPOS, TRUE, (pcci->pCV->lQ+50)/100);
				SetDlgItemInt(hdlg, IDC_EDIT_QUALITY, (pcci->pCV->lQ+50)/100, FALSE);
			}


			if ((pcci->pCV->dwFlags & ICMF_COMPVARS_VALID) && pcci->pCV->lKey) {
				CheckDlgButton(hdlg, IDC_USE_KEYFRAMES, BST_CHECKED);
				SetDlgItemInt(hdlg, IDC_KEYRATE, pcci->pCV->lKey, FALSE);
			} else
				CheckDlgButton(hdlg, IDC_USE_KEYFRAMES, BST_UNCHECKED);
			
			if ((pcci->pCV->dwFlags & ICMF_COMPVARS_VALID) && pcci->pCV->lDataRate) {
				CheckDlgButton(hdlg, IDC_USE_DATARATE, BST_CHECKED);
				SetDlgItemInt(hdlg, IDC_DATARATE, pcci->pCV->lDataRate, FALSE);
			} else
				CheckDlgButton(hdlg, IDC_USE_DATARATE, BST_UNCHECKED);

			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {

				case IDCANCEL:
					EndDialog(hdlg, FALSE);
					return TRUE;

				case IDOK:

					if (!(pcci->pCV->dwFlags & ICMF_COMPVARS_VALID)) {
						memset(pcci->pCV, 0, sizeof(COMPVARS));

						pcci->pCV->dwFlags = ICMF_COMPVARS_VALID;
					}
					pcci->pCV->fccType = 'CDIV';

					ind = SendDlgItemMessage(hdlg, IDC_COMP_LIST, LB_GETCURSEL, 0, 0);
					if (ind > 0) {
						ind = SendDlgItemMessage(hdlg, IDC_COMP_LIST, LB_GETITEMDATA, ind, 0);

						pcci->pCV->fccHandler = pcci->pCompInfo[ind].fccHandler;
					} else
						pcci->pCV->fccHandler = NULL;

					if (IsDlgButtonChecked(hdlg, IDC_USE_KEYFRAMES))
						pcci->pCV->lKey = GetDlgItemInt(hdlg, IDC_KEYRATE, NULL, FALSE);
					else
						pcci->pCV->lKey = 0;

					if (IsDlgButtonChecked(hdlg, IDC_USE_DATARATE))
						pcci->pCV->lDataRate = GetDlgItemInt(hdlg, IDC_DATARATE, NULL, FALSE);
					else
						pcci->pCV->lDataRate = 0;

					pcci->pCV->lQ = SendDlgItemMessage(hdlg, IDC_QUALITY_SLIDER, TBM_GETPOS, 0, 0)*100;

					if (pcci->hic)
						ICSendMessage(pcci->hic, ICM_SETQUALITY, (DWORD)&pcci->pCV->lQ, 0);

					if (pcci->pCV->hic)
						ICClose(pcci->pCV->hic);
					pcci->pCV->hic = pcci->hic;
					pcci->hic = NULL;

					EndDialog(hdlg, TRUE);
					return TRUE;

				case IDC_CONFIGURE:
					if (pcci->hic) {
						ICConfigure(pcci->hic, hdlg);
						delete pcci->pState;
						pcci->pState = NULL;

						if (pcci->piiCurrent)
							ReenableOptions(hdlg, pcci->hic, pcci->piiCurrent);
					}
					return TRUE;

				case IDC_ABOUT:
					if (pcci->hic)
						ICAbout(pcci->hic, hdlg);
					return TRUE;

				case IDC_COMP_LIST:
					switch(HIWORD(wParam)) {
					case LBN_SELCHANGE:
						{
							int ind = SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0);
							int data = SendMessage((HWND)lParam, LB_GETITEMDATA, ind, 0);

							if (ind == LB_ERR)
								return TRUE;

							ICINFO *pii = data>=0 ? &pcci->pCompInfo[data] : NULL;

							SelectCompressor(pii, hdlg, pcci);

						}
						return TRUE;
					}

				case IDC_EDIT_QUALITY:
					if (HIWORD(wParam)==EN_KILLFOCUS) {
						BOOL fSuccess;
						int v;

						v = (int)GetDlgItemInt(hdlg, IDC_EDIT_QUALITY, &fSuccess, TRUE);

						if (!fSuccess) {
							MessageBeep(MB_ICONEXCLAMATION);
							SetFocus((HWND)lParam);
							return TRUE;
						}

						if (v < 0 || v > 100) {
							v = v<0?0:100;

							SetDlgItemInt(hdlg, IDC_EDIT_QUALITY, v, TRUE);
						}
						SendDlgItemMessage(hdlg, IDC_QUALITY_SLIDER, TBM_SETPOS, TRUE, v);
					}
					return TRUE;

			}
			return FALSE;

		case WM_HSCROLL:
			if (lParam) {
				pcci->pCV->lQ = SendDlgItemMessage(hdlg, IDC_QUALITY_SLIDER, TBM_GETPOS, 0, 0);
				SetDlgItemInt(hdlg, IDC_EDIT_QUALITY, pcci->pCV->lQ, FALSE);

				// Well... it seems Microsoft's ICCompressorChoose() never sends this.

				//if (pcci->hic)
				//	ICSendMessage(pcci->hic, ICM_SETQUALITY, pcci->pCV->lQ, 0);
			}
			return TRUE;

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					VDShowHelp(hdlg, L"d-videocompression.html");
			}
			return TRUE;
	}

	return FALSE;
}
