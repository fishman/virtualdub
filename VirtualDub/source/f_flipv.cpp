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
#include "VBitmap.h"

static int flipv_run(const FilterActivation *fa, const FilterFunctions *ff) {
	return 0;
}

static long flipv_param(FilterActivation *fa, const FilterFunctions *ff) {
	const VDXPixmapLayout& pxsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxdst = *fa->dst.mpPixmapLayout;

	// flip the primary plane
	fa->dst.depth = 0;
	pxdst = pxsrc;

	pxdst.data += pxdst.pitch*(pxdst.h - 1);
	pxdst.pitch = -pxdst.pitch;

	int subh;
	switch(pxsrc.format) {
		case nsVDXPixmap::kPixFormat_XRGB1555:
		case nsVDXPixmap::kPixFormat_RGB565:
		case nsVDXPixmap::kPixFormat_RGB888:
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_Y8:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV:
			break;
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar:
			subh = pxdst.h;
			pxdst.data2 += pxdst.pitch2*(subh - 1);
			pxdst.pitch2 = -pxdst.pitch2;
			pxdst.data3 += pxdst.pitch3*(subh - 1);
			pxdst.pitch3 = -pxdst.pitch3;
			break;
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
			subh = (pxdst.h + 1) >> 1;
			pxdst.data2 += pxdst.pitch2*(subh - 1);
			pxdst.pitch2 = -pxdst.pitch2;
			pxdst.data3 += pxdst.pitch3*(subh - 1);
			pxdst.pitch3 = -pxdst.pitch3;
			break;
		case nsVDXPixmap::kPixFormat_YUV410_Planar:
			subh = (pxdst.h + 3) >> 2;
			pxdst.data2 += pxdst.pitch2*(subh - 1);
			pxdst.pitch2 = -pxdst.pitch2;
			pxdst.data3 += pxdst.pitch3*(subh - 1);
			pxdst.pitch3 = -pxdst.pitch3;
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}

	return FILTERPARAM_SUPPORTS_ALTFORMATS;
}

FilterDefinition filterDef_flipv={
	0,0,NULL,
	"flip vertically",
	"Vertically flips an image.\n\n[YCbCr processing]",
	NULL,NULL,
	0,
	NULL,NULL,
	flipv_run,
	flipv_param
};

////////////////////////////////////////////////////////////

static int fliph_run(const FilterActivation *fa, const FilterFunctions *ff) {
	Pixel *src = fa->src.data, *srct;
	Pixel *dst = fa->dst.data-1;
	unsigned long h, w;

	h = fa->dst.h;
	do {
		srct = src;
		w = fa->dst.w;
		do {
			dst[w] = *srct++;
		} while(--w);
		src = (Pixel *)((char *)src + fa->src.pitch);
		dst = (Pixel *)((char *)dst + fa->dst.pitch);
	} while(--h);

	return 0;
}

FilterDefinition filterDef_fliph={
	0,0,NULL,
	"flip horizontally",
	"Horizontally flips an image.\n\n",
	NULL,NULL,
	0,
	NULL,NULL,
	fliph_run
};