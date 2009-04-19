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

typedef struct MyFilterData {
	bool is_first_frame;
} MyFilterData;

///////////////////////////////////

static int tsoften_run(const FilterActivation *fa, const FilterFunctions *ff) {	
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	Pixel *src = (Pixel *)fa->last->data;
	Pixel *dst = (Pixel *)fa->dst.data;
	long w, h;

	if (mfd->is_first_frame) {
		mfd->is_first_frame = FALSE;

		return 0;
	}

	h = fa->dst.h;
	do {
		w = fa->dst.w;

		do {
			*dst = (((*dst&0xfefefe) + (*src&0xfefefe))>>1) + ((*src&0x010101) & (*dst&0x010101));
			++dst;
			++src;
		} while(--w);

		src = (Pixel *)((char *)src + fa->last->modulo);
		dst = (Pixel *)((char *)dst + fa->dst.modulo);
	} while(--h);

	return 0;
}

static long tsoften_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.offset	= fa->src.offset;
	fa->dst.modulo	= fa->src.modulo;
	fa->dst.pitch	= fa->src.pitch;
	return FILTERPARAM_NEEDS_LAST;
}

static int tsoften_start(FilterActivation *fa, const FilterFunctions *ff) {
	if (!fa->filter_data)
		if (!(fa->filter_data = (void *)new MyFilterData)) return 1;

	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	mfd->is_first_frame = TRUE;

	return 0;
}

static int tsoften_stop(FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	delete mfd;
	fa->filter_data = NULL;

	return 0;
}




FilterDefinition filterDef_tsoften={
	0,0,NULL,
	"motion blur",
	"Blurs adjacent frames together.\n\n",
	NULL,NULL,
	sizeof(MyFilterData),
	NULL,NULL,
	tsoften_run,
	tsoften_param,
	NULL,
	NULL,
	tsoften_start,
	tsoften_stop,
};