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

#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixel.h>

uint32 VDPixmapSample(const VDPixmap& px, sint32 x, sint32 y) {
	if (x >= px.w)
		x = px.w - 1;
	if (y >= px.h)
		y = px.h - 1;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	switch(px.format) {
	case nsVDPixmap::kPixFormat_Pal1:
		{
			uint8 idx = ((const uint8 *)px.data + px.pitch*y)[x >> 3];

			return px.palette[(idx >> (7 - (x & 7))) & 1];
		}

	case nsVDPixmap::kPixFormat_Pal2:
		{
			uint8 idx = ((const uint8 *)px.data + px.pitch*y)[x >> 2];

			return px.palette[(idx >> (6 - (x & 3)*2)) & 3];
		}

	case nsVDPixmap::kPixFormat_Pal4:
		{
			uint8 idx = ((const uint8 *)px.data + px.pitch*y)[x >> 1];

			if (!(x & 1))
				idx >>= 4;

			return px.palette[idx & 15];
		}

	case nsVDPixmap::kPixFormat_Pal8:
		{
			uint8 idx = ((const uint8 *)px.data + px.pitch*y)[x];

			return px.palette[idx];
		}

	case nsVDPixmap::kPixFormat_XRGB1555:
		{
			uint16 c = ((const uint16 *)((const uint8 *)px.data + px.pitch*y))[x];
			uint32 r = c & 0x7c00;
			uint32 g = c & 0x03e0;
			uint32 b = c & 0x001f;
			uint32 rgb = (r << 9) + (g << 6) + (b << 3);

			return rgb + ((rgb >> 5) & 0x070707);
		}
		break;

	case nsVDPixmap::kPixFormat_RGB565:
		{
			uint16 c = ((const uint16 *)((const uint8 *)px.data + px.pitch*y))[x];
			uint32 r = c & 0xf800;
			uint32 g = c & 0x07e0;
			uint32 b = c & 0x001f;
			uint32 rb = (r << 8) + (b << 3);

			return rb + ((rb >> 5) & 0x070007) + (g << 5) + ((g >> 1) & 0x0300);
		}
		break;

	case nsVDPixmap::kPixFormat_RGB888:
		{
			const uint8 *src = (const uint8 *)px.data + px.pitch*y + 3*x;
			uint32 b = src[0];
			uint32 g = src[1];
			uint32 r = src[2];

			return (r << 16) + (g << 8) + b;
		}
		break;

	case nsVDPixmap::kPixFormat_XRGB8888:
		return ((const uint32 *)((const uint8 *)px.data + px.pitch*y))[x];

	case nsVDPixmap::kPixFormat_Y8:
		{
			uint8 luma = ((const uint8 *)px.data + px.pitch*y)[x];

			return ((luma - 16)*255/219) * 0x010101;
		}
		break;
	case nsVDPixmap::kPixFormat_YUV422_UYVY:
		break;
	case nsVDPixmap::kPixFormat_YUV422_YUYV:
		break;
	case nsVDPixmap::kPixFormat_YUV444_XVYU:
		break;

	case nsVDPixmap::kPixFormat_YUV444_Planar:
		return VDConvertYCbCrToRGB(VDPixmapSample8(px.data, px.pitch, x, y), VDPixmapSample8(px.data2, px.pitch2, x, y), VDPixmapSample8(px.data3, px.pitch3, x, y));

	case nsVDPixmap::kPixFormat_YUV422_Planar:
		{
			sint32 u = (x << 7) + 128;
			sint32 v = (y << 8);
			uint32 w2 = px.w >> 1;
			uint32 h2 = px.h;

			return VDConvertYCbCrToRGB(
						VDPixmapSample8(px.data, px.pitch, x, y),
						VDPixmapInterpolateSample8(px.data2, px.pitch2, w2, h2, u, v),
						VDPixmapInterpolateSample8(px.data3, px.pitch3, w2, h2, u, v));
		}

	case nsVDPixmap::kPixFormat_YUV420_Planar:
		{
			sint32 u = (x << 7) + 128;
			sint32 v = (y << 7);
			uint32 w2 = px.w >> 1;
			uint32 h2 = px.h >> 1;

			return VDConvertYCbCrToRGB(
						VDPixmapSample8(px.data, px.pitch, x, y),
						VDPixmapInterpolateSample8(px.data2, px.pitch2, w2, h2, u, v),
						VDPixmapInterpolateSample8(px.data3, px.pitch3, w2, h2, u, v));
		}

	case nsVDPixmap::kPixFormat_YUV411_Planar:
		{
			sint32 u = (x << 6) + 128;
			sint32 v = (y << 8);
			uint32 w2 = px.w >> 2;
			uint32 h2 = px.h;

			return VDConvertYCbCrToRGB(
						VDPixmapSample8(px.data, px.pitch, x, y),
						VDPixmapInterpolateSample8(px.data2, px.pitch2, w2, h2, u, v),
						VDPixmapInterpolateSample8(px.data3, px.pitch3, w2, h2, u, v));
		}

	case nsVDPixmap::kPixFormat_YUV410_Planar:
		{
			sint32 u = (x << 6) + 128;
			sint32 v = (y << 6);
			uint32 w2 = px.w >> 2;
			uint32 h2 = px.h >> 2;

			return VDConvertYCbCrToRGB(
						VDPixmapSample8(px.data, px.pitch, x, y),
						VDPixmapInterpolateSample8(px.data2, px.pitch2, w2, h2, u, v),
						VDPixmapInterpolateSample8(px.data3, px.pitch3, w2, h2, u, v));
		}

	default:
		VDASSERT(false);
		break;
	}

	return 0;
}

uint8 VDPixmapInterpolateSample8(const void *data, ptrdiff_t pitch, uint32 w, uint32 h, sint32 x_256, sint32 y_256) {
	// bias coordinates to half-integer
	x_256 -= 128;
	y_256 -= 128;

	// clamp coordinates
	x_256 &= ~(x_256 >> 31);
	y_256 &= ~(y_256 >> 31);

	uint32 w_256 = (w - 1) << 8;
	uint32 h_256 = (h - 1) << 8;
	x_256 ^= (x_256 ^ w_256) & ((x_256 - w_256) >> 31);
	y_256 ^= (y_256 ^ h_256) & ((y_256 - h_256) >> 31);

	const uint8 *row0 = (const uint8 *)data + pitch * (y_256 >> 8);
	const uint8 *row1 = row0;

	if ((uint32)y_256 < h_256)
		row1 += pitch;

	ptrdiff_t xstep = (uint32)x_256 < w_256 ? 1 : 0;
	sint32 xoffset = x_256 & 255;
	sint32 yoffset = y_256 & 255;
	sint32 p00 = row0[0];
	sint32 p10 = row0[xstep];
	sint32 p01 = row1[0];
	sint32 p11 = row1[xstep];
	sint32 p0 = (p00 << 8) + (p10 - p00)*xoffset;
	sint32 p1 = (p01 << 8) + (p11 - p01)*xoffset;
	sint32 p = ((p0 << 8) + (p1 - p0)*yoffset + 0x8000) >> 16;

	return (uint8)p;
}

uint32 VDConvertYCbCrToRGB(uint8 y0, uint8 cb0, uint8 cr0) {
	sint32  y =  y0 -  16;
	sint32 cb = cb0 - 128;
	sint32 cr = cr0 - 128;

	sint32 y2 = y * 76309 + 0x8000;
	sint32 r = y2 + cr * 104597;
	sint32 g = y2 + cr * -53279 + cb * -25674;
	sint32 b = y2 + cb * 132201;

	r &= ~(r >> 31);
	g &= ~(g >> 31);
	b &= ~(b >> 31);
	r += (0xffffff - r) & ((0xffffff - r) >> 31);
	g += (0xffffff - g) & ((0xffffff - g) >> 31);
	b += (0xffffff - b) & ((0xffffff - b) >> 31);

	return (r & 0xff0000) + ((g & 0xff0000) >> 8) + (b >> 16);
}

uint32 VDConvertRGBToYCbCr(uint32 c) {
	return VDConvertRGBToYCbCr((uint8)(c >> 16), (uint8)(c >> 8), (uint8)c);
}

uint32 VDConvertRGBToYCbCr(uint8 r8, uint8 g8, uint8 b8) {
	sint32 r  = r8;
	sint32 g  = g8;
	sint32 b  = b8;
	sint32 yt = 1052*r + 2065*g + 401*b;
	sint32 y  = (yt + 0x10800) >> 4;
	sint32 cr = (10507932*r - yt*2987 + 0x80800000U) >> 8;
	sint32 cb = ( 8312025*b - yt*2363 + 0x80800000U) >> 24;

	return (uint8)cb + (y & 0xff00) + (cr&0xff0000);
}
