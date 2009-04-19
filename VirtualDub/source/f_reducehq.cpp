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
#include "filter.h"

#define READSRC(byteoff, lineoff) (*(unsigned long *)((char *)src + pitch*(lineoff) + (byteoff*4)))

extern "C" void asm_reduce2hq_run(
		void *dst,
		void *src,
		unsigned long width,
		unsigned long height,
		unsigned long srcstride,
		unsigned long dststride);

static int reduce2hq_run(const FilterActivation *fa, const FilterFunctions *ff) {
	unsigned long w,h;
	unsigned long *src = (unsigned long *)fa->src.data, *dst = (unsigned long *)fa->dst.data;
	unsigned long pitch = fa->src.pitch;
	unsigned long rb, g;

	if (!(fa->src.h & 1)) {
		src -= pitch>>2;

		if (!(fa->src.w & 1)) {
			rb	=   ((READSRC( 0,1)&0xff00ff) + (READSRC( 0,2)&0xff00ff)
					+(READSRC( 1,1)&0xff00ff) + (READSRC( 1,2)&0xff00ff));
			g	=   ((READSRC( 0,1)&0x00ff00) + (READSRC( 0,2)&0x00ff00)
					+(READSRC( 1,1)&0x00ff00) + (READSRC( 1,2)&0x00ff00));
			*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
			++src;
		}
		++src;

		w = (fa->src.w-1)/2;
		do {
			rb	=   ((READSRC(-1,1)&0xff00ff) + (READSRC(-1,2)&0xff00ff)
					+(READSRC( 0,1)&0xff00ff) + (READSRC( 0,2)&0xff00ff)
					+(READSRC( 1,1)&0xff00ff) + (READSRC( 1,2)&0xff00ff));
			g	=   ((READSRC(-1,1)&0x00ff00) + (READSRC(-1,2)&0x00ff00)
					+(READSRC( 0,1)&0x00ff00) + (READSRC( 0,2)&0x00ff00)
					+(READSRC( 1,1)&0x00ff00) + (READSRC( 1,2)&0x00ff00));
			*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
			src += 2;
		} while(--w);

		src += (fa->src.modulo + fa->src.pitch)>>2;
		dst += fa->dst.modulo>>2;
	}

	asm_reduce2hq_run(
		fa->src.w&1 ? dst : dst+1,
		fa->src.w&1 ? src+1 : src+2,
		(fa->src.w-1)/2,
		(fa->src.h-1)/2,
		fa->src.pitch,
		fa->dst.pitch);

	if (!(fa->src.w & 1)) {
		h = (fa->src.h-1)/2;
		do {
			rb	=   (((READSRC( 0,0)&0xff00ff) + (READSRC( 0,1)&0xff00ff) + (READSRC( 0,2)&0xff00ff))*2
					+(READSRC( 1,0)&0xff00ff) + (READSRC( 1,1)&0xff00ff) + (READSRC( 1,2)&0xff00ff));
			g	=   (((READSRC( 0,0)&0x00ff00) + (READSRC( 0,1)&0x00ff00) + (READSRC( 0,2)&0x00ff00))*2
					+(READSRC( 1,0)&0x00ff00) + (READSRC( 1,1)&0x00ff00) + (READSRC( 1,2)&0x00ff00));
			*dst = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);

			src += fa->src.pitch>>1;
			dst += fa->dst.pitch>>2;
		} while(--h);
	}

	return 0;
}

static long reduce2hq_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.w /= 2;
	fa->dst.h /= 2;
	fa->dst.pitch = fa->dst.w*4;

	return FILTERPARAM_SWAP_BUFFERS;
}

FilterDefinition filterDef_reduce2hq={
	0,0,NULL,
	"2:1 reduction (high quality)",
	"Reduces the size of each frame by 2:1 in both directions. A 3x3 overlapping matrix is used.\n\n[Assembly optimized] [MMX optimized]",
	NULL,NULL,
	0,
	NULL,NULL,
	reduce2hq_run,
	reduce2hq_param,
};