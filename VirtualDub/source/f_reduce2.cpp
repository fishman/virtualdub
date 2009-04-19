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

extern "C" void asm_reduceby2_32(
		void *dst,
		void *src,
		unsigned long width,
		unsigned long height,
		unsigned long srcstride,
		unsigned long dststride);

static int reduce_run(const FilterActivation *fa, const FilterFunctions *ff) {
	asm_reduceby2_32(fa->dst.data, fa->src.data, fa->dst.w, fa->dst.h, fa->src.pitch, fa->dst.pitch);
	
	return 0;
}

static long reduce_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.w /= 2;
	fa->dst.h /= 2;
	fa->dst.pitch = fa->dst.w*4;

	return FILTERPARAM_SWAP_BUFFERS;
}

FilterDefinition filterDef_reduceby2={
	0,0,NULL,
	"2:1 reduction",
	"Reduces the size of an image by 2:1 in both directions. A 2x2 non-overlapping matrix is used.\n\n[Assembly optimized] [MMX optimized]",
	NULL,NULL,
	0,
	NULL,NULL,
	reduce_run,
	reduce_param,
};