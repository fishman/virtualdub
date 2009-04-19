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

#include <vd2/system/memory.h>

#include "resource.h"
#include "filter.h"

extern HINSTANCE g_hInst;

extern "C" void asm_grayscale_run(
		void *dst,
		unsigned long width,
		unsigned long height,
		unsigned long stride
		);

///////////////////////////////////

#ifndef _M_IX86
static void grayscale_run_rgb32(const VDXPixmap& pxdst) {
	uint8 *row = (uint8 *)pxdst.data;
	ptrdiff_t pitch = pxdst.pitch;
	uint32 h = pxdst.h;
	uint32 w = pxdst.w;

	for(uint32 y=0; y<h; ++y) {
		uint8 *p = row;

		for(uint32 x=0; x<w; ++x) {
			uint8 y = (((int)p[0] * 19 + (int)p[1]*183 + (int)p[2] * 54) >> 8);

			p[0] = y;
			p[1] = y;
			p[2] = y;
			p += 4;
		}

		row += pitch;
	}
}
#endif

static void grayscale_run_yuv(const VDXPixmap& pxdst, int xbits, int ybits) {
	int w = -(-pxdst.w >> xbits);
	int h = -(-pxdst.h >> ybits);

	VDMemset8Rect(pxdst.data2, pxdst.pitch2, 0x80, w, h);
	VDMemset8Rect(pxdst.data3, pxdst.pitch3, 0x80, w, h);
}

static int grayscale_run(const FilterActivation *fa, const FilterFunctions *ff) {
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;

	switch(pxdst.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
#ifdef _M_IX86
			asm_grayscale_run(
					pxdst.data,
					pxdst.w,
					pxdst.h,
					pxdst.pitch
					);
#else
			grayscale_run_rgb32(pxdst);
#endif
			break;

		case nsVDXPixmap::kPixFormat_YUV444_Planar:
			grayscale_run_yuv(pxdst, 0, 0);
			break;
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
			grayscale_run_yuv(pxdst, 1, 0);
			break;
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
			grayscale_run_yuv(pxdst, 1, 1);
			break;
		case nsVDXPixmap::kPixFormat_YUV411_Planar:
			grayscale_run_yuv(pxdst, 2, 0);
			break;
		case nsVDXPixmap::kPixFormat_YUV410_Planar:
			grayscale_run_yuv(pxdst, 2, 2);
			break;
	}

	return 0;
}

static long grayscale_param(FilterActivation *fa, const FilterFunctions *ff) {
	const VDXPixmapLayout& pxsrc = *fa->src.mpPixmapLayout;
	switch(pxsrc.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV410_Planar:
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}

	VDXPixmapLayout& pxdst = *fa->dst.mpPixmapLayout;

	fa->dst.depth = 0;
	pxdst = pxsrc;
	return FILTERPARAM_SUPPORTS_ALTFORMATS;
}

FilterDefinition filterDef_grayscale={
	0,0,NULL,
	"grayscale",
	"Rips the color out of your image.\n\n[Assembly optimized] [YCbCr processing]",
	NULL,NULL,
	0,
	NULL,NULL,
	grayscale_run,
	grayscale_param,
	NULL,
	NULL,
};