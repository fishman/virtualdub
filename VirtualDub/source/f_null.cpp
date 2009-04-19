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

int null_run(const FilterActivation *fa, const FilterFunctions *ff) {
	return 0;
}

long null_param(FilterActivation *fa, const FilterFunctions *ff) {
	switch(fa->src.mpPixmapLayout->format) {
	case nsVDXPixmap::kPixFormat_XRGB1555:
	case nsVDXPixmap::kPixFormat_RGB565:
	case nsVDXPixmap::kPixFormat_RGB888:
	case nsVDXPixmap::kPixFormat_XRGB8888:
	case nsVDXPixmap::kPixFormat_Y8:
	case nsVDXPixmap::kPixFormat_YUV422_UYVY:
	case nsVDXPixmap::kPixFormat_YUV422_YUYV:
	case nsVDXPixmap::kPixFormat_YUV444_Planar:
	case nsVDXPixmap::kPixFormat_YUV422_Planar:
	case nsVDXPixmap::kPixFormat_YUV420_Planar:
	case nsVDXPixmap::kPixFormat_YUV411_Planar:
	case nsVDXPixmap::kPixFormat_YUV410_Planar:
		break;

	default:
		return FILTERPARAM_NOT_SUPPORTED;
	}

	fa->dst.offset	= fa->src.offset;
	return FILTERPARAM_SUPPORTS_ALTFORMATS;
}

FilterDefinition filterDef_null={
	0,0,NULL,
	"null transform",
	"Does nothing. Typically used as a placeholder for cropping.",
	NULL,NULL,
	0,
	NULL,NULL,
	null_run,
	null_param,
	NULL,
	NULL,
};