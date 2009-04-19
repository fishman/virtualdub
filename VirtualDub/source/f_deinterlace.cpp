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

#if defined(VD_COMPILER_MSVC) && (defined(VD_CPU_X86) || defined(VD_CPU_AMD64))
	#include <emmintrin.h>
#endif

#include "resource.h"
#include "filter.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/fraction.h>
#include <vd2/system/memory.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDXFrame/VideoFilter.h>

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

extern HINSTANCE g_hInst;

#if defined(VD_COMPILER_MSVC) && defined(VD_CPU_X86)
	#pragma warning(disable: 4799)		// warning C4799: function has no EMMS instruction
#endif

///////////////////////////////////////////////////////////////////////////

#ifdef _M_IX86
static void __declspec(naked) asm_blend_row_clipped(void *dst, const void *src, uint32 w, ptrdiff_t srcpitch) {
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

static void __declspec(naked) asm_blend_row(void *dst, const void *src, uint32 w, ptrdiff_t srcpitch) {
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

static void __declspec(naked) asm_blend_row_MMX(void *dst, const void *src, uint32 w, ptrdiff_t srcpitch) {
	static const __declspec(align(8)) __int64 mask0 = 0xfcfcfcfcfcfcfcfci64;
	static const __declspec(align(8)) __int64 mask1 = 0x7f7f7f7f7f7f7f7fi64;
	static const __declspec(align(8)) __int64 mask2 = 0x3f3f3f3f3f3f3f3fi64;
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
		inc		ebp
		shr		ebp,1
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

		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret
	};
}

static void __declspec(naked) asm_blend_row_ISSE(void *dst, const void *src, uint32 w, ptrdiff_t srcpitch) {
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

		inc		ebp
		shr		ebp,1
		pcmpeqb	mm7, mm7

		align	16
xloop:
		movq	mm0, [esi]
		movq	mm2, mm7
		pxor	mm0, mm7

		pxor	mm2, [esi+edx*2]
		pavgb	mm0, mm2
		pxor	mm0, mm7

		pavgb	mm0, [esi+edx]
		add		esi,8

		movq	[edi+esi-8],mm0
		dec		ebp
		jne		xloop

		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret
	};
}
#else
static void asm_blend_row_clipped(void *dst0, const void *src0, uint32 w, ptrdiff_t srcpitch) {
	uint32 *dst = (uint32 *)dst0;
	const uint32 *src = (const uint32 *)src0;
	const uint32 *src2 = (const uint32 *)((const char *)src + srcpitch);

	do {
		const uint32 x = *src++;
		const uint32 y = *src2++;

		*dst++ = (x|y) - (((x^y)&0xfefefefe)>>1);
	} while(--w);
}

static void asm_blend_row(void *dst0, const void *src0, uint32 w, ptrdiff_t srcpitch) {
	uint32 *dst = (uint32 *)dst0;
	const uint32 *src = (const uint32 *)src0;
	const uint32 *src2 = (const uint32 *)((const char *)src + srcpitch);
	const uint32 *src3 = (const uint32 *)((const char *)src2 + srcpitch);

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

#if defined(VD_CPU_X86) || defined(VD_CPU_AMD64)
	static void asm_blend_row_SSE2(void *dst, const void *src, uint32 w, ptrdiff_t srcpitch) {
		__m128i zero = _mm_setzero_si128();
		__m128i inv = _mm_cmpeq_epi8(zero, zero);

		w = (w + 3) >> 2;

		const __m128i *src1 = (const __m128i *)src;
		const __m128i *src2 = (const __m128i *)((const char *)src + srcpitch);
		const __m128i *src3 = (const __m128i *)((const char *)src + srcpitch*2);
		__m128i *dstrow = (__m128i *)dst;
		do {
			__m128i a = *src1++;
			__m128i b = *src2++;
			__m128i c = *src3++;

			*dstrow++ = _mm_avg_epu8(_mm_xor_si128(_mm_avg_epu8(_mm_xor_si128(a, inv), _mm_xor_si128(c, inv)), inv), b);
		} while(--w);
	}

	void ela_L8_SSE2(__m128i *dst, const __m128i *srcat, const __m128i *srcab, int w16) {
		do {
			__m128i top0 = srcat[0];
			__m128i top1 = srcat[1];
			__m128i top2 = srcat[2];
			__m128i bot0 = srcab[0];
			__m128i bot1 = srcab[1];
			__m128i bot2 = srcab[2];
			++srcat;
			++srcab;

			__m128i topl2 = _mm_or_si128(_mm_srli_si128(top0, 16 - 3), _mm_slli_si128(top1, 3));
			__m128i topl1 = _mm_or_si128(_mm_srli_si128(top0, 16 - 2), _mm_slli_si128(top1, 2));
			__m128i topc0 = _mm_or_si128(_mm_srli_si128(top0, 16 - 1), _mm_slli_si128(top1, 1));
			__m128i topr1 = top1;
			__m128i topr2 = _mm_or_si128(_mm_srli_si128(top1, 1), _mm_slli_si128(top2, 16 - 1));
			__m128i topr3 = _mm_or_si128(_mm_srli_si128(top1, 2), _mm_slli_si128(top2, 16 - 2));

			__m128i botl2 = _mm_or_si128(_mm_srli_si128(bot0, 16 - 3), _mm_slli_si128(bot1, 3));
			__m128i botl1 = _mm_or_si128(_mm_srli_si128(bot0, 16 - 2), _mm_slli_si128(bot1, 2));
			__m128i botc0 = _mm_or_si128(_mm_srli_si128(bot0, 16 - 1), _mm_slli_si128(bot1, 1));
			__m128i botr1 = bot1;
			__m128i botr2 = _mm_or_si128(_mm_srli_si128(bot1, 1), _mm_slli_si128(bot2, 16 - 1));
			__m128i botr3 = _mm_or_si128(_mm_srli_si128(bot1, 2), _mm_slli_si128(bot2, 16 - 2));

			__m128i rawscorec0 = _mm_or_si128(_mm_subs_epu8(topc0, botc0), _mm_subs_epu8(botc0, topc0));
			__m128i rawscorel1 = _mm_or_si128(_mm_subs_epu8(topl1, botr1), _mm_subs_epu8(botr1, topl1));
			__m128i rawscorel2 = _mm_or_si128(_mm_subs_epu8(topl2, botr2), _mm_subs_epu8(botr2, topl2));
			__m128i rawscorer1 = _mm_or_si128(_mm_subs_epu8(topr1, botl1), _mm_subs_epu8(botl1, topr1));
			__m128i rawscorer2 = _mm_or_si128(_mm_subs_epu8(topr2, botl2), _mm_subs_epu8(botl2, topr2));

			dst[0] = rawscorec0;
			dst[1] = rawscorel1;
			dst[2] = rawscorel2;
			dst[3] = rawscorer1;
			dst[4] = rawscorer2;
			dst[5] = _mm_avg_epu8(topr1, botr1);
			dst[6] = _mm_avg_epu8(topc0, botr2);
			dst[7] = _mm_avg_epu8(topl1, botr3);
			dst[8] = _mm_avg_epu8(topr2, botc0);
			dst[9] = _mm_avg_epu8(topr3, botl1);
			dst += 10;
		} while(--w16);
	}

	void nela_L8_SSE2(__m128i *dst, const __m128i *elabuf, int w16) {
		__m128i zero = _mm_setzero_si128();
		__m128i x80b = _mm_set1_epi8((char)0x80);

		do {
			__m128i x0, x1, x2, y;

			x0 = elabuf[0];
			y = elabuf[10];
			x1 = _mm_or_si128(_mm_srli_si128(x0, 1), _mm_slli_si128(y, 15));
			x2 = _mm_or_si128(_mm_srli_si128(x0, 2), _mm_slli_si128(y, 14));
			__m128i scorec0 = _mm_avg_epu8(_mm_avg_epu8(x0, x2), x1);

			x0 = elabuf[1];
			y = elabuf[11];
			x1 = _mm_or_si128(_mm_srli_si128(x0, 1), _mm_slli_si128(y, 15));
			x2 = _mm_or_si128(_mm_srli_si128(x0, 2), _mm_slli_si128(y, 14));
			__m128i scorel1 = _mm_avg_epu8(_mm_avg_epu8(x0, x2), x1);

			x0 = elabuf[2];
			y = elabuf[12];
			x1 = _mm_or_si128(_mm_srli_si128(x0, 1), _mm_slli_si128(y, 15));
			x2 = _mm_or_si128(_mm_srli_si128(x0, 2), _mm_slli_si128(y, 14));
			__m128i scorel2 = _mm_avg_epu8(_mm_avg_epu8(x0, x2), x1);

			x0 = elabuf[3];
			y = elabuf[13];
			x1 = _mm_or_si128(_mm_srli_si128(x0, 1), _mm_slli_si128(y, 15));
			x2 = _mm_or_si128(_mm_srli_si128(x0, 2), _mm_slli_si128(y, 14));
			__m128i scorer1 = _mm_avg_epu8(_mm_avg_epu8(x0, x2), x1);

			x0 = elabuf[4];
			y = elabuf[14];
			x1 = _mm_or_si128(_mm_srli_si128(x0, 1), _mm_slli_si128(y, 15));
			x2 = _mm_or_si128(_mm_srli_si128(x0, 2), _mm_slli_si128(y, 14));
			__m128i scorer2 = _mm_avg_epu8(_mm_avg_epu8(x0, x2), x1);

			scorec0 = _mm_xor_si128(scorec0, x80b);
			scorel1 = _mm_xor_si128(scorel1, x80b);
			scorel2 = _mm_xor_si128(scorel2, x80b);
			scorer1 = _mm_xor_si128(scorer1, x80b);
			scorer2 = _mm_xor_si128(scorer2, x80b);

			// result = (scorel1 < scorec0) ? (scorel2 < scorel1 ? l2 : l1) : (scorer1 < scorec0) ? (scorer2 < scorer1 ? r2 : r1) : c0

			__m128i cmplt_l1_c0 = _mm_cmplt_epi8(scorel1, scorec0);
			__m128i cmplt_r1_c0 = _mm_cmplt_epi8(scorer1, scorec0);
			__m128i cmplt_l1_r1 = _mm_cmplt_epi8(scorel1, scorer1);

			__m128i is_l1 = _mm_and_si128(cmplt_l1_r1, cmplt_l1_c0);
			__m128i is_r1 = _mm_andnot_si128(cmplt_l1_r1, cmplt_r1_c0);
			__m128i is_c0_inv = _mm_or_si128(cmplt_l1_c0, cmplt_r1_c0);
			__m128i is_c0 = _mm_andnot_si128(is_c0_inv, _mm_cmpeq_epi8(zero, zero));

			__m128i is_l2 = _mm_and_si128(is_l1, _mm_cmplt_epi8(scorel2, scorel1));
			__m128i is_r2 = _mm_and_si128(is_r1, _mm_cmplt_epi8(scorer2, scorer1));

			is_l1 = _mm_andnot_si128(is_l2, is_l1);
			is_r1 = _mm_andnot_si128(is_r2, is_r1);

			__m128i mask_c0 = is_c0;
			__m128i mask_l1 = is_l1;
			__m128i mask_l2 = is_l2;
			__m128i mask_r1 = is_r1;
			__m128i mask_r2 = is_r2;

			__m128i result_c0 = _mm_and_si128(elabuf[5], mask_c0);
			__m128i result_l1 = _mm_and_si128(elabuf[6], mask_l1);
			__m128i result_l2 = _mm_and_si128(elabuf[7], mask_l2);
			__m128i result_r1 = _mm_and_si128(elabuf[8], mask_r1);
			__m128i result_r2 = _mm_and_si128(elabuf[9], mask_r2);

			elabuf += 10;

			__m128i pred = _mm_or_si128(_mm_or_si128(_mm_or_si128(result_l1, result_l2), _mm_or_si128(result_r1, result_r2)), result_c0);

			*dst++ = pred;
		} while(--w16);
	}
#endif

namespace {
	void ela_L8_scalar(uint8 *dst, const uint8 *srcat, const uint8 *srcab, int w16) {
		int w = w16 << 4;

		srcat += 16;
		srcab += 16;
		do {
			int topl2 = srcat[-3];
			int topl1 = srcat[-2];
			int topc0 = srcat[-1];
			int topr1 = srcat[0];
			int topr2 = srcat[1];
			int topr3 = srcat[2];

			int botl2 = srcab[-3];
			int botl1 = srcab[-2];
			int botc0 = srcab[-1];
			int botr1 = srcab[0];
			int botr2 = srcab[1];
			int botr3 = srcab[2];
			++srcat;
			++srcab;

			int rawscorec0 = abs(topc0 - botc0);
			int rawscorel1 = abs(topl1 - botr1);
			int rawscorel2 = abs(topl2 - botr2);
			int rawscorer1 = abs(topr1 - botl1);
			int rawscorer2 = abs(topr2 - botl2);

			dst[0] = (uint8)rawscorec0;
			dst[1] = (uint8)rawscorel1;
			dst[2] = (uint8)rawscorel2;
			dst[3] = (uint8)rawscorer1;
			dst[4] = (uint8)rawscorer2;
			dst[5] = (uint8)((topr1 + botr1 + 1) >> 1);
			dst[6] = (uint8)((topc0 + botr2 + 1) >> 1);
			dst[7] = (uint8)((topl1 + botr3 + 1) >> 1);
			dst[8] = (uint8)((topr2 + botc0 + 1) >> 1);
			dst[9] = (uint8)((topr3 + botl1 + 1) >> 1);
			dst += 10;
		} while(--w);
	}

	void nela_L8_scalar(uint8 *dst, const uint8 *elabuf, int w16) {
		int w = w16 << 4;

		do {
			int scorec0 = elabuf[10]*2 + (elabuf[0] + elabuf[20]);
			int result = elabuf[5];

			int scorel1 = elabuf[11]*2 + (elabuf[1] + elabuf[21]);
			if (scorel1 < scorec0) {
				result = elabuf[6];
				scorec0 = scorel1;

				int scorel2 = elabuf[12]*2 + (elabuf[2] + elabuf[22]);
				if (scorel2 < scorec0) {
					result = elabuf[7];
					scorec0 = scorel2;
				}
			}

			int scorer1 = elabuf[13]*2 + (elabuf[3] + elabuf[23]);
			if (scorer1 < scorec0) {
				result = elabuf[8];
				scorec0 = scorer1;

				int scorer2 = elabuf[14]*2 + (elabuf[4] + elabuf[24]);
				if (scorer2 < scorec0)
					result = elabuf[9];
			}

			elabuf += 10;

			*dst++ = (uint8)result;
		} while(--w);
	}

	void BlendScanLine_NELA_scalar(void *dst, const void *srcT, const void *srcB, uint32 w, uint8 *tempBuf) {
		const uint8 *srcat = (const uint8 *)srcT;
		const uint8 *srcab = (const uint8 *)srcB;
		uint32 w16 = (w + 15) >> 4;
		uint32 wr = w16 << 4;

		uint8 *elabuf = tempBuf;
		uint8 *topbuf = elabuf + 10*wr;
		uint8 *botbuf = topbuf + wr + 32;

		uint32 woffset = w & 15;
		topbuf[13] = topbuf[14] = topbuf[15] = srcat[0];
		botbuf[13] = botbuf[14] = botbuf[15] = srcab[0];

		for(uint32 x=0; x<wr; ++x) {
			topbuf[x+16] = srcat[x];
			botbuf[x+16] = srcab[x];
		}

		if (woffset) {
			uint8 *topfinal = &topbuf[w+16];
			uint8 *botfinal = &botbuf[w+16];
			const uint8 tv = topfinal[-1];
			const uint8 bv = botfinal[-1];

			for(uint32 i = woffset; i < 16; ++i) {
				*topfinal++ = tv;
				*botfinal++ = bv;
			}
		}

		topbuf[wr+16] = topbuf[wr+17] = topbuf[wr+18] = topbuf[wr+15];
		topbuf[wr+16] = topbuf[wr+17] = botbuf[wr+18] = botbuf[wr+15];

		ela_L8_scalar(elabuf, topbuf, botbuf, w16);
		nela_L8_scalar((uint8 *)dst, elabuf, w16);
	}

	void ela_X8R8G8B8_scalar(uint32 *dst, const uint8 *srcat, const uint8 *srcab, int w4) {
		srcat += 4;
		srcab += 4;
		do {
			const uint8 *src1 = srcat;
			const uint8 *src2 = srcab + 16;

			for(int i=0; i<5; ++i) {
				int er = abs((int)src1[2] - (int)src2[2]);
				int eg = abs((int)src1[1] - (int)src2[1]);
				int eb = abs((int)src1[0] - (int)src2[0]);
				*dst++ = er*54 + eg*183 + eb*19;
				src1 += 4;
				src2 -= 4;
			}

			srcat += 4;
			srcab += 4;
		} while(--w4);
	}

#if defined(VD_CPU_X86)
	void __declspec(naked) __cdecl ela_X8R8G8B8_MMX(uint32 *dst, const uint8 *srcat, const uint8 *srcab, int w4) {
		static const __declspec(align(16)) uint64 kCoeff = 0x00003600b70013ull;

		__asm {
			push		ebp
			push		edi
			push		esi
			push		ebx

			mov			ebx, [esp+4+16]
			mov			ecx, [esp+8+16]
			mov			edx, [esp+12+16]
			add			ecx, 4
			add			edx, 4
			mov			esi, [esp+16+16]
			movq		mm6, qword ptr [kCoeff]
			pxor		mm7, mm7

			align	16
xloop:
			movd		mm0, [ecx]
			movd		mm2, [edx + 16]
			movq		mm1, mm0
			psubusb		mm0, mm2
			psubusb		mm2, mm1
			por			mm0, mm2
			punpcklbw	mm0, mm7
			pmaddwd		mm0, mm6
			movq		mm1, mm0
			psrlq		mm0, 32
			paddd		mm0, mm1
			movd		[ebx], mm0

			movd		mm0, [ecx + 4]
			movd		mm2, [edx + 12]
			movq		mm1, mm0
			psubusb		mm0, mm2
			psubusb		mm2, mm1
			por			mm0, mm2
			punpcklbw	mm0, mm7
			pmaddwd		mm0, mm6
			movq		mm1, mm0
			psrlq		mm0, 32
			paddd		mm0, mm1
			movd		[ebx + 4], mm0

			movd		mm0, [ecx + 8]
			movd		mm2, [edx + 8]
			movq		mm1, mm0
			psubusb		mm0, mm2
			psubusb		mm2, mm1
			por			mm0, mm2
			punpcklbw	mm0, mm7
			pmaddwd		mm0, mm6
			movq		mm1, mm0
			psrlq		mm0, 32
			paddd		mm0, mm1
			movd		[ebx + 8], mm0

			movd		mm0, [ecx + 12]
			movd		mm2, [edx + 4]
			movq		mm1, mm0
			psubusb		mm0, mm2
			psubusb		mm2, mm1
			por			mm0, mm2
			punpcklbw	mm0, mm7
			pmaddwd		mm0, mm6
			movq		mm1, mm0
			psrlq		mm0, 32
			paddd		mm0, mm1
			movd		[ebx + 12], mm0

			movd		mm0, [ecx + 16]
			movd		mm2, [edx]
			movq		mm1, mm0
			psubusb		mm0, mm2
			psubusb		mm2, mm1
			por			mm0, mm2
			punpcklbw	mm0, mm7
			pmaddwd		mm0, mm6
			movq		mm1, mm0
			psrlq		mm0, 32
			paddd		mm0, mm1
			movd		[ebx + 16], mm0

			add			ebx, 20
			add			ecx, 4
			add			edx, 4
			dec			esi
			jne			xloop

			emms
			pop			ebx
			pop			esi
			pop			edi
			pop			ebp
			ret
		}
		srcat += 4;
		srcab += 4;
		do {
			const uint8 *src1 = srcat;
			const uint8 *src2 = srcab + 16;

			for(int i=0; i<5; ++i) {
				int er = abs((int)src1[2] - (int)src2[2]);
				int eg = abs((int)src1[1] - (int)src2[1]);
				int eb = abs((int)src1[0] - (int)src2[0]);
				*dst++ = er*54 + eg*183 + eb*19;
				src1 += 4;
				src2 -= 4;
			}

			srcat += 4;
			srcab += 4;
		} while(--w4);
	}
#endif

	void nela_X8R8G8B8_scalar(uint32 *dst, const uint32 *elabuf, const uint8 *srca, const uint8 *srcb, int w4) {
		do {
			int scorec0 = elabuf[7]*2 + (elabuf[2] + elabuf[12]);
			int offset = 0;

			int scorel1 = elabuf[6]*2 + (elabuf[1] + elabuf[11]);
			if (scorel1 < scorec0) {
				offset = -4;
				scorec0 = scorel1;

				int scorel2 = elabuf[5]*2 + (elabuf[0] + elabuf[10]);
				if (scorel2 < scorec0) {
					offset = -8;
					scorec0 = scorel2;
				}
			}

			int scorer1 = elabuf[8]*2 + (elabuf[3] + elabuf[13]);
			if (scorer1 < scorec0) {
				offset = 4;
				scorec0 = scorer1;

				int scorer2 = elabuf[9]*2 + (elabuf[4] + elabuf[14]);
				if (scorer2 < scorec0)
					offset = 8;
			}

			elabuf += 5;

			const uint32 a = *(const uint32 *)(srca + offset);
			const uint32 b = *(const uint32 *)(srcb - offset);
			*dst++ = (a|b) - (((a^b) & 0xfefefefe) >> 1);
			srca += 4;
			srcb += 4;
		} while(--w4);
	}

	void BlendScanLine_NELA_X8R8G8B8_scalar(void *dst, const void *srcT, const void *srcB, uint32 w, void *tempBuf) {
		const uint32 *srcat = (const uint32 *)srcT;
		const uint32 *srcab = (const uint32 *)srcB;
		uint32 w4 = (w + 3) >> 2;
		uint32 *elabuf = (uint32 *)tempBuf;
		uint32 *topbuf = elabuf + 5*w4;
		uint32 *botbuf = topbuf + w4 + 8;

		topbuf[0] = topbuf[1] = topbuf[2] = topbuf[3] = srcat[0];
		botbuf[0] = botbuf[1] = botbuf[2] = botbuf[3] = srcab[0];

		for(uint32 x=0; x<w4; ++x) {
			topbuf[x+4] = srcat[x];
			botbuf[x+4] = srcab[x];
		}

		topbuf[w4+4] = topbuf[w4+5] = topbuf[w4+6] = topbuf[w4+7] = topbuf[w4+3];
		botbuf[w4+4] = botbuf[w4+5] = botbuf[w4+6] = botbuf[w4+7] = botbuf[w4+3];

		ela_X8R8G8B8_scalar(elabuf, (const uint8 *)topbuf, (const uint8 *)botbuf, w4);
		nela_X8R8G8B8_scalar((uint32 *)dst, elabuf, (const uint8 *)(topbuf + 4), (const uint8 *)(botbuf + 4), w4);
	}

#if defined(VD_CPU_X86)
	void BlendScanLine_NELA_X8R8G8B8_MMX(void *dst, const void *srcT, const void *srcB, uint32 w, void *tempBuf) {
		const uint32 *srcat = (const uint32 *)srcT;
		const uint32 *srcab = (const uint32 *)srcB;
		uint32 w4 = (w + 3) >> 2;
		uint32 *elabuf = (uint32 *)tempBuf;
		uint32 *topbuf = elabuf + 5*w4;
		uint32 *botbuf = topbuf + w4 + 8;

		topbuf[0] = topbuf[1] = topbuf[2] = topbuf[3] = srcat[0];
		botbuf[0] = botbuf[1] = botbuf[2] = botbuf[3] = srcab[0];

		for(uint32 x=0; x<w4; ++x) {
			topbuf[x+4] = srcat[x];
			botbuf[x+4] = srcab[x];
		}

		topbuf[w4+4] = topbuf[w4+5] = topbuf[w4+6] = topbuf[w4+7] = topbuf[w4+3];
		botbuf[w4+4] = botbuf[w4+5] = botbuf[w4+6] = botbuf[w4+7] = botbuf[w4+3];

		ela_X8R8G8B8_MMX(elabuf, (const uint8 *)topbuf, (const uint8 *)botbuf, w4);
		nela_X8R8G8B8_scalar((uint32 *)dst, elabuf, (const uint8 *)(topbuf + 4), (const uint8 *)(botbuf + 4), w4);
	}
#endif

	void BlendScanLine_NELA_SSE2(void *dst, const void *srcT, const void *srcB, uint32 w, __m128i *tempBuf) {
		const __m128i *srcat = (const __m128i *)srcT;
		const __m128i *srcab = (const __m128i *)srcB;
		uint32 w16 = (w + 15) >> 4;
		__m128i *elabuf = tempBuf;
		__m128i *topbuf = elabuf + 10*w16;
		__m128i *botbuf = topbuf + w16 + 2;

		uint32 woffset = w & 15;
		topbuf[0] = srcat[0];
		botbuf[0] = srcab[0];

		for(uint32 x=0; x<w16; ++x) {
			topbuf[x+1] = srcat[x];
			botbuf[x+1] = srcab[x];
		}

		if (woffset) {
			uint8 *topfinal = (uint8 *)&topbuf[w16] + woffset;
			uint8 *botfinal = (uint8 *)&botbuf[w16] + woffset;
			const uint8 tv = topfinal[-1];
			const uint8 bv = botfinal[-1];

			for(uint32 i = woffset; i < 16; ++i) {
				*topfinal++ = tv;
				*botfinal++ = bv;
			}
		}

		topbuf[w16+1] = topbuf[w16];
		botbuf[w16+1] = botbuf[w16];

		ela_L8_SSE2(elabuf, topbuf, botbuf, w16);
		nela_L8_SSE2((__m128i *)dst, elabuf, w16);
	}

	void InterpPlane_NELA_X8R8G8B8(void *dst, ptrdiff_t dstpitch, const void *src, ptrdiff_t srcpitch, uint32 w, uint32 h, bool interpField2) {
		uint32 w16 = (w + 15) >> 4;
		vdfastvector<uint8, vdaligned_alloc<uint8> > tempbuf((12 * w16 + 4) * 16);
		void *elabuf = tempbuf.data();

		if (!interpField2)
			memcpy(dst, src, w16 << 4);

		int y0 = interpField2 ? 1 : 2;
		for(uint32 y = y0; y < h - 1; y += 2) {
			const __m128i *srcat = (const __m128i *)((const char *)src + srcpitch * (y-1));
			const __m128i *srcab = (const __m128i *)((const char *)src + srcpitch * (y+1));

#if defined(VD_CPU_X86)
			if (MMX_enabled)
				BlendScanLine_NELA_X8R8G8B8_MMX((char *)dst + dstpitch*y, srcat, srcab, w, (uint8 *)elabuf);
			else
#endif
				BlendScanLine_NELA_X8R8G8B8_scalar((char *)dst + dstpitch*y, srcat, srcab, w, (uint8 *)elabuf);
		}

		if (interpField2)
			memcpy((char *)dst + dstpitch*(h - 1), (const char *)src + srcpitch*(h - 1), w16 << 4);
	}

	void InterpPlane_NELA(void *dst, ptrdiff_t dstpitch, const void *src, ptrdiff_t srcpitch, uint32 w, uint32 h, bool interpField2) {
		uint32 w16 = (w + 15) >> 4;
		vdfastvector<uint8, vdaligned_alloc<uint8> > tempbuf((12 * w16 + 4) * 16);
		void *elabuf = tempbuf.data();

		if (!interpField2)
			memcpy(dst, src, w16 << 4);

		int y0 = interpField2 ? 1 : 2;
		if (SSE2_enabled) {
			for(uint32 y = y0; y < h - 1; y += 2) {
				const __m128i *srcat = (const __m128i *)((const char *)src + srcpitch * (y-1));
				const __m128i *srcab = (const __m128i *)((const char *)src + srcpitch * (y+1));

				BlendScanLine_NELA_SSE2((char *)dst + dstpitch*y, srcat, srcab, w, (__m128i *)elabuf);
			}
		} else {
			for(uint32 y = y0; y < h - 1; y += 2) {
				const __m128i *srcat = (const __m128i *)((const char *)src + srcpitch * (y-1));
				const __m128i *srcab = (const __m128i *)((const char *)src + srcpitch * (y+1));

				BlendScanLine_NELA_scalar((char *)dst + dstpitch*y, srcat, srcab, w, (uint8 *)elabuf);
			}
		}

		if (interpField2)
			memcpy((char *)dst + dstpitch*(h - 1), (const char *)src + srcpitch*(h - 1), w16 << 4);
	}

	void Average_scalar(void *dst, ptrdiff_t dstPitch, const void *src1, const void *src2, ptrdiff_t srcPitch, uint32 w16, uint32 h) {
		uint32 w4 = w16 << 2;
		do {
			uint32 *dstv = (uint32 *)dst;
			uint32 *src1v = (uint32 *)src1;
			uint32 *src2v = (uint32 *)src2;

			for(uint32 i=0; i<w4; ++i) {
				uint32 a = src1v[i];
				uint32 b = src2v[i];

				dstv[i] = (a|b) - (((a^b) & 0xfefefefe) >> 1);
			}

			dst = (char *)dst + dstPitch;
			src1 = (char *)src1 + srcPitch;
			src2 = (char *)src2 + srcPitch;
		} while(--h);
	}

	void Average_SSE2(void *dst, ptrdiff_t dstPitch, const void *src1, const void *src2, ptrdiff_t srcPitch, uint32 w16, uint32 h) {
		do {
			__m128i *dstv = (__m128i *)dst;
			__m128i *src1v = (__m128i *)src1;
			__m128i *src2v = (__m128i *)src2;

			for(uint32 i=0; i<w16; ++i)
				dstv[i] = _mm_avg_epu8(src1v[i], src2v[i]);

			dst = (char *)dst + dstPitch;
			src1 = (char *)src1 + srcPitch;
			src2 = (char *)src2 + srcPitch;
		} while(--h);
	}

	void InterpPlane_Bob(void *dst, ptrdiff_t dstpitch, const void *src, ptrdiff_t srcpitch, uint32 w, uint32 h, bool interpField2) {
		void (*blend_func)(void *dst, ptrdiff_t dstPitch, const void *src1, const void *src2, ptrdiff_t srcPitch, uint32 w16, uint32 h);
#if defined(VD_CPU_X86)
		if (SSE2_enabled)
			blend_func = Average_SSE2;
		else
			blend_func = Average_scalar;
#else
		blend_func = Average_SSE2;
#endif

		w = (w + 3) >> 2;

		int y0 = interpField2 ? 1 : 2;

		if (!interpField2)
			memcpy(dst, src, w * 4);

		if (h > y0) {
			blend_func((char *)dst + dstpitch*y0,
				dstpitch*2,
				(const char *)src + srcpitch*(y0 - 1),
				(const char *)src + srcpitch*(y0 + 1),
				srcpitch*2,
				(w + 3) >> 2,
				(h - y0) >> 1);
		}

		if (interpField2)
			memcpy((char *)dst + dstpitch*(h - 1), (const char *)src + srcpitch*(h - 1), w*4);

#ifdef _M_IX86
		if (MMX_enabled)
			__asm emms
#endif
	}

	void BlendPlane(void *dst, ptrdiff_t dstpitch, const void *src, ptrdiff_t srcpitch, uint32 w, uint32 h) {
		void (*blend_func)(void *, const void *, uint32, ptrdiff_t);
#if defined(VD_CPU_X86)
		if (SSE2_enabled)
			blend_func = asm_blend_row_SSE2;
		else
			blend_func = ISSE_enabled ? asm_blend_row_ISSE : MMX_enabled ? asm_blend_row_MMX : asm_blend_row;
#else
		blend_func = asm_blend_row_SSE2;
#endif

		w = (w + 3) >> 2;

		asm_blend_row_clipped(dst, src, w, srcpitch);
		if (h-=2)
			do {
				dst = ((char *)dst + dstpitch);

				blend_func(dst, src, w, srcpitch);

				src = ((char *)src + srcpitch);
			} while(--h);

		asm_blend_row_clipped((char *)dst + dstpitch, src, w, srcpitch);

#ifdef _M_IX86
		if (MMX_enabled)
			__asm emms
#endif
	}

	void Absdiff_SSE2(void *dst, ptrdiff_t dstPitch, const void *src1, const void *src2, ptrdiff_t srcPitch, uint32 w16, uint32 h) {
		do {
			__m128i *dstv = (__m128i *)dst;
			__m128i *src1v = (__m128i *)src1;
			__m128i *src2v = (__m128i *)src2;

			for(uint32 i=0; i<w16; ++i) {
				__m128i a = src1v[i];
				__m128i b = src2v[i];

				dstv[i] = _mm_or_si128(_mm_subs_epu8(a, b), _mm_subs_epu8(b, a));
			}

			dst = (char *)dst + dstPitch;
			src1 = (char *)src1 + srcPitch;
			src2 = (char *)src2 + srcPitch;
		} while(--h);
	}

	void Absdiff_scalar(void *dst, ptrdiff_t dstPitch, const void *src1, const void *src2, ptrdiff_t srcPitch, uint32 w16, uint32 h) {
		uint32 w = w16 << 4;

		do {
			uint8 *dstv = (uint8 *)dst;
			uint8 *src1v = (uint8 *)src1;
			uint8 *src2v = (uint8 *)src2;

			for(uint32 i=0; i<w; ++i) {
				int a = src1v[i];
				int b = src2v[i];

				dstv[i] = (uint8)abs(a - b);
			}

			dst = (char *)dst + dstPitch;
			src1 = (char *)src1 + srcPitch;
			src2 = (char *)src2 + srcPitch;
		} while(--h);
	}

	void AbsDiffBlended_SSE2(void *dst, ptrdiff_t dstPitch, const void *src1, const void *src2, ptrdiff_t srcPitch, uint32 w16, uint32 h) {
		bool firstRow = true;
		do {
			__m128i *dstv = (__m128i *)dst;
			__m128i *src1v = (__m128i *)src1;
			__m128i *src2v = (__m128i *)src2;

			__m128i *dstv2 = (__m128i *)((char *)dst + dstPitch);
			if (firstRow) {
				for(uint32 i=0; i<w16; ++i) {
					__m128i a = src1v[i];
					__m128i b = src2v[i];

					dstv2[i] = dstv[i] = _mm_or_si128(_mm_subs_epu8(a, b), _mm_subs_epu8(b, a));
				}
			} else {

				for(uint32 i=0; i<w16; ++i) {
					__m128i a = src1v[i];
					__m128i b = src2v[i];

					__m128i r = _mm_or_si128(_mm_subs_epu8(a, b), _mm_subs_epu8(b, a));

					dstv[i] = _mm_avg_epu8(dstv[i], r);
					dstv2[i] = r;
				}
			}

			firstRow = false;
			dst = dstv2;
			src1 = (char *)src1 + srcPitch;
			src2 = (char *)src2 + srcPitch;
		} while(--h);
	}

	void AbsDiffBlended_scalar(void *dst, ptrdiff_t dstPitch, const void *src1, const void *src2, ptrdiff_t srcPitch, uint32 w16, uint32 h) {
		uint32 w = w16 << 4;
		bool firstRow = true;
		do {
			uint8 *dstv = (uint8 *)dst;
			uint8 *src1v = (uint8 *)src1;
			uint8 *src2v = (uint8 *)src2;

			uint8 *dstv2 = (uint8 *)((char *)dst + dstPitch);
			if (firstRow) {
				for(uint32 i=0; i<w; ++i) {
					int a = src1v[i];
					int b = src2v[i];

					dstv2[i] = dstv[i] = (uint8)abs(a - b);
				}
			} else {

				for(uint32 i=0; i<w; ++i) {
					int a = src1v[i];
					int b = src2v[i];

					int r = abs(a - b);

					dstv[i] = (uint8)(((int)dstv[i] + r + 1) >> 1);
					dstv2[i] = (uint8)r;
				}
			}

			firstRow = false;
			dst = dstv2;
			src1 = (char *)src1 + srcPitch;
			src2 = (char *)src2 + srcPitch;
		} while(--h);
	}

	void YadifTemporal_SSE2(void *dst,
					const void *srct,
					const void *srcb,
					const void *srcAvgT0,	// field 1/3 average, two scanlines up
					const void *srcAvgC0,	// field 1/3 average, center scanline
					const void *srcAvgB0,	// field 1/3 average, two scanlines down
					const void *srcAdfC0,	// field 1/3 absdiff, center scanline
					const void *srcAdfP0,	// field 0/2 absdiff, averaged one scanline up and down
					const void *srcAdfN0,	// field 2/4 absdiff, averaged one scanline up and down
					uint32 w16
					) {
		// compute temporal differences
		//
		//   a b
		//  c d e
		//   f g
		//  h i j
		//   k l
		//	01234
		//
		//	p0 = avg(a, b)
		//	p1 = d
		//	p2 = avg(f, g)
		//	p3 = i
		//	p4 = avg(k, l)
		//
		//	mx = min(min(subus(p2, p3), subus(p2, p1)), max(subus(p0, p1), subus(p4, p3)))
		//	mn = min(min(subus(p3, p2), subus(p1, p2)), max(subus(p1, p0), subus(p3, p4)))
		//
		//	tdiffc = absdiff(f, g)/2
		//	tdiffp = avg(absdiff(c, d), absdiff(h, i))
		//	tdiffn = avg(absdiff(d, e), absdiff(i, j))
		//	tdiff = max(tdiffp, tdiffn)
		//	tdiff = max(max(tdiff, mn), mx)
		//	lo = subus(p2, tdiff)
		//	hi = addus(p2, tdiff)
		//
		//	result = min(max(static_result, lo), hi)
		//
		// We precompute:
		//
		//	absdiffblend(frame0, frame2) [preserved field]
		//	absdiffblend(frame2, frame4) [preserved field]
		//	absdiff(frame1, frame3) [interpolated field]
		//	average(frame1, frame3) [interpolated field]

		__m128i *dst2 = (__m128i *)dst;
		const __m128i *srct2 = (const __m128i *)srct;
		const __m128i *srcb2 = (const __m128i *)srcb;

		// scanline averages, one field forward and back
		const __m128i *srcAvgT = (const __m128i *)srcAvgT0;
		const __m128i *srcAvgC = (const __m128i *)srcAvgC0;
		const __m128i *srcAvgB = (const __m128i *)srcAvgB0;

		// difference scanlines, one field forward and back
		const __m128i *srcAdfC = (const __m128i *)srcAdfC0;

		// difference scanlines, current field and two fields back
		// difference scanlines, current field and two fields forward
		const __m128i *srcAdfP = (const __m128i *)srcAdfP0;
		const __m128i *srcAdfN = (const __m128i *)srcAdfN0;

		__m128i zero = _mm_setzero_si128();
		do {
			// compute temporal differences
			//
			//   a b
			//  c d e
			//   f g
			//  h i j
			//   k l
			//
			//	01234

			__m128i pxd		= *srct2++;
			__m128i pxi		= *srcb2++;
			__m128i pxAvgAB	= *srcAvgT++;
			__m128i pxAvgFG	= *srcAvgC++;
			__m128i pxAvgKL	= *srcAvgB++;
			__m128i pxAdfFG = *srcAdfC++;

			__m128i tdiffc	= _mm_avg_epu8(pxAdfFG, zero);
			__m128i tdiffp	= *srcAdfP++;
			__m128i tdiffn	= *srcAdfN++;
			__m128i tdiff	= _mm_max_epu8(tdiffc, _mm_max_epu8(tdiffp, tdiffn));

			// additional spatial deinterlacing
			__m128i p0 = pxAvgAB;
			__m128i p1 = pxd;
			__m128i p2 = pxAvgFG;
			__m128i p3 = pxi;
			__m128i p4 = pxAvgKL;

			__m128i mx = _mm_min_epu8(_mm_min_epu8(_mm_subs_epu8(p2, p3), _mm_subs_epu8(p2, p1)), _mm_max_epu8(_mm_subs_epu8(p0, p1), _mm_subs_epu8(p4, p3)));
			__m128i mn = _mm_min_epu8(_mm_min_epu8(_mm_subs_epu8(p3, p2), _mm_subs_epu8(p1, p2)), _mm_max_epu8(_mm_subs_epu8(p1, p0), _mm_subs_epu8(p3, p4)));
			tdiff = _mm_max_epu8(_mm_max_epu8(tdiff, mn), mx);

			__m128i lo = _mm_subs_epu8(p2, tdiff);
			__m128i hi = _mm_adds_epu8(p2, tdiff);

			__m128i pred = *dst2;
			pred = _mm_max_epu8(lo, pred);
			pred = _mm_min_epu8(hi, pred);

			*dst2++ = pred;
		} while(--w16);
	}

	void YadifTemporal_scalar(void *dst,
					const void *srct,
					const void *srcb,
					const void *srcAvgT0,	// field 1/3 average, two scanlines up
					const void *srcAvgC0,	// field 1/3 average, center scanline
					const void *srcAvgB0,	// field 1/3 average, two scanlines down
					const void *srcAdfC0,	// field 1/3 absdiff, center scanline
					const void *srcAdfP0,	// field 0/2 absdiff, averaged one scanline up and down
					const void *srcAdfN0,	// field 2/4 absdiff, averaged one scanline up and down
					uint32 w16
					) {
		// compute temporal differences
		//
		//   a b
		//  c d e
		//   f g
		//  h i j
		//   k l
		//	01234
		//
		//	p0 = avg(a, b)
		//	p1 = d
		//	p2 = avg(f, g)
		//	p3 = i
		//	p4 = avg(k, l)
		//
		//	mx = min(min(subus(p2, p3), subus(p2, p1)), max(subus(p0, p1), subus(p4, p3)))
		//	mn = min(min(subus(p3, p2), subus(p1, p2)), max(subus(p1, p0), subus(p3, p4)))
		//
		//	tdiffc = absdiff(f, g)/2
		//	tdiffp = avg(absdiff(c, d), absdiff(h, i))
		//	tdiffn = avg(absdiff(d, e), absdiff(i, j))
		//	tdiff = max(tdiffp, tdiffn)
		//	tdiff = max(max(tdiff, mn), mx)
		//	lo = subus(p2, tdiff)
		//	hi = addus(p2, tdiff)
		//
		//	result = min(max(static_result, lo), hi)
		//
		// We precompute:
		//
		//	absdiffblend(frame0, frame2) [preserved field]
		//	absdiffblend(frame2, frame4) [preserved field]
		//	absdiff(frame1, frame3) [interpolated field]
		//	average(frame1, frame3) [interpolated field]

		uint8 *dst2 = (uint8 *)dst;
		const uint8 *srct2 = (const uint8 *)srct;
		const uint8 *srcb2 = (const uint8 *)srcb;

		// scanline averages, one field forward and back
		const uint8 *srcAvgT = (const uint8 *)srcAvgT0;
		const uint8 *srcAvgC = (const uint8 *)srcAvgC0;
		const uint8 *srcAvgB = (const uint8 *)srcAvgB0;

		// difference scanlines, one field forward and back
		const uint8 *srcAdfC = (const uint8 *)srcAdfC0;

		// difference scanlines, current field and two fields back
		// difference scanlines, current field and two fields forward
		const uint8 *srcAdfP = (const uint8 *)srcAdfP0;
		const uint8 *srcAdfN = (const uint8 *)srcAdfN0;

		uint32 w = w16 << 4;
		do {
			// compute temporal differences
			//
			//   a b
			//  c d e
			//   f g
			//  h i j
			//   k l
			//
			//	01234

			int pxd		= *srct2++;
			int pxi		= *srcb2++;
			int pxAvgAB	= *srcAvgT++;
			int pxAvgFG	= *srcAvgC++;
			int pxAvgKL	= *srcAvgB++;
			int pxAdfFG = *srcAdfC++;

			int tdiffc	= (pxAdfFG + 1) >> 1;
			int tdiffp	= *srcAdfP++;
			int tdiffn	= *srcAdfN++;

#define fastmin(a, b) ((a) + (((b) - (a)) & (((b) - (a)) >> 31)))
#define fastmax(a, b) ((b) - (((b) - (a)) & (((b) - (a)) >> 31)))
#define fastminmax(a, b, mn, mx) ((t = (b) - (a)), (t &= t >> 31), ((mn) = (a) + t), ((mx) = (b) - t))
			int t;

			int tdiff	= fastmax(tdiffc, fastmax(tdiffp, tdiffn));

			// additional spatial deinterlacing
			int p0 = pxAvgAB;
			int p1 = pxd;
			int p2 = pxAvgFG;
			int p3 = pxi;
			int p4 = pxAvgKL;

			int d12 = p1 - p2;
			int d10 = p1 - p0;
			int d32 = p3 - p2;
			int d34 = p3 - p4;

			int min_32_12;
			int max_32_12;
			fastminmax(d32, d12, min_32_12, max_32_12);

			int min_10_34;
			int max_10_34;
			fastminmax(d10, d34, min_10_34, max_10_34);

			int mx = fastmax(max_32_12, min_10_34);
			int mn = fastmin(min_32_12, max_10_34);
			tdiff = fastmax(fastmax(tdiff, mn), -mx);

			int lo = p2 - tdiff;
			int hi = p2 + tdiff;

			int pred = *dst2;
			pred = fastmax(lo, pred);
			pred = fastmin(hi, pred);

#undef fastminmax
#undef fastmax
#undef fastmin

			*dst2++ = (uint8)pred;
		} while(--w);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

struct VDVideoFilterDeinterlaceConfig {
	enum Mode {
		kModeYadif,
		kModeELA,
		kModeBob,
		kModeBlend,
		kModeDuplicate,
		kModeDiscard,
		kModeUnfold,
		kModeFold,
		kModeCount
	};

	Mode	mMode;
	bool	mTFF;
	bool	mDoubleRate;

	VDVideoFilterDeinterlaceConfig()
		: mMode(kModeYadif)
		, mTFF(true)
		, mDoubleRate(false)
	{
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class VDVideoFilterDeinterlaceDialog : public VDDialogFrameW32 {
public:
	VDVideoFilterDeinterlaceDialog(VDVideoFilterDeinterlaceConfig& config)
		: VDDialogFrameW32(IDD_FILTER_DEINTERLACE)
		, mConfig(config)
	{}

protected:
	void OnDataExchange(bool write);

	VDVideoFilterDeinterlaceConfig& mConfig;
};

void VDVideoFilterDeinterlaceDialog::OnDataExchange(bool write) {
	if (write) {
		if (IsButtonChecked(IDC_MODE_INTERP_YADIF))
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeYadif;
		else if (IsButtonChecked(IDC_MODE_INTERP_ELA))
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeELA;
		else if (IsButtonChecked(IDC_MODE_INTERP_BOB))
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeBob;
		else if (IsButtonChecked(IDC_MODE_INTERP_BLEND))
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeBlend;
		else if (IsButtonChecked(IDC_MODE_DUP))
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeDuplicate;
		else if (IsButtonChecked(IDC_MODE_DISCARD))
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeDiscard;
		else if (IsButtonChecked(IDC_MODE_UNFOLD))
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeUnfold;
		else if (IsButtonChecked(IDC_MODE_FOLD))
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeFold;

		if (IsButtonChecked(IDC_FIELDMODE_KEEPTOP)) {
			mConfig.mTFF = true;
			mConfig.mDoubleRate = false;
		} else if (IsButtonChecked(IDC_FIELDMODE_KEEPBOTTOM)) {
			mConfig.mTFF = false;
			mConfig.mDoubleRate = false;
		} else if (IsButtonChecked(IDC_FIELDMODE_DOUBLETFF)) {
			mConfig.mTFF = true;
			mConfig.mDoubleRate = true;
		} else if (IsButtonChecked(IDC_FIELDMODE_DOUBLEBFF)) {
			mConfig.mTFF = false;
			mConfig.mDoubleRate = true;
		}
	} else {
		switch(mConfig.mMode) {
			case VDVideoFilterDeinterlaceConfig::kModeYadif:
				CheckButton(IDC_MODE_INTERP_YADIF, true);
				break;
			case VDVideoFilterDeinterlaceConfig::kModeELA:
				CheckButton(IDC_MODE_INTERP_ELA, true);
				break;
			case VDVideoFilterDeinterlaceConfig::kModeBob:
				CheckButton(IDC_MODE_INTERP_BOB, true);
				break;
			case VDVideoFilterDeinterlaceConfig::kModeBlend:
				CheckButton(IDC_MODE_INTERP_BLEND, true);
				break;
			case VDVideoFilterDeinterlaceConfig::kModeDuplicate:
				CheckButton(IDC_MODE_DUP, true);
				break;
			case VDVideoFilterDeinterlaceConfig::kModeDiscard:
				CheckButton(IDC_MODE_DISCARD, true);
				break;
			case VDVideoFilterDeinterlaceConfig::kModeUnfold:
				CheckButton(IDC_MODE_UNFOLD, true);
				break;
			case VDVideoFilterDeinterlaceConfig::kModeFold:
				CheckButton(IDC_MODE_FOLD, true);
				break;
		}

		static const int kFieldModeIDs[2][2]={
			IDC_FIELDMODE_KEEPBOTTOM,
			IDC_FIELDMODE_KEEPTOP,
			IDC_FIELDMODE_DOUBLEBFF,
			IDC_FIELDMODE_DOUBLETFF,
		};

		CheckButton(kFieldModeIDs[mConfig.mDoubleRate][mConfig.mTFF], true);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class VDVideoFilterDeinterlace : public VDXVideoFilter {
public:
	uint32 GetParams();
	void Start();
	void End();
	void Run();
	bool Configure(VDXHWND hwnd);
	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	bool Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher);
	bool OnInvalidateCaches();

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	void Run_Yadif(bool field2);
	void Run_ELA(bool field2);
	void Run_Bob(bool field2);
	void Run_Blend(bool field2);
	void Run_Duplicate(bool field2);
	void Run_Discard(bool field2);
	void Run_Fold(bool field2);
	void Run_Unfold(bool field2);

	void ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc);
	void ScriptConfigOld(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc);

	uint32		mLumaRowBytes;
	uint32		mChromaRowBytes;
	ptrdiff_t	mLumaPlanePitch;
	ptrdiff_t	mChromaPlanePitch;
	int			mBufferCount;
	bool		mbDoublingRate;

	VDVideoFilterDeinterlaceConfig mConfig;

	enum BufferType {
		kTypeNone,
		kTypeAverage,
		kTypeAbsDiff,
		kTypeAbsDiffBlended
	};

	struct BufferDesc {
		sint64 mField;
		BufferType mType;

		void Clear() {
			mField = 0;
			mType = kTypeNone;
		}

		bool operator==(const BufferDesc& x) const {
			return mField == x.mField && mType == x.mType;
		}

		bool operator!=(const BufferDesc& x) const {
			return mField != x.mField && mType != x.mType;
		}
	};

	struct Buffer {
		vdblock<uint8, vdaligned_alloc<uint8> > mStorage;
		BufferDesc mDesc;

		void Swap(Buffer& b) {
			mStorage.swap(b.mStorage);
			std::swap(mDesc, b.mDesc);
		}
	};

	enum { kMaxBuffers = 8 };

	Buffer	mBuffers[3][kMaxBuffers];

	vdblock<uint8, vdaligned_alloc<uint8> > mElaBuffer;
};

VDXVF_BEGIN_SCRIPT_METHODS(VDVideoFilterDeinterlace)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVideoFilterDeinterlace, ScriptConfig, "iii")
	VDXVF_DEFINE_SCRIPT_METHOD2(VDVideoFilterDeinterlace, ScriptConfigOld, "i")
VDXVF_END_SCRIPT_METHODS()

uint32 VDVideoFilterDeinterlace::GetParams() {
	VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	if (mConfig.mMode == VDVideoFilterDeinterlaceConfig::kModeFold)
		pxldst.w >>= 1;

	switch(pxlsrc.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
			if (mConfig.mMode == VDVideoFilterDeinterlaceConfig::kModeYadif)
				return FILTERPARAM_NOT_SUPPORTED;

			mLumaRowBytes = pxldst.w * 4;
			mChromaRowBytes = 0;
			break;

		case nsVDXPixmap::kPixFormat_Y8:
			mLumaRowBytes = pxldst.w;
			mChromaRowBytes = 0;
			break;

		case nsVDXPixmap::kPixFormat_YUV444_Planar:
			mLumaRowBytes = pxldst.w;
			mChromaRowBytes = pxldst.w;
			break;

		case nsVDXPixmap::kPixFormat_YUV422_Planar:
			mLumaRowBytes = pxldst.w;
			mChromaRowBytes = pxldst.w >> 1;
			break;

		case nsVDXPixmap::kPixFormat_YUV411_Planar:
			mLumaRowBytes = pxldst.w;
			mChromaRowBytes = pxldst.w >> 2;
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}

	mLumaPlanePitch = (mLumaRowBytes + 15) & ~15;
	mChromaPlanePitch = (mChromaRowBytes + 15) & ~15;

	mbDoublingRate = (mConfig.mDoubleRate && mConfig.mMode != VDVideoFilterDeinterlaceConfig::kModeFold && mConfig.mMode != VDVideoFilterDeinterlaceConfig::kModeUnfold);
	mBufferCount = 4;

	if (mbDoublingRate) {
		VDFraction fr(fa->dst.mFrameRateHi, fa->dst.mFrameRateLo);
		fr *= 2;
		fa->dst.mFrameRateHi = fr.getHi();
		fa->dst.mFrameRateLo = fr.getLo();
		fa->dst.mFrameCount *= 2;
		mBufferCount = 8;
	}

	fa->dst.depth = 0;

	switch(mConfig.mMode) {
	case VDVideoFilterDeinterlaceConfig::kModeYadif:
		return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_ALIGN_SCANLINES | FILTERPARAM_SWAP_BUFFERS;

	case VDVideoFilterDeinterlaceConfig::kModeELA:
		return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_ALIGN_SCANLINES;

	case VDVideoFilterDeinterlaceConfig::kModeBob:
		return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_ALIGN_SCANLINES;

	case VDVideoFilterDeinterlaceConfig::kModeBlend:
		return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_ALIGN_SCANLINES | FILTERPARAM_SWAP_BUFFERS;

	case VDVideoFilterDeinterlaceConfig::kModeDuplicate:
		pxldst.data = pxlsrc.data;
		pxldst.data2 = pxlsrc.data2;
		pxldst.data3 = pxlsrc.data3;
		break;

	case VDVideoFilterDeinterlaceConfig::kModeDiscard:
		if (mConfig.mTFF) {
			pxldst.data		= pxlsrc.data;
			pxldst.data2	= pxlsrc.data2;
			pxldst.data3	= pxlsrc.data3;
		} else {
			pxldst.data		= pxlsrc.data + pxldst.pitch;
			pxldst.data2	= pxlsrc.data2 + pxldst.pitch2;
			pxldst.data3	= pxlsrc.data3 + pxldst.pitch3;
		}

		pxldst.h		>>= 1;
		pxldst.pitch	*= 2;
		pxldst.pitch2	*= 2;
		pxldst.pitch3	*= 2;

		if (mbDoublingRate) {
			pxldst.pitch = 0;
			return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_SUPPORTS_ALTFORMATS;
		}

		break;
	case VDVideoFilterDeinterlaceConfig::kModeUnfold:
		pxldst.h		>>= 1;
		pxldst.w		<<= 1;
		pxldst.pitch	= 0;
		return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_SUPPORTS_ALTFORMATS;

	case VDVideoFilterDeinterlaceConfig::kModeFold:
		pxldst.h		<<= 1;
		pxldst.pitch	= 0;
		return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_SUPPORTS_ALTFORMATS;
	}

	return FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void VDVideoFilterDeinterlace::Start() {
	uint32 h2 = fa->dst.mpPixmap->h >> 1;

	for(int plane=0; plane<3; ++plane) {
		ptrdiff_t pitch = plane ? mChromaPlanePitch : mLumaPlanePitch;

		if (!pitch)
			break;

		for(int i=0; i<kMaxBuffers; ++i) {
			mBuffers[plane][i].mStorage.resize(pitch * (h2 + 1));
		}
	}

	OnInvalidateCaches();

	uint32 wr = (mLumaRowBytes + 15) & ~15;
	mElaBuffer.resize(12 * wr + 4*16);
}

void VDVideoFilterDeinterlace::End() {
	for(int plane=0; plane<3; ++plane) {
		for(int i=0; i<kMaxBuffers; ++i) {
			mBuffers[plane][i].mStorage.clear();
		}
	}

	mElaBuffer.clear();
}

void VDVideoFilterDeinterlace::Run() {
	bool field2 = !mConfig.mTFF;

	if (mConfig.mDoubleRate && (((int)fa->mpOutputFrames[0]->mFrameNumber) & 1) != 0)
		field2 = !field2;

	switch(mConfig.mMode) {
	case VDVideoFilterDeinterlaceConfig::kModeYadif:
		Run_Yadif(field2);
		break;

	case VDVideoFilterDeinterlaceConfig::kModeELA:
		Run_ELA(field2);
		break;

	case VDVideoFilterDeinterlaceConfig::kModeBob:
		Run_Bob(field2);
		break;

	case VDVideoFilterDeinterlaceConfig::kModeBlend:
		Run_Blend(field2);
		break;

	case VDVideoFilterDeinterlaceConfig::kModeDuplicate:
		Run_Duplicate(field2);
		break;

	case VDVideoFilterDeinterlaceConfig::kModeDiscard:
		Run_Discard(field2);
		break;

	case VDVideoFilterDeinterlaceConfig::kModeUnfold:
		Run_Unfold(field2);
		break;

	case VDVideoFilterDeinterlaceConfig::kModeFold:
		Run_Fold(field2);
		break;
	}
}

void VDVideoFilterDeinterlace::Run_Yadif(bool keepField2) {
	// we want to interpolate the opposite of the field we're keeping
	bool interpolatingBottomField = !keepField2;

	const VDXPixmap& pxdst = *fa->dst.mpPixmap;
	const uint32 h2 = pxdst.h >> 1;

	const bool topFieldFirst = mConfig.mTFF;
	const bool interpolatingSecondField = mConfig.mTFF ? interpolatingBottomField : !interpolatingBottomField;
	const bool interpolatingTopField = !interpolatingBottomField;
	const int planeCount = mChromaRowBytes ? 3 : 1;
	sint64 field2 = fa->mpOutputFrames[0]->mFrameNumber;

	if (!mConfig.mDoubleRate)
		field2 = field2 * 2 + (interpolatingTopField == topFieldFirst);

	const sint64 field1 = field2 - 1;
	const sint64 field0 = field2 - 2;

	for(int plane=0; plane<planeCount; ++plane) {
		struct Plane {
			void *data;
			ptrdiff_t pitch;
		};

		const uint32 rowBytes = plane ? mChromaRowBytes : mLumaRowBytes;
		const ptrdiff_t planePitch = (rowBytes + 15) & ~15;
		Plane srcPlanes[5];
		int baseOffset = interpolatingSecondField ? 1 : 0;

		switch(plane) {
			case 0:
				for(int i=0; i<5; ++i) {
					const VDXPixmap& pxsrc = *fa->mpSourceFrames[(i + baseOffset) >> 1]->mpPixmap;
					srcPlanes[i].data = pxsrc.data;
					srcPlanes[i].pitch = pxsrc.pitch;
				}
				break;

			case 1:
				for(int i=0; i<5; ++i) {
					const VDXPixmap& pxsrc = *fa->mpSourceFrames[(i + baseOffset) >> 1]->mpPixmap;
					srcPlanes[i].data = pxsrc.data2;
					srcPlanes[i].pitch = pxsrc.pitch2;
				}
				break;

			case 2:
				for(int i=0; i<5; ++i) {
					const VDXPixmap& pxsrc = *fa->mpSourceFrames[(i + baseOffset) >> 1]->mpPixmap;
					srcPlanes[i].data = pxsrc.data3;
					srcPlanes[i].pitch = pxsrc.pitch3;
				}
				break;
		}

		for(int i=0; i<5; ++i) {
			Plane& spl = srcPlanes[i];
			bool secondaryField = (((unsigned)field0 + i) & 1) != 0;

			if (secondaryField == topFieldFirst)
				spl.data = (char *)spl.data + spl.pitch;

			spl.pitch += spl.pitch;
		}

		const ptrdiff_t srcPitch = srcPlanes[0].pitch;

		// Lock these frames:
		//	absdiffblend(field0, field2) [preserved field]
		//	absdiffblend(field2, field4) [preserved field]
		//	absdiff(field1, field3) [interpolated field]
		//	average(field1, field3) [interpolated field]
		const BufferDesc neededBuffers[4]={
			{ field0, kTypeAbsDiffBlended },
			{ field2, kTypeAbsDiffBlended },
			{ field1, kTypeAbsDiff },
			{ field1, kTypeAverage }
		};

		uint32 dirtyBuffers = 15;

		Buffer *const planeBuffers = mBuffers[plane];
		for(int i=0; i<mBufferCount; ++i) {
			for(int j=0; j<mBufferCount; ++j) {
				if (planeBuffers[j].mDesc == neededBuffers[i]) {
					if (i != j) {
						planeBuffers[i].Swap(planeBuffers[j]);
						dirtyBuffers |= (1 << j);
					}

					dirtyBuffers &= ~(1 << i);
					break;
				}
			}
		}

		for(int i=0; i<mBufferCount; ++i) {
			if (!(dirtyBuffers & (1 << i)))
				break;

			int srcOffset = (int)neededBuffers[i].mField - (int)field0;
			switch(neededBuffers[i].mType) {
				case kTypeAverage:
					if (SSE2_enabled)
						Average_SSE2(planeBuffers[i].mStorage.data(), planePitch, srcPlanes[srcOffset].data, srcPlanes[srcOffset+2].data, srcPitch, planePitch >> 4, h2);
					else
						Average_scalar(planeBuffers[i].mStorage.data(), planePitch, srcPlanes[srcOffset].data, srcPlanes[srcOffset+2].data, srcPitch, planePitch >> 4, h2);
					break;

				case kTypeAbsDiff:
					if (SSE2_enabled)
						Absdiff_SSE2(planeBuffers[i].mStorage.data(), planePitch, srcPlanes[srcOffset].data, srcPlanes[srcOffset+2].data, srcPitch, planePitch >> 4, h2);
					else
						Absdiff_scalar(planeBuffers[i].mStorage.data(), planePitch, srcPlanes[srcOffset].data, srcPlanes[srcOffset+2].data, srcPitch, planePitch >> 4, h2);
					break;

				case kTypeAbsDiffBlended:
					if (SSE2_enabled)
						AbsDiffBlended_SSE2(planeBuffers[i].mStorage.data(), planePitch, srcPlanes[srcOffset].data, srcPlanes[srcOffset+2].data, srcPitch, planePitch >> 4, h2);
					else
						AbsDiffBlended_scalar(planeBuffers[i].mStorage.data(), planePitch, srcPlanes[srcOffset].data, srcPlanes[srcOffset+2].data, srcPitch, planePitch >> 4, h2);
					break;
			}

			planeBuffers[i].mDesc = neededBuffers[i];
		}

		//	top			bottom
		//
		//	-1				-1
		//		-1		0
		//	0	*		*	0
		//		0		+1
		//	+1				+1
		void *dst = plane==0 ? pxdst.data : plane == 1 ? pxdst.data2 : pxdst.data3;
		ptrdiff_t dstpitch = plane==0 ? pxdst.pitch : plane == 1 ? pxdst.pitch2 : pxdst.pitch3;

		if (interpolatingTopField) {
			VDMemcpyRect((char *)dst + dstpitch, dstpitch*2, srcPlanes[2].data, srcPitch, rowBytes, h2);
		} else {
			VDMemcpyRect((char *)dst, dstpitch*2, srcPlanes[2].data, srcPitch, rowBytes, h2);
			dst = (char *)dst + dstpitch;
		}

		dstpitch += dstpitch;

		void *elabuf = mElaBuffer.data();

		for(uint32 y=0; y<h2; ++y) {
			int y_top = (interpolatingTopField && y > 0 ? y-1 : y);
			int y_bot = (!interpolatingTopField && y < h2-1 ? y+1 : y);
			const void *srcT = (const char *)srcPlanes[2].data + srcPlanes[2].pitch * y_top;
			const void *srcB = (const char *)srcPlanes[2].data + srcPlanes[2].pitch * y_bot;
			const void *srcAvgT = (const char *)planeBuffers[3].mStorage.data() + planePitch * (y>0 ? y-1 : y);
			const void *srcAvgC = (const char *)planeBuffers[3].mStorage.data() + planePitch * y;
			const void *srcAvgB = (const char *)planeBuffers[3].mStorage.data() + planePitch * (y<h2-1 ? y+1 : y);
			const void *srcAdfC = (const char *)planeBuffers[2].mStorage.data() + planePitch * y;
			const void *srcAdfP = (const char *)planeBuffers[0].mStorage.data() + planePitch * y;
			const void *srcAdfN = (const char *)planeBuffers[1].mStorage.data() + planePitch * y;

			if (SSE2_enabled) {
				BlendScanLine_NELA_SSE2(dst, srcT, srcB, rowBytes, (__m128i *)elabuf);
				YadifTemporal_SSE2(dst, srcT, srcB, srcAvgT, srcAvgC, srcAvgB, srcAdfC, srcAdfP, srcAdfN, planePitch >> 4);
			} else {
				BlendScanLine_NELA_scalar(dst, srcT, srcB, rowBytes, (uint8 *)elabuf);
				YadifTemporal_scalar(dst, srcT, srcB, srcAvgT, srcAvgC, srcAvgB, srcAdfC, srcAdfP, srcAdfN, planePitch >> 4);
			}

			dst = (char *)dst + dstpitch;
		}
	}
}

void VDVideoFilterDeinterlace::Run_ELA(bool field2) {
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;
	const VDXPixmap& pxsrc = *fa->src.mpPixmap;

	// we want to interpolate the opposite of the field we're keeping
	field2 = !field2;

	if (pxdst.format == nsVDXPixmap::kPixFormat_XRGB8888) {
		InterpPlane_NELA_X8R8G8B8(pxdst.data, pxdst.pitch, pxsrc.data, pxsrc.pitch, mLumaRowBytes, pxdst.h, field2);
		return;
	}

	InterpPlane_NELA(pxdst.data, pxdst.pitch, pxsrc.data, pxsrc.pitch, mLumaRowBytes, pxdst.h, field2);

	if (mChromaRowBytes) {
		InterpPlane_NELA(pxdst.data2, pxdst.pitch2, pxsrc.data2, pxsrc.pitch2, mChromaRowBytes, pxdst.h, field2);
		InterpPlane_NELA(pxdst.data3, pxdst.pitch3, pxsrc.data3, pxsrc.pitch3, mChromaRowBytes, pxdst.h, field2);
	}
}

void VDVideoFilterDeinterlace::Run_Bob(bool field2) {
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;
	const VDXPixmap& pxsrc = *fa->src.mpPixmap;

	// we want to interpolate the opposite of the field we're keeping
	field2 = !field2;

	InterpPlane_Bob(pxdst.data, pxdst.pitch, pxsrc.data, pxsrc.pitch, mLumaRowBytes, pxdst.h, field2);

	if (mChromaRowBytes) {
		InterpPlane_Bob(pxdst.data2, pxdst.pitch2, pxsrc.data2, pxsrc.pitch2, mChromaRowBytes, pxdst.h, field2);
		InterpPlane_Bob(pxdst.data3, pxdst.pitch3, pxsrc.data3, pxsrc.pitch3, mChromaRowBytes, pxdst.h, field2);
	}
}

void VDVideoFilterDeinterlace::Run_Blend(bool field2) {
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;
	const VDXPixmap& pxsrc = *fa->src.mpPixmap;

	BlendPlane(pxdst.data, pxdst.pitch, pxsrc.data, pxsrc.pitch, mLumaRowBytes, pxdst.h);

	if (mChromaRowBytes) {
		BlendPlane(pxdst.data2, pxdst.pitch2, pxsrc.data2, pxsrc.pitch2, mChromaRowBytes, pxdst.h);
		BlendPlane(pxdst.data3, pxdst.pitch3, pxsrc.data3, pxsrc.pitch3, mChromaRowBytes, pxdst.h);
	}
}

void VDVideoFilterDeinterlace::Run_Duplicate(bool field2) {
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;
	const uint32 h2 = pxdst.h >> 1;

	if (field2) {
		VDMemcpyRect(pxdst.data, pxdst.pitch*2, (const char *)pxdst.data + pxdst.pitch, pxdst.pitch*2, mLumaRowBytes, h2);

		if (mChromaRowBytes) {
			VDMemcpyRect(pxdst.data2, pxdst.pitch2*2, (const char *)pxdst.data2 + pxdst.pitch2, pxdst.pitch2*2, mChromaRowBytes, h2);
			VDMemcpyRect(pxdst.data3, pxdst.pitch3*2, (const char *)pxdst.data3 + pxdst.pitch3, pxdst.pitch3*2, mChromaRowBytes, h2);
		}
	} else {
		VDMemcpyRect((char *)pxdst.data + pxdst.pitch, pxdst.pitch*2, pxdst.data, pxdst.pitch*2, mLumaRowBytes, h2);

		if (mChromaRowBytes) {
			VDMemcpyRect((char *)pxdst.data2 + pxdst.pitch2, pxdst.pitch2*2, pxdst.data2, pxdst.pitch2*2, mChromaRowBytes, h2);
			VDMemcpyRect((char *)pxdst.data3 + pxdst.pitch3, pxdst.pitch3*2, pxdst.data3, pxdst.pitch3*2, mChromaRowBytes, h2);
		}
	}
}

void VDVideoFilterDeinterlace::Run_Discard(bool field2) {
	if (!mbDoublingRate)
		return;

	const VDXPixmap& pxdst = *fa->dst.mpPixmap;
	const VDXPixmap& pxsrc = *fa->src.mpPixmap;
	const uint32 h = pxdst.h;

	if (field2) {
		VDMemcpyRect(pxdst.data, pxdst.pitch, (const char *)pxsrc.data + pxsrc.pitch, pxdst.pitch*2, mLumaRowBytes, h);

		if (mChromaRowBytes) {
			VDMemcpyRect(pxdst.data2, pxdst.pitch2, (const char *)pxsrc.data2 + pxsrc.pitch2, pxdst.pitch2*2, mChromaRowBytes, h);
			VDMemcpyRect(pxdst.data3, pxdst.pitch3, (const char *)pxsrc.data3 + pxsrc.pitch3, pxdst.pitch3*2, mChromaRowBytes, h);
		}
	} else {
		VDMemcpyRect(pxdst.data, pxdst.pitch, pxsrc.data, pxsrc.pitch*2, mLumaRowBytes, h);

		if (mChromaRowBytes) {
			VDMemcpyRect(pxdst.data2, pxdst.pitch2, pxsrc.data2, pxsrc.pitch2*2, mChromaRowBytes, h);
			VDMemcpyRect(pxdst.data3, pxdst.pitch3, pxsrc.data3, pxsrc.pitch3*2, mChromaRowBytes, h);
		}
	}
}

void VDVideoFilterDeinterlace::Run_Fold(bool field2) {
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;
	const VDXPixmap& pxsrc = *fa->src.mpPixmap;
	const uint32 h2 = pxdst.h >> 1;

	VDMemcpyRect(pxdst.data, pxdst.pitch*2, pxsrc.data, pxsrc.pitch, mLumaRowBytes, h2);
	VDMemcpyRect((char *)pxdst.data + pxdst.pitch, pxdst.pitch*2, (const char *)pxsrc.data + mLumaRowBytes, pxsrc.pitch, mLumaRowBytes, h2);

	if (mChromaRowBytes) {
		VDMemcpyRect(pxdst.data2, pxdst.pitch2*2, pxsrc.data2, pxsrc.pitch2, mLumaRowBytes, h2);
		VDMemcpyRect((char *)pxdst.data2 + pxdst.pitch2, pxdst.pitch2*2, (const char *)pxsrc.data2 + mChromaRowBytes, pxsrc.pitch2, mChromaRowBytes, h2);

		VDMemcpyRect(pxdst.data3, pxdst.pitch3*2, pxsrc.data3, pxsrc.pitch3, mChromaRowBytes, h2);
		VDMemcpyRect((char *)pxdst.data3 + pxdst.pitch3, pxdst.pitch3*2, (const char *)pxsrc.data3 + mChromaRowBytes, pxsrc.pitch3, mChromaRowBytes, h2);
	}
}

void VDVideoFilterDeinterlace::Run_Unfold(bool field2) {
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;
	const VDXPixmap& pxsrc = *fa->src.mpPixmap;
	const uint32 h = pxdst.h;

	VDMemcpyRect(pxdst.data, pxdst.pitch, pxsrc.data, pxsrc.pitch*2, mLumaRowBytes, h);
	VDMemcpyRect((char *)pxdst.data + mLumaRowBytes, pxdst.pitch, (const char *)pxsrc.data + pxsrc.pitch, pxsrc.pitch*2, mLumaRowBytes, h);

	if (mChromaRowBytes) {
		VDMemcpyRect(pxdst.data2, pxdst.pitch2, pxsrc.data2, pxsrc.pitch2*2, mChromaRowBytes, h);
		VDMemcpyRect((char *)pxdst.data2 + mChromaRowBytes, pxdst.pitch2, (const char *)pxsrc.data2 + pxsrc.pitch2, pxsrc.pitch2*2, mChromaRowBytes, h);

		VDMemcpyRect(pxdst.data3, pxdst.pitch3, pxsrc.data3, pxsrc.pitch3*2, mChromaRowBytes, h);
		VDMemcpyRect((char *)pxdst.data3 + mChromaRowBytes, pxdst.pitch3, (const char *)pxsrc.data3 + pxsrc.pitch3, pxsrc.pitch3*2, mChromaRowBytes, h);
	}
}

bool VDVideoFilterDeinterlace::Configure(VDXHWND hwnd) {
	VDVideoFilterDeinterlaceConfig cfg(mConfig);
	VDVideoFilterDeinterlaceDialog dlg(mConfig);

	if (dlg.ShowDialog((VDGUIHandle)hwnd))
		return true;

	mConfig = cfg;
	return false;
}

void VDVideoFilterDeinterlace::GetSettingString(char *buf, int maxlen) {
	static const char *const kModes[]={
		"yadif",
		"ELA",
		"bob",
		"blend",
		"duplicate",
		"discard",
		"unfold",
		"fold"
	};

	static const char *const kFieldModes[2][2]={
		"keep bottom",
		"keep top",
		"double-BFF",
		"double-TFF",
	};

	SafePrintf(buf, maxlen, " (mode: %s, %s)", kModes[mConfig.mMode], kFieldModes[mConfig.mDoubleRate][mConfig.mTFF]);
}

void VDVideoFilterDeinterlace::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d,%d,%d)", mConfig.mMode, mConfig.mTFF, mConfig.mDoubleRate);
}

bool VDVideoFilterDeinterlace::Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher) {
	if (mbDoublingRate)
		frame >>= 1;

	if (mConfig.mMode == VDVideoFilterDeinterlaceConfig::kModeYadif) {
		prefetcher->PrefetchFrame(0, frame-1, 0);
		prefetcher->PrefetchFrame(0, frame, 0);
		prefetcher->PrefetchFrame(0, frame+1, 0);
	} else {
		prefetcher->PrefetchFrame(0, frame, 0);
	}

	return true;
}

bool VDVideoFilterDeinterlace::OnInvalidateCaches() {
	for(int plane=0; plane<3; ++plane) {
		for(int i=0; i<kMaxBuffers; ++i) {
			mBuffers[plane][i].mDesc.Clear();
		}
	}

	return true;
}

void VDVideoFilterDeinterlace::ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	int mode =  argv[0].asInt();

	if (mode < 0 || mode >= VDVideoFilterDeinterlaceConfig::kModeCount)
		mode = 0;

	mConfig.mMode = (VDVideoFilterDeinterlaceConfig::Mode)mode;
	mConfig.mTFF = argv[1].asInt() != 0;
	mConfig.mDoubleRate = argv[2].asInt() != 0;
}

void VDVideoFilterDeinterlace::ScriptConfigOld(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	int mode =  argv[0].asInt();

	if (mode < 0 || mode > 6)
		mode = 0;

	mConfig.mTFF = true;
	mConfig.mDoubleRate = false;

	switch(mode) {
		case 0:
		default:
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeBlend;
			break;

		case 1:
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeDuplicate;
			break;

		case 2:
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeDuplicate;
			mConfig.mTFF = false;
			break;

		case 3:
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeDiscard;
			break;

		case 4:
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeDiscard;
			mConfig.mTFF = false;
			break;

		case 5:
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeFold;
			break;

		case 6:
			mConfig.mMode = VDVideoFilterDeinterlaceConfig::kModeUnfold;
			break;
	}
}

VDXFilterDefinition filterDef_deinterlace = VDXVideoFilterDefinition<VDVideoFilterDeinterlace>(
	NULL,
	"deinterlace",
	"Removes scanline artifacts from interlaced video. Parts based on the Yadif deinterlacing algorithm by Michael Niedermayer."
	);

#ifdef _MSC_VER
	#pragma warning(disable: 4505)	// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{48,{flat}}' }'' : unreferenced local function has been removed

#endif