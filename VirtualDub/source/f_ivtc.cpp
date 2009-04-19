//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"

#include <vd2/system/cpuaccel.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/fraction.h>
#include <vd2/system/memory.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDLib/Dialog.h>
#include <intrin.h>
#include <emmintrin.h>

#include "resource.h"
#include "filter.h"

////////////////////////////////////////////////////////////

namespace {
	struct IVTCScore {
		sint64 mVar[2];
		sint64 mVarShift[2];
	};

	IVTCScore operator+(const IVTCScore& x, const IVTCScore& y) {
		IVTCScore r;

		r.mVar[0] = x.mVar[0] + y.mVar[0];
		r.mVar[1] = x.mVar[1] + y.mVar[1];
		r.mVarShift[0] = x.mVarShift[0] + y.mVarShift[0];
		r.mVarShift[1] = x.mVarShift[1] + y.mVarShift[1];
		return r;
	}

#ifdef VD_CPU_X86
	#pragma warning(disable: 4799)	// warning C4799: function '`anonymous namespace'::ComputeScanLineImprovement_XRGB8888_MMX' has no EMMS instruction

	void __declspec(naked) ComputeScanLineImprovement_X8R8G8B8_MMX(const void *src1, const void *src2, ptrdiff_t pitch, uint32 w, sint64 *var, sint64 *varshift) {
		__asm {
			push		ebx

			mov			eax,[esp+8]
			mov			ecx,[esp+16]
			mov			edx,[esp+12]
			mov			ebx,[esp+20]

			pxor		mm5,mm5
			pxor		mm6,mm6
	xloop:
			movd		mm0,[eax]
			pxor		mm7,mm7

			movd		mm1,[eax+ecx*2]
			punpcklbw	mm0,mm7			;mm0 = pA

			movd		mm2,[eax+ecx]
			punpcklbw	mm1,mm7			;mm1 = pC

			movd		mm3,[edx+ecx]
			punpcklbw	mm2,mm7			;mm2 = pB

			punpcklbw	mm3,mm7			;mm3 = pE
			paddw		mm0,mm1			;mm0 = pA + pC

			paddw		mm3,mm3			;mm3 = 2*pE
			paddw		mm2,mm2			;mm2 = 2*pB

			psubw		mm3,mm0			;mm3 = 2*pE - (pA + pC)
			psubw		mm0,mm2			;mm0 = pA + pC - 2*pB

			psllq		mm3,16
			add			eax,4

			pmaddwd		mm3,mm3			;mm3 = sq(pA + pC - 2*pE)	[mm5 mm3 ---]
			psllq		mm0,16

			pmaddwd		mm0,mm0			;mm0 = sq(pA + pC - 2*pB)	[mm0 mm5 mm3]
			add			edx,4

			paddd		mm5,mm3
			dec			ebx

			paddd		mm6,mm0
			jne			xloop

			movq		mm0, mm6
			psrlq		mm0, 32
			movq		mm1, mm5
			psrlq		mm1, 32
			paddd		mm0, mm6
			movd		eax, mm0
			paddd		mm1, mm5
			movd		edx, mm1

			mov			ecx, [esp+24]
			add			dword ptr [ecx], eax
			adc			dword ptr [ecx+4], 0

			mov			ecx, [esp+28]
			add			dword ptr [ecx], edx
			adc			dword ptr [ecx+4], 0

			pop			ebx
			ret
		}
	}
#endif

#if defined(VD_CPU_X86) || defined(VD_CPU_AMD64)
	IVTCScore ComputeScanImprovement_X8R8G8B8_SSE2(const void *src1, const void *src2, ptrdiff_t srcpitch, uint32 w, uint32 h) {
		IVTCScore score = {0};

		__m128i zero = _mm_setzero_si128();

		uint32 w2 = w >> 1;

		static const __m128i mask = { -1, -1, -1, -1, -1, -1, 0, 0, -1, -1, -1, -1, -1, -1, 0, 0 };

		bool firstfield = true;
		do {
			__m128i var = zero;
			__m128i varshift = zero;

			const uint8 *src1r0 = (const uint8 *)src1;
			const uint8 *src1r1 = src1r0 + srcpitch;
			const uint8 *src1r2 = src1r1 + srcpitch;
			const uint8 *src2r = (const uint8 *)src2 + srcpitch;
			for(uint32 x=0; x<w2; ++x) {
				__m128i rA = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r0), zero);
				__m128i rB = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r1), zero);
				__m128i rC = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r2), zero);
				__m128i rE = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src2r), zero);
				__m128i rAC = _mm_add_epi16(rA, rC);
				__m128i d1 = _mm_sub_epi16(rAC, _mm_add_epi16(rB, rB));		// combing in current frame
				__m128i d3 = _mm_sub_epi16(rAC, _mm_add_epi16(rE, rE));		// combing in merged frame

				d1 = _mm_and_si128(d1, mask);
				d3 = _mm_and_si128(d3, mask);

				var = _mm_add_epi32(var, _mm_madd_epi16(d1, d1));
				varshift = _mm_add_epi32(varshift, _mm_madd_epi16(d3, d3));

				src1r0 += 8;
				src1r1 += 8;
				src1r2 += 8;
				src2r += 8;
			}

			if (w & 1) {
				__m128i rA = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)src1r0), zero);
				__m128i rB = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)src1r1), zero);
				__m128i rC = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)src1r2), zero);
				__m128i rE = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)src2r), zero);
				__m128i rAC = _mm_add_epi16(rA, rC);
				__m128i d1 = _mm_sub_epi16(rAC, _mm_add_epi16(rB, rB));		// combing in current frame
				__m128i d3 = _mm_sub_epi16(rAC, _mm_add_epi16(rE, rE));		// combing in merged frame

				d1 = _mm_and_si128(d1, mask);
				d3 = _mm_and_si128(d3, mask);

				var = _mm_add_epi32(var, _mm_madd_epi16(d1, d1));
				varshift = _mm_add_epi32(varshift, _mm_madd_epi16(d3, d3));
			}

			src1 = (const uint8 *)src1 + srcpitch;
			src2 = (const uint8 *)src2 + srcpitch;

			var = _mm_add_epi32(var, _mm_shuffle_epi32(var, 0xee));
			varshift = _mm_add_epi32(varshift, _mm_shuffle_epi32(varshift, 0xee));
			var = _mm_add_epi32(var, _mm_shuffle_epi32(var, 0x55));
			varshift = _mm_add_epi32(varshift, _mm_shuffle_epi32(varshift, 0x55));

			uint32 ivar = _mm_cvtsi128_si32(var);
			uint32 ivarshift = _mm_cvtsi128_si32(varshift);

			if (firstfield) {
				score.mVar[0] += ivar;
				score.mVarShift[0] += ivarshift;
			} else {
				score.mVar[1] += ivar;
				score.mVarShift[1] += ivarshift;
			}

			firstfield = !firstfield;
		} while(--h);

		return score;
	}
