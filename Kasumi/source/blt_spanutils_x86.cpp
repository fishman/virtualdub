//	VirtualDub - Video processing and capture application
//	Graphics support library
//	Copyright (C) 1998-2007 Avery Lee
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

#include "blt_spanutils_x86.h"

#ifdef _MSC_VER
	#pragma warning(disable: 4799)		// warning C4799: function 'nsVDPixmapSpanUtils::vdasm_horiz_expand2x_coaligned_ISSE' has no EMMS instruction
#endif

namespace nsVDPixmapSpanUtils {

	void __declspec(naked) __cdecl vdasm_horiz_expand2x_coaligned_ISSE(void *dst, const void *src, uint32 count) {
		__asm {
			mov		ecx, [esp+8]
			mov		edx, [esp+4]
			mov		eax, [esp+12]
xloop:
			movq	mm0, [ecx]
			movq	mm1, mm0
			pavgb	mm0, [ecx+1]
			movq	mm2, mm1
			punpcklbw	mm1, mm0
			punpckhbw	mm2, mm0

			movq	[edx], mm1
			movq	[edx+8], mm2
			add		edx, 16
			add		ecx, 8

			sub		eax, 16
			jne		xloop
			ret
		}
	}

	void horiz_expand2x_coaligned_ISSE(uint8 *dst, const uint8 *src, sint32 w) {
		if (w >= 17) {
			uint32 fastcount = (w - 1) & ~15;

			vdasm_horiz_expand2x_coaligned_ISSE(dst, src, fastcount);
			dst += fastcount;
			src += fastcount >> 1;
			w -= fastcount;
		}

		w = -w;
		if ((w+=2) < 0) {
			do {
				dst[0] = src[0];
				dst[1] = (uint8)((src[0] + src[1] + 1)>>1);
				dst += 2;
				++src;
			} while((w+=2)<0);
		}

		w -= 2;
		while(w < 0) {
			++w;
			*dst++ = src[0];
		}
	}

	void __declspec(naked) __cdecl vdasm_vert_expand2x_centered_ISSE(void *dst, const void *src1, const void *src3, uint32 count) {
		__asm {
			push	ebx
			mov		ebx, [esp+12+4]
			mov		ecx, [esp+8+4]
			mov		edx, [esp+4+4]
			mov		eax, [esp+16+4]

			add		ebx, eax
			add		ecx, eax
			add		edx, eax
			neg		eax

			pcmpeqb	mm7, mm7
xloop:
			movq	mm0, [ebx+eax]
			movq	mm1, [ecx+eax]
			movq	mm2, mm0

			movq	mm3, [ebx+eax+8]
			pxor	mm0, mm7
			pxor	mm1, mm7

			movq	mm4, [ecx+eax+8]
			movq	mm5, mm3
			pxor	mm3, mm7

			pxor	mm4, mm7
			pavgb	mm0, mm1
			pavgb	mm3, mm4

			pxor	mm0, mm7
			pxor	mm3, mm7
			pavgb	mm0, mm2

			movq	[edx+eax], mm0
			pavgb	mm3, mm5

			movq	[edx+eax+8], mm3
			add		eax, 16
			jne		xloop

			pop		ebx
			ret
		}
	}

	void vert_expand2x_centered_ISSE(uint8 *dst, const uint8 *const *srcs, sint32 w, uint8 phase) {
		const uint8 *src3 = srcs[0];
		const uint8 *src1 = srcs[1];

		if (phase >= 128)
			std::swap(src1, src3);

		uint32 fastcount = w & ~15;

		if (fastcount) {
			vdasm_vert_expand2x_centered_ISSE(dst, src1, src3, fastcount);
			dst += fastcount;
			src1 += fastcount;
			src3 += fastcount;
			w -= fastcount;
		}

		if (w) {
			do {
				*dst++ = (uint8)((*src1++ + 3**src3++ + 2) >> 2);
			} while(--w);
		}
	}
}
