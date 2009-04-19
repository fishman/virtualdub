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

#include "resource.h"
#include "filter.h"
#include "gui.h"
#include "VBitmap.h"
#include <vd2/system/cpuaccel.h>

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

extern HINSTANCE g_hInst;

#if defined(VD_COMPILER_MSVC) && defined(VD_CPU_X86)
	#pragma warning(disable: 4799)		// warning C4799: function has no EMMS instruction
#endif

///////////////////////////////////////////////////////////////////////////

enum {
	MODE_BLEND		= 0,
	MODE_DUP1		= 1,
	MODE_DUP2		= 2,
	MODE_DISCARD1	= 3,
	MODE_DISCARD2	= 4,
	MODE_UNFOLD		= 5,
	MODE_FOLD		= 6
};

typedef struct MyFilterData {
	int mode;
} MyFilterData;

static const char *g_szModes[]={
	"blend",
	"dup 1",
	"dup 2",
	"discard 1",
	"discard 2",
	"unfold",
	"fold"
};

///////////////////////////////////////////////////////////////////////////

#ifdef _M_IX86
static void __declspec(naked) asm_blend_row_clipped(Pixel *dst, const Pixel *src, PixDim w, PixOffset srcpitch) {
	__asm {
		push	ebp
		push	edi
		push	esi
		push	ebx

		mov		edi,[esp+20]
		mov		esi,[esp+24]
		sub		edi,esi
		mov		ebp,[esp+28]
		mov		edx,[esp+32]

xloop:
		mov		ecx,[esi]
		mov		eax,0fefefefeh

		mov		ebx,[esi+edx]
		and		eax,ecx

		shr		eax,1
		and		ebx,0fefefefeh

		shr		ebx,1
		add		esi,4

		add		eax,ebx
		dec		ebp

		mov		[edi+esi-4],eax
		jnz		xloop

		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret
	};
}

static void __declspec(naked) asm_blend_row(Pixel *dst, const Pixel *src, PixDim w, PixOffset srcpitch) {
	__asm {
		push	ebp
		push	edi
		push	esi
		push	ebx

		mov		edi,[esp+20]
		mov		esi,[esp+24]
		sub		edi,esi
		mov		ebp,[esp+28]
		mov		edx,[esp+32]

xloop:
		mov		ecx,[esi]
		mov		eax,0fcfcfcfch

		mov		ebx,[esi+edx]
		and		eax,ecx

		shr		ebx,1
		mov		ecx,[esi+edx*2]

		shr		ecx,2
		and		ebx,07f7f7f7fh

		shr		eax,2
		and		ecx,03f3f3f3fh

		add		eax,ebx
		add		esi,4

		add		eax,ecx
		dec		ebp

		mov		[edi+esi-4],eax
		jnz		xloop

		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret
	};
}

static void __declspec(naked) asm_blend_row_MMX(Pixel *dst, const Pixel *src, PixDim w, PixOffset srcpitch) {
	static const __int64 mask0 = 0xfcfcfcfcfcfcfcfci64;
	static const __int64 mask1 = 0x7f7f7f7f7f7f7f7fi64;
	static const __int64 mask2 = 0x3f3f3f3f3f3f3f3fi64;
	__asm {
		push	ebp
		push	edi
		push	esi
		push	ebx

		mov		edi,[esp+20]
		mov		esi,[esp+24]
		sub		edi,esi
		mov		ebp,[esp+28]
		mov		edx,[esp+32]

		movq	mm5,mask0
		movq	mm6,mask1
		movq	mm7,mask2
		shr		ebp,1
		jz		oddpart

xloop:
		movq	mm2,[esi]
		movq	mm0,mm5

		movq	mm1,[esi+edx]
		pand	mm0,mm2

		psrlq	mm1,1
		movq	mm2,[esi+edx*2]

		psrlq	mm2,2
		pand	mm1,mm6

		psrlq	mm0,2
		pand	mm2,mm7

		paddb	mm0,mm1
		add		esi,8

		paddb	mm0,mm2
		dec		ebp

		movq	[edi+esi-8],mm0
		jne		xloop

oddpart:
		test	byte ptr [esp+28],1
		jz		nooddpart

		mov		ecx,[esi]
		mov		eax,0fcfcfcfch
		mov		ebx,[esi+edx]
		and		eax,ecx
		shr		ebx,1
		mov		ecx,[esi+edx*2]
		shr		ecx,2
		and		ebx,07f7f7f7fh
		shr		eax,2
		and		ecx,03f3f3f3fh
		add		eax,ebx
		add		eax,ecx
		mov		[edi+esi],eax

nooddpart:

		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret
	};
}
#else
static void asm_blend_row_clipped(Pixel *dst, const Pixel *src, PixDim w, PixOffset srcpitch) {
	const Pixel *src2 = (const Pixel *)((const char *)src + srcpitch);

	do {
		const uint32 x = *src++;
		const uint32 y = *src2++;

		*dst++ = (x|y) - (((x^y)&0xfefefefe)>>1);
	} while(--w);
}

static void asm_blend_row(Pixel *dst, const Pixel *src, PixDim w, PixOffset srcpitch) {
	const Pixel *src2 = (const Pixel *)((const char *)src + srcpitch);
	const Pixel *src3 = (const Pixel *)((const char *)src2 + srcpitch);

	do {
		const uint32 a = *src++;
		const uint32 b = *src2++;
		const uint32 c = *src3++;
		const uint32 hi = (a & 0xfcfcfc) + 2*(b & 0xfcfcfc) + (c & 0xfcfcfc);
		const uint32 lo = (a & 0x030303) + 2*(b & 0x030303) + (c & 0x030303) + 0x020202;

		*dst++ = (hi + (lo & 0x0c0c0c))>>2;
	} while(--w);
}
#endif

static int deinterlace_run(const FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	Pixel32 *src = fa->src.data;
	Pixel32 *dst = fa->dst.data;
	PixDim h = fa->src.h;
	void (*blend_func)(Pixel32 *, const Pixel32 *, PixDim, PixOffset);

	switch(mfd->mode) {
	case MODE_BLEND:
		if (h<2) return 0;

#ifdef _M_IX86
		blend_func = MMX_enabled ? asm_blend_row_MMX : asm_blend_row;
#else
		blend_func = asm_blend_row;
#endif

		asm_blend_row_clipped(dst, src, fa->src.w, fa->src.pitch);
		if (h-=2)
			do {
				dst = (Pixel *)((char *)dst + fa->dst.pitch);

				blend_func(dst, src, fa->src.w, fa->src.pitch);

				src = (Pixel *)((char *)src + fa->src.pitch);
			} while(--h);

		asm_blend_row_clipped((Pixel *)((char *)dst + fa->dst.pitch), src, fa->src.w, fa->src.pitch);

#ifdef _M_IX86
		if (MMX_enabled)
			__asm emms
#endif

		break;
	case MODE_DUP2:
		src = (Pixel *)((char *)src + fa->src.pitch);
		dst = (Pixel *)((char *)dst - fa->dst.pitch);
	case MODE_DUP1:
		dst = (Pixel *)((char *)dst + fa->dst.pitch);

		if (h>>=1)
			do {
				memcpy(dst, src, fa->src.w*4);

				src = (Pixel *)((char *)src + fa->src.pitch*2);
				dst = (Pixel *)((char *)dst + fa->dst.pitch*2);
			} while(--h);
		break;

	case MODE_UNFOLD:
		{
			VBitmap vbmsrc((VBitmap&)fa->src);

			vbmsrc.modulo += vbmsrc.pitch;
			vbmsrc.pitch<<=1;
			vbmsrc.h >>= 1;

			((VBitmap&)fa->dst).BitBlt(0, 0, &vbmsrc, 0, 0, -1, -1);

			vbmsrc.data = (Pixel32 *)((char *)vbmsrc.data + fa->src.pitch);

			((VBitmap&)fa->dst).BitBlt(fa->src.w, 0, (const VBitmap *)&vbmsrc, 0, 0, -1, -1);
		}
		break;

	case MODE_FOLD:
		{
			VBitmap vbmdst((VBitmap&)fa->dst);

			vbmdst.modulo += vbmdst.pitch;
			vbmdst.pitch<<=1;
			vbmdst.h >>= 1;

			vbmdst.BitBlt(0, 0, (const VBitmap *)&fa->src, 0, 0, -1, -1);

			vbmdst.data = (Pixel32 *)((char *)vbmdst.data + fa->dst.pitch);

			vbmdst.BitBlt(0, 0, (const VBitmap *)&fa->src, fa->dst.w, 0, -1, -1);
		}
		break;
	}

	return 0;
}

static long deinterlace_param(FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	switch(mfd->mode) {
	case MODE_BLEND:
		return FILTERPARAM_SWAP_BUFFERS;

	case MODE_DUP1:
	case MODE_DUP2:
		fa->dst.offset = fa->src.offset;
		break;

	case MODE_DISCARD1:
		fa->dst.offset	= fa->src.offset + fa->dst.pitch;
		fa->dst.h		>>= 1;
		fa->dst.pitch	<<= 1;
		fa->dst.modulo	= fa->dst.pitch - 4*fa->dst.w;
		break;
	case MODE_DISCARD2:
		fa->dst.offset	= fa->src.offset;
		fa->dst.h		>>= 1;
		fa->dst.pitch	<<= 1;
		fa->dst.modulo	= fa->dst.pitch - 4*fa->dst.w;
		break;

	case MODE_UNFOLD:
		fa->dst.h		>>= 1;
		fa->dst.w		<<=1;
		fa->dst.AlignTo8();
		return FILTERPARAM_SWAP_BUFFERS;

	case MODE_FOLD:
		fa->dst.h		<<= 1;
		fa->dst.w		>>= 1;
		fa->dst.AlignTo8();
		return FILTERPARAM_SWAP_BUFFERS;
	}

	return 0;
}

static INT_PTR CALLBACK DeinterlaceDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	MyFilterData *mfd;

    switch (message)
    {
        case WM_INITDIALOG:
			{
				mfd = (MyFilterData *)lParam;
				SetWindowLongPtr(hDlg, DWLP_USER, (LONG)mfd);

				switch(mfd->mode) {
				case MODE_BLEND:		CheckDlgButton(hDlg, IDC_MODE_BLEND, BST_CHECKED); break;
				case MODE_DUP1:			CheckDlgButton(hDlg, IDC_MODE_DUP1, BST_CHECKED); break;
				case MODE_DUP2:			CheckDlgButton(hDlg, IDC_MODE_DUP2, BST_CHECKED); break;
				case MODE_DISCARD1:		CheckDlgButton(hDlg, IDC_MODE_DISCARD1, BST_CHECKED); break;
				case MODE_DISCARD2:		CheckDlgButton(hDlg, IDC_MODE_DISCARD2, BST_CHECKED); break;
				case MODE_UNFOLD:		CheckDlgButton(hDlg, IDC_MODE_UNFOLD, BST_CHECKED); break;
				case MODE_FOLD:			CheckDlgButton(hDlg, IDC_MODE_FOLD, BST_CHECKED); break;
				}
			}

            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					mfd = (MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
					
					if (IsDlgButtonChecked(hDlg, IDC_MODE_BLEND))		mfd->mode = MODE_BLEND;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_DUP1))		mfd->mode = MODE_DUP1;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_DUP2))		mfd->mode = MODE_DUP2;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_DISCARD1))	mfd->mode = MODE_DISCARD1;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_DISCARD2))	mfd->mode = MODE_DISCARD2;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_UNFOLD))		mfd->mode = MODE_UNFOLD;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_FOLD))		mfd->mode = MODE_FOLD;

					EndDialog(hDlg, 0);
				}
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, 1);
				return TRUE;

			}
            break;

    }
    return FALSE;
}

static int deinterlace_config(FilterActivation *fa, const FilterFunctions *ff, VDXHWND hWnd) {
	return DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_DEINTERLACE), (HWND)hWnd, DeinterlaceDlgProc, (LPARAM)fa->filter_data);
}

static void deinterlace_string(const FilterActivation *fa, const FilterFunctions *ff, char *buf) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	wsprintf(buf, " (mode: %s)", g_szModes[mfd->mode]);
}

static void deinterlace_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	mfd->mode	= argv[0].asInt();

	if (mfd->mode < 0 || mfd->mode > 6)
		mfd->mode = 0;
}

static ScriptFunctionDef deinterlace_func_defs[]={
	{ (ScriptFunctionPtr)deinterlace_script_config, "Config", "0i" },
	{ NULL },
};

static CScriptObject deinterlace_obj={
	NULL, deinterlace_func_defs
};

static bool deinterlace_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d)", mfd->mode);

	return true;
}

FilterDefinition filterDef_deinterlace={
	0,0,NULL,
	"deinterlace",
	"Removes scanline artifacts from interlaced video.\r\n\r\n[Assembly optimized][MMX optimized]",
	NULL,NULL,
	sizeof(MyFilterData),
	NULL,NULL,
	deinterlace_run,
	deinterlace_param,
	deinterlace_config,
	deinterlace_string,
	NULL,
	NULL,

	&deinterlace_obj,
	deinterlace_script_line,
};