#endif

	IVTCScore ComputeScanImprovement_XRGB8888(const void *src1, const void *src2, ptrdiff_t srcpitch, uint32 w, uint32 h) {
		IVTCScore score = {0};

#if defined(VD_CPU_X86) || defined(VD_CPU_AMD64)
		if (SSE2_enabled)
			return ComputeScanImprovement_X8R8G8B8_SSE2(src1, src2, srcpitch, w, h);
#endif

#ifdef VD_CPU_X86
		if (MMX_enabled) {
			int phase = 0;
			do {
				ComputeScanLineImprovement_X8R8G8B8_MMX(src1, src2, srcpitch, w, &score.mVar[phase], &score.mVarShift[phase]);

				phase ^= 1;
				src1 = (const char *)src1 + srcpitch;
				src2 = (const char *)src2 + srcpitch;
			} while(--h);

			__asm emms
			return score;
		}
#endif

		bool firstfield = true;
		do {
			uint32 var = 0;
			uint32 varshift = 0;

			const uint8 *src1r0 = (const uint8 *)src1;
			const uint8 *src1r1 = src1r0 + srcpitch;
			const uint8 *src1r2 = src1r1 + srcpitch;
			const uint8 *src2r = (const uint8 *)src2 + srcpitch;
			for(uint32 x=0; x<w; ++x) {
				int bA = src1r0[0];
				int gA = src1r0[1];
				int rA = src1r0[2];
				int bB = src1r1[0];
				int gB = src1r1[1];
				int rB = src1r1[2];
				int bC = src1r2[0];
				int gC = src1r2[1];
				int rC = src1r2[2];
				int bE = src2r[0];
				int gE = src2r[1];
				int rE = src2r[2];
				int rd1 = rA + rC - 2*rB;		// combing in current frame
				int gd1 = gA + gC - 2*gB;
				int bd1 = bA + bC - 2*bB;
				int rd3 = rA + rC - 2*rE;		// combing in merged frame
				int gd3 = gA + gC - 2*gE;
				int bd3 = bA + bC - 2*bE;

				var += rd1*rd1;
				var += gd1*gd1;
				var += bd1*bd1;
				varshift += rd3*rd3;
				varshift += gd3*gd3;
				varshift += bd3*bd3;

				src1r0 += 4;
				src1r1 += 4;
				src1r2 += 4;
				src2r += 4;
			}

			src1 = (const uint8 *)src1 + srcpitch;
			src2 = (const uint8 *)src2 + srcpitch;

			if (firstfield) {
				score.mVar[0] += var;
				score.mVarShift[0] += varshift;
			} else {
				score.mVar[1] += var;
				score.mVarShift[1] += varshift;
			}

			firstfield = !firstfield;
		} while(--h);

		return score;
	}

