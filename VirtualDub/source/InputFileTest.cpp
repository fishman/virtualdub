//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#include <vd2/system/error.h>
#include <vd2/Kasumi/region.h>
#include <vd2/Kasumi/text.h>
#include <vd2/Kasumi/triblt.h>
#include <vd2/Kasumi/pixmapops.h>

#include "VideoSource.h"
#include "InputFile.h"
#include "InputFileTestPhysics.h"
#include "gui.h"

extern const char g_szError[];


class VDVideoSourceTest : public VideoSource {
public:
	VDVideoSourceTest(int mode);
	~VDVideoSourceTest();

	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);
	bool _isKey(VDPosition samp)					{ return true; }
	VDPosition nearestKey(VDPosition lSample)			{ return lSample; }
	VDPosition prevKey(VDPosition lSample)				{ return lSample>0 ? lSample-1 : -1; }
	VDPosition nextKey(VDPosition lSample)				{ return lSample<mSampleLast ? lSample+1 : -1; }

	bool setTargetFormat(int depth);

	void invalidateFrameBuffer()			{ mCachedFrame = -1; }
	bool isFrameBufferValid()				{ return mCachedFrame >= 0; }
	bool isStreaming()						{ return false; }

	const void *streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition sample_num, VDPosition target_num);

	const void *getFrame(VDPosition frameNum);

	char getFrameTypeChar(VDPosition lFrameNum)	{ return 'K'; }
	eDropType getDropType(VDPosition lFrameNum)	{ return kIndependent; }
	bool isKeyframeOnly()					{ return true; }
	bool isType1()							{ return false; }
	bool isDecodable(VDPosition sample_num)		{ return true; }

private:
	void DrawRotatingCubeFrame(VDPixmap& dst, bool interlaced, bool oddField, int frameIdx, bool isyuv);
	void DrawPhysFrame(VDPixmap& dst, bool oddField, int frame);

	VDPosition	mCachedFrame;
	const int	mMode;

	VDPixmapBuffer	mRGB32Buffer;
	VDPixmapBuffer	m422Buffer;
	VDTestVidPhysVideo	mVideo;

	VDPixmapPathRasterizer	mTextRasterizer;
	VDPixmapRegion			mTextOutlineBrush;
	VDPixmapRegion			mTextRegion;
	VDPixmapRegion			mTextBorderRegion;
};

///////////////////////////////////////////////////////////////////////////

VDVideoSourceTest::VDVideoSourceTest(int mode)
	: mMode(mode)
{
	mSampleFirst = 0;
	mSampleLast = 1000;

	mpTargetFormatHeader.resize(sizeof(BITMAPINFOHEADER));
	BITMAPINFOHEADER *pFormat = (BITMAPINFOHEADER *)allocFormat(sizeof(BITMAPINFOHEADER));

	int w = 640;
	int h = 480;

	AllocFrameBuffer(w * h * 4);

	pFormat->biSize				= sizeof(BITMAPINFOHEADER);
	pFormat->biWidth			= w;
	pFormat->biHeight			= h;
	pFormat->biPlanes			= 1;
	pFormat->biCompression		= 0xFFFFFFFFUL;
	pFormat->biBitCount			= 0;
	pFormat->biXPelsPerMeter	= 0;
	pFormat->biYPelsPerMeter	= 0;
	pFormat->biClrUsed			= 0;
	pFormat->biClrImportant		= 0;

	invalidateFrameBuffer();

	// Fill out streamInfo

	streamInfo.fccType					= VDAVIStreamInfo::kTypeVideo;
	streamInfo.fccHandler				= NULL;
	streamInfo.dwFlags					= 0;
	streamInfo.dwCaps					= 0;
	streamInfo.wPriority				= 0;
	streamInfo.wLanguage				= 0;
	streamInfo.dwScale					= 1001;
	streamInfo.dwRate					= 30000;
	streamInfo.dwStart					= 0;
	streamInfo.dwLength					= VDClampToUint32(mSampleLast);
	streamInfo.dwInitialFrames			= 0;
	streamInfo.dwSuggestedBufferSize	= 0;
	streamInfo.dwQuality				= (DWORD)-1;
	streamInfo.dwSampleSize				= 0;
	streamInfo.rcFrameLeft				= 0;
	streamInfo.rcFrameTop				= 0;
	streamInfo.rcFrameRight				= (uint16)getImageFormat()->biWidth;
	streamInfo.rcFrameBottom			= (uint16)getImageFormat()->biHeight;

	if (mMode == 7 || mMode == 8) {
		VDTestVidPhysSimulator sim;

		for(int i=0; i<800; ++i) {
			sim.EncodeFrame(mVideo);
			sim.Step(1001.0f / 24000.0f);
		}

		mRGB32Buffer.init(w, h, nsVDPixmap::kPixFormat_XRGB8888);
	}

	VDPixmapCreateRoundRegion(mTextOutlineBrush, 16.0f);
}

VDVideoSourceTest::~VDVideoSourceTest() {
}

int VDVideoSourceTest::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *plBytesRead, uint32 *plSamplesRead) {
	if (plBytesRead)
		*plBytesRead = 0;

	if (plSamplesRead)
		*plSamplesRead = 0;

	if (!lpBuffer) {
		if (plBytesRead)
			*plBytesRead = sizeof(VDPosition);

		if (plSamplesRead)
			*plSamplesRead = 1;

		return 0;
	}

	if (sizeof(VDPosition) > cbBuffer) {
		if (plBytesRead)
			*plBytesRead = sizeof(VDPosition);

		return IVDStreamSource::kBufferTooSmall;
	}

	*(VDPosition *)lpBuffer = lStart;
	
	if (plBytesRead)
		*plBytesRead = sizeof(VDPosition);

	if (plSamplesRead)
		*plSamplesRead = 1;

	return 0;
}

bool VDVideoSourceTest::setTargetFormat(int format) {
	if (!format)
		format = nsVDPixmap::kPixFormat_XRGB8888;

	switch(format) {
	case nsVDPixmap::kPixFormat_YUV422_UYVY:
	case nsVDPixmap::kPixFormat_YUV422_YUYV:
		m422Buffer.init(640, 480, nsVDPixmap::kPixFormat_YUV422_Planar);
		// fall through
	case nsVDPixmap::kPixFormat_XRGB8888:
	case nsVDPixmap::kPixFormat_YUV444_Planar:
	case nsVDPixmap::kPixFormat_YUV422_Planar:
	case nsVDPixmap::kPixFormat_YUV420_Planar:
	case nsVDPixmap::kPixFormat_YUV411_Planar:
	case nsVDPixmap::kPixFormat_YUV410_Planar:
		if (!VideoSource::setTargetFormat(format))
			return false;

		invalidateFrameBuffer();

		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////

namespace {
	void DrawZonePlateXRGB8888(VDPixmap& dst, int x0, int y0, int w, int h, uint32 multiplier) {
		uint32 *row = (uint32 *)((char *)dst.data + dst.pitch*y0) + x0;

		float iw = 1.0f / (float)w;
		float ih = 1.0f / (float)h;

		float tscale = nsVDMath::kfPi * (float)w;

		for(int y = 0; y < h; ++y) {
			float dy = (float)y * ih - 0.5f;
			float t2 = dy*dy;

			for(int x = 0; x < w; ++x) {
				float dx = (float)x * iw - 0.5f;
				float t = (t2 + dx*dx)*tscale;
				float v = cosf(t);

				row[x] = (uint32)VDClampedRoundFixedToUint8Fast(0.5f + 0.5f*v) * multiplier;
			}

			vdptrstep(row, dst.pitch);
		}
	}

	void DrawZonePlateA8(VDPixmap& dst, int plane, int x0, int y0, int w, int h, uint8 minval, uint8 maxval) {
		uint8 *row;
		ptrdiff_t pitch;
		
		switch(plane) {
		case 0:
		default:
			pitch = dst.pitch;
			row = (uint8 *)((char *)dst.data + dst.pitch*y0) + x0;
			break;
		case 1:
			pitch = dst.pitch2;
			row = (uint8 *)((char *)dst.data2 + dst.pitch2*y0) + x0;
			break;
		case 2:
			pitch = dst.pitch3;
			row = (uint8 *)((char *)dst.data3 + dst.pitch3*y0) + x0;
			break;
		}

		float bias = ((int)maxval + (int)minval) / 510.0f;
		float scale = ((int)maxval - (int)minval) / 510.0f;

		float iw = 1.0f / (float)w;
		float ih = 1.0f / (float)h;
		float tscale = nsVDMath::kfPi * (float)w;
		for(int y = 0; y < h; ++y) {
			float dy = (float)y * ih - 0.5f;
			float t2 = dy*dy;

			for(int x = 0; x < w; ++x) {
				float dx = (float)x * iw - 0.5f;
				float t = (t2 + dx*dx)*tscale;
				float v = cosf(t);

				row[x] = (uint8)VDClampedRoundFixedToUint8Fast(bias + scale*v);
			}

			vdptrstep(row, pitch);
		}
	}
}

const void *VDVideoSourceTest::streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition frame_num, VDPosition target_sample) {
	// We may get a zero-byte frame if we already have the image.
	if (!data_len)
		return getFrameBuffer();

	// clear framebuffer
	const char *format = "";
	uint32 textcolor = 0xFF98FFFF;
	uint32 black = 0xFF000000;
	bool isyuv = false;

	VDPixmap *dst = &mTargetFormat;

	switch(dst->format) {
		case nsVDPixmap::kPixFormat_XRGB8888:
			format = "32-bit RGB";
			VDMemset32Rect(dst->data, dst->pitch, 0xFF404040, dst->w, dst->h);
			break;
		case nsVDPixmap::kPixFormat_YUV422_UYVY:
			format = "4:2:2 interleaved YCbCr (UYVY)";
			textcolor = 0xFF40EBC0;
			black = 0xFF801080;
			isyuv = true;
			dst = &m422Buffer;
			VDMemset8Rect(dst->data, dst->pitch, 0x47, dst->w, dst->h);
			VDMemset8Rect(dst->data2, dst->pitch2, 0x80, dst->w >> 1, dst->h);
			VDMemset8Rect(dst->data3, dst->pitch3, 0x80, dst->w >> 1, dst->h);
			break;
		case nsVDPixmap::kPixFormat_YUV422_YUYV:
			format = "4:2:2 interleaved YCbCr (YUYV)";
			textcolor = 0xFF40EBC0;
			black = 0xFF801080;
			isyuv = true;
			dst = &m422Buffer;
			VDMemset8Rect(dst->data, dst->pitch, 0x47, dst->w, dst->h);
			VDMemset8Rect(dst->data2, dst->pitch2, 0x80, dst->w >> 1, dst->h);
			VDMemset8Rect(dst->data3, dst->pitch3, 0x80, dst->w >> 1, dst->h);
			break;
		case nsVDPixmap::kPixFormat_YUV444_Planar:
			format = "4:4:4 planar YCbCr";
			textcolor = 0xFF40EBC0;
			black = 0xFF801080;
			isyuv = true;
			VDMemset8Rect(dst->data, dst->pitch, 0x47, dst->w, dst->h);
			VDMemset8Rect(dst->data2, dst->pitch2, 0x80, dst->w, dst->h);
			VDMemset8Rect(dst->data3, dst->pitch3, 0x80, dst->w, dst->h);
			break;
		case nsVDPixmap::kPixFormat_YUV422_Planar:
			format = "4:2:2 planar YCbCr";
			textcolor = 0xFF40EBC0;
			black = 0xFF801080;
			isyuv = true;
			VDMemset8Rect(dst->data, dst->pitch, 0x47, dst->w, dst->h);
			VDMemset8Rect(dst->data2, dst->pitch2, 0x80, dst->w >> 1, dst->h);
			VDMemset8Rect(dst->data3, dst->pitch3, 0x80, dst->w >> 1, dst->h);
			break;
		case nsVDPixmap::kPixFormat_YUV420_Planar:
			format = "4:2:0 planar YCbCr";
			textcolor = 0xFF40EBC0;
			black = 0xFF801080;
			isyuv = true;
			VDMemset8Rect(dst->data, dst->pitch, 0x47, dst->w, dst->h);
			VDMemset8Rect(dst->data2, dst->pitch2, 0x80, dst->w >> 1, dst->h >> 1);
			VDMemset8Rect(dst->data3, dst->pitch3, 0x80, dst->w >> 1, dst->h >> 1);
			break;
		case nsVDPixmap::kPixFormat_YUV411_Planar:
			format = "4:1:1 planar YCbCr";
			textcolor = 0xFF40EBC0;
			black = 0xFF801080;
			isyuv = true;
			VDMemset8Rect(dst->data, dst->pitch, 0x47, dst->w, dst->h);
			VDMemset8Rect(dst->data2, dst->pitch2, 0x80, dst->w >> 2, dst->h);
			VDMemset8Rect(dst->data3, dst->pitch3, 0x80, dst->w >> 2, dst->h);
			break;
		case nsVDPixmap::kPixFormat_YUV410_Planar:
			format = "4:1:0 planar YCbCr";
			textcolor = 0xFF40EBC0;
			black = 0xFF801080;
			isyuv = true;
			VDMemset8Rect(dst->data, dst->pitch, 0x47, dst->w, dst->h);
			VDMemset8Rect(dst->data2, dst->pitch2, 0x80, dst->w >> 2, dst->h >> 2);
			VDMemset8Rect(dst->data3, dst->pitch3, 0x80, dst->w >> 2, dst->h >> 2);
			break;
	}

	vdfloat4x4 ortho;
	ortho.m[0].set(2.0f/640.0f, 0.0f, 0.0f, -1.0f);
	ortho.m[1].set(0.0f, 2.0f/480.0f, 0.0f, -1.0f);
	ortho.m[2].set(0.0f, 0.0f, 0.0f, 0.0f);
	ortho.m[3].set(0.0f, 0.0f, 0.0f, 1.0f);

	if (mMode == 0) {
		DrawRotatingCubeFrame(*dst, false, false, (int)frame_num, isyuv);
	} else if (mMode == 1) {
		int frameBase = (int)frame_num << 1;
		DrawRotatingCubeFrame(*dst, true, false, frameBase + 0, isyuv);
		DrawRotatingCubeFrame(*dst, true, true,  frameBase + 1, isyuv);
	} else if (mMode == 2) {
		int frameBase = (int)frame_num << 1;
		DrawRotatingCubeFrame(*dst, true, false, frameBase + 1, isyuv);
		DrawRotatingCubeFrame(*dst, true, true,  frameBase + 0, isyuv);
	} else if (mMode == 3) {
		static const int kIndices[6]={0,1,2,3,5,4};
		VDTriColorVertex triv[6];

		triv[0].x = 320.0f;
		triv[0].y = 240.0f;
		triv[0].z = 0.0f;
		if (isyuv) {
			triv[0].r = 128.0f / 255.0f;
			triv[0].g = 235.0f / 255.0f;
			triv[0].b = 128.0f / 255.0f;
			triv[0].a = 1.0f;
		} else {
			triv[0].r = 1.0f;
			triv[0].g = 1.0f;
			triv[0].b = 1.0f;
			triv[0].a = 1.0f;
		}

		triv[1] = triv[0];
		triv[1].x = 380.0f;
		triv[1].y = 140.0f;

		triv[2] = triv[1];
		triv[2].x = 260.0f;

		triv[3].x = 320.0f;
		triv[3].y = 240.0f;
		triv[3].z = 0.0f;
		if (isyuv) {
			triv[3].r = 240.0f / 255.0f;
			triv[3].g = 128.0f / 255.0f;
			triv[3].b = 240.0f / 255.0f;
			triv[3].a = 1.0f;
		} else {
			triv[3].r = 1.0f;
			triv[3].g = 0.0f;
			triv[3].b = 1.0f;
			triv[3].a = 1.0f;
		}

		triv[4] = triv[3];
		triv[4].x = 380.0f;
		triv[4].y = 340.0f;

		triv[5] = triv[4];
		triv[5].x = 260.0f;

		VDPixmapTriFill(*dst, triv, 6, kIndices, 6, &ortho[0][0]);
	} else if (mMode == 4) {
		static const int kIndices[6]={0,1,2,2,1,3};
		VDTriColorVertex triv[4];

		static const char *const kNames[2][3]={
			{ "Red", "Green", "Blue" },
			{ "Cr", "Y", "Cb" },
		};

		for(int channel = 0; channel < 3; ++channel) {
			for(int v=0; v<4; ++v) {
				triv[v].x = 90.0f + ((v&1) ? 512.0f : 0.0f);
				triv[v].y = 40.0f + 120.0f * channel + ((v&2) ? 80.0f : 0.0f);
				triv[v].z = 0;

				if (isyuv) {
					triv[v].r = 128.0f / 255.0f;
					triv[v].g = 128.0f / 255.0f;
					triv[v].b = 128.0f / 255.0f;

					switch(channel) {
					case 0:
						triv[v].r = v&1 ? 1.0f : 0.0f;
						break;
					case 1:
						triv[v].g = v&1 ? 1.0f : 0.0f;
						break;
					case 2:
						triv[v].b = v&1 ? 1.0f : 0.0f;
						break;
					}
				} else {
					triv[v].r = 0;
					triv[v].g = 0;
					triv[v].b = 0;

					switch(channel) {
					case 0:
						triv[v].r = v&1 ? 1.0f : 0.0f;
						break;
					case 1:
						triv[v].g = v&1 ? 1.0f : 0.0f;
						break;
					case 2:
						triv[v].b = v&1 ? 1.0f : 0.0f;
						break;
					}
				}
			}
			VDPixmapTriFill(*dst, triv, 4, kIndices, 6, &ortho[0][0]);

			VDPixmapPathRasterizer rast;
			VDPixmapConvertTextToPath(rast, NULL, 24.0f * 64.0f, 10.0f * 64.0f, (60.0f + 120.0f * channel) * 64.0f, kNames[isyuv][channel]);

			VDPixmapRegion region;
			VDPixmapRegion border;
			VDPixmapRegion brush;
			rast.ScanConvert(region);

			VDPixmapCreateRoundRegion(brush, 16.0f);
			VDPixmapConvolveRegion(border, region, brush);

			VDPixmapFillRegionAntialiased8x(*dst, border, 0, 0, black);
			VDPixmapFillRegionAntialiased8x(*dst, region, 0, 0, textcolor);
		}
	} else if (mMode == 5) {
		uint8 *dstrow = (uint8 *)dst->data;
		for(int y=0; y<480; ++y) {
			uint8 *dstp = dstrow;

			switch(dst->format) {
			case nsVDPixmap::kPixFormat_XRGB8888:
				for(int x=0; x<640; ++x) {
					((uint32 *)dstp)[x] = (x^y)&1 ? 0xFFFFFF : 0;
				}
				break;
			case nsVDPixmap::kPixFormat_YUV422_UYVY:
				VDMemset32(dstrow, y&1 ? 0x1080EB80 : 0xEB801080, 320);
				break;
			case nsVDPixmap::kPixFormat_YUV422_YUYV:
				VDMemset32(dstrow, y&1 ? 0x801080EB : 0x80EB8010, 320);
				break;
			case nsVDPixmap::kPixFormat_YUV444_Planar:
			case nsVDPixmap::kPixFormat_YUV422_Planar:
			case nsVDPixmap::kPixFormat_YUV420_Planar:
			case nsVDPixmap::kPixFormat_YUV410_Planar:
				VDMemset32(dstrow, y&1 ? 0x10EB10EB : 0xEB10EB10, 160);
				break;
			}

			vdptrstep(dstrow, dst->pitch);
		}
	} else if (mMode == 6) {
		switch(dst->format) {
		case nsVDPixmap::kPixFormat_XRGB8888:
			DrawZonePlateXRGB8888(*dst, 112,  16, 192, 192, 0x00010000);
			DrawZonePlateXRGB8888(*dst, 336,  16, 192, 192, 0x00000100);
			DrawZonePlateXRGB8888(*dst, 112, 224, 192, 192, 0x00000001);
			DrawZonePlateXRGB8888(*dst, 336, 224, 192, 192, 0x00010101);
			break;
		case nsVDPixmap::kPixFormat_YUV444_Planar:
			DrawZonePlateA8(*dst, 0, 224,  16, 192, 192, 16, 235);
			DrawZonePlateA8(*dst, 1, 112, 224, 192, 192, 16, 240);
			DrawZonePlateA8(*dst, 2, 336, 224, 192, 192, 16, 240);
			break;
		case nsVDPixmap::kPixFormat_YUV422_UYVY:
		case nsVDPixmap::kPixFormat_YUV422_YUYV:
		case nsVDPixmap::kPixFormat_YUV422_Planar:
			DrawZonePlateA8(*dst, 0, 224,  16, 192, 192, 16, 235);
			DrawZonePlateA8(*dst, 1,  56, 224,  96, 192, 16, 240);
			DrawZonePlateA8(*dst, 2, 168, 224,  96, 192, 16, 240);
			break;
		case nsVDPixmap::kPixFormat_YUV420_Planar:
			DrawZonePlateA8(*dst, 0, 224,  16, 192, 192, 16, 235);
			DrawZonePlateA8(*dst, 1,  56, 112,  96,  96, 16, 240);
			DrawZonePlateA8(*dst, 2, 168, 112,  96,  96, 16, 240);
			break;
		case nsVDPixmap::kPixFormat_YUV411_Planar:
			DrawZonePlateA8(*dst, 0, 224,  16, 192, 192, 16, 235);
			DrawZonePlateA8(*dst, 1,  28, 224,  48, 192, 16, 240);
			DrawZonePlateA8(*dst, 2,  84, 224,  48, 192, 16, 240);
			break;
		case nsVDPixmap::kPixFormat_YUV410_Planar:
			DrawZonePlateA8(*dst, 0, 224,  16, 192, 192, 16, 235);
			DrawZonePlateA8(*dst, 1,  28,  56,  48,  48, 16, 240);
			DrawZonePlateA8(*dst, 2,  84,  56,  48,  48, 16, 240);
			break;
		}
	} else if (mMode == 7) {
		// A1 A1 B1 C1 D1
		// A2 B2 C2 C2 D2
		int frame32 = (int)frame_num;
		int frameBase = frame32 / 5 * 4;

		VDPixmap pxfield0(VDPixmapExtractField(mRGB32Buffer, false));
		VDPixmap pxfield1(VDPixmapExtractField(mRGB32Buffer, true));

		switch(frame32 % 5) {
			case 0:
				DrawPhysFrame(pxfield0, false, frameBase+0);
				DrawPhysFrame(pxfield1, true, frameBase+0);
				break;
			case 1:
				DrawPhysFrame(pxfield0, false, frameBase+0);
				DrawPhysFrame(pxfield1, true, frameBase+1);
				break;
			case 2:
				DrawPhysFrame(pxfield0, false, frameBase+1);
				DrawPhysFrame(pxfield1, true, frameBase+2);
				break;
			case 3:
				DrawPhysFrame(pxfield0, false, frameBase+2);
				DrawPhysFrame(pxfield1, true, frameBase+2);
				break;
			case 4:
				DrawPhysFrame(pxfield0, false, frameBase+3);
				DrawPhysFrame(pxfield1, true, frameBase+3);
				break;
		}

		VDPixmapBlt(*dst, mRGB32Buffer);
	} else if (mMode == 8) {
		// A1 B1 C1 C1 D1
		// A2 A2 B2 C2 D2
		int frame32 = (int)frame_num;
		int frameBase = frame32 / 5 * 4;

		VDPixmap pxfield0(VDPixmapExtractField(mRGB32Buffer, false));
		VDPixmap pxfield1(VDPixmapExtractField(mRGB32Buffer, true));

		switch(frame32 % 5) {
			case 0:
				DrawPhysFrame(pxfield0, false, frameBase+0);
				DrawPhysFrame(pxfield1, true, frameBase+0);
				break;
			case 1:
				DrawPhysFrame(pxfield0, false, frameBase+1);
				DrawPhysFrame(pxfield1, true, frameBase+0);
				break;
			case 2:
				DrawPhysFrame(pxfield0, false, frameBase+2);
				DrawPhysFrame(pxfield1, true, frameBase+1);
				break;
			case 3:
				DrawPhysFrame(pxfield0, false, frameBase+2);
				DrawPhysFrame(pxfield1, true, frameBase+2);
				break;
			case 4:
				DrawPhysFrame(pxfield0, false, frameBase+3);
				DrawPhysFrame(pxfield1, true, frameBase+3);
				break;
		}

		VDPixmapBlt(*dst, mRGB32Buffer);
	}

	// draw text
	static const char *const kModeNames[]={
		"RGB color cube",
		"RGB color cube (TFF)",
		"RGB color cube (BFF)",
		"Chroma subsampling offset",
		"Channel levels",
		"Checkerboard",
		"Zone plates",
		"3:2 pulldown (TFF)",
		"3:2 pulldown (BFF)"
	};

	char buf[128];
	sprintf(buf, "%s - frame %d (%s)", kModeNames[mMode], (int)frame_num, format);

	mTextRasterizer.Clear();
	VDPixmapConvertTextToPath(mTextRasterizer, NULL, 24.0f * 64.0f, 20.0f * 64.0f, 460.0f * 64.0f, buf);

	mTextRasterizer.ScanConvert(mTextRegion);

	VDPixmapConvolveRegion(mTextBorderRegion, mTextRegion, mTextOutlineBrush);

	VDPixmapFillRegionAntialiased8x(*dst, mTextBorderRegion, 0, 0, black);
	VDPixmapFillRegionAntialiased8x(*dst, mTextRegion, 0, 0, textcolor);

	switch(mTargetFormat.format) {
		case nsVDPixmap::kPixFormat_YUV422_UYVY:
		case nsVDPixmap::kPixFormat_YUV422_YUYV:
			VDPixmapBlt(mTargetFormat, *dst);
			break;
	}

	mCachedFrame = frame_num;
	return mpFrameBuffer;
}

const void *VDVideoSourceTest::getFrame(VDPosition frameNum) {
	if (mCachedFrame == frameNum)
		return mpFrameBuffer;

	return streamGetFrame(&frameNum, sizeof(VDPosition), FALSE, frameNum, frameNum);
}

void VDVideoSourceTest::DrawRotatingCubeFrame(VDPixmap& dst, bool interlaced, bool oddField, int frame_num, bool isyuv) {
	// draw cube
	static const int kIndices[] = {
		0, 1, 4, 4, 1, 5,
		2, 6, 3, 3, 6, 7,
		0, 4, 2, 2, 4, 6,
		1, 3, 5, 5, 3, 7,
		0, 2, 1, 1, 2, 3,
		4, 5, 6, 6, 5, 7,
	};

	VDTriColorVertex vertices[8];

	for(int i=0; i<8; ++i) {
		vertices[i].x = i&1 ? 1.0f : -1.0f;
		vertices[i].y = i&2 ? 1.0f : -1.0f;
		vertices[i].z = i&4 ? 1.0f : -1.0f;

		float r = (i&1) ? 1.0f : 0.0f;
		float g = (i&2) ? 1.0f : 0.0f;
		float b = (i&4) ? 1.0f : 0.0f;

		if (isyuv) {
			float y = 0.299f*r + 0.587f*g + 0.114f*b;
			float cr = 0.713f*(r-y);
			float cb = 0.564f*(b-y);
			vertices[i].r = cr*224.0f/255.0f + 128.0f/255.0f;
			vertices[i].g = y*219.0f/255.0f + 16.0f/255.0f;
			vertices[i].b = cb*224.0f/255.0f + 128.0f/255.0f;
		} else {
			vertices[i].r = r;
			vertices[i].g = g;
			vertices[i].b = b;
		}

		vertices[i].a = 1.0f;
	}

	float rot = (float)frame_num * 0.02f;

	if (interlaced)
		rot *= 0.5f;

	const float kNear = 0.1f;
	const float kFar = 500.0f;
	const float w = 640.0f / 5000.0f;
	const float h = 480.0f / 5000.0f;

	vdfloat4x4 proj;

	proj.m[0].set(2.0f*kNear/w, 0.0f, 0.0f, 0.0f);
	proj.m[1].set(0.0f, 2.0f*kNear/h, 0.0f, 0.0f);
	proj.m[2].set(0.0f, 0.0f, -(kFar+kNear)/(kFar-kNear), -2.0f*kNear*kFar/(kFar-kNear));
	proj.m[3].set(0.0f, 0.0f, -1.0f, 0.0f);

	vdfloat4x4 objpos;

	objpos.m[0].set(10.0f, 0.0f, 0.0f, 0.0f);
	objpos.m[1].set(0.0f, 10.0f, 0.0f, 0.0f);
	objpos.m[2].set(0.0f, 0.0f, 10.0f, -50.0f);
	objpos.m[3].set(0.0f, 0.0f, 0.0f, 1.0f);

	vdfloat4x4 xf = proj * objpos * vdfloat4x4(vdfloat4x4::rotation_x, rot*1.0f) * vdfloat4x4(vdfloat4x4::rotation_y, rot*1.7f) * vdfloat4x4(vdfloat4x4::rotation_z, rot*2.3f);

	if (interlaced) {
		VDPixmap field(VDPixmapExtractField(dst, oddField));

		if (oddField)
			proj.m[2].z += proj.m[2].w / 480.0f;
		else
			proj.m[2].z -= proj.m[2].w / 480.0f;

		VDPixmapTriFill(field, vertices, 8, kIndices, 36, &xf[0][0]);
	} else {
		VDPixmapTriFill(dst, vertices, 8, kIndices, 36, &xf[0][0]);
	}
}

void VDVideoSourceTest::DrawPhysFrame(VDPixmap& dst, bool oddField, int frameIdx) {
	VDMemset32Rect(dst.data, dst.pitch, 0x00, 640, 240);

	VDTriColorVertex vx[10];

	const VDTestVidPhysFrame& frame = mVideo.mFrames[frameIdx];
	const VDTestVidPhysPartPos *parts = mVideo.mParticles.data() + frame.mFirstParticle;

	for(int i=0; i<3; ++i) {
		float t = (float)i * 6.28318f / 3.0f + frame.mTriRotation;
		vx[i].x = 320.0f + 100.0f*cosf(t);
		vx[i].y = 240.0f - 100.0f*sinf(t);
		vx[i].z = 0.0f;
		vx[i].r = (i == 0) ? 1.0f : 0.0f;
		vx[i].g = (i == 1) ? 1.0f : 0.0f;
		vx[i].b = (i == 2) ? 1.0f : 0.0f;
		vx[i].a = 255;
	}

	const float xf[16]={
		2.0f / 640.0f, 0.0f, 0.0f, -1.0f,
		0.0f, -2.0f / 480.0f, 0.0f, 1.0f + (oddField ? -1.0f / 480.0f : 1.0f / 480.0f),
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};

	static const int indices[27]={0,1,2,0,2,3,0,3,4,0,4,5,0,5,6,0,6,7,0,7,8,0,8,9,0,9,1};
	VDPixmapTriFill(dst, vx, 3, indices, 3, xf);

	for(int i=0; i<frame.mParticleCount; ++i) {
		const VDTestVidPhysPartPos& par = parts[i];

		for(int j=0; j<10; ++j) {
			float t = (float)j * 6.28318f / 10.0f;
			vx[j].x = par.mX * (1.0f/16.0f) + cosf(t)*5.0f;
			vx[j].y = par.mY * (1.0f/16.0f) - sinf(t)*5.0f;
			vx[j].z = 0.0f;
			vx[j].r = 1.0f;
			vx[j].g = 1.0f;
			vx[j].b = 1.0f;
			vx[j].a = 1.0f;
		}

		VDPixmapTriFill(dst, vx, 10, indices, 27, xf);
	}
}

///////////////////////////////////////////////////////////////////////////////
class VDInputFileTestOptions : public InputFileOptions {
public:
	VDInputFileTestOptions();
	~VDInputFileTestOptions();
	bool read(const char *buf);
	int write(char *buf, int buflen) const;

public:
	int mMode;
};

VDInputFileTestOptions::VDInputFileTestOptions()
	: mMode(0)
{
}

VDInputFileTestOptions::~VDInputFileTestOptions() {
}

bool VDInputFileTestOptions::read(const char *buf) {
	if (buf[0] != 1)
		return false;

	mMode = buf[1];
	return true;
}

int VDInputFileTestOptions::write(char *buf, int buflen) const {
	if (!buf)
		return 2;

	if (buflen < 2)
		return 0;

	buf[0] = 1;
	buf[1] = (char)mMode;

	return 2;
}

class VDInputFileTestOptionsDialog : public VDDialogBase {
	VDInputFileTestOptions& mOpts;

public:
	VDInputFileTestOptionsDialog(VDInputFileTestOptions& opts) : mOpts(opts) {}

	bool HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item) {
		if (type == kEventAttach) {
			mpBase = pBase;
			SetValue(100, mOpts.mMode);
			pBase->ExecuteAllLinks();
		} else if (type == kEventSelect) {
			if (id == 10) {
				mOpts.mMode = GetValue(100);
				pBase->EndModal(true);
				return true;
			} else if (id == 11) {
				pBase->EndModal(false);
				return true;
			}
		}
		return false;
	}
};

///////////////////////////////////////////////////////////////////////////////

class VDInputFileTest : public InputFile {
public:
	VDInputFileTest();
	~VDInputFileTest();

	void Init(const wchar_t *szFile);

	void setOptions(InputFileOptions *_ifo);
	InputFileOptions *createOptions(const void *buf, uint32 len);
	InputFileOptions *promptForOptions(VDGUIHandle hwnd);

	void setAutomated(bool fAuto);

	bool GetVideoSource(int index, IVDVideoSource **ppSrc);
	bool GetAudioSource(int index, AudioSource **ppSrc);

protected:
	VDInputFileTestOptions	mOptions;
};

VDInputFileTest::VDInputFileTest() {
}

VDInputFileTest::~VDInputFileTest() {
}

void VDInputFileTest::Init(const wchar_t *) {
}

void VDInputFileTest::setOptions(InputFileOptions *_ifo) {
	mOptions = *static_cast<VDInputFileTestOptions *>(_ifo);
}

InputFileOptions *VDInputFileTest::createOptions(const void *buf, uint32 len) {
	VDInputFileTestOptions *opts = new_nothrow VDInputFileTestOptions;

	if (opts) {
		if (opts->read((const char *)buf))
			return opts;

		delete opts;
	}

	return NULL;
}

InputFileOptions *VDInputFileTest::promptForOptions(VDGUIHandle hwnd) {
	vdautoptr<VDInputFileTestOptions> testopts(new_nothrow VDInputFileTestOptions);

	if (!testopts)
		return NULL;

	vdautoptr<IVDUIWindow> peer(VDUICreatePeer((VDGUIHandle)hwnd));

	IVDUIWindow *pWin = VDCreateDialogFromResource(3000, peer);
	VDInputFileTestOptionsDialog dlg(*testopts);

	IVDUIBase *pBase = vdpoly_cast<IVDUIBase *>(pWin);
	
	pBase->SetCallback(&dlg, false);
	int result = pBase->DoModal();

	peer->Shutdown();

	if (!result)
		throw MyUserAbortError();

	return testopts.release();
}

void VDInputFileTest::setAutomated(bool fAuto) {
}

bool VDInputFileTest::GetVideoSource(int index, IVDVideoSource **ppSrc) {
	if (index)
		return false;

	IVDVideoSource *videoSrc = new VDVideoSourceTest(mOptions.mMode);
	*ppSrc = videoSrc;
	videoSrc->AddRef();
	return true;
}

bool VDInputFileTest::GetAudioSource(int index, AudioSource **ppSrc) {
	return false;
}

///////////////////////////////////////////////////////////////////////////

class VDInputDriverTest : public vdrefcounted<IVDInputDriver> {
public:
	const wchar_t *GetSignatureName() { return L"Test video input driver (internal)"; }

	int GetDefaultPriority() {
		return -1000;
	}

	uint32 GetFlags() { return kF_Video; }

	const wchar_t *GetFilenamePattern() {
		// This input driver is never meant to be used for a file.
		return NULL;
	}

	bool DetectByFilename(const wchar_t *pszFilename) {
		return false;
	}

	int DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
		return -1;
	}

	InputFile *CreateInputFile(uint32 flags) {
		return new VDInputFileTest;
	}
};

extern IVDInputDriver *VDCreateInputDriverTest() { return new VDInputDriverTest; }
