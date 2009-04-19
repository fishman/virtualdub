#include <vd2/Kasumi/pixmap.h>
#include "uberblit.h"
#include "uberblit_gen.h"

uint32 VDPixmapGetFormatTokenFromFormat(int format) {
	using namespace nsVDPixmap;
	switch(format) {
	case kPixFormat_Pal1:			return kVDPixType_1 | kVDPixSamp_444 | kVDPixSpace_Pal;
	case kPixFormat_Pal2:			return kVDPixType_2 | kVDPixSamp_444 | kVDPixSpace_Pal;
	case kPixFormat_Pal4:			return kVDPixType_4 | kVDPixSamp_444 | kVDPixSpace_Pal;
	case kPixFormat_Pal8:			return kVDPixType_8 | kVDPixSamp_444 | kVDPixSpace_Pal;
	case kPixFormat_XRGB1555:		return kVDPixType_1555_LE | kVDPixSamp_444 | kVDPixSpace_BGR;
	case kPixFormat_RGB565:			return kVDPixType_565_LE | kVDPixSamp_444 | kVDPixSpace_BGR;
	case kPixFormat_RGB888:			return kVDPixType_888 | kVDPixSamp_444 | kVDPixSpace_BGR;
	case kPixFormat_XRGB8888:		return kVDPixType_8888 | kVDPixSamp_444 | kVDPixSpace_BGR;
	case kPixFormat_Y8:				return kVDPixType_8 | kVDPixSamp_444 | kVDPixSpace_Y_601;
	case kPixFormat_YUV422_UYVY:	return kVDPixType_B8G8_R8G8 | kVDPixSamp_422 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV422_YUYV:	return kVDPixType_G8B8_G8R8 | kVDPixSamp_422 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV444_XVYU:	return kVDPixType_8888 | kVDPixSamp_444 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV444_Planar:	return kVDPixType_8_8_8 | kVDPixSamp_444 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV422_Planar:	return kVDPixType_8_8_8 | kVDPixSamp_422 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV420_Planar:	return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV411_Planar:	return kVDPixType_8_8_8 | kVDPixSamp_411 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV410_Planar:	return kVDPixType_8_8_8 | kVDPixSamp_410 | kVDPixSpace_YCC_601;
	default:
		return 0;
	}
}

const VDPixmapSamplingInfo& VDPixmapGetSamplingInfo(uint32 samplingToken) {
	static const VDPixmapSamplingInfo kPixmapSamplingInfo[]={
		/* Null			*/ {  0,  0,  0,  0,  0 },
		/* 444			*/ {  0,  0,  0,  0,  0 },
		/* 422			*/ { -4,  0,  0,  1,  0 },
		/* 420_MPEG2	*/ { -4,  0,  0,  1,  1 },
		/* 420_MPEG2INT	*/ { -4,  0,  0,  1,  1 },
		/* 420_MPEG1	*/ {  0,  0,  0,  1,  1 },
		/* 420_DVPAL	*/ { -4,  0,  0,  1,  1 },
		/* 411			*/ { -6,  0,  0,  2,  0 },
		/* 410			*/ { -6,  0,  0,  2,  2 }
	};

	uint32 index = (samplingToken & kVDPixSamp_Mask) >> kVDPixSamp_Bits;

	return index >= sizeof(kPixmapSamplingInfo)/sizeof(kPixmapSamplingInfo[0]) ? kPixmapSamplingInfo[0] : kPixmapSamplingInfo[index];
}

namespace {
	uint32 BlitterConvertSampling(VDPixmapUberBlitterGenerator& gen, uint32 srcToken, uint32 dstSamplingToken, sint32 w, sint32 h) {
		const VDPixmapSamplingInfo& srcInfo = VDPixmapGetSamplingInfo(srcToken);
		const VDPixmapSamplingInfo& dstInfo = VDPixmapGetSamplingInfo(dstSamplingToken);

		// convert destination chroma origin to luma space
		int c_x = ((8 + dstInfo.mCXOffset16) << dstInfo.mCXBits) - 8;
		int cr_y = ((8 + dstInfo.mCrYOffset16) << dstInfo.mCYBits) - 8;
		int cb_y = ((8 + dstInfo.mCbYOffset16) << dstInfo.mCYBits) - 8;

		// convert luma chroma location to source chroma space
		c_x = ((8 + c_x) >> srcInfo.mCXBits) - 8 - srcInfo.mCXOffset16;
		cr_y = ((8 + cr_y) >> srcInfo.mCYBits) - 8 - srcInfo.mCrYOffset16;
		cb_y = ((8 + cb_y) >> srcInfo.mCYBits) - 8 - srcInfo.mCbYOffset16;

		float cxo = c_x / 16.0f;
		float cxf = ((16 << dstInfo.mCXBits) >> srcInfo.mCXBits) / 16.0f;
		float cyf = ((16 << dstInfo.mCYBits) >> srcInfo.mCYBits) / 16.0f;
		sint32 cw = w >> dstInfo.mCXBits;
		sint32 ch = w >> dstInfo.mCXBits;

		gen.swap(2);
		gen.linear(cxo, cxf, cw, cb_y / 16.0f, cyf, ch);
		gen.swap(2);
		gen.linear(cxo, cxf, cw, cr_y / 16.0f, cyf, ch);

		return (srcToken & ~kVDPixSamp_Mask) | kVDPixSamp_444;
	}

	uint32 BlitterConvertType(VDPixmapUberBlitterGenerator& gen, uint32 srcToken, uint32 dstToken, sint32 w, sint32 h) {
		uint32 dstType = dstToken & kVDPixType_Mask;

		while((srcToken ^ dstToken) & kVDPixType_Mask) {
			uint32 srcType = srcToken & kVDPixType_Mask;
			uint32 targetType = dstType;

	type_reconvert:
			switch(targetType) {
				case kVDPixType_1555_LE:
					switch(srcType) {
						case kVDPixType_565_LE:
							gen.conv_565_to_555();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_1555_LE;
							break;

						case kVDPixType_8888:
							gen.conv_8888_to_555();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_1555_LE;
							break;
						case kVDPixType_B8G8_R8G8:
						case kVDPixType_G8B8_G8R8:
							targetType = kVDPixType_8_8_8;
							goto type_reconvert;
						default:
							targetType = kVDPixType_8888;
							goto type_reconvert;
					}
					break;

				case kVDPixType_565_LE:
					switch(srcType) {
						case kVDPixType_1555_LE:
							gen.conv_555_to_565();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_565_LE;
							break;
						case kVDPixType_8888:
							gen.conv_8888_to_565();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_565_LE;
							break;
						case kVDPixType_B8G8_R8G8:
						case kVDPixType_G8B8_G8R8:
							targetType = kVDPixType_8_8_8;
							goto type_reconvert;
						default:
							targetType = kVDPixType_8888;
							goto type_reconvert;
					}
					break;

				case kVDPixType_888:
					switch(srcType) {
						case kVDPixType_8888:
							gen.conv_8888_to_888();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_888;
							break;
						default:
							targetType = kVDPixType_8888;
							goto type_reconvert;
					}
					break;

				case kVDPixType_8888:
					switch(srcType) {
						case kVDPixType_1555_LE:
							gen.conv_555_to_8888();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8888;
							break;
						case kVDPixType_565_LE:
							gen.conv_565_to_8888();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8888;
							break;
						case kVDPixType_888:
							gen.conv_888_to_8888();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8888;
							break;
						case kVDPixType_32Fx4_LE:
							gen.conv_X32F_to_8888();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8888;
							break;
						case kVDPixType_8_8_8:
							if ((srcToken & kVDPixSamp_Mask) != kVDPixSamp_444)
								srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_444, w, h);
							gen.interleave_X8R8G8B8();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8888;
							break;
						default:
							VDASSERT(false);
							break;
					}
					break;

				case kVDPixType_8:
					switch(srcType) {
						case kVDPixType_8_8_8:
							gen.pop();
							gen.swap(1);
							gen.pop();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
							break;
						default:
							targetType = kVDPixType_8_8_8;
							break;
					}
					break;

				case kVDPixType_8_8_8:
					switch(srcType) {
						case kVDPixType_B8G8_R8G8:
							gen.dup();
							gen.dup();
							gen.extract_8in32(2, w >> 1, h);
							gen.swap(2);
							gen.extract_8in16(1, w, h);
							gen.swap(1);
							gen.extract_8in32(0, w >> 1, h);
							srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSamp_Mask)) | kVDPixType_8_8_8 | kVDPixSamp_422;
							break;
						case kVDPixType_G8B8_G8R8:
							if ((srcToken & kVDPixSamp_Mask) != kVDPixSamp_422)
								srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_422, w, h);
							gen.dup();
							gen.dup();
							gen.extract_8in32(3, w >> 1, h);
							gen.swap(2);
							gen.extract_8in16(0, w, h);
							gen.swap(1);
							gen.extract_8in32(1, w >> 1, h);
							srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSamp_Mask)) | kVDPixType_8_8_8 | kVDPixSamp_422;
							break;
						default:
							VDASSERT(false);
							break;
					}
					break;

				case kVDPixType_B8G8_R8G8:
					switch(srcType) {
					case kVDPixType_8_8_8:
						if ((srcToken ^ dstToken) & kVDPixSamp_Mask)
							srcToken = BlitterConvertSampling(gen, srcToken, dstToken, w, h);

						gen.interleave_B8G8_R8G8();
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSamp_Mask)) | kVDPixType_B8G8_R8G8;
						break;
					case kVDPixType_G8B8_G8R8:
						gen.swap_8in16(w, h, w*2);
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSamp_Mask)) | kVDPixType_B8G8_R8G8;
						break;
					default:
						targetType = kVDPixType_8_8_8;
						goto type_reconvert;
					}
					break;

				case kVDPixType_G8B8_G8R8:
					switch(srcType) {
					case kVDPixType_8_8_8:
						if ((srcToken ^ dstToken) & kVDPixSamp_Mask)
							srcToken = BlitterConvertSampling(gen, srcToken, dstToken, w, h);

						gen.interleave_G8B8_G8R8();
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSamp_Mask)) | kVDPixType_G8B8_G8R8;
						break;
					case kVDPixType_B8G8_R8G8:
						gen.swap_8in16(w, h, w*2);
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSamp_Mask)) | kVDPixType_G8B8_G8R8;
						break;
					default:
						targetType = kVDPixType_8_8_8;
						goto type_reconvert;
					}
					break;

				default:
					VDASSERT(false);
					break;
			}
		}

		return srcToken;
	}
}

IVDPixmapBlitter *VDPixmapCreateBlitter(const VDPixmap& dst, const VDPixmap& src) {
	uint32 srcToken = VDPixmapGetFormatTokenFromFormat(src.format);
	uint32 dstToken = VDPixmapGetFormatTokenFromFormat(dst.format);

	VDPixmapUberBlitterGenerator gen;

	// load source channels
	int w = src.w;
	int h = src.h;

	switch(srcToken & kVDPixType_Mask) {
	case kVDPixType_1:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, (w + 7) >> 3);
		break;

	case kVDPixType_2:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, (w + 3) >> 2);
		break;

	case kVDPixType_4:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, (w + 1) >> 1);
		break;

	case kVDPixType_8:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w);
		break;

	case kVDPixType_555_LE:
	case kVDPixType_565_LE:
	case kVDPixType_1555_LE:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w*2);
		break;

	case kVDPixType_888:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w*3);
		break;

	case kVDPixType_8888:
	case kVDPixType_32F_LE:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w*4);
		break;

	case kVDPixType_32Fx4_LE:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w*16);
		break;

	case kVDPixType_B8G8_R8G8:
	case kVDPixType_G8B8_G8R8:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, ((w + 1) & ~1)*2);
		break;
	case kVDPixType_8_8_8:
		{
			uint32 ytoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
			uint32 cbtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
			uint32 crtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;

			const VDPixmapSamplingInfo& sampInfo = VDPixmapGetSamplingInfo(srcToken);

			int cxbits = sampInfo.mCXBits;
			int cybits = sampInfo.mCYBits;
			gen.ldsrc(0, 2, 0, 0, w >> cxbits, h >> cybits, cbtoken, w >> cxbits);
			gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w);
			gen.ldsrc(0, 1, 0, 0, w >> cxbits, h >> cybits, crtoken, w >> cxbits);
		}
		break;
	}

	// check if we need a color space change
	if ((srcToken ^ dstToken) & kVDPixSpace_Mask) {
		// first, if we're dealing with an interleaved format, deinterleave it
		switch(srcToken & kVDPixType_Mask) {
		case kVDPixType_B8G8_R8G8:
			{
				uint32 ytoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
				uint32 cbtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
				uint32 crtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;

				gen.dup();
				gen.dup();
				gen.extract_8in32(2, w >> 1, h);
				gen.swap(2);
				gen.extract_8in16(1, w, h);
				gen.swap(1);
				gen.extract_8in32(0, w >> 1, h);
			}
			break;
		case kVDPixType_G8B8_G8R8:
			{
				uint32 ytoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
				uint32 cbtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
				uint32 crtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;

				gen.dup();
				gen.dup();
				gen.extract_8in32(3, w >> 1, h);
				gen.swap(2);
				gen.extract_8in16(0, w, h);
				gen.swap(1);
				gen.extract_8in32(1, w >> 1, h);
			}
			break;
		}

		// if the source is subsampled, converge on 4:4:4 subsampling
		const VDPixmapSamplingInfo& sampInfo = VDPixmapGetSamplingInfo(srcToken);

		if (sampInfo.mCXBits | sampInfo.mCYBits | sampInfo.mCXOffset16 | sampInfo.mCbYOffset16 | sampInfo.mCrYOffset16)
			srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_444, w, h);

		// change color spaces
		uint32 dstSpace = dstToken & kVDPixSpace_Mask;
		while((srcToken ^ dstToken) & kVDPixSpace_Mask) {
			uint32 srcSpace = srcToken & kVDPixSpace_Mask;
			uint32 targetSpace = dstSpace;

space_reconvert:
			switch(targetSpace) {
				case kVDPixSpace_BGR:
					switch(srcSpace) {
					case kVDPixSpace_YCC_709:
						gen.ycbcr709_to_rgb32();
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_8888;
						break;
					case kVDPixSpace_YCC_601:
						gen.ycbcr601_to_rgb32();
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_8888;
						break;
					case kVDPixSpace_Y_601:
						targetSpace = kVDPixSpace_YCC_601;
						goto space_reconvert;
					default:
						VDASSERT(false);
						break;
					}
					break;
				case kVDPixSpace_Y_601:
					if (srcSpace == kVDPixSpace_YCC_601) {
						gen.pop();
						gen.swap(1);
						gen.pop();
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_Y_601 | kVDPixType_8;
						break;
					}
					// fall through
				case kVDPixSpace_YCC_601:
					switch(srcSpace) {
					case kVDPixSpace_BGR:
						srcToken = BlitterConvertType(gen, srcToken, kVDPixType_8888, w, h);
						gen.rgb32_to_ycbcr601();
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_YCC_601 | kVDPixType_8_8_8;
						break;
					case kVDPixSpace_Y_601:
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_YCC_601 | kVDPixType_8;
						gen.ldconst(0x80, w, w, h, srcToken);
						gen.swap(1);
						gen.ldconst(0x80, w, w, h, srcToken);
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_YCC_601 | kVDPixType_8_8_8;
						break;
					default:
						VDASSERT(false);
						break;
					}
					break;
				case kVDPixSpace_YCC_709:
					switch(srcSpace) {
					case kVDPixSpace_BGR:
						srcToken = BlitterConvertType(gen, srcToken, kVDPixType_8888, w, h);
						gen.rgb32_to_ycbcr709();
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_YCC_709 | kVDPixType_8_8_8;
						break;
					default:
						VDASSERT(false);
						break;
					}
					break;
			}
		}
	}

	// check if we need a type change
	srcToken = BlitterConvertType(gen, srcToken, dstToken, w, h);

	// convert subsampling if necessary
	if ((srcToken & kVDPixType_Mask) == kVDPixType_8_8_8) {
		if ((srcToken ^ dstToken) & kVDPixSamp_Mask)
			srcToken = BlitterConvertSampling(gen, srcToken, dstToken, w, h);
	}

	return gen.create();
}