#ifdef VD_CPU_X86
	#pragma warning(disable: 4799)	// warning C4799: function '`anonymous namespace'::ComputeScanLineImprovement_XRGB8888_MMX' has no EMMS instruction

	void __declspec(naked) ComputeScanLineImprovement_L8_MMX(const void *src1, const void *src2, ptrdiff_t pitch, uint32 w, sint64 *var, sint64 *varshift) {
		__asm {
			push		ebx

			mov			eax,[esp+8]
			mov			ecx,[esp+16]
			mov			edx,[esp+12]
			mov			ebx,[esp+20]

			pxor		mm5,mm5
			pxor		mm6,mm6

			sub			ebx, 3
			jbe			xfin
	xloop:
			movd		mm0,[eax]
			pxor		mm7,mm7

			movd		mm1,[eax+ecx*2]
			punpcklbw	mm0,mm7			;mm0 = pA

			movd		mm2,[eax+ecx]
			punpcklbw	mm1,mm7			;mm1 = pC

			movd		mm3,[edx+ecx]
			punpcklbw	mm2,mm7			;mm2 = pB

			punpcklbw	mm3,mm7			;mm3 = pE
			paddw		mm0,mm1			;mm0 = pA + pC

			paddw		mm3,mm3			;mm3 = 2*pE
			paddw		mm2,mm2			;mm2 = 2*pB

			psubw		mm3,mm0			;mm3 = 2*pE - (pA + pC)
			psubw		mm0,mm2			;mm0 = pA + pC - 2*pB

			pmaddwd		mm3,mm3			;mm3 = sq(pA + pC - 2*pE)	[mm5 mm3 ---]
			add			eax,4

			pmaddwd		mm0,mm0			;mm0 = sq(pA + pC - 2*pB)	[mm0 mm5 mm3]
			add			edx,4

			paddd		mm5,mm3
			sub			ebx,4

			paddd		mm6,mm0
			ja			xloop
			jz			xend
	xfin:
			pcmpeqb		mm4, mm4
			mov			ebx, [esp+20]
			and			ebx, 3
			shl			ebx, 4
			movd		mm0, ebx
			psrlq		mm4, mm0

			movd		mm0,[eax]
			movd		mm1,[eax+ecx*2]
			punpcklbw	mm0,mm7			;mm0 = pA
			movd		mm2,[eax+ecx]
			punpcklbw	mm1,mm7			;mm1 = pC
			movd		mm3,[edx+ecx]
			punpcklbw	mm2,mm7			;mm2 = pB
			punpcklbw	mm3,mm7			;mm3 = pE
			paddw		mm0,mm1			;mm0 = pA + pC
			paddw		mm3,mm3			;mm3 = 2*pE
			paddw		mm2,mm2			;mm2 = 2*pB
			psubw		mm3,mm0			;mm3 = 2*pE - (pA + pC)
			psubw		mm0,mm2			;mm0 = pA + pC - 2*pB
			pand		mm3,mm4
			pand		mm0,mm4
			pmaddwd		mm3,mm3			;mm3 = sq(pA + pC - 2*pE)	[mm5 mm3 ---]
			pmaddwd		mm0,mm0			;mm0 = sq(pA + pC - 2*pB)	[mm0 mm5 mm3]
			paddd		mm5,mm3
			paddd		mm6,mm0
	xend:
			movq		mm0, mm6
			psrlq		mm0, 32
			movq		mm1, mm5
			psrlq		mm1, 32
			paddd		mm0, mm6
			movd		eax, mm0
			paddd		mm1, mm5
			movd		edx, mm1

			mov			ecx, [esp+24]
			add			dword ptr [ecx], eax
			adc			dword ptr [ecx+4], 0

			mov			ecx, [esp+28]
			add			dword ptr [ecx], edx
			adc			dword ptr [ecx+4], 0

			pop			ebx
			ret
		}
	}
#endif

#if defined(VD_CPU_X86) || defined(VD_CPU_AMD64)
	IVTCScore ComputeScanImprovement_L8_SSE2(const void *src1, const void *src2, ptrdiff_t srcpitch, uint32 w, uint32 h) {
		IVTCScore score = {0};

		__m128i zero = _mm_setzero_si128();

		uint32 w8 = w >> 3;

		static const uint8 kMaskArray[32] = {
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		};

		__m128i mask = _mm_loadl_epi64((const __m128i *)(kMaskArray + 2*(w & 7)));

		bool firstfield = true;
		do {
			__m128i var = zero;
			__m128i varshift = zero;

			const uint8 *src1r0 = (const uint8 *)src1;
			const uint8 *src1r1 = src1r0 + srcpitch;
			const uint8 *src1r2 = src1r1 + srcpitch;
			const uint8 *src2r = (const uint8 *)src2 + srcpitch;
			for(uint32 x=0; x<w8; ++x) {
				__m128i rA = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r0), zero);
				__m128i rB = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r1), zero);
				__m128i rC = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r2), zero);
				__m128i rE = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src2r), zero);
				__m128i rAC = _mm_add_epi16(rA, rC);
				__m128i d1 = _mm_sub_epi16(rAC, _mm_add_epi16(rB, rB));		// combing in current frame
				__m128i d3 = _mm_sub_epi16(rAC, _mm_add_epi16(rE, rE));		// combing in merged frame

				var = _mm_add_epi32(var, _mm_madd_epi16(d1, d1));
				varshift = _mm_add_epi32(varshift, _mm_madd_epi16(d3, d3));

				src1r0 += 8;
				src1r1 += 8;
				src1r2 += 8;
				src2r += 8;
			}

			if (w & 7) {
				__m128i rA = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r0), zero);
				__m128i rB = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r1), zero);
				__m128i rC = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r2), zero);
				__m128i rE = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src2r), zero);
				__m128i rAC = _mm_add_epi16(rA, rC);
				__m128i d1 = _mm_sub_epi16(rAC, _mm_add_epi16(rB, rB));		// combing in current frame
				__m128i d3 = _mm_sub_epi16(rAC, _mm_add_epi16(rE, rE));		// combing in merged frame

				d1 = _mm_and_si128(d1, mask);
				d3 = _mm_and_si128(d3, mask);

				var = _mm_add_epi32(var, _mm_madd_epi16(d1, d1));
				varshift = _mm_add_epi32(varshift, _mm_madd_epi16(d3, d3));
			}

			src1 = (const uint8 *)src1 + srcpitch;
			src2 = (const uint8 *)src2 + srcpitch;

			var = _mm_add_epi32(var, _mm_shuffle_epi32(var, 0xee));
			varshift = _mm_add_epi32(varshift, _mm_shuffle_epi32(varshift, 0xee));
			var = _mm_add_epi32(var, _mm_shuffle_epi32(var, 0x55));
			varshift = _mm_add_epi32(varshift, _mm_shuffle_epi32(varshift, 0x55));

			uint32 ivar = _mm_cvtsi128_si32(var);
			uint32 ivarshift = _mm_cvtsi128_si32(varshift);

			if (firstfield) {
				score.mVar[0] += ivar;
				score.mVarShift[0] += ivarshift;
			} else {
				score.mVar[1] += ivar;
				score.mVarShift[1] += ivarshift;
			}

			firstfield = !firstfield;
		} while(--h);

		return score;
	}
#endif

	IVTCScore ComputeScanImprovement(const void *src1, const void *src2, ptrdiff_t srcpitch, uint32 w, uint32 h) {
#if defined(VD_CPU_X86) || defined(VD_CPU_AMD64)
		if (SSE2_enabled)
			return ComputeScanImprovement_L8_SSE2(src1, src2, srcpitch, w, h);
#endif

		IVTCScore score = {0};

#ifdef VD_CPU_X86
		if (MMX_enabled) {
			int phase = 0;
			do {
				ComputeScanLineImprovement_L8_MMX(src1, src2, srcpitch, w, &score.mVar[phase], &score.mVarShift[phase]);

				phase ^= 1;
				src1 = (const char *)src1 + srcpitch;
				src2 = (const char *)src2 + srcpitch;
			} while(--h);

			__asm emms
			return score;
		}
#endif

		bool firstfield = true;
		do {
			uint32 var = 0;
			uint32 varshift = 0;

			// This is the algorithm comment from the original VideoTelecineRemover code, although
			// it only sort of applies now:
			//
			//	now using original intended algorithm, plus checking if the second frame is also combed
			//	so it doesn't easily take the frame to be dropped as the frame to be decombed. 
			//	Check below to see fix...  This actually works very well! Beats any commercial
			//	software out there!
			//
			//	Without the fix, the reason the broken algorithm sorta worked is because, in the 
			//	sequence [A1/A2] [A1/B2], if A2 sorta looked like B2, it would actually take the 
			//	right offset... but when scene changes a lot, this does not work at all. 
			//
			//	Samuel Audet <guardia@cam.órg>

			const uint8 *src1r0 = (const uint8 *)src1;
			const uint8 *src1r1 = src1r0 + srcpitch;
			const uint8 *src1r2 = src1r1 + srcpitch;
			const uint8 *src2r = (const uint8 *)src2 + srcpitch;
			for(uint32 x=0; x<w; ++x) {
				int rA = src1r0[x];
				int rB = src1r1[x];
				int rC = src1r2[x];
				int rE = src2r[x];
				int d1 = rA + rC - 2*rB;		// combing in current frame
				int d3 = rA + rC - 2*rE;		// combing in merged frame

				var += d1*d1;
				varshift += d3*d3;
			}

			src1 = (const uint8 *)src1 + srcpitch;
			src2 = (const uint8 *)src2 + srcpitch;

			if (firstfield) {
				score.mVar[0] += var;
				score.mVarShift[0] += varshift;
			} else {
				score.mVar[1] += var;
				score.mVarShift[1] += varshift;
			}

			firstfield = !firstfield;
		} while(--h);

		return score;
	}

	IVTCScore ComputeScanImprovement(const VDXPixmap& px1, const VDXPixmap& px2) {
		uint32 w = px1.w;
		uint32 h = px1.h;

		IVTCScore zero = {0};

		if (h < 4)
			return zero;

		h -= 4;

		const void *src1data  = (const char *)px1.data  + 2*px1.pitch;
		const void *src1data2 = (const char *)px1.data2 + 2*px1.pitch2;
		const void *src1data3 = (const char *)px1.data3 + 2*px1.pitch3;
		const void *src2data  = (const char *)px2.data  + 2*px2.pitch;
		const void *src2data2 = (const char *)px2.data2 + 2*px2.pitch2;
		const void *src2data3 = (const char *)px2.data3 + 2*px2.pitch3;

		VDASSERT(px1.pitch == px2.pitch);

		switch(px1.format) {
			case nsVDXPixmap::kPixFormat_XRGB8888:
				return	ComputeScanImprovement_XRGB8888(src1data, px2.data, px2.pitch, w, h);

			case nsVDXPixmap::kPixFormat_YUV444_Planar:
				return	ComputeScanImprovement(src1data,  src2data,  px2.pitch, w, h) +
						ComputeScanImprovement(src1data2, src2data2, px2.pitch2, w, h) +
						ComputeScanImprovement(src1data3, src2data3, px2.pitch3, w, h);

			case nsVDXPixmap::kPixFormat_YUV422_Planar:
				return	ComputeScanImprovement(src1data,  src2data,  px2.pitch, w, h) +
						ComputeScanImprovement(src1data2, src2data2, px2.pitch2, w >> 1, h) +
						ComputeScanImprovement(src1data3, src2data3, px2.pitch3, w >> 1, h);

			case nsVDXPixmap::kPixFormat_YUV411_Planar:
				return	ComputeScanImprovement(src1data,  src2data,  px2.pitch, w, h) +
						ComputeScanImprovement(src1data2, src2data2, px2.pitch2, w >> 2, h) +
						ComputeScanImprovement(src1data3, src2data3, px2.pitch3, w >> 2, h);

			case nsVDXPixmap::kPixFormat_YUV422_UYVY:
			case nsVDXPixmap::kPixFormat_YUV422_YUYV:
				return ComputeScanImprovement(src1data, px2.data, px2.pitch, w*2, h);

			default:
				VDASSERT(false);
		}

		return zero;
	}

	VDXPixmap SlicePixmap(const VDXPixmap& px, bool field2) {
		VDXPixmap t(px);

		if (field2) {
			t.data = (char *)t.data + t.pitch;

			if (t.data2)
				t.data2 = (char *)t.data2 + t.pitch2;

			if (t.data3)
				t.data3 = (char *)t.data3 + t.pitch3;
		}

		t.pitch += t.pitch;
		t.pitch2 += t.pitch2;
		t.pitch3 += t.pitch3;
		t.h >>= 1;

		return t;
	}

	void CopyPlane(void *dst, ptrdiff_t dstpitch, const void *src, ptrdiff_t srcpitch, uint32 w, uint32 h) {
		for(uint32 y=0; y<h; ++y) {
			memcpy(dst, src, w);

			dst = (char *)dst + dstpitch;
			src = (const char *)src + srcpitch;
		}
	}

	void CopyPixmap(const VDXPixmap& dst, const VDXPixmap& src) {
		const uint32 w = dst.w;
		const uint32 h = dst.h;

		switch(dst.format) {
			case nsVDXPixmap::kPixFormat_XRGB8888:
				CopyPlane(dst.data, dst.pitch, src.data, src.pitch, w*4, h);
				break;

			case nsVDXPixmap::kPixFormat_YUV444_Planar:
				CopyPlane(dst.data, dst.pitch, src.data, src.pitch, w, h);
				CopyPlane(dst.data2, dst.pitch2, src.data2, src.pitch2, w, h);
				CopyPlane(dst.data3, dst.pitch3, src.data3, src.pitch3, w, h);
				break;

			case nsVDXPixmap::kPixFormat_YUV422_Planar:
				CopyPlane(dst.data, dst.pitch, src.data, src.pitch, w, h);
				CopyPlane(dst.data2, dst.pitch2, src.data2, src.pitch2, w >> 1, h);
				CopyPlane(dst.data3, dst.pitch3, src.data3, src.pitch3, w >> 1, h);
				break;

			case nsVDXPixmap::kPixFormat_YUV411_Planar:
				CopyPlane(dst.data, dst.pitch, src.data, src.pitch, w, h);
				CopyPlane(dst.data2, dst.pitch2, src.data2, src.pitch2, w >> 2, h);
				CopyPlane(dst.data3, dst.pitch3, src.data3, src.pitch3, w >> 2, h);
				break;

			case nsVDXPixmap::kPixFormat_YUV422_UYVY:
			case nsVDXPixmap::kPixFormat_YUV422_YUYV:
				CopyPlane(dst.data, dst.pitch, src.data, src.pitch, w*2, h);
				break;

			default:
				VDASSERT(false);
				break;
		}
	}
}

////////////////////////////////////////////////////////////

struct VDVideoFilterIVTCConfig {
	enum FieldMode {
		kFieldMode_Autoselect,
		kFieldMode_TFF,
		kFieldMode_BFF,
		kFieldModeCount
	};

	bool mbReduceRate;
	int mOffset;
	FieldMode mFieldMode;

	VDVideoFilterIVTCConfig()
		: mbReduceRate(false)
		, mOffset(-1)
		, mFieldMode(kFieldMode_Autoselect)
	{
	}
};

class VDVideoFilterIVTCDialog : public VDDialogFrameW32 {
public:
	VDVideoFilterIVTCDialog(VDVideoFilterIVTCConfig& config)
		: VDDialogFrameW32(IDD_FILTER_IVTC)
		, mConfig(config) {}

protected:
	void OnDataExchange(bool write);

	VDVideoFilterIVTCConfig& mConfig;
};

void VDVideoFilterIVTCDialog::OnDataExchange(bool write) {
	if (write) {
		mConfig.mbReduceRate = IsButtonChecked(IDC_MODE_REDUCE);

		if (IsButtonChecked(IDC_FIELDORDER_AUTO))
			mConfig.mFieldMode = VDVideoFilterIVTCConfig::kFieldMode_Autoselect;
		else if (IsButtonChecked(IDC_FIELDORDER_TFF))
			mConfig.mFieldMode = VDVideoFilterIVTCConfig::kFieldMode_TFF;
		else if (IsButtonChecked(IDC_FIELDORDER_BFF))
			mConfig.mFieldMode = VDVideoFilterIVTCConfig::kFieldMode_BFF;

		if (IsButtonChecked(IDC_PHASE_MANUAL)) {
			uint32 v = GetControlValueUint32(IDC_PHASE);
			if (v >= 5)
				FailValidation(IDC_PHASE);
			else
				mConfig.mOffset = v;
		} else {
			mConfig.mOffset = -1;
		}
	} else {
		if (mConfig.mbReduceRate)
			CheckButton(IDC_MODE_REDUCE, true);
		else
			CheckButton(IDC_MODE_DECOMB, true);

		switch(mConfig.mFieldMode) {
			case VDVideoFilterIVTCConfig::kFieldMode_Autoselect:
				CheckButton(IDC_FIELDORDER_AUTO, true);
				break;
			case VDVideoFilterIVTCConfig::kFieldMode_TFF:
				CheckButton(IDC_FIELDORDER_TFF, true);
				break;
			case VDVideoFilterIVTCConfig::kFieldMode_BFF:
				CheckButton(IDC_FIELDORDER_BFF, true);
				break;
		}

		if (mConfig.mOffset >= 0) {
			CheckButton(IDC_PHASE_MANUAL, true);
			SetControlTextF(IDC_PHASE, L"%u", mConfig.mOffset);
		} else {
			CheckButton(IDC_PHASE_ADAPTIVE, true);
		}
	}
}

class VDVideoFilterIVTC : public VDXVideoFilter {
public:
	uint32 GetParams();
	void Start();
	void Run();
	bool OnInvalidateCaches();
	bool Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher);
	bool Configure(VDXHWND);
	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	void ScriptConfig(IVDXScriptInterpreter *interp, const VDXScriptValue *argv, int argc);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	VDVideoFilterIVTCConfig mConfig;

	struct CacheEntry {
		sint64	mBaseFrame1;
		sint64	mBaseFrame2;
		IVTCScore	mScore;
	};

	enum { kCacheSize = 64 };
	CacheEntry mCache[kCacheSize];
};

VDXVF_BEGIN_SCRIPT_METHODS(VDVideoFilterIVTC)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVideoFilterIVTC, ScriptConfig, "iii")
VDXVF_END_SCRIPT_METHODS()

uint32 VDVideoFilterIVTC::GetParams() {
	const VDXPixmapLayout& srcLayout = *fa->src.mpPixmapLayout;

	switch(srcLayout.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV:
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}

	if (mConfig.mbReduceRate) {
		VDFraction fr(fa->dst.mFrameRateHi, fa->dst.mFrameRateLo);

		fr *= VDFraction(4, 5);

		fa->dst.mFrameRateHi = fr.getHi();
		fa->dst.mFrameRateLo = fr.getLo();
		fa->dst.mFrameCount = ((fa->dst.mFrameCount + 4)/5) * 4;
	}

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_ALIGN_SCANLINES | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void VDVideoFilterIVTC::Start() {
	for(int i=0; i<kCacheSize; ++i) {
		mCache[i].mBaseFrame1 = -1;
		mCache[i].mBaseFrame2 = -1;
	}
}

void VDVideoFilterIVTC::Run() {
	const VDXPixmap *pxsrc[11]={
			fa->mpSourceFrames[0]->mpPixmap,
			fa->mpSourceFrames[1]->mpPixmap,
			fa->mpSourceFrames[2]->mpPixmap,
			fa->mpSourceFrames[3]->mpPixmap,
			fa->mpSourceFrames[4]->mpPixmap,
			fa->mpSourceFrames[5]->mpPixmap,
			fa->mpSourceFrames[6]->mpPixmap,
			fa->mpSourceFrames[7]->mpPixmap,
			fa->mpSourceFrames[8]->mpPixmap,
			fa->mpSourceFrames[9]->mpPixmap,
			fa->mpSourceFrames[10]->mpPixmap
	};

	const VDXPixmap& pxdst = *fa->mpOutputFrames[0]->mpPixmap;

	IVTCScore scores[20];
	for(int i=0; i<10; ++i) {
		const VDXPixmap& src1 = *pxsrc[i];
		const VDXPixmap& src2 = *pxsrc[i+1];
		sint64 frame1 = fa->mpSourceFrames[i]->mFrameNumber;
		sint64 frame2 = fa->mpSourceFrames[i+1]->mFrameNumber;
		int hkey = (int)frame1 & (kCacheSize - 1);

		CacheEntry& cent = mCache[hkey];
		if (cent.mBaseFrame1 != frame1 || cent.mBaseFrame2 != frame2) {
			cent.mScore = ComputeScanImprovement(src1, src2);
			cent.mBaseFrame1 = frame1;
			cent.mBaseFrame2 = frame2;
		}

		scores[i] = scores[i+10] = cent.mScore;
//		VDDEBUG("Scores[%d]: %10lld | %-10lld\n", i, scores[i][0], scores[i][1]);
	}

	// The raw scores we have are the amount of improvement we get at that frame from
	// shifting the opposite field back one frame.
	//
	// Polarity == false means TFF field order.

	sint64 pscores[5][2];
	for(int i=0; i<5; ++i) {
		pscores[i][0]	= scores[i+1].mVarShift[0] + scores[i+2].mVarShift[0] - scores[i+1].mVar[0] - scores[i+2].mVar[0]
						+ scores[i+6].mVarShift[0] + scores[i+7].mVarShift[0] - scores[i+6].mVar[0] - scores[i+7].mVar[0];
		pscores[i][1]	= scores[i+1].mVarShift[1] + scores[i+2].mVarShift[1] - scores[i+1].mVar[1] - scores[i+2].mVar[1]
						+ scores[i+6].mVarShift[1] + scores[i+7].mVarShift[1] - scores[i+6].mVar[1] - scores[i+7].mVar[1];

		VDDEBUG("Pscores[%d]: %10lld | %-10lld\n", i, pscores[i][0], pscores[i][1]);
	}

	int bestPhase = -1;
	bool bestPolarity = false;
	sint64 bestScore = 0x7ffffffffffffffll;

	static const uint8 kPolarityMasks[3]={ 0x03, 0x01, 0x02 };
	const uint8 polMask = kPolarityMasks[mConfig.mFieldMode];

	int minOffset = 0;
	int maxOffset = 5;

	if (mConfig.mOffset >= 0) {
		int offset = mConfig.mOffset - (int)(fa->mpSourceFrames[5]->mFrameNumber % 5) - 1;

		if (offset < 0)
			offset += 5;

		minOffset = offset;
		maxOffset = minOffset + 1;
	}

	for(int i=minOffset; i<maxOffset; ++i) {
		for(int pol=0; pol<2; ++pol) {
			if (!(polMask & (1 << pol)))
				continue;

			sint64 score = pscores[i][pol];

			if (score < bestScore) {
				bestScore = score;
				bestPhase = i;
				bestPolarity = pol>0;
			}
		}
	}

	// 3/2/3/2 interleave pattern:
	//	A A B C D	TFF		A B C C D
	//	A B C C D			A B C C D
	//	0 4 3 2 1		->	0 4 3 2 1
	//	A B C C D	BFF		A B C C D
	//	A A B C D			A B C C D

	VDDEBUG("bestPhase: %d/%d\n", bestPhase, bestPolarity);

	if (mConfig.mbReduceRate) {
		// Compute where the second duplicate C frame occurs in a repeated 5-frame, phase-0
		// pattern.
		int dupeOffset = (int)((fa->src.mFrameNumber + bestPhase + 3) % 5);

		// If we're past that point, then read one frame ahead.
		int localOffset = (int)fa->dst.mFrameNumber & 3;
		if (localOffset >= dupeOffset) {
			CopyPixmap(SlicePixmap(pxdst, bestPolarity), SlicePixmap(*pxsrc[6], bestPolarity));
			CopyPixmap(SlicePixmap(pxdst, !bestPolarity), SlicePixmap(*pxsrc[bestPhase == 0 || bestPhase == 4 ? 7 : 6], !bestPolarity));
			return;
		}
	}

	CopyPixmap(SlicePixmap(pxdst, bestPolarity), SlicePixmap(*pxsrc[5], bestPolarity));
	CopyPixmap(SlicePixmap(pxdst, !bestPolarity), SlicePixmap(*pxsrc[bestPhase == 4 || bestPhase == 3 ? 6 : 5], !bestPolarity));
}

bool VDVideoFilterIVTC::OnInvalidateCaches() {
	for(int i=0; i<kCacheSize; ++i) {
		mCache[i].mBaseFrame1 = -1;
		mCache[i].mBaseFrame2 = -1;
	}

	return true;
}

bool VDVideoFilterIVTC::Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher) {
	if (mConfig.mbReduceRate)
		frame = (frame >> 2) * 5 + (frame & 3);

	prefetcher->PrefetchFrame(0, frame-5, 0);
	prefetcher->PrefetchFrame(0, frame-4, 0);
	prefetcher->PrefetchFrame(0, frame-3, 0);
	prefetcher->PrefetchFrame(0, frame-2, 0);
	prefetcher->PrefetchFrame(0, frame-1, 0);
	prefetcher->PrefetchFrame(0, frame  , 0);
	prefetcher->PrefetchFrame(0, frame+1, 0);
	prefetcher->PrefetchFrame(0, frame+2, 0);
	prefetcher->PrefetchFrame(0, frame+3, 0);
	prefetcher->PrefetchFrame(0, frame+4, 0);
	prefetcher->PrefetchFrame(0, frame+5, 0);
	return true;
}

bool VDVideoFilterIVTC::Configure(VDXHWND parent) {
	VDVideoFilterIVTCConfig old(mConfig);
	VDVideoFilterIVTCDialog dlg(mConfig);

	if (dlg.ShowDialog((VDGUIHandle)parent))
		return true;

	mConfig = old;
	return false;
}

void VDVideoFilterIVTC::GetSettingString(char *buf, int maxlen) {
	static const char *const kModes[]={
		"auto",
		"tff",
		"bff"
	};

	SafePrintf(buf, maxlen, " (%s, %s, %s)", mConfig.mbReduceRate ? "reduce" : "decomb", kModes[mConfig.mFieldMode], mConfig.mOffset < 0 ? "auto" : "manual");
}

void VDVideoFilterIVTC::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d,%d,%d)", mConfig.mbReduceRate, mConfig.mFieldMode, mConfig.mOffset);
}

void VDVideoFilterIVTC::ScriptConfig(IVDXScriptInterpreter *interp, const VDXScriptValue *argv, int argc) {
	mConfig.mbReduceRate = argv[0].asInt() != 0;
	
	int i = argv[1].asInt();
	if (i < 0 || i >= VDVideoFilterIVTCConfig::kFieldModeCount)
		i = VDVideoFilterIVTCConfig::kFieldMode_Autoselect;

	mConfig.mFieldMode = (VDVideoFilterIVTCConfig::FieldMode)i;

	mConfig.mOffset = argv[2].asInt();
	if (mConfig.mOffset < -1 || mConfig.mOffset > 4)
		interp->ScriptError(VDXScriptError::FCALL_OUT_OF_RANGE);
}

///////////////////////////////////////////////////////////////////////////

extern const VDXFilterDefinition filterDef_ivtc =
	VDXVideoFilterDefinition<VDVideoFilterIVTC>(
		NULL,
		"IVTC", 
		"Removes 3:2 pulldown (telecine) from video.");

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)
