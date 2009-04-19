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
#include <windows.h>

#include <vd2/system/vdtypes.h>
#include <vector>

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

#include "misc.h"
#include <vd2/system/cpuaccel.h>
#include "resource.h"
#include "gui.h"
#include "filter.h"
#include "vbitmap.h"

extern HINSTANCE g_hInst;

///////////////////////

// 4:2:0 MPEG-1:			[1 2 1]/4
// 4:2:0/4:2:2 MPEG-2:		1 [1 1]/2
// 4:1:0:					[1 3 3 1]/8

namespace {
	enum {
		kModeDisable,
		kMode420MPEG1,
		kMode420MPEG2,
		kMode422,
		kMode410,
		kMode411,
	};

	inline int luma(const Pixel32 p) {
		return ((p>>16)&255)*77 + ((p>>8)&255)*150 + (p&255)*29;
	}

	inline Pixel32 filterMPEG1(Pixel32 c1, int y1, Pixel32 c2, int y2, Pixel32 c3, int y3) {
		int yd = (2*y2 - (y1+y3) + 512) >> 10;
		unsigned r = yd + ((((c1>>16)&0xff) + 2*((c2>>16)&0xff) + ((c3>>16)&0xff) + 2) >> 2);
		unsigned g = yd + ((((c1>> 8)&0xff) + 2*((c2>> 8)&0xff) + ((c3>> 8)&0xff) + 2) >> 2);
		unsigned b = yd + ((((c1    )&0xff) + 2*((c2    )&0xff) + ((c3    )&0xff) + 2) >> 2);

		if (r >= 0x100)
			r = ((int)~r >> 31) & 0xff;
		if (g >= 0x100)
			g = ((int)~g >> 31) & 0xff;
		if (b >= 0x100)
			b = ((int)~b >> 31) & 0xff;

		return (r<<16) + (g<<8) + b;
	}

	void FilterHorizontalMPEG1(Pixel32 *dst, const Pixel32 *src, int count) {
		if (count < 1)
			return;

		Pixel32 c1, c2, c3;
		int y1, y2, y3;

		c2 = c3 = *src++;
		y2 = y3 = luma(c2);

		--count;
		for(int repcount = 1; repcount >= 0; --repcount) {
			if (count>0) do {
				c1 = c2;
				y1 = y2;
				c2 = c3;
				y2 = y3;
				c3 = *src++;
				y3 = luma(c3);

				*dst++ = filterMPEG1(c1, y1, c2, y2, c3, y3);
			} while(--count);

			count = 1;
			--src;
		}
	}

	void FilterVerticalMPEG1(Pixel32 *dst, const Pixel32 *const *src, int count) {
		if (count <= 0)
			return;

		const Pixel32 *src0 = src[-3];
		const Pixel32 *src1 = src[-2];
		const Pixel32 *src2 = src[-1];

		do {
			const Pixel32 c0 = *src0++;
			const Pixel32 c1 = *src1++;
			const Pixel32 c2 = *src2++;
			const int y0 = luma(c0);
			const int y1 = luma(c1);
			const int y2 = luma(c2);

			*dst++ = filterMPEG1(c0, y0, c1, y1, c2, y2);
		} while(--count);
	}

	inline Pixel32 filterMPEG2(Pixel32 c1, int y1, Pixel32 c2, int y2) {
		int yd = (y1 - y2 + 256) >> 9;
		unsigned r = yd + ((((c1>>16)&0xff) + ((c2>>16)&0xff) + 1) >> 1);
		unsigned g = yd + ((((c1>> 8)&0xff) + ((c2>> 8)&0xff) + 1) >> 1);
		unsigned b = yd + ((((c1    )&0xff) + ((c2    )&0xff) + 1) >> 1);

		if (r >= 0x100)
			r = ((int)~r >> 31) & 0xff;
		if (g >= 0x100)
			g = ((int)~g >> 31) & 0xff;
		if (b >= 0x100)
			b = ((int)~b >> 31) & 0xff;

		return (r<<16) + (g<<8) + b;
	}

	void FilterHorizontalMPEG2(Pixel32 *dst, const Pixel32 *src, int count) {
		Pixel32 c1, c2;
		int y1, y2;

		c2 = *src++;
		y2 = luma(c2);

		if (--count > 0)
			do {
				c1 = c2;
				y1 = y2;
				c2 = *src++;
				y2 = luma(c2);

				*dst++ = filterMPEG2(c1, y1, c2, y2);
			} while(--count);

		*dst++ = c2;
	}

	inline Pixel32 filterMPEG4(Pixel32 c1, int y1, Pixel32 c2, int y2, Pixel32 c3, int y3, Pixel32 c4, int y4, Pixel32 c5, int y5) {
		int yd = (6*y3 - y1 - 2*(y2+y4) - y5 + 1024) >> 11;
		unsigned r = yd + ((((c1>>16)&0xff) + ((c5>>16)&0xff) + 2*(((c2>>16)&0xff) + ((c3>>16)&0xff) + ((c4>>16)&0xff)) + 4) >> 3);
		unsigned g = yd + ((((c1>> 8)&0xff) + ((c5>> 8)&0xff) + 2*(((c2>> 8)&0xff) + ((c3>> 8)&0xff) + ((c4>> 8)&0xff)) + 4) >> 3);
		unsigned b = yd + ((((c1    )&0xff) + ((c5    )&0xff) + 2*(((c2    )&0xff) + ((c3    )&0xff) + ((c4    )&0xff)) + 4) >> 3);

		if (r >= 0x100)
			r = ((int)~r >> 31) & 0xff;
		if (g >= 0x100)
			g = ((int)~g >> 31) & 0xff;
		if (b >= 0x100)
			b = ((int)~b >> 31) & 0xff;

		return (r<<16) + (g<<8) + b;
	}

	void FilterHorizontalMPEG4(Pixel32 *dst, const Pixel32 *src, int count) {
		if (count < 2) {
			while(count-->0)
				*dst++ = *src++;
			return;
		}

		Pixel32 c1, c2, c3, c4, c5;
		int y1, y2, y3, y4, y5;

		c2 = c3 = c4 = *src++;
		y2 = y3 = y4 = luma(c2);
		c5 = *src++;
		y5 = luma(c5);

		count -= 2;
		for(int tailcount = 2; tailcount >= 0; --tailcount) {
			if (count > 0)
				do {
					c1 = c2;
					y1 = y2;
					c2 = c3;
					y2 = y3;
					c3 = c4;
					y3 = y4;
					c4 = c5;
					y4 = y5;
					c5 = *src++;
					y5 = luma(c5);
					*dst++ = filterMPEG4(c1, y1, c2, y2, c3, y3, c4, y4, c5, y5);
				} while(--count);

			count = 1;
			--src;
		}
	}

	void FilterVerticalMPEG4(Pixel32 *dst, const Pixel32 *const *src, int count) {
		if (count <= 0)
			return;

		const Pixel32 *src0 = src[-5];
		const Pixel32 *src1 = src[-4];
		const Pixel32 *src2 = src[-3];
		const Pixel32 *src3 = src[-2];
		const Pixel32 *src4 = src[-1];

		do {
			const Pixel32 c0 = *src0++;
			const Pixel32 c1 = *src1++;
			const Pixel32 c2 = *src2++;
			const Pixel32 c3 = *src3++;
			const Pixel32 c4 = *src4++;
			const int y0 = luma(c0);
			const int y1 = luma(c1);
			const int y2 = luma(c2);
			const int y3 = luma(c3);
			const int y4 = luma(c4);

			*dst++ = filterMPEG4(c0, y0, c1, y1, c2, y2, c3, y3, c4, y4);
		} while(--count);
	}

	void FilterHorizontalCopy(Pixel32 *dst, const Pixel32 *src, int count) {
		memcpy(dst, src, count*4);
	}
	void FilterVerticalCopy(Pixel32 *dst, const Pixel32 *const *src, int count) {
		memcpy(dst, src[-1], count*4);
	}
}

///////////////////////////////////////////////////////////////////////////

#ifdef _M_IX86
extern "C" void asm_chromasmoother_FilterHorizontalMPEG1_MMX(Pixel32 *dst, const Pixel32 *src, int count);
extern "C" void asm_chromasmoother_FilterHorizontalMPEG2_MMX(Pixel32 *dst, const Pixel32 *src, int count);
extern "C" void asm_chromasmoother_FilterVerticalMPEG1_MMX(Pixel32 *dst, const Pixel32 *const *src, int count);
#endif

struct ChromaSmootherFilter {
	ChromaSmootherFilter() : mMode(0) {}

	int Run(const FilterActivation *fa, const FilterFunctions *ff);
	int Param(FilterActivation *fa, const FilterFunctions *ff);
	int Config(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd);
	int Start(FilterActivation *fa, const FilterFunctions *ff);
	int Stop(FilterActivation *fa, const FilterFunctions *ff);
	void String2(const FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen);
	void ScriptConfig(IScriptInterpreter *isi, FilterActivation *fa, CScriptValue *argv, int argc);
	bool ScriptLine(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen);

	std::vector<Pixel32> mTempRows;
	Pixel32 *mTempWindow[5];

	int		mMode;
};

///////////////////////////////////////////////////////////////////////////

int ChromaSmootherFilter::Run(const FilterActivation *fa, const FilterFunctions *ff) {
	const char *src = (const char *)fa->src.data;
	const ptrdiff_t srcpitch = fa->src.pitch;
	char *dst = (char *)fa->dst.data;
	ptrdiff_t dstpitch = fa->dst.pitch;
	const int w = fa->dst.w;
	const int h = fa->dst.h;

	int nextsrc = 0;
	int preroll = 0;
	int postlen = 1;

	void (*pHorizFilt)(Pixel32 *dst, const Pixel32 *src, int count) = FilterHorizontalCopy;
	void (*pVertFilt)(Pixel32 *dst, const Pixel32 *const *src, int count) = FilterVerticalCopy;
	
#ifdef _M_IX86
	switch(mMode) {
	case kMode420MPEG1:
		if (w >= 1)
			pHorizFilt = MMX_enabled ? asm_chromasmoother_FilterHorizontalMPEG1_MMX : FilterHorizontalMPEG1;
		pVertFilt	= MMX_enabled ? asm_chromasmoother_FilterVerticalMPEG1_MMX : FilterVerticalMPEG1;
		preroll		= 1;
		postlen		= 2;
		break;
	case kMode420MPEG2:
		pHorizFilt	= MMX_enabled ? asm_chromasmoother_FilterHorizontalMPEG2_MMX : FilterHorizontalMPEG2;
		pVertFilt	= MMX_enabled ? asm_chromasmoother_FilterVerticalMPEG1_MMX : FilterVerticalMPEG1;
		preroll		= 1;
		postlen		= 2;
		break;
	case kMode422:
		pHorizFilt	= MMX_enabled ? asm_chromasmoother_FilterHorizontalMPEG2_MMX : FilterHorizontalMPEG2;
		break;
	case kMode410:
		pHorizFilt	= FilterHorizontalMPEG4;
		pVertFilt	= FilterVerticalMPEG4;
		preroll		= 1;
		postlen		= 3;
		break;
	case kMode411:
		if (w >= 2)
			pHorizFilt = FilterHorizontalMPEG4;
		break;
	}
#else
	switch(mMode) {
	case kMode420MPEG1:
		if (w >= 1)
			pHorizFilt = FilterHorizontalMPEG1;
		pVertFilt	= FilterVerticalMPEG1;
		preroll		= 1;
		postlen		= 2;
		break;
	case kMode420MPEG2:
		pHorizFilt	= FilterHorizontalMPEG2;
		pVertFilt	= FilterVerticalMPEG1;
		preroll		= 1;
		postlen		= 2;
		break;
	case kMode422:
		pHorizFilt	= FilterHorizontalMPEG2;
		break;
	case kMode410:
		pHorizFilt	= FilterHorizontalMPEG4;
		pVertFilt	= FilterVerticalMPEG4;
		preroll		= 1;
		postlen		= 3;
		break;
	case kMode411:
		if (w >= 2)
			pHorizFilt = FilterHorizontalMPEG4;
		break;
	}
#endif

	for(int y=0; y<h; ++y) {
		while(nextsrc < y+postlen) {
			std::rotate(mTempWindow + 0, mTempWindow + 1, mTempWindow + 5);
			pHorizFilt(mTempWindow[4], (const Pixel32 *)src, w);

			if (preroll > 0)
				--preroll;
			else if (++nextsrc < h)
				src += srcpitch;
		}

		pVertFilt((Pixel32 *)dst, mTempWindow + 5, w);
		dst += dstpitch;
	}

#ifndef _M_AMD64
	if (MMX_enabled)
		__asm emms
#endif

	return 0;
}

int ChromaSmootherFilter::Param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.AlignTo8();

	return FILTERPARAM_SWAP_BUFFERS;
}

int ChromaSmootherFilter::Start(FilterActivation *fa, const FilterFunctions *ff) {
	const int w = fa->dst.w;

	mTempRows.resize(w * 5);
	for(int i=0; i<5; ++i)
		mTempWindow[i] = &mTempRows[w * i];
	return 0;
}

int ChromaSmootherFilter::Stop(FilterActivation *fa, const FilterFunctions *ff) {
	std::vector<Pixel32>().swap(mTempRows);
	return 0;
}

void ChromaSmootherFilter::String2(const FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	static const char *const sModes[]={
		"disabled",
		"4:2:0 MPEG-1",
		"4:2:0 MPEG-2",
		"4:2:2",
		"4:1:0",
		"4:1:1",
	};

	_snprintf(buf, buflen, " (mode: %s)", sModes[mMode]);
}

void ChromaSmootherFilter::ScriptConfig(IScriptInterpreter *isi, FilterActivation *fa, CScriptValue *argv, int argc) {
	mMode = argv[0].asInt();
}

bool ChromaSmootherFilter::ScriptLine(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	_snprintf(buf, buflen, "Config(%d)", mMode);
	return true;
}

class ChromaSmootherFilterDialog : public VDDialogBaseW32 {
public:
	ChromaSmootherFilterDialog(int& mode, IFilterPreview *pifp) : VDDialogBaseW32(IDD_FILTER_CHROMASMOOTHER), mMode(mode), mpPreview(pifp) {}

	bool Activate(VDGUIHandle hParent) {
		return ActivateDialog(hParent) != 0;
	}

protected:
	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_INITDIALOG:
				if (mpPreview)
					mpPreview->InitButton((VDXHWND)GetDlgItem(mhdlg, IDC_PREVIEW));

				switch(mMode) {
				case kModeDisable:
					CheckDlgButton(mhdlg, IDC_MODE_DISABLE, BST_CHECKED);
					break;
				case kMode420MPEG1:
					CheckDlgButton(mhdlg, IDC_MODE_420MPEG1, BST_CHECKED);
					break;
				case kMode420MPEG2:
					CheckDlgButton(mhdlg, IDC_MODE_420MPEG2, BST_CHECKED);
					break;
				case kMode422:
					CheckDlgButton(mhdlg, IDC_MODE_422, BST_CHECKED);
					break;
				case kMode410:
					CheckDlgButton(mhdlg, IDC_MODE_410, BST_CHECKED);
					break;
				case kMode411:
					CheckDlgButton(mhdlg, IDC_MODE_411, BST_CHECKED);
					break;
				}
				return TRUE;

			case WM_COMMAND:
				switch(LOWORD(wParam)) {
				case IDC_MODE_DISABLE:
					if (HIWORD(wParam) == BN_CLICKED) {
						mMode = kModeDisable;
						mpPreview->RedoFrame();
					}
					break;
				case IDC_MODE_420MPEG1:
					if (HIWORD(wParam) == BN_CLICKED) {
						mMode = kMode420MPEG1;
						mpPreview->RedoFrame();
					}
					break;
				case IDC_MODE_420MPEG2:
					if (HIWORD(wParam) == BN_CLICKED) {
						mMode = kMode420MPEG2;
						mpPreview->RedoFrame();
					}
					break;
				case IDC_MODE_422:
					if (HIWORD(wParam) == BN_CLICKED) {
						mMode = kMode422;
						mpPreview->RedoFrame();
					}
					break;
				case IDC_MODE_410:
					if (HIWORD(wParam) == BN_CLICKED) {
						mMode = kMode410;
						mpPreview->RedoFrame();
					}
					break;
				case IDC_MODE_411:
					if (HIWORD(wParam) == BN_CLICKED) {
						mMode = kMode411;
						mpPreview->RedoFrame();
					}
					break;
				case IDOK:
					End(true);
					return TRUE;
				case IDCANCEL:
					End(false);
					return TRUE;
				case IDC_PREVIEW:
					if (mpPreview)
						mpPreview->Toggle((VDXHWND)mhdlg);
					return TRUE;
				}
				break;
		}
		return FALSE;
	}

	IFilterPreview *const mpPreview;
	int&	mMode;
};

int ChromaSmootherFilter::Config(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd) {
	const int mOldMode = mMode;
	ChromaSmootherFilterDialog dlg(mMode, fa->ifp);

	if (!dlg.Activate((VDGUIHandle)hwnd)) {
		mMode = mOldMode;
		return 1;
	}

	return 0;
}

////////////////////

static int chromasmoother_run(const FilterActivation *fa, const FilterFunctions *ff) {
	ChromaSmootherFilter *pf = (ChromaSmootherFilter *)fa->filter_data;
	return pf->Run(fa, ff);
}

static long chromasmoother_param(FilterActivation *fa, const FilterFunctions *ff) {
	ChromaSmootherFilter *pf = (ChromaSmootherFilter *)fa->filter_data;
	return pf->Param(fa, ff);
}

static int chromasmoother_config(FilterActivation *fa, const FilterFunctions *ff, VDXHWND hwnd) {
	ChromaSmootherFilter *pf = (ChromaSmootherFilter *)fa->filter_data;
	return pf->Config(fa, ff, (HWND)hwnd);
}

static int chromasmoother_start(FilterActivation *fa, const FilterFunctions *ff) {
	ChromaSmootherFilter *pf = (ChromaSmootherFilter *)fa->filter_data;
	return pf->Start(fa, ff);
}

static int chromasmoother_stop(FilterActivation *fa, const FilterFunctions *ff) {
	ChromaSmootherFilter *pf = (ChromaSmootherFilter *)fa->filter_data;
	return pf->Stop(fa, ff);
}

static int chromasmoother_init(FilterActivation *fa, const FilterFunctions *ff) {
	try {
		new(fa->filter_data) ChromaSmootherFilter;
	} catch(...) {
		return 1;
	}
	return 0;
}

static void chromasmoother_deinit(FilterActivation *fa, const FilterFunctions *ff) {
	((ChromaSmootherFilter *)fa->filter_data)->~ChromaSmootherFilter();
}

static void chromasmoother_copy(FilterActivation *fa, const FilterFunctions *ff, void *dst) {
	new(dst) ChromaSmootherFilter(*(ChromaSmootherFilter*)fa->filter_data);
}

static void chromasmoother_string2(const FilterActivation *fa, const FilterFunctions *ff, char *buf, int maxlen) {
	((ChromaSmootherFilter *)fa->filter_data)->String2(fa, ff, buf, maxlen);
}

static void chromasmoother_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	((ChromaSmootherFilter *)((FilterActivation *)lpVoid)->filter_data)->ScriptConfig(isi, (FilterActivation *)lpVoid, argv, argc);
}

static ScriptFunctionDef chromasmoother_func_defs[]={
	{ (ScriptFunctionPtr)chromasmoother_script_config, "Config", "0i" },
	{ NULL },
};

static CScriptObject chromasmoother_obj={
	NULL, chromasmoother_func_defs
};

static bool chromasmoother_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	return ((ChromaSmootherFilter *)fa->filter_data)->ScriptLine(fa, ff, buf, buflen);
}


FilterDefinition filterDef_chromasmoother={
	0,0,NULL,
	"chroma smoother",
	"Applies linear interpolation to point-upsampled chroma without affecting luma."
			,
	NULL,NULL,
	sizeof(ChromaSmootherFilter),
	chromasmoother_init,
	chromasmoother_deinit,
	chromasmoother_run,
	chromasmoother_param,
	chromasmoother_config,
	NULL,
	chromasmoother_start,
	chromasmoother_stop,
	&chromasmoother_obj,
	chromasmoother_script_line,
	chromasmoother_string2,
	NULL,
	NULL,
	chromasmoother_copy,
};