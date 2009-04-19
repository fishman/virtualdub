//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2002 Avery Lee
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
#include <new>

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

#include "misc.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/filesys.h>
#include "resource.h"
#include "gui.h"
#include "filter.h"
#include "vbitmap.h"
#include "oshelper.h"
#include "image.h"
#include <vd2/system/error.h>

extern HINSTANCE g_hInst;

///////////////////////////////////////////////////////////////////////////

typedef struct LogoFilterData {
	char	szLogoPath[MAX_PATH];
	char	szAlphaPath[MAX_PATH];

	int		pos_x, pos_y;
	int		justify_x, justify_y;
	int		opacity;

	bool	bEnableAlphaBlending;
	bool	bNonPremultAlpha;
	bool	bEnableSecondaryAlpha;

	bool	bAlphaBlendingRequired;
	VBitmap	vbLogo;
	IFilterPreview *ifp;
} LogoFilterData;

///////////////////////////////////////////////////////////////////////////

// The Blinn /255 rounded algorithm:
//
// i = a*b + 128;
// y = (i + (i>>8)) >> 8;

static void AlphaBltSCALAR(Pixel32 *dst, PixOffset dstoff, const Pixel32 *src, PixOffset srcoff, PixDim w, PixDim h) {
	if (w<=0 || h<=0)
		return;

	srcoff -= 4*w;
	dstoff -= 4*w;

	do {
		int w2 = w;

		do {
			const Pixel32 x = *dst++;
			const Pixel32 y = *src++;
			const Pixel32 a = y >> 24;
			Pixel32 r = ((x>>16)&0xff)*a + 128;
			Pixel32 g = ((x>> 8)&0xff)*a + 128;
			Pixel32 b = ((x    )&0xff)*a + 128;

			r = (((r + (r>>8)) << 8)&0xff0000) + (y & 0xff0000);
			g = (((g + (g>>8))     )&0x00ff00) + (y & 0x00ff00);
			b = (((b + (b>>8)) >> 8)&0x0000ff) + (y & 0x0000ff);

			if (r >= 0x01000000) r = 0x00ff0000;
			if (g >= 0x00010000) g = 0x0000ff00;
			if (b >= 0x00000100) b = 0x000000ff;

			dst[-1] = r + g + b;
		} while(--w2);
		src = (Pixel32 *)((char *)src + srcoff);
		dst = (Pixel32 *)((char *)dst + dstoff);
	} while(--h);
}

static void CombineAlphaBltSCALAR(Pixel32 *dst, PixOffset dstoff, const Pixel32 *src, PixOffset srcoff, PixDim w, PixDim h) {
	if (w<=0 || h<=0)
		return;

	srcoff -= 4*w;
	dstoff -= 4*w;

	do {
		int w2 = w;
		do {
			const Pixel32 x = *dst++;
			const Pixel32 y = *src++;
			const Pixel32 a = (((y>>16)&0xff)*0x4CCCCC + ((y>>8)&0xff)*0x970A3E + (y&0xff)*0x1C28F6 + 0x800000) & 0xff000000;

			dst[-1] = (x & 0x00ffffff) + a;
		} while(--w2);
		src = (Pixel32 *)((char *)src + srcoff);
		dst = (Pixel32 *)((char *)dst + dstoff);
	} while(--h);
}

static void PremultiplyAlphaSCALAR(Pixel32 *dst, PixOffset dstoff, PixDim w, PixDim h) {
	if (w<=0 || h<=0)
		return;

	dstoff -= 4*w;

	do {
		int w2 = w;
		do {
			const Pixel32 x = *dst++;
			const Pixel32 a = x >> 24;
			Pixel32 r = ((x>>16)&0xff) * a;
			Pixel32 g = ((x>> 8)&0xff) * a;
			Pixel32 b = ((x    )&0xff) * a;

			r = (r + (r>>8))>>8;
			g = (g + (g>>8))>>8;
			b = (b + (b>>8))>>8;

			dst[-1] = (a<<24) + (r<<16) + (g<<8) + b;
		} while(--w2);
		dst = (Pixel32 *)((char *)dst + dstoff);
	} while(--h);
}

