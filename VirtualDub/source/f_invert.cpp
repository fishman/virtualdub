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
#include "VBitmap.h"

extern HINSTANCE g_hInst;

///////////////////////////////////

namespace {
#ifdef _M_IX86
	void __declspec(naked) VDInvertRect32(Pixel32 *data, long w, long h, ptrdiff_t pitch) {
		__asm {
			push	ebp
			push	edi
			push	esi
			push	ebx

			mov		edi,[esp+4+16]
			mov		edx,[esp+8+16]
			mov		ecx,[esp+12+16]
			mov		esi,[esp+16+16]
			mov		eax,edx
			xor		edx,-1
			shl		eax,2
			inc		edx
			add		edi,eax
			test	edx,1
			jz		yloop
			sub		edi,4
	yloop:
			mov		ebp,edx
			inc		ebp
			sar		ebp,1
			jz		zero
	xloop:
			mov		eax,[edi+ebp*8  ]
			mov		ebx,[edi+ebp*8+4]
			xor		eax,-1
			xor		ebx,-1
			mov		[edi+ebp*8  ],eax
			mov		[edi+ebp*8+4],ebx
			inc		ebp
			jne		xloop
	zero:
			test	edx,1
			jz		notodd
			not		dword ptr [edi]
	notodd:
			add		edi,esi
			dec		ecx
			jne		yloop

			pop		ebx
			pop		esi
			pop		edi
			pop		ebp
			ret
		};
	}
#else
	void VDInvertRect32(Pixel32 *data, long w, long h, ptrdiff_t pitch) {
		pitch -= 4*w;

		do {
			long wt = w;
			do {
				*data = ~*data;
				++data;
			} while(--wt);

			data = (Pixel32 *)((char *)data + pitch);
		} while(--h);
	}
#endif
}

///////////////////////////////////

int invert_run(const FilterActivation *fa, const FilterFunctions *ff) {	
	VDInvertRect32(
			fa->src.data,
			fa->src.w,
			fa->src.h,
			fa->src.pitch
			);

	return 0;
}

long invert_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.offset = fa->src.offset;
	return 0;
}

FilterDefinition filterDef_invert={
	0,0,NULL,
	"invert",
	"Inverts the colors in the image.\n\n[Assembly optimized]",
	NULL,NULL,
	0,
	NULL,NULL,
	invert_run,
	invert_param,
	NULL,
	NULL,
};
