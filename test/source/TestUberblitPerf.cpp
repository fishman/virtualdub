#include "test.h"
#include <intrin.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/time.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "../../Kasumi/h/uberblit.h"

DEFINE_TEST_NONAUTO(UberblitPerf) {
	static const int kFormats[]={
		nsVDPixmap::kPixFormat_Pal1,
		nsVDPixmap::kPixFormat_Pal2,
		nsVDPixmap::kPixFormat_Pal4,
		nsVDPixmap::kPixFormat_Pal8,
		nsVDPixmap::kPixFormat_XRGB1555,
		nsVDPixmap::kPixFormat_RGB565,
		nsVDPixmap::kPixFormat_RGB888,
		nsVDPixmap::kPixFormat_XRGB8888,
		nsVDPixmap::kPixFormat_Y8,
		nsVDPixmap::kPixFormat_YUV422_UYVY,
		nsVDPixmap::kPixFormat_YUV422_YUYV,
		nsVDPixmap::kPixFormat_YUV444_Planar,
		nsVDPixmap::kPixFormat_YUV422_Planar,
		nsVDPixmap::kPixFormat_YUV420_Planar,
		nsVDPixmap::kPixFormat_YUV411_Planar,
		nsVDPixmap::kPixFormat_YUV410_Planar,
	};

	enum {
#if 1
		kSrcStart = 4,
		kDstStart = 4,
#else
		kSrcStart = 9,
		kDstStart = 7,
#endif
		kFormatCount = sizeof(kFormats)/sizeof(kFormats[0])
	};

	vdautoarrayptr<char> dstbuf(new char[65536*4]);
	vdautoarrayptr<char> srcbuf(new char[65536*4]);
	uint32 palette[256]={0};

	VDPixmap srcPixmaps[kFormatCount];

	for(int srcformat = kSrcStart; srcformat < kFormatCount; ++srcformat) {
		VDPixmapLayout layout;
		VDPixmapCreateLinearLayout(layout, kFormats[srcformat], 256, 256, 16);
		srcPixmaps[srcformat] = VDPixmapFromLayout(layout, srcbuf.get());
		srcPixmaps[srcformat].palette = palette;
	}

	for(int dstformat = kDstStart; dstformat < kFormatCount; ++dstformat) {
		VDPixmapLayout layout;
		VDPixmapCreateLinearLayout(layout, kFormats[dstformat], 256, 256, 16);
		VDPixmap dstPixmap = VDPixmapFromLayout(layout, srcbuf.get());
		dstPixmap.palette = palette;

		const char *dstName = VDPixmapGetInfo(kFormats[dstformat]).name;

		for(int srcformat = kSrcStart; srcformat < kFormatCount; ++srcformat) {
			const VDPixmap& srcPixmap = srcPixmaps[srcformat];
			const char *srcName = VDPixmapGetInfo(kFormats[srcformat]).name;

			uint64 bltTime = (uint64)(sint64)-1;
			for(int i=0; i<10; ++i) {
				uint64 t1 = VDGetPreciseTick();			
				VDPixmapBlt(dstPixmap, srcPixmap);
				t1 = VDGetPreciseTick() - t1;

				if (bltTime > t1)
					bltTime = t1;
			}

			vdautoptr<IVDPixmapBlitter> blitter(VDPixmapCreateBlitter(dstPixmap, srcPixmap));

			uint64 uberTime = (uint64)(sint64)-1;
//			for(;;){
			for(int i=0; i<10; ++i) {
				uint64 t1 = VDGetPreciseTick();
				blitter->Blit(dstPixmap, srcPixmap);
				t1 = VDGetPreciseTick() - t1;

				if (uberTime > t1)
					uberTime = t1;
			}
//			}

			double bltSpeed = 256.0 * 256.0 / 1000000.0 / (double)bltTime * VDGetPreciseTicksPerSecond();
			double uberSpeed = 256.0 * 256.0 / 1000000.0 / (double)uberTime * VDGetPreciseTicksPerSecond();

			printf("%-20s %-20s %8.2fMP/sec %8.2fMP/sec\n", dstName, srcName, bltSpeed, uberSpeed);
		}
	}

	return 0;
}