static void ScalePremultipliedAlphaSCALAR(Pixel32 *dst, PixOffset dstoff, PixDim w, PixDim h, unsigned scale8) {
	if (w<=0 || h<=0)
		return;

	dstoff -= 4*w;

	do {
		int w2 = w;
		do {
			const Pixel32 x = *dst;
			Pixel32 ag = ((x>>8)&0xff00ff) * scale8 + 0x800080;
			Pixel32 rb = ((x   )&0xff00ff) * scale8 + 0x800080;

			*dst = (ag & 0xff00ff00) + ((rb & 0xff00ff00)>>8);
			++dst;
		} while(--w2);
		dst = (Pixel32 *)((char *)dst + dstoff);
	} while(--h);
}

#ifdef _M_IX86
static void __declspec(naked) __cdecl AlphaBltMMX(Pixel32 *dst, PixOffset dstoff, const Pixel32 *src, PixOffset srcoff, PixDim w, PixDim h) {
	static const __int64 x80w = 0x0080008000800080;
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx
		mov			eax, [esp+20+16]
		mov			ecx, [esp+4+16]
		shl			eax, 2
		mov			edx, [esp+12+16]
		movq		mm6, x80w
		pxor		mm7, mm7
		mov			ebx, [esp+24+16]
		add			ecx, eax
		add			edx, eax
		neg			eax
		mov			edi, [esp+8+16]
		mov			[esp+20+16],eax
		mov			esi, [esp+16+16]
yloop:
		mov			eax, [esp+20+16]
xloop:
		movd		mm0, [ecx+eax]

		movd		mm1, [edx+eax]
		punpcklbw	mm0, mm7

		movq		mm2, mm1
		paddw		mm0, mm0

		punpcklbw	mm2, mm2
		paddw		mm0, mm0

		punpckhwd	mm2, mm2
		punpckhdq	mm2, mm2
		movq		mm3, mm2
		psrlw		mm2, 2
		psraw		mm3, 15
		psubw		mm2, mm3

		pmulhw		mm0, mm2
		packuswb	mm0, mm0
		paddusb		mm0, mm1
		movd		[ecx+eax], mm0

		add			eax, 4
		jne			xloop

		add			ecx, edi
		add			edx, esi
		dec			ebx
		jne			yloop

		emms
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
	}
}

static void __declspec(naked) __cdecl CombineAlphaBltMMX(Pixel32 *dst, PixOffset dstoff, const Pixel32 *src, PixOffset srcoff, PixDim w, PixDim h) {
	// 0.30 0.59 0.11
	static const __int64 lumafact = 0x000026664B860E14;
	static const __int64 rounder  = 0x0000800000000000;
	static const __int64 x80w	  = 0x0080008000800080;
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx
		mov			eax, [esp+20+16]
		mov			ecx, [esp+4+16]
		shl			eax, 2
		mov			edx, [esp+12+16]
		movq		mm5, lumafact
		pcmpeqd		mm4, mm4
		pslld		mm4, 24
		movq		mm3, rounder
		pxor		mm7, mm7
		mov			ebx, [esp+24+16]
		add			ecx, eax
		add			edx, eax
		neg			eax
		mov			edi, [esp+8+16]
		mov			[esp+20+16],eax
		mov			esi, [esp+16+16]
yloop:
		mov			eax, [esp+20+16]
xloop:
		movd		mm1, [edx+eax]
		movq		mm0, mm4
		pandn		mm0, [ecx+eax]
		punpcklbw	mm1, mm7
		paddw		mm1, mm1

		pmaddwd		mm1, mm5				;mm1 = (luma1 << 16) + (luma2 << 48)
		punpckldq	mm2, mm1
		paddd		mm1, mm3
		paddd		mm1, mm2
		packuswb	mm1, mm1
		pand		mm1, mm4
		paddb		mm0, mm1

		movd		[ecx+eax], mm0

		add			eax, 4
		jne			xloop

		add			ecx, edi
		add			edx, esi
		dec			ebx
		jne			yloop

		emms
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
	}
}

static void __declspec(naked) __cdecl PremultiplyAlphaMMX(Pixel32 *dst, PixOffset dstoff, PixDim w, PixDim h) {
	// 0.30 0.59 0.11
	static const __int64 lumafact = 0x000026664B860E14;
	static const __int64 rounder  = 0x0000800000000000;
	static const __int64 x80w	  = 0x0080008000800080;
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx
		mov			eax, [esp+12+16]
		mov			ecx, [esp+4+16]
		shl			eax, 2
		movq		mm5, lumafact
		pcmpeqd		mm4, mm4
		pslld		mm4, 24
		movq		mm3, rounder
		pxor		mm7, mm7
		mov			ebx, [esp+16+16]
		add			ecx, eax
		add			edx, eax
		neg			eax
		mov			edi, [esp+8+16]
		mov			[esp+12+16],eax
yloop:
		mov			eax, [esp+12+16]
xloop:
		movd		mm1, [ecx+eax]
		movq		mm0, mm4
		por			mm0, mm1
		punpcklbw	mm1, mm7
		punpcklbw	mm0, mm7

		punpckhwd	mm1, mm1
		punpckhdq	mm1, mm1

		pmullw		mm0, mm1
		paddw		mm0, x80w
		movq		mm1, mm0
		psrlw		mm1, 8
		paddw		mm0, mm1
		psrlw		mm0, 8
		packuswb	mm0, mm0

		movd		[ecx+eax], mm0

		add			eax, 4
		jne			xloop

		add			ecx, edi
		dec			ebx
		jne			yloop

		emms
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
	}
}

static void __declspec(naked) __cdecl ScalePremultipliedAlphaMMX(Pixel32 *dst, PixOffset dstoff, PixDim w, PixDim h, unsigned alpha) {
	static const __int64 x80w	  = 0x0080008000800080;
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx
		mov			eax, [esp+12+16]
		mov			ecx, [esp+4+16]
		shl			eax, 2
		movd		mm5, [esp+20+16]
		pxor		mm7, mm7
		movq		mm4, x80w
		punpcklwd	mm5, mm5
		punpckldq	mm5, mm5
		mov			ebx, [esp+16+16]
		add			ecx, eax
		add			edx, eax
		neg			eax
		mov			edi, [esp+8+16]
		mov			[esp+12+16],eax
yloop:
		mov			eax, [esp+12+16]
xloop:
		movd		mm0, [ecx+eax]
		punpcklbw	mm0, mm7
		pmullw		mm0, mm5
		paddw		mm0, mm4
		psrlw		mm0, 8
		packuswb	mm0, mm0
		movd		[ecx+eax], mm0

		add			eax, 4
		jne			xloop

		add			ecx, edi
		dec			ebx
		jne			yloop

		emms
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
	}
}
#endif

///////////////////////////////////////////////////////////////////////////

static bool dualclip(const VBitmap& v1, int& x1, int& y1, const VBitmap& v2, int& x2, int& y2, int& w, int& h) {
	if (x1 < 0) { x2 -= x1; w += x1; x1 = 0; }
	if (y1 < 0) { y2 -= y1; h += y1; y1 = 0; }
	if (x2 < 0) { x1 -= x2; w += x2; x2 = 0; }
	if (y2 < 0) { y1 -= y2; h += y2; y2 = 0; }
	if (x1+w > v1.w) { w = v1.w - x1; }
	if (y1+h > v1.h) { h = v1.h - y1; }
	if (x2+w > v2.w) { w = v2.w - x2; }
	if (y2+h > v2.h) { h = v2.h - y2; }

	return w>0 && h>0;
}

static void AlphaBlt(VBitmap& dst, int x1, int y1, const VBitmap& src, int x2, int y2, int w, int h) {
	if (!dualclip(dst, x1, y1, src, x2, y2, w, h))
		return;

#ifdef _M_IX86
	(MMX_enabled ? AlphaBltMMX : AlphaBltSCALAR)(dst.Address32(x1, y1+h-1), dst.pitch, src.Address32(x2, y2+h-1), src.pitch, w, h);
#else
	AlphaBltSCALAR(dst.Address32(x1, y1+h-1), dst.pitch, src.Address32(x2, y2+h-1), src.pitch, w, h);
#endif
}

static void CombineAlphaBlt(VBitmap& dst, int x1, int y1, const VBitmap& src, int x2, int y2, int w, int h) {
	if (!dualclip(dst, x1, y1, src, x2, y2, w, h))
		return;

#ifdef _M_IX86
	(MMX_enabled ? CombineAlphaBltMMX : CombineAlphaBltSCALAR)(dst.Address32(x1, y1+h-1), dst.pitch, src.Address32(x2, y2+h-1), src.pitch, w, h);
#else
	CombineAlphaBltSCALAR(dst.Address32(x1, y1+h-1), dst.pitch, src.Address32(x2, y2+h-1), src.pitch, w, h);
#endif
}

static void PremultiplyAlpha(VBitmap& dst, int x1, int y1, int w, int h) {
	if (x1   < 0   ) { w += x1; x1 = 0; }
	if (y1   < 0   ) { h += y1; y1 = 0; }
	if (x1+w > dst.w) { w = dst.w - x1; }
	if (y1+h > dst.h) { h = dst.h - y1; }

	if (w<=0 || h<=0)
		return;

#ifdef _M_IX86
	(MMX_enabled ? PremultiplyAlphaMMX : PremultiplyAlphaSCALAR)(dst.Address32(x1, y1+h-1), dst.pitch, w, h);
#else
	PremultiplyAlphaSCALAR(dst.Address32(x1, y1+h-1), dst.pitch, w, h);
#endif
}

static void ScalePremultipliedAlpha(VBitmap& dst, int x1, int y1, int w, int h, unsigned scale8) {
	if (x1   < 0   ) { w += x1; x1 = 0; }
	if (y1   < 0   ) { h += y1; y1 = 0; }
	if (x1+w > dst.w) { w = dst.w - x1; }
	if (y1+h > dst.h) { h = dst.h - y1; }

	if (w<=0 || h<=0)
		return;

#ifdef _M_IX86
	(MMX_enabled ? ScalePremultipliedAlphaMMX : ScalePremultipliedAlphaSCALAR)(dst.Address32(x1, y1+h-1), dst.pitch, w, h, scale8);
#else
	ScalePremultipliedAlphaSCALAR(dst.Address32(x1, y1+h-1), dst.pitch, w, h, scale8);
#endif
}

static void SetAlpha(VBitmap& vbdst, int x1, int y1, int w, int h) {
	if (x1   < 0   ) { w += x1; x1 = 0; }
	if (y1   < 0   ) { h += y1; y1 = 0; }
	if (x1+w > vbdst.w) { w = vbdst.w - x1; }
	if (y1+h > vbdst.h) { h = vbdst.h - y1; }

	if (w<=0 || h<=0)
		return;

	Pixel32 *dst = vbdst.Address32(x1, y1+h-1);
	PixOffset dstoff = vbdst.pitch - 4*w;

	do {
		int w2 = w;
		do {
			*dst++ |= 0xff000000;
		} while(--w2);
		dst = (Pixel32 *)((char *)dst + dstoff);
	} while(--h);
}

static void InvertAlpha(VBitmap& vbdst, int x1, int y1, int w, int h) {
	if (x1   < 0   ) { w += x1; x1 = 0; }
	if (y1   < 0   ) { h += y1; y1 = 0; }
	if (x1+w > vbdst.w) { w = vbdst.w - x1; }
	if (y1+h > vbdst.h) { h = vbdst.h - y1; }

	if (w<=0 || h<=0)
		return;

	Pixel32 *dst = vbdst.Address32(x1, y1+h-1);
	PixOffset dstoff = vbdst.pitch - 4*w;

	do {
		int w2 = w;
		do {
			*dst++ ^= 0xff000000;
		} while(--w2);
		dst = (Pixel32 *)((char *)dst + dstoff);
	} while(--h);
}

///////////////////////////////////////////////////////////////////////////

static int logo_run(const FilterActivation *fa, const FilterFunctions *ff) {
	LogoFilterData *mfd = (LogoFilterData *)fa->filter_data;
	int x = mfd->pos_x + (((fa->dst.w - mfd->vbLogo.w) * mfd->justify_x + 1)>>1);
	int y = mfd->pos_y + (((fa->dst.h - mfd->vbLogo.h) * mfd->justify_y + 1)>>1);

	if (mfd->bAlphaBlendingRequired) {
		AlphaBlt((VBitmap&)fa->dst, x, y, mfd->vbLogo, 0, 0, fa->dst.w, fa->dst.h);
	} else
		((VBitmap&)fa->dst).BitBlt(x, y, &mfd->vbLogo, 0, 0, -1, -1);

	return 0;
}

static long logo_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.offset = fa->src.offset;
	return 0;
}

static int logo_init(FilterActivation *fa, const FilterFunctions *ff) {
	LogoFilterData *mfd = (LogoFilterData *)fa->filter_data;

	mfd->opacity = 0x10000;
	return 0;
}

static int logo_start(FilterActivation *fa, const FilterFunctions *ff) {
	LogoFilterData *mfd = (LogoFilterData *)fa->filter_data;

	new(&mfd->vbLogo) VBitmap; // I don't want to hear it....

	bool bHasAlpha;

	DecodeImage(mfd->szLogoPath, mfd->vbLogo, 32, bHasAlpha);

	if (mfd->bEnableAlphaBlending) {
		if (mfd->bEnableSecondaryAlpha) {
			VBitmap vbAlphaLogo;
			vbAlphaLogo.data = NULL;
			try {
				bool bSecondHasAlpha;
				DecodeImage(mfd->szAlphaPath, vbAlphaLogo, 32, bSecondHasAlpha);
				if (vbAlphaLogo.w != mfd->vbLogo.w || vbAlphaLogo.h != mfd->vbLogo.h)
					throw MyError("Alpha image has different size than logo image (%dx%d vs. %dx%d)", vbAlphaLogo.w, vbAlphaLogo.h, mfd->vbLogo.w, mfd->vbLogo.h);
				CombineAlphaBlt(mfd->vbLogo, 0, 0, vbAlphaLogo, 0, 0, vbAlphaLogo.w, vbAlphaLogo.h);
				delete[] vbAlphaLogo.data;
			} catch(...) {
				delete[] vbAlphaLogo.data;
				throw;
			}
		} else if (!bHasAlpha)
			throw MyError("cannot alpha blend logo: image does not have an alpha channel.");

		if (mfd->bNonPremultAlpha)
			PremultiplyAlpha(mfd->vbLogo, 0, 0, mfd->vbLogo.w, mfd->vbLogo.h);
	} else {
		SetAlpha(mfd->vbLogo, 0, 0, mfd->vbLogo.w, mfd->vbLogo.h);
	}

	int opacity8 = (mfd->opacity * 255 + 0x8000) >> 16;
	mfd->bAlphaBlendingRequired = mfd->bEnableAlphaBlending;
	if (opacity8<255) {
		mfd->bAlphaBlendingRequired = true;
		ScalePremultipliedAlpha(mfd->vbLogo, 0, 0, mfd->vbLogo.w, mfd->vbLogo.h, opacity8);
	}

	InvertAlpha(mfd->vbLogo, 0, 0, mfd->vbLogo.w, mfd->vbLogo.h);

	return 0;
}

static int logo_stop(FilterActivation *fa, const FilterFunctions *ff) {
	LogoFilterData *mfd = (LogoFilterData *)fa->filter_data;

	delete[] mfd->vbLogo.data;
	mfd->vbLogo.data = NULL;

	return 0;
}

static const char *logoOpenImage(HWND hwnd, const char *oldfn) {
	OPENFILENAME ofn;
	static char szFile[MAX_PATH];
	char szFileTitle[MAX_PATH];

	///////////////

	if (oldfn)
		strcpy(szFile, oldfn);

	szFileTitle[0]=0;

	ofn.lStructSize			= OPENFILENAME_SIZE_VERSION_400;
	ofn.hwndOwner			= hwnd;
	ofn.lpstrFilter			= "Image file (*.bmp,*.tga,*.jpg,*.jpeg,*.png)\0*.bmp;*.tga;*.jpg;*.jpeg;*.png\0All files (*.*)\0*.*\0";
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szFile;
	ofn.nMaxFile			= sizeof szFile;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= sizeof szFileTitle;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Select image";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
	ofn.lpstrDefExt			= NULL;

	if (GetOpenFileName(&ofn))
		return szFile;

	return NULL;
}

static void LogoReenableDlgControls(HWND hdlg) {
	bool bAlphaBlendingEnabled = 0!=IsDlgButtonChecked(hdlg, IDC_ALPHABLEND);
	bool bUsingSecondaryAlpha = bAlphaBlendingEnabled && (0 != IsDlgButtonChecked(hdlg, IDC_SECONDARYALPHA));

	EnableWindow(GetDlgItem(hdlg, IDC_ALPHAFILE), bAlphaBlendingEnabled);
	EnableWindow(GetDlgItem(hdlg, IDC_SECONDARYALPHA), bAlphaBlendingEnabled);
	EnableWindow(GetDlgItem(hdlg, IDC_PREMULTALPHA), bAlphaBlendingEnabled);
	EnableWindow(GetDlgItem(hdlg, IDC_ALPHAFILE), bUsingSecondaryAlpha);
	EnableWindow(GetDlgItem(hdlg, IDC_ALPHAFILE_BROWSE), bUsingSecondaryAlpha);
}

static void LogoUpdateOffsets(HWND hDlg, LogoFilterData *mfd) {
	long pos_x;
	long pos_y;
	BOOL success;

	pos_x = GetDlgItemInt(hDlg, IDC_XPOS, &success, TRUE);
	if (!success) {
		SetFocus(GetDlgItem(hDlg, IDC_XPOS));
		MessageBeep(MB_ICONEXCLAMATION);
		return;
	}

	pos_y = GetDlgItemInt(hDlg, IDC_YPOS, &success, TRUE);
	if (!success) {
		SetFocus(GetDlgItem(hDlg, IDC_YPOS));
		MessageBeep(MB_ICONEXCLAMATION);
		return;
	}

	if (pos_x != mfd->pos_x || pos_y != mfd->pos_y) {
		mfd->pos_x = pos_x;
		mfd->pos_y = pos_y;
		mfd->ifp->RedoFrame();
	}
}

static void LogoUpdateOpacityText(HWND hDlg, LogoFilterData *mfd) {
	char buf[32];
	sprintf(buf, "%d%%", VDRoundToInt(mfd->opacity * (100 / 65536.0)));
	SetDlgItemText(hDlg, IDC_STATIC_OPACITY, buf);
}

static void LogoUpdateOpacity(HWND hDlg, LogoFilterData *mfd) {
	int percent = SendDlgItemMessage(hDlg, IDC_OPACITY, TBM_GETPOS, 0, 0);
	long opacity = (percent * 65536 + 50) / 100;

	if (opacity != mfd->opacity) {
		mfd->opacity = opacity;
		mfd->ifp->UndoSystem();
		mfd->ifp->RedoSystem();

		LogoUpdateOpacityText(hDlg, mfd);
	}
}

static INT_PTR APIENTRY logoDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	LogoFilterData *mfd = (struct LogoFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				mfd = (LogoFilterData *)lParam;
				SetWindowLongPtr(hDlg, DWLP_USER, (LONG)mfd);

				SetDlgItemText(hDlg, IDC_LOGOFILE, mfd->szLogoPath);
				SetDlgItemText(hDlg, IDC_ALPHAFILE, mfd->szAlphaPath);
				CheckDlgButton(hDlg, IDC_ALPHABLEND, mfd->bEnableAlphaBlending);
				CheckDlgButton(hDlg, IDC_SECONDARYALPHA, mfd->bEnableSecondaryAlpha);
				CheckDlgButton(hDlg, IDC_PREMULTALPHA, !mfd->bNonPremultAlpha);
				SetDlgItemInt(hDlg, IDC_XPOS, mfd->pos_x, TRUE);
				SetDlgItemInt(hDlg, IDC_YPOS, mfd->pos_y, TRUE);

				SendDlgItemMessage(hDlg, IDC_SPIN_XOFFSET, UDM_SETRANGE, 0, MAKELONG((short)-(UD_MINVAL-1/2), (short)+(UD_MINVAL-1/2)));
				SendDlgItemMessage(hDlg, IDC_SPIN_YOFFSET, UDM_SETRANGE, 0, MAKELONG((short)+(UD_MINVAL-1/2), (short)-(UD_MINVAL-1/2)));

				SendDlgItemMessage(hDlg, IDC_OPACITY, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
				SendDlgItemMessage(hDlg, IDC_OPACITY, TBM_SETPOS, TRUE, VDRoundToInt(mfd->opacity * (100 / 65536.0)));
				LogoUpdateOpacityText(hDlg, mfd);

				static const UINT idbypos[3][3]={
					IDC_DIR_TOPLEFT,
					IDC_DIR_TOPCENTER,
					IDC_DIR_TOPRIGHT,
					IDC_DIR_MIDDLELEFT,
					IDC_DIR_MIDDLECENTER,
					IDC_DIR_MIDDLERIGHT,
					IDC_DIR_BOTTOMLEFT,
					IDC_DIR_BOTTOMCENTER,
					IDC_DIR_BOTTOMRIGHT
				};

				CheckDlgButton(hDlg, idbypos[mfd->justify_y][mfd->justify_x], TRUE);

				mfd->ifp->InitButton((VDXHWND)GetDlgItem(hDlg, IDC_PREVIEW));

				LogoReenableDlgControls(hDlg);
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

			case IDC_ALPHABLEND:
				mfd->bEnableAlphaBlending = 0 != SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
				mfd->ifp->UndoSystem();
				mfd->ifp->RedoSystem();
				LogoReenableDlgControls(hDlg);
				return TRUE;

			case IDC_SECONDARYALPHA:
				mfd->bEnableSecondaryAlpha = 0 != SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
				mfd->ifp->UndoSystem();
				mfd->ifp->RedoSystem();
				LogoReenableDlgControls(hDlg);
				return TRUE;

			case IDC_PREMULTALPHA:
				mfd->bNonPremultAlpha = 0 == SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
				mfd->ifp->UndoSystem();
				mfd->ifp->RedoSystem();
				return TRUE;

			case IDC_LOGOFILE:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
					GetDlgItemText(hDlg, IDC_LOGOFILE, mfd->szLogoPath, sizeof mfd->szLogoPath);
					mfd->ifp->UndoSystem();
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_ALPHAFILE:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
					GetDlgItemText(hDlg, IDC_ALPHAFILE, mfd->szAlphaPath, sizeof mfd->szAlphaPath);
					mfd->ifp->UndoSystem();
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_LOGOFILE_BROWSE:
				if (const char *fn = logoOpenImage(hDlg, mfd->szLogoPath)) {
					SetDlgItemText(hDlg, IDC_LOGOFILE, fn);
					strcpy(mfd->szLogoPath, fn);
					mfd->ifp->UndoSystem();
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_ALPHAFILE_BROWSE:
				if (const char *fn = logoOpenImage(hDlg, mfd->szAlphaPath)) {
					SetDlgItemText(hDlg, IDC_ALPHAFILE, fn);
					strcpy(mfd->szAlphaPath, fn);
					mfd->ifp->UndoSystem();
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_XPOS:
				if (HIWORD(wParam) == EN_KILLFOCUS)
					LogoUpdateOffsets(hDlg, mfd);
				return TRUE;

			case IDC_YPOS:
				if (HIWORD(wParam) == EN_KILLFOCUS)
					LogoUpdateOffsets(hDlg, mfd);

				return TRUE;

			case IDC_DIR_TOPLEFT:			mfd->justify_x=0; mfd->justify_y=0; mfd->ifp->RedoFrame(); return TRUE;
			case IDC_DIR_TOPCENTER:			mfd->justify_x=1; mfd->justify_y=0; mfd->ifp->RedoFrame(); return TRUE;
			case IDC_DIR_TOPRIGHT:			mfd->justify_x=2; mfd->justify_y=0; mfd->ifp->RedoFrame(); return TRUE;
			case IDC_DIR_MIDDLELEFT:		mfd->justify_x=0; mfd->justify_y=1; mfd->ifp->RedoFrame(); return TRUE;
			case IDC_DIR_MIDDLECENTER:		mfd->justify_x=1; mfd->justify_y=1; mfd->ifp->RedoFrame(); return TRUE;
			case IDC_DIR_MIDDLERIGHT:		mfd->justify_x=2; mfd->justify_y=1; mfd->ifp->RedoFrame(); return TRUE;
			case IDC_DIR_BOTTOMLEFT:		mfd->justify_x=0; mfd->justify_y=2; mfd->ifp->RedoFrame(); return TRUE;
			case IDC_DIR_BOTTOMCENTER:		mfd->justify_x=1; mfd->justify_y=2; mfd->ifp->RedoFrame(); return TRUE;
			case IDC_DIR_BOTTOMRIGHT:		mfd->justify_x=2; mfd->justify_y=2; mfd->ifp->RedoFrame(); return TRUE;

			case IDC_PREVIEW:
				mfd->ifp->Toggle((VDXHWND)hDlg);
				return TRUE;

            }
            break;

		case WM_HSCROLL:
		case WM_VSCROLL:
			if (lParam)
				switch(GetWindowLong((HWND)lParam, GWL_ID)) {
				case IDC_XPOS:
				case IDC_YPOS:
				case IDC_SPIN_XOFFSET:
				case IDC_SPIN_YOFFSET:
					LogoUpdateOffsets(hDlg, mfd);
					break;
				case IDC_OPACITY:
					LogoUpdateOpacity(hDlg, mfd);
					break;
				}
			return TRUE;
    }
    return FALSE;
}

static int logo_config(FilterActivation *fa, const FilterFunctions *ff, VDXHWND hWnd) {
	LogoFilterData *mfd = (LogoFilterData *)fa->filter_data;
	LogoFilterData mfd2 = *mfd;
	int ret;

	mfd->ifp = fa->ifp;

	ret = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_LOGO), (HWND)hWnd, logoDlgProc, (LPARAM)mfd);

	if (ret)
		*mfd = mfd2;

	return ret;
}

static void logo_string2(const FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	LogoFilterData *mfd = (LogoFilterData *)fa->filter_data;

	if (mfd->bEnableAlphaBlending && mfd->bEnableSecondaryAlpha)
		_snprintf(buf, buflen, " (logo:\"%s\", alpha:\"%s\")", VDFileSplitPath(mfd->szLogoPath), VDFileSplitPath(mfd->szAlphaPath));
	else
		_snprintf(buf, buflen, " (logo:\"%s\", alpha:%s)", VDFileSplitPath(mfd->szLogoPath), mfd->bEnableAlphaBlending ? "on" : "off");
}

static void logo_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;

	LogoFilterData *mfd = (LogoFilterData *)fa->filter_data;

	mfd->szLogoPath[0] = 0;
	mfd->szAlphaPath[0] = 0;
	mfd->pos_x = argv[1].asInt();
	mfd->pos_y = argv[2].asInt();
	mfd->justify_x = 0;
	mfd->justify_y = 0;

	mfd->bEnableSecondaryAlpha = false;

	strncpy(mfd->szLogoPath, *argv[0].asString(), sizeof mfd->szLogoPath);
	mfd->szLogoPath[sizeof mfd->szLogoPath - 1] = 0;

	if (argv[3].isString()) {
		strncpy(mfd->szAlphaPath, *argv[3].asString(), sizeof mfd->szAlphaPath);
		mfd->szAlphaPath[sizeof mfd->szAlphaPath - 1] = 0;
		mfd->bEnableAlphaBlending = true;
		mfd->bEnableSecondaryAlpha = true;
	} else {
		mfd->bEnableAlphaBlending = !!argv[3].asInt();
	}

	mfd->bNonPremultAlpha = !!argv[4].asInt();

	int xj = argv[5].asInt();
	int yj = argv[6].asInt();

	if (xj>=0 && xj<3 && yj>=0 && yj<3) {
		mfd->justify_x = xj;
		mfd->justify_y = yj;
	}

	mfd->opacity = argv[7].asInt();
}

static ScriptFunctionDef logo_func_defs[]={
	{ (ScriptFunctionPtr)logo_script_config, "Config", "0siiiiiii" },
	{ (ScriptFunctionPtr)logo_script_config, NULL, "0siisiiii" },
	{ NULL },
};

static CScriptObject logo_obj={
	NULL, logo_func_defs
};

static bool logo_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	LogoFilterData *mfd = (LogoFilterData *)fa->filter_data;
	char tmp[1024];

	strncpy(tmp, strCify(mfd->szLogoPath), sizeof tmp);
	tmp[1023] = 0;

	if (mfd->bEnableAlphaBlending && mfd->bEnableSecondaryAlpha)
		_snprintf(buf, buflen, "Config(\"%s\", %d, %d, \"%s\", %d, %d, %d, %d)", tmp, mfd->pos_x, mfd->pos_y, strCify(mfd->szAlphaPath), mfd->bNonPremultAlpha, mfd->justify_x, mfd->justify_y, mfd->opacity);
	else
		_snprintf(buf, buflen, "Config(\"%s\", %d, %d, %d, %d, %d, %d, %d)", tmp, mfd->pos_x, mfd->pos_y, mfd->bEnableAlphaBlending, mfd->bNonPremultAlpha, mfd->justify_x, mfd->justify_y, mfd->opacity);

	return true;
}

FilterDefinition filterDef_logo={
	0,0,NULL,
	"logo",
	"Overlays an image over video."
#ifdef USE_ASM
			"\n\n[Assembly optimized] [MMX optimized]"
#endif
			,
	NULL,NULL,
	sizeof(LogoFilterData),
	logo_init,
	NULL,
	logo_run,
	logo_param,
	logo_config,
	NULL,
	logo_start,
	logo_stop,

	&logo_obj,
	logo_script_line,

	logo_string2
};
