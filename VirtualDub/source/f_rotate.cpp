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

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

#include "resource.h"
#include "filter.h"
#include "VBitmap.h"

extern HINSTANCE g_hInst;

enum {
	MODE_LEFT90 = 0,
	MODE_RIGHT90 = 1,
	MODE_180 = 2
};

static const char *const g_szMode[]={
	"left 90\xb0",
	"right 90\xb0",
	"180\xb0",
};

typedef struct MyFilterData {
	int mode;
} MyFilterData;

///////////////////////////////////////////////////////////////////////////

static int rotate_run(const FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	Pixel32 *src, *dst, *dst0;
	PixDim w0 = fa->src.w, w;
	PixDim h = fa->src.h;
	PixOffset dpitch = fa->dst.pitch;
	PixOffset dmodulo = fa->dst.modulo;
	PixOffset smodulo = fa->src.modulo;

	switch(mfd->mode) {
	case MODE_LEFT90:
		src = fa->src.data;
		dst0 = fa->dst.data + fa->dst.w;

		do {
			dst = --dst0;
			w = w0;
			do {
				*dst = *src++;
				dst = (Pixel32*)((char *)dst + dpitch);
			} while(--w);

			src = (Pixel32*)((char *)src + smodulo);
		} while(--h);
		break;

	case MODE_RIGHT90:
		src = fa->src.data;
		dst0 = (Pixel32 *)((char *)fa->dst.data + fa->dst.pitch*(fa->dst.h-1));

		do {
			dst = dst0++;
			w = w0;
			do {
				*dst = *src++;
				dst = (Pixel32*)((char *)dst - dpitch);
			} while(--w);

			src = (Pixel32*)((char *)src + smodulo);
		} while(--h);
		break;

	case MODE_180:
		src = fa->src.data;
		dst = (Pixel32 *)((char *)fa->dst.data + fa->dst.pitch*(fa->dst.h-1) + fa->dst.w*4 - 4);

		h>>=1;
		if (h) do {
			w = w0;
			do {
				Pixel32 a, b;

				a = *src;
				b = *dst;

				*src++ = b;
				*dst-- = a;
			} while(--w);

			src = (Pixel32*)((char *)src + smodulo);
			dst = (Pixel32*)((char *)dst - dmodulo);
		} while(--h);

		// if there is an odd line, flip half of it

		if (fa->src.h & 1) {
			w = w0>>1;
			if (w) do {
				Pixel32 a, b;

				a = *src;
				b = *dst;

				*src++ = b;
				*dst-- = a;
			} while(--w);
		}
		break;
	}
	return 0;
}

static long rotate_param(FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	if (mfd->mode == MODE_180)
		return 0;

	fa->dst.w = fa->src.h;
	fa->dst.h = fa->src.w;
	fa->dst.AlignTo8();

	return FILTERPARAM_SWAP_BUFFERS;
}

static INT_PTR APIENTRY rotateDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			{
				MyFilterData *mfd = (MyFilterData *)lParam;
				SetWindowLongPtr(hDlg, DWLP_USER, (LONG)mfd);

				switch(mfd->mode) {
				case MODE_LEFT90:	CheckDlgButton(hDlg, IDC_ROTATE_LEFT, BST_CHECKED); break;
				case MODE_RIGHT90:	CheckDlgButton(hDlg, IDC_ROTATE_RIGHT, BST_CHECKED); break;
				case MODE_180:		CheckDlgButton(hDlg, IDC_ROTATE_180, BST_CHECKED); break;
				}
			}
            return (TRUE);

        case WM_COMMAND:                      
            if (LOWORD(wParam) == IDOK) {
				MyFilterData *mfd = (struct MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

				if (IsDlgButtonChecked(hDlg, IDC_ROTATE_LEFT)) mfd->mode = MODE_LEFT90;
				if (IsDlgButtonChecked(hDlg, IDC_ROTATE_RIGHT)) mfd->mode = MODE_RIGHT90;
				if (IsDlgButtonChecked(hDlg, IDC_ROTATE_180)) mfd->mode = MODE_180;

				EndDialog(hDlg, 0);
				return TRUE;
			} else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, 1);  
                return TRUE;
            }
            break;
    }
    return FALSE;
}

static int rotate_config(FilterActivation *fa, const FilterFunctions *ff, VDXHWND hWnd) {
	return DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_ROTATE), (HWND)hWnd, rotateDlgProc, (LPARAM)fa->filter_data);
}

static void rotate_string(const FilterActivation *fa, const FilterFunctions *ff, char *buf) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	wsprintf(buf, " (%s)", g_szMode[mfd->mode]);
}

static void rotate_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	mfd->mode	= argv[0].asInt();

	if (mfd->mode < 0 || mfd->mode > 2)
		mfd->mode = 0;
}

static ScriptFunctionDef rotate_func_defs[]={
	{ (ScriptFunctionPtr)rotate_script_config, "Config", "0i" },
	{ NULL },
};

static CScriptObject rotate_obj={
	NULL, rotate_func_defs
};

static bool rotate_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d)", mfd->mode);

	return true;
}

FilterDefinition filterDef_rotate={
	0,0,NULL,
	"rotate",
	"Rotates an image by 90, 180, or 270 degrees.",
	NULL,NULL,
	sizeof(MyFilterData),
	NULL,NULL,
	rotate_run,
	rotate_param,
	rotate_config,
	rotate_string,
	NULL,
	NULL,

	&rotate_obj,
	rotate_script_line,
};