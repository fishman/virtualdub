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
#include "filters.h"

extern const VDXFilterDefinition
#ifdef _DEBUG
	filterDef_debugerror,
	filterDef_showinfo,
#endif
	filterDef_fieldbob2,
	filterDef_warpresize,
	filterDef_warpsharp,
	filterDef_blur,
	filterDef_blurhi,
	filterDef_brightcont,
	filterDef_interlace,
	filterDef_ivtc,
	filterDef_fieldswap,
	filterDef_interpolate;

#ifdef _M_IX86
extern const VDXFilterDefinition
	filterDef_box,
	filterDef_tv,
	filterDef_timesmooth;
#endif

extern FilterDefinition
	filterDef_chromasmoother,
	filterDef_fieldbob,
	filterDef_fill,
	filterDef_invert,
	filterDef_null,
	filterDef_tsoften,
	filterDef_resize,
	filterDef_flipv,
	filterDef_fliph,
	filterDef_deinterlace,
	filterDef_rotate,
	filterDef_hsv,
	filterDef_convertformat,
	filterDef_threshold,
	filterDef_grayscale,
	filterDef_levels,
	filterDef_logo,
	filterDef_perspective

#ifdef _M_IX86
	,
	filterDef_reduceby2,
	filterDef_convolute,
	filterDef_sharpen,
	filterDef_emboss,
	filterDef_reduce2hq,
	filterDef_smoother,
	filterDef_rotate2
#endif
	;

static const FilterDefinition *const builtin_filters[]={
	&filterDef_chromasmoother,
	&filterDef_fieldbob,
	&filterDef_fieldbob2,
	&filterDef_fieldswap,
	&filterDef_fill,
	&filterDef_invert,
	&filterDef_null,
	&filterDef_tsoften,
	&filterDef_resize,
	&filterDef_flipv,
	&filterDef_fliph,
	&filterDef_deinterlace,
	&filterDef_rotate,
	&filterDef_hsv,
	&filterDef_warpresize,
	&filterDef_convertformat,
	&filterDef_threshold,
	&filterDef_grayscale,
	&filterDef_levels,
	&filterDef_logo,
	&filterDef_brightcont,
	&filterDef_warpsharp,
	&filterDef_perspective,
	&filterDef_blur,
	&filterDef_blurhi,
	&filterDef_interlace,
	&filterDef_ivtc,
	&filterDef_interpolate,

#ifdef _DEBUG
	&filterDef_debugerror,
	&filterDef_showinfo,
#endif

#ifdef _M_IX86
	&filterDef_reduceby2,
	&filterDef_convolute,
	&filterDef_sharpen,
	&filterDef_emboss,
	&filterDef_reduce2hq,
	&filterDef_tv,
	&filterDef_smoother,
	&filterDef_rotate2,
	&filterDef_box,
	&filterDef_timesmooth,
#endif
	NULL
};

void InitBuiltinFilters() {
	const FilterDefinition *cur, *const *cpp;

	cpp = builtin_filters;
	while(cur = *cpp++)
		FilterAddBuiltin(cur);
}
