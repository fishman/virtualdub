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

#include "filter.h"
#include "VBitmap.h"
#include <vd2/system/cpuaccel.h>
#include "resource.h"
#include "ScriptInterpreter.h"
#include "ScriptValue.h"

#define KERNEL		(7)

extern HINSTANCE g_hInst;

class timesmoothFilterData {
public:
   Pixel32 *accum;
   int framecount;
   int strength;
   int *square_table;
   IFilterPreview *ifp;
   bool bIsFirstFrame;
};

/////////////////////////////////////////////////////////////

long timesmooth_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.offset = fa->src.offset;
	return FILTERPARAM_HAS_LAG(KERNEL/2);
}

int timesmooth_start(FilterActivation *fa, const FilterFunctions *ff) {
	timesmoothFilterData *mfd = (timesmoothFilterData *)fa->filter_data;
	int i;

	mfd->accum = new_nothrow Pixel32[fa->src.w * fa->src.h * KERNEL];
	mfd->square_table = new_nothrow int[255*2+1];

	if (!mfd->accum || !mfd->square_table)
		ff->ExceptOutOfMemory();

	memset(mfd->accum, 0, fa->src.w*fa->src.h*KERNEL*4);

	mfd->framecount = 0;

	for(i=0; i<=510; ++i)
		mfd->square_table[i] = (i-255)*(i-255);

	mfd->bIsFirstFrame = true;

	return 0;
}

int timesmooth_end(FilterActivation *fa, const FilterFunctions *ff) {
	timesmoothFilterData *mfd = (timesmoothFilterData *)fa->filter_data;

	delete[] mfd->accum; mfd->accum = NULL;
	delete[] mfd->square_table; mfd->square_table = NULL;

	return 0;
}

/////////////////////////////////////////////////////////////

#define SCALE(i) (0x0000000100010001i64 * (0x10000 / (i)))
#define SCALE2(i)	SCALE((i)+0),SCALE((i)+1),SCALE((i)+2),SCALE((i)+3),SCALE((i)+4),\
					SCALE((i)+5),SCALE((i)+6),SCALE((i)+7),SCALE((i)+8),SCALE((i)+9)

	static const __int64 scaletab[]={
		0,
		0x00007fff7fff7fffi64,		// special case for 1
		0x00007fff7fff7fffi64,		// special case for 2
		SCALE(3),
		SCALE(4),
		SCALE(5),
		SCALE(6),
		SCALE(7),
		SCALE(8),
		SCALE(9),
		SCALE2(10),
		SCALE2(20),
		SCALE2(30),
		SCALE2(40),
		SCALE2(50),
		SCALE2(60),
		SCALE2(70),
		SCALE2(80),
		SCALE2(90),
		SCALE2(100),
		SCALE2(110),
		SCALE2(120),
	};

#undef SCALE

#ifdef _M_IX86
static void __declspec(naked) asm_timesmooth_run_MMX(Pixel32 *dstbuf, Pixel32 *accumbuf, int w, int h, int modulo, int kernel, int noffset, int coffset, int strength) {
	static const __int64 sixteen = 0x0010001000100010i64;
	static const __int64 noalpha = 0x0000000000ffffffi64;
	static const __int64 noalpha2 = 0x0000ffffffffffffi64;

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		movd		mm6,[esp+36+16]			;strength+32
		pxor		mm7,mm7

		mov			esi,[esp+4+16]			;src/dstbuf
		mov			edi,[esp+8+16]			;accumbuf

		mov			ebp,[esp+16+16]			;hcount
yloop:
		mov			ecx,[esp+12+16]			;wcount
xloop:
		mov			ebx,[esp+28+16]			;replace offset
		mov			eax,[esp+32+16]			;center offset
		movd		mm5,[esi]				;get new pixel
		pand		mm5,noalpha
		movd		[edi+ebx],mm5			;put into place
		movq		mm4,mm7					;clear pixel accumulator
		movd		mm5,[edi+eax]			;get center pixel
		punpcklbw	mm5,mm7
		add			esi,4
		mov			edx,[esp+24+16]			;load kernel length

		;mm4: pixel accumulator
		;mm5: center pixel
		;mm6: shift
		;mm7: zero

timeloop:
		movd		mm0,[edi]
		movq		mm1,mm5

		punpcklbw	mm0,mm7

		psubw		mm1,mm0

		pmaddwd		mm1,mm1
		add			edi,4

		punpckldq	mm2,mm1

		paddd		mm1,mm2					;mm0 = |r|^2 + |g|^2 + |b|^2 in high dword

		movq		mm2,sixteen
		psrlq		mm1,mm6

		punpcklwd	mm1,mm1

		punpckldq	mm1,mm1

		psubusw		mm2,mm1

		pmullw		mm0,mm2

		psllq		mm2,48

		paddw		mm4,mm2					;add scale to scale accumulator
		sub			edx,1
		paddw		mm4,mm0					;add scaled pixel to pixel accumulator
		jne			timeloop

		movq		mm0,mm4
		psrlq		mm4,48

		movd		eax,mm4					;eax = scale accumulator

		pmulhw		mm0,[scaletab+eax*8]	;produce output pixel

		packuswb	mm0,mm0
		dec			ecx

;		mov			eax,[esp+32+16]			;center offset
;		movd		[edi+ebx],mm0			;put into place

		movd		[esi-4],mm0
		jne			xloop

		add			esi,[esp+20+16]			;skip to next scanline

		dec			ebp
		jne			yloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		emms
		ret
	};
}
#endif

static int timesmooth_run(const FilterActivation *fa, const FilterFunctions *ff) {
	timesmoothFilterData *sfd = (timesmoothFilterData *)fa->filter_data;
	int offset1 = sfd->framecount;
	int offset2 = ((sfd->framecount+KERNEL-(KERNEL/2))%KERNEL);

	if (sfd->bIsFirstFrame) {
		PixDim w = fa->src.w, x;
		PixDim y = fa->src.h;
		Pixel32 *srcdst = fa->src.data;
		Pixel32 *accum = sfd->accum;

		do {
			x = w;

			do {
				accum[0] = accum[1] = accum[2] = accum[3] =
					accum[4] = accum[5] = accum[6] = *srcdst++ & 0xffffff;

				accum += 7;
			} while(--x);

			srcdst = (Pixel32 *)((char *)srcdst + fa->src.modulo);

		} while(--y);

		sfd->bIsFirstFrame = false;

		sfd->framecount = 1;

		return 0;
	}

#ifdef _M_IX86
	if (MMX_enabled)
		asm_timesmooth_run_MMX(fa->src.data, sfd->accum, fa->src.w, fa->src.h, fa->src.modulo, KERNEL, offset1*4, offset2*4, sfd->strength+32);
	else
#endif
	{
		PixDim w = fa->src.w, x;
		PixDim y = fa->src.h;
		Pixel32 *srcdst = fa->src.data;
		Pixel32 *accum = sfd->accum;
		int *squaretab = sfd->square_table;
		int strength = sfd->strength;
		int i;

		do {
			x = w;
			do {
				const Pixel32 center = accum[offset2];
				const int *crtab = squaretab + 255 - ((center>>16)&0xff);
				const int *cgtab = squaretab + 255 - ((center>> 8)&0xff);
				const int *cbtab = squaretab + 255 - ((center    )&0xff);
				int raccum = 0, gaccum = 0, baccum = 0;
				int count = 0;

				accum[offset1] = *srcdst & 0xffffff;

				for(i=0; i<KERNEL; ++i) {
					const Pixel32 c = *accum++;
					int cr = (c>>16)&0xff;
					int cg = (c&0xff00)>>8;
					int cb = c&0xff;
					int sqerr = (crtab[cr] + cgtab[cg] + cbtab[cb]) >> strength;

					if (sqerr > 16)
						sqerr = 16;

					sqerr = 16 - sqerr;

					raccum += cr * sqerr;
					gaccum += cg * sqerr;
					baccum += cb * sqerr;
					count += sqerr;
				}

				int divisor = ((long *)scaletab)[2*count+1];

				raccum = (raccum * divisor)>>16;
				gaccum = (gaccum * divisor)>>16;
				baccum = (baccum * divisor)>>16;

				*srcdst++ = (raccum<<16) + (gaccum<<8) + baccum;
				
			} while(--x);

			srcdst = (Pixel32 *)((char *)srcdst + fa->src.modulo);

		} while(--y);
	}

	if (++sfd->framecount >= KERNEL)
		sfd->framecount = 0;

	return 0;
}

static void timesmooth_string(const FilterActivation *fa, const FilterFunctions *ff, char *buf) {
	timesmoothFilterData *mfd = (timesmoothFilterData *)fa->filter_data;

	wsprintf(buf, " (%d)", mfd->strength);
}

static INT_PTR CALLBACK timesmoothDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	timesmoothFilterData *mfd = (timesmoothFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				HWND hwndItem;

				mfd = (timesmoothFilterData *)lParam;

				SetWindowLongPtr(hDlg, DWLP_USER, (LONG)mfd);

				hwndItem = GetDlgItem(hDlg, IDC_STRENGTH);
				SendMessage(hwndItem, TBM_SETRANGE, TRUE, MAKELONG(0, 10));
				SendMessage(hwndItem, TBM_SETPOS, TRUE, mfd->strength);

				mfd->ifp->InitButton((VDXHWND)GetDlgItem(hDlg, IDC_PREVIEW));
			}
            return (TRUE);

        case WM_COMMAND:                      
			switch(LOWORD(wParam)) {
			case IDOK:
				mfd->ifp->Close();
				EndDialog(hDlg, 0);
				return TRUE;

			case IDCANCEL:
				mfd->ifp->Close();
                EndDialog(hDlg, 1);
                return TRUE;

			case IDC_PREVIEW:
				mfd->ifp->Toggle((VDXHWND)hDlg);
				return TRUE;
			}
			break;

		case WM_HSCROLL:
			mfd->strength = SendDlgItemMessage(hDlg, IDC_STRENGTH, TBM_GETPOS, 0, 0);
			mfd->ifp->RedoFrame();
			break;

    }
    return FALSE;
}

static int timesmooth_config(FilterActivation *fa, const FilterFunctions *ff, VDXHWND hWnd) {
	timesmoothFilterData *mfd = (timesmoothFilterData *)fa->filter_data;
	timesmoothFilterData mfd2 = *mfd;
	int ret;

	mfd->ifp = fa->ifp;

	ret = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_TIMESMOOTH), (HWND)hWnd, timesmoothDlgProc, (LPARAM)mfd);

	if (ret)
		*mfd = mfd2;

	return ret;
}

/////////////////////////////////////////////////////////////

static void timesmooth_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;

	timesmoothFilterData *mfd = (timesmoothFilterData *)fa->filter_data;

	mfd->strength = argv[0].asInt();
}

static ScriptFunctionDef timesmooth_func_defs[]={
	{ (ScriptFunctionPtr)timesmooth_script_config, "Config", "0i" },
	{ NULL },
};

static CScriptObject timesmooth_obj={
	NULL, timesmooth_func_defs
};

static bool timesmooth_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	timesmoothFilterData *mfd = (timesmoothFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d)", mfd->strength);

	return true;
}

/////////////////////////////////////////////////////////////

extern const VDXFilterDefinition filterDef_timesmooth={
	0,0,NULL,
	"temporal smoother",
	"Performs an adaptive smooth along the time axis.\n\n[Assembly optimized][MMX optimized][Lag: 4]",
	NULL,
	NULL,sizeof(timesmoothFilterData),
	NULL,NULL,
	timesmooth_run,
	timesmooth_param,
	timesmooth_config,
	timesmooth_string,
	timesmooth_start,
	timesmooth_end,

	&timesmooth_obj,
	timesmooth_script_line,
};
