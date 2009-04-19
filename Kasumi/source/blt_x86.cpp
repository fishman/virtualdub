#include <vd2/system/vdtypes.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>

#define DECLARE_PALETTED(x, y) extern void VDPixmapBlt_##x##_to_##y##_reference(void *dst0, ptrdiff_t dstpitch, const void *src0, ptrdiff_t srcpitch, vdpixsize w, vdpixsize h, const void *pal0);
#define DECLARE_RGB(x, y) extern void VDPixmapBlt_##x##_to_##y##_reference(void *dst0, ptrdiff_t dstpitch, const void *src0, ptrdiff_t srcpitch, vdpixsize w, vdpixsize h);
#define DECLARE_RGB_ASM(x, y) extern "C" void vdasm_pixblt_##x##_to_##y(void *dst0, ptrdiff_t dstpitch, const void *src0, ptrdiff_t srcpitch, vdpixsize w, vdpixsize h);
#define DECLARE_RGB_ASM_MMX(x, y) extern "C" void vdasm_pixblt_##x##_to_##y##_MMX(void *dst0, ptrdiff_t dstpitch, const void *src0, ptrdiff_t srcpitch, vdpixsize w, vdpixsize h);
#define DECLARE_YUV(x, y) extern void VDPixmapBlt_##x##_to_##y##_reference(void *dst0, ptrdiff_t dstpitch, const void *src0, ptrdiff_t srcpitch, vdpixsize w, vdpixsize h);
#define DECLARE_YUV_REV(x, y) void VDPixmapBlt_##x##_to_##y##_reference(void *dst0, ptrdiff_t dstpitch, const void *src0, ptrdiff_t srcpitch, vdpixsize w, vdpixsize h)
#define DECLARE_YUV_PLANAR(x, y) extern void VDPixmapBlt_##x##_to_##y##_reference(const VDPixmap& dst, const VDPixmap& src, vdpixsize w, vdpixsize h);

									DECLARE_RGB_ASM(RGB565,	  XRGB1555);	DECLARE_RGB_ASM_MMX(RGB565,   XRGB1555);
									DECLARE_RGB_ASM(RGB888,   XRGB1555);
									DECLARE_RGB_ASM(XRGB8888, XRGB1555);	DECLARE_RGB_ASM_MMX(XRGB8888, XRGB1555);
									DECLARE_RGB_ASM(XRGB1555, RGB565);		DECLARE_RGB_ASM_MMX(XRGB1555, RGB565);
									DECLARE_RGB_ASM(RGB888,   RGB565);
									DECLARE_RGB_ASM(XRGB8888, RGB565);		DECLARE_RGB_ASM_MMX(XRGB8888, RGB565);
DECLARE_RGB(XRGB1555, RGB888);
DECLARE_RGB(RGB565,   RGB888);
									DECLARE_RGB_ASM(XRGB8888, RGB888);		DECLARE_RGB_ASM_MMX(XRGB8888, RGB888);
									DECLARE_RGB_ASM(XRGB1555, XRGB8888);	DECLARE_RGB_ASM_MMX(XRGB1555, XRGB8888);
									DECLARE_RGB_ASM(RGB565,   XRGB8888);	DECLARE_RGB_ASM_MMX(RGB565,   XRGB8888);
									DECLARE_RGB_ASM(RGB888,   XRGB8888);	DECLARE_RGB_ASM_MMX(RGB888,   XRGB8888);

DECLARE_PALETTED(Pal1, Any8);
DECLARE_PALETTED(Pal1, Any16);
DECLARE_PALETTED(Pal1, Any24);
DECLARE_PALETTED(Pal1, Any32);
DECLARE_PALETTED(Pal2, Any8);
DECLARE_PALETTED(Pal2, Any16);
DECLARE_PALETTED(Pal2, Any24);
DECLARE_PALETTED(Pal2, Any32);
DECLARE_PALETTED(Pal4, Any8);
DECLARE_PALETTED(Pal4, Any16);
DECLARE_PALETTED(Pal4, Any24);
DECLARE_PALETTED(Pal4, Any32);
DECLARE_PALETTED(Pal8, Any8);
DECLARE_PALETTED(Pal8, Any16);
DECLARE_PALETTED(Pal8, Any24);
DECLARE_PALETTED(Pal8, Any32);

DECLARE_YUV(XVYU, UYVY);
DECLARE_YUV(XVYU, YUYV);
DECLARE_YUV(Y8, UYVY);
DECLARE_YUV(Y8, YUYV);
DECLARE_YUV(UYVY, Y8);
DECLARE_YUV(YUYV, Y8);
DECLARE_YUV(UYVY, YUYV);
DECLARE_YUV_PLANAR(YUV411, YV12);

DECLARE_YUV(UYVY, XRGB1555);
DECLARE_YUV(UYVY, RGB565);
DECLARE_YUV(UYVY, RGB888);
DECLARE_YUV(UYVY, XRGB8888);
DECLARE_YUV(YUYV, XRGB1555);
DECLARE_YUV(YUYV, RGB565);
DECLARE_YUV(YUYV, RGB888);
DECLARE_YUV(YUYV, XRGB8888);
DECLARE_YUV(Y8, XRGB1555);
DECLARE_YUV(Y8, RGB565);
DECLARE_YUV(Y8, RGB888);
DECLARE_YUV(Y8, XRGB8888);

DECLARE_YUV_REV(XRGB1555, Y8);
DECLARE_YUV_REV(RGB565,   Y8);
DECLARE_YUV_REV(RGB888,   Y8);
DECLARE_YUV_REV(XRGB8888, Y8);

DECLARE_YUV_REV(XRGB1555, XVYU);
DECLARE_YUV_REV(RGB565,   XVYU);
DECLARE_YUV_REV(RGB888,   XVYU);
DECLARE_YUV_REV(XRGB8888, XVYU);

DECLARE_YUV_PLANAR(YV12, XRGB1555);
DECLARE_YUV_PLANAR(YV12, RGB565);
DECLARE_YUV_PLANAR(YV12, RGB888);
DECLARE_YUV_PLANAR(YV12, XRGB8888);

DECLARE_YUV_PLANAR(YUV411, XRGB1555);
DECLARE_YUV_PLANAR(YUV411, RGB565);
DECLARE_YUV_PLANAR(YUV411, RGB888);
DECLARE_YUV_PLANAR(YUV411, XRGB8888);

extern void VDPixmapBlt_YUVPlanar_decode_reference(const VDPixmap& dst, const VDPixmap& src, vdpixsize w, vdpixsize h);
extern void VDPixmapBlt_YUVPlanar_encode_reference(const VDPixmap& dst, const VDPixmap& src, vdpixsize w, vdpixsize h);
extern void VDPixmapBlt_YUVPlanar_convert_reference(const VDPixmap& dst, const VDPixmap& src, vdpixsize w, vdpixsize h);

using namespace nsVDPixmap;

tpVDPixBltTable VDGetPixBltTableX86Scalar() {
	static void *sReferenceMap[kPixFormat_Max_Standard][kPixFormat_Max_Standard] = {0};

	sReferenceMap[kPixFormat_Pal1][kPixFormat_Y8      ] = VDPixmapBlt_Pal1_to_Any8_reference;
	sReferenceMap[kPixFormat_Pal1][kPixFormat_XRGB1555] = VDPixmapBlt_Pal1_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal1][kPixFormat_RGB565  ] = VDPixmapBlt_Pal1_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal1][kPixFormat_RGB888  ] = VDPixmapBlt_Pal1_to_Any24_reference;
	sReferenceMap[kPixFormat_Pal1][kPixFormat_XRGB8888] = VDPixmapBlt_Pal1_to_Any32_reference;
	sReferenceMap[kPixFormat_Pal2][kPixFormat_Y8      ] = VDPixmapBlt_Pal2_to_Any8_reference;
	sReferenceMap[kPixFormat_Pal2][kPixFormat_XRGB1555] = VDPixmapBlt_Pal2_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal2][kPixFormat_RGB565  ] = VDPixmapBlt_Pal2_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal2][kPixFormat_RGB888  ] = VDPixmapBlt_Pal2_to_Any24_reference;
	sReferenceMap[kPixFormat_Pal2][kPixFormat_XRGB8888] = VDPixmapBlt_Pal2_to_Any32_reference;
	sReferenceMap[kPixFormat_Pal4][kPixFormat_Y8      ] = VDPixmapBlt_Pal4_to_Any8_reference;
	sReferenceMap[kPixFormat_Pal4][kPixFormat_XRGB1555] = VDPixmapBlt_Pal4_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal4][kPixFormat_RGB565  ] = VDPixmapBlt_Pal4_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal4][kPixFormat_RGB888  ] = VDPixmapBlt_Pal4_to_Any24_reference;
	sReferenceMap[kPixFormat_Pal4][kPixFormat_XRGB8888] = VDPixmapBlt_Pal4_to_Any32_reference;
	sReferenceMap[kPixFormat_Pal8][kPixFormat_Y8      ] = VDPixmapBlt_Pal8_to_Any8_reference;
	sReferenceMap[kPixFormat_Pal8][kPixFormat_XRGB1555] = VDPixmapBlt_Pal8_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal8][kPixFormat_RGB565  ] = VDPixmapBlt_Pal8_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal8][kPixFormat_RGB888  ] = VDPixmapBlt_Pal8_to_Any24_reference;
	sReferenceMap[kPixFormat_Pal8][kPixFormat_XRGB8888] = VDPixmapBlt_Pal8_to_Any32_reference;

	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_RGB565  ] = vdasm_pixblt_XRGB1555_to_RGB565;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_RGB888  ] = VDPixmapBlt_XRGB1555_to_RGB888_reference;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_XRGB8888] = vdasm_pixblt_XRGB1555_to_XRGB8888;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_XRGB1555] = vdasm_pixblt_RGB565_to_XRGB1555;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_RGB888  ] = VDPixmapBlt_RGB565_to_RGB888_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_XRGB8888] = vdasm_pixblt_RGB565_to_XRGB8888;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_XRGB1555] = vdasm_pixblt_RGB888_to_XRGB1555;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_RGB565  ] = vdasm_pixblt_RGB888_to_RGB565;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_XRGB8888] = vdasm_pixblt_RGB888_to_XRGB8888;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_XRGB1555] = vdasm_pixblt_XRGB8888_to_XRGB1555;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_RGB565  ] = vdasm_pixblt_XRGB8888_to_RGB565;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_RGB888  ] = vdasm_pixblt_XRGB8888_to_RGB888;

	sReferenceMap[kPixFormat_YUV444_XVYU][kPixFormat_YUV422_UYVY] = VDPixmapBlt_XVYU_to_UYVY_reference;
	sReferenceMap[kPixFormat_YUV444_XVYU][kPixFormat_YUV422_YUYV] = VDPixmapBlt_XVYU_to_YUYV_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV422_UYVY] = VDPixmapBlt_Y8_to_UYVY_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV422_YUYV] = VDPixmapBlt_Y8_to_YUYV_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_Y8] = VDPixmapBlt_UYVY_to_Y8_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_Y8] = VDPixmapBlt_YUYV_to_Y8_reference;

	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_XRGB1555] = VDPixmapBlt_UYVY_to_XRGB1555_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_RGB565  ] = VDPixmapBlt_UYVY_to_RGB565_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_RGB888  ] = VDPixmapBlt_UYVY_to_RGB888_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_XRGB8888] = VDPixmapBlt_UYVY_to_XRGB8888_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_XRGB1555] = VDPixmapBlt_YUYV_to_XRGB1555_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_RGB565  ] = VDPixmapBlt_YUYV_to_RGB565_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_RGB888  ] = VDPixmapBlt_YUYV_to_RGB888_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_XRGB8888] = VDPixmapBlt_YUYV_to_XRGB8888_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_XRGB1555] = VDPixmapBlt_Y8_to_XRGB1555_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_RGB565  ] = VDPixmapBlt_Y8_to_RGB565_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_RGB888  ] = VDPixmapBlt_Y8_to_RGB888_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_XRGB8888] = VDPixmapBlt_Y8_to_XRGB8888_reference;

	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_YUV444_XVYU] = VDPixmapBlt_XRGB1555_to_XVYU_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_YUV444_XVYU] = VDPixmapBlt_RGB565_to_XVYU_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_YUV444_XVYU] = VDPixmapBlt_RGB888_to_XVYU_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_YUV444_XVYU] = VDPixmapBlt_XRGB8888_to_XVYU_reference;

	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_Y8] = VDPixmapBlt_XRGB1555_to_Y8_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_Y8] = VDPixmapBlt_RGB565_to_Y8_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_Y8] = VDPixmapBlt_RGB888_to_Y8_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_Y8] = VDPixmapBlt_XRGB8888_to_Y8_reference;

#if 0
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YV12_to_XRGB1555_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YV12_to_RGB565_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YV12_to_RGB888_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YV12_to_XRGB8888_reference;

	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YUV411_to_XRGB1555_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YUV411_to_RGB565_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YUV411_to_RGB888_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YUV411_to_XRGB8888_reference;
#else
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_YUV422_UYVY] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_YUV422_YUYV] = VDPixmapBlt_YUVPlanar_decode_reference;

	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_YUV422_UYVY] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_YUV422_YUYV] = VDPixmapBlt_YUVPlanar_decode_reference;

	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_YUV422_UYVY] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_YUV422_YUYV] = VDPixmapBlt_YUVPlanar_decode_reference;

	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV422_UYVY] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV422_YUYV] = VDPixmapBlt_YUVPlanar_decode_reference;

	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_YUV422_UYVY] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_YUV422_YUYV] = VDPixmapBlt_YUVPlanar_decode_reference;
#endif

	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUV411_to_YV12_reference;

	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_YUV422_YUYV] = VDPixmapBlt_UYVY_to_YUYV_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_YUV422_UYVY] = VDPixmapBlt_UYVY_to_YUYV_reference;		// not an error -- same routine

	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;

	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_Y8           ] = VDPixmapBlt_YUVPlanar_convert_reference;

	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_Y8           ] = VDPixmapBlt_YUVPlanar_convert_reference;

	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_Y8           ] = VDPixmapBlt_YUVPlanar_convert_reference;

	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_Y8           ] = VDPixmapBlt_YUVPlanar_convert_reference;

	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_Y8           ] = VDPixmapBlt_YUVPlanar_convert_reference;

	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;

	return sReferenceMap;
}

tpVDPixBltTable VDGetPixBltTableX86MMX() {
	static void *sReferenceMap[kPixFormat_Max_Standard][kPixFormat_Max_Standard] = {0};

	sReferenceMap[kPixFormat_Pal1][kPixFormat_Y8      ] = VDPixmapBlt_Pal1_to_Any8_reference;
	sReferenceMap[kPixFormat_Pal1][kPixFormat_XRGB1555] = VDPixmapBlt_Pal1_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal1][kPixFormat_RGB565  ] = VDPixmapBlt_Pal1_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal1][kPixFormat_RGB888  ] = VDPixmapBlt_Pal1_to_Any24_reference;
	sReferenceMap[kPixFormat_Pal1][kPixFormat_XRGB8888] = VDPixmapBlt_Pal1_to_Any32_reference;
	sReferenceMap[kPixFormat_Pal2][kPixFormat_Y8      ] = VDPixmapBlt_Pal2_to_Any8_reference;
	sReferenceMap[kPixFormat_Pal2][kPixFormat_XRGB1555] = VDPixmapBlt_Pal2_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal2][kPixFormat_RGB565  ] = VDPixmapBlt_Pal2_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal2][kPixFormat_RGB888  ] = VDPixmapBlt_Pal2_to_Any24_reference;
	sReferenceMap[kPixFormat_Pal2][kPixFormat_XRGB8888] = VDPixmapBlt_Pal2_to_Any32_reference;
	sReferenceMap[kPixFormat_Pal4][kPixFormat_Y8      ] = VDPixmapBlt_Pal4_to_Any8_reference;
	sReferenceMap[kPixFormat_Pal4][kPixFormat_XRGB1555] = VDPixmapBlt_Pal4_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal4][kPixFormat_RGB565  ] = VDPixmapBlt_Pal4_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal4][kPixFormat_RGB888  ] = VDPixmapBlt_Pal4_to_Any24_reference;
	sReferenceMap[kPixFormat_Pal4][kPixFormat_XRGB8888] = VDPixmapBlt_Pal4_to_Any32_reference;
	sReferenceMap[kPixFormat_Pal8][kPixFormat_Y8      ] = VDPixmapBlt_Pal8_to_Any8_reference;
	sReferenceMap[kPixFormat_Pal8][kPixFormat_XRGB1555] = VDPixmapBlt_Pal8_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal8][kPixFormat_RGB565  ] = VDPixmapBlt_Pal8_to_Any16_reference;
	sReferenceMap[kPixFormat_Pal8][kPixFormat_RGB888  ] = VDPixmapBlt_Pal8_to_Any24_reference;
	sReferenceMap[kPixFormat_Pal8][kPixFormat_XRGB8888] = VDPixmapBlt_Pal8_to_Any32_reference;

	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_RGB565  ] = vdasm_pixblt_XRGB1555_to_RGB565_MMX;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_RGB888  ] = VDPixmapBlt_XRGB1555_to_RGB888_reference;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_XRGB8888] = vdasm_pixblt_XRGB1555_to_XRGB8888_MMX;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_XRGB1555] = vdasm_pixblt_RGB565_to_XRGB1555_MMX;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_RGB888  ] = VDPixmapBlt_RGB565_to_RGB888_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_XRGB8888] = vdasm_pixblt_RGB565_to_XRGB8888_MMX;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_XRGB1555] = vdasm_pixblt_RGB888_to_XRGB1555;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_RGB565  ] = vdasm_pixblt_RGB888_to_RGB565;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_XRGB8888] = vdasm_pixblt_RGB888_to_XRGB8888_MMX;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_XRGB1555] = vdasm_pixblt_XRGB8888_to_XRGB1555_MMX;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_RGB565  ] = vdasm_pixblt_XRGB8888_to_RGB565_MMX;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_RGB888  ] = vdasm_pixblt_XRGB8888_to_RGB888_MMX;

	sReferenceMap[kPixFormat_YUV444_XVYU][kPixFormat_YUV422_UYVY] = VDPixmapBlt_XVYU_to_UYVY_reference;
	sReferenceMap[kPixFormat_YUV444_XVYU][kPixFormat_YUV422_YUYV] = VDPixmapBlt_XVYU_to_YUYV_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV422_UYVY] = VDPixmapBlt_Y8_to_UYVY_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV422_YUYV] = VDPixmapBlt_Y8_to_YUYV_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_Y8] = VDPixmapBlt_UYVY_to_Y8_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_Y8] = VDPixmapBlt_YUYV_to_Y8_reference;

	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_XRGB1555] = VDPixmapBlt_UYVY_to_XRGB1555_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_RGB565  ] = VDPixmapBlt_UYVY_to_RGB565_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_RGB888  ] = VDPixmapBlt_UYVY_to_RGB888_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_XRGB8888] = VDPixmapBlt_UYVY_to_XRGB8888_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_XRGB1555] = VDPixmapBlt_YUYV_to_XRGB1555_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_RGB565  ] = VDPixmapBlt_YUYV_to_RGB565_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_RGB888  ] = VDPixmapBlt_YUYV_to_RGB888_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_XRGB8888] = VDPixmapBlt_YUYV_to_XRGB8888_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_XRGB1555] = VDPixmapBlt_Y8_to_XRGB1555_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_RGB565  ] = VDPixmapBlt_Y8_to_RGB565_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_RGB888  ] = VDPixmapBlt_Y8_to_RGB888_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_XRGB8888] = VDPixmapBlt_Y8_to_XRGB8888_reference;

	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_YUV444_XVYU] = VDPixmapBlt_XRGB1555_to_XVYU_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_YUV444_XVYU] = VDPixmapBlt_RGB565_to_XVYU_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_YUV444_XVYU] = VDPixmapBlt_RGB888_to_XVYU_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_YUV444_XVYU] = VDPixmapBlt_XRGB8888_to_XVYU_reference;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_Y8] = VDPixmapBlt_XRGB1555_to_Y8_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_Y8] = VDPixmapBlt_RGB565_to_Y8_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_Y8] = VDPixmapBlt_RGB888_to_Y8_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_Y8] = VDPixmapBlt_XRGB8888_to_Y8_reference;

#if 0
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YV12_to_XRGB1555_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YV12_to_RGB565_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YV12_to_RGB888_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YV12_to_XRGB8888_reference;

	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YUV411_to_XRGB1555_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YUV411_to_RGB565_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YUV411_to_RGB888_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YUV411_to_XRGB8888_reference;
#else
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_YUV422_UYVY] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_YUV422_YUYV] = VDPixmapBlt_YUVPlanar_decode_reference;

	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_YUV422_UYVY] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_YUV422_YUYV] = VDPixmapBlt_YUVPlanar_decode_reference;

	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_YUV422_UYVY] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_YUV422_YUYV] = VDPixmapBlt_YUVPlanar_decode_reference;

	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV422_UYVY] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV422_YUYV] = VDPixmapBlt_YUVPlanar_decode_reference;

	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_XRGB1555] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_RGB565  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_RGB888  ] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_XRGB8888] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_YUV422_UYVY] = VDPixmapBlt_YUVPlanar_decode_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_YUV422_YUYV] = VDPixmapBlt_YUVPlanar_decode_reference;
#endif
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUV411_to_YV12_reference;

	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_YUV422_YUYV] = VDPixmapBlt_UYVY_to_YUYV_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_YUV422_UYVY] = VDPixmapBlt_UYVY_to_YUYV_reference;		// not an error -- same routine

	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB1555][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB565  ][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_RGB888  ][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_XRGB8888][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_UYVY][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;
	sReferenceMap[kPixFormat_YUV422_YUYV][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_encode_reference;

	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV444_Planar][kPixFormat_Y8           ] = VDPixmapBlt_YUVPlanar_convert_reference;

	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV422_Planar][kPixFormat_Y8           ] = VDPixmapBlt_YUVPlanar_convert_reference;

	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV420_Planar][kPixFormat_Y8           ] = VDPixmapBlt_YUVPlanar_convert_reference;

	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV411_Planar][kPixFormat_Y8           ] = VDPixmapBlt_YUVPlanar_convert_reference;

	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_YUV410_Planar][kPixFormat_Y8           ] = VDPixmapBlt_YUVPlanar_convert_reference;

	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV444_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV422_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV420_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV411_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;
	sReferenceMap[kPixFormat_Y8][kPixFormat_YUV410_Planar] = VDPixmapBlt_YUVPlanar_convert_reference;

	return sReferenceMap;
}
