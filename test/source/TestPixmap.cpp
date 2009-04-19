#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/system/memory.h>
#include <tchar.h>
#include "test.h"

DEFINE_TEST(Pixmap) {
	using namespace nsVDPixmap;

	// test pal1
	for(int format=kPixFormat_Pal1; format<=kPixFormat_Pal8; ++format) {

		_tprintf(_T("    Testing format %hs\n"), VDPixmapGetInfo(format).name);

		int testw = 2048 >> (format - kPixFormat_Pal1);
		int teststep = 8 >> (format - kPixFormat_Pal1);

		VDPixmapBuffer srcbuf(testw, 2, format);

		int palcount = 1 << (1 << (format - kPixFormat_Pal1));
		for(int k=0; k<palcount; ++k) {
			uint32 v = 0;

			if (k & 1)
				v |= 0x000000ff;
			if (k & 2)
				v |= 0x0000ff00;
			if (k & 4)
				v |= 0x00ff0000;
			if (k & 8)
				v |= 0xff000000;

			((uint32 *)srcbuf.palette)[k] = v;
		}

		for(int q=0; q<256; ++q)
			((uint8 *)srcbuf.data)[q] = ((uint8 *)srcbuf.data)[srcbuf.pitch + q] = (uint8)q;

		VDInvertMemory(vdptroffset(srcbuf.data, srcbuf.pitch), 256);

		VDPixmapBuffer intbuf[4];
		
		intbuf[0].init(testw, 2, kPixFormat_XRGB1555);
		intbuf[1].init(testw, 2, kPixFormat_RGB565);
		intbuf[2].init(testw, 2, kPixFormat_RGB888);
		intbuf[3].init(testw, 2, kPixFormat_XRGB8888);

		VDPixmapBuffer dstbuf(testw, 2, kPixFormat_RGB888);

		for(int x1=0; x1<testw; x1+=teststep) {
			int xlimit = std::min<int>(testw, x1+64);
			for(int x2=x1+8; x2<xlimit; x2+=teststep) {
				for(int i=0; i<4; ++i) {
					VDMemset8Rect(intbuf[i].data, intbuf[i].pitch, 0, intbuf[i].w * VDPixmapGetInfo(intbuf[i].format).qsize, intbuf[i].h);
					VDVERIFY(VDPixmapBlt(intbuf[i], x1, 0, srcbuf, x1, 0, x2-x1, 2));
				}

				for(int j=0; j<3; ++j) {
					VDMemset8Rect(dstbuf.data, dstbuf.pitch, 0, 3*dstbuf.w, dstbuf.h);
					VDVERIFY(VDPixmapBlt(dstbuf, intbuf[j]));

					VDVERIFY(!VDCompareRect(intbuf[2].data, intbuf[2].pitch, dstbuf.data, dstbuf.pitch, 3*dstbuf.w, dstbuf.h));
				}
			}
		}

	}

	// test primary color conversion
	for(int size=7; size<=9; ++size) {
		VDPixmapBuffer src(9, 9, kPixFormat_XRGB8888);
		VDPixmapBuffer output(9, 9, kPixFormat_XRGB8888);

		for(int srcformat = nsVDPixmap::kPixFormat_XRGB1555; srcformat < nsVDPixmap::kPixFormat_Max_Standard; ++srcformat) {
			if (srcformat == kPixFormat_YUV444_XVYU)
				continue;

			for(int dstformat = nsVDPixmap::kPixFormat_XRGB1555; dstformat < nsVDPixmap::kPixFormat_Max_Standard; ++dstformat) {
				if (dstformat == kPixFormat_YUV444_XVYU)
					continue;

				VDPixmap srccrop(src);
				srccrop.w = size;
				srccrop.h = size;

				VDPixmapBuffer in(size, size, srcformat);
				VDPixmapBuffer out(size, size, dstformat);

				static const uint32 kColors[8]={
					0xff000000,
					0xffffffff,
					0xff0000ff,
					0xff00ff00,
					0xff00ffff,
					0xffff0000,
					0xffff00ff,
					0xffffff00,
				};

				int maxtest = (srcformat == kPixFormat_Y8 || dstformat == kPixFormat_Y8) ? 2 : 8;

				for(int v=0; v<maxtest; ++v) {
					VDMemset32Rect(src.data, src.pitch, kColors[v], size, size);

					VDVERIFY(VDPixmapBlt(in, srccrop));
					VDVERIFY(VDPixmapBlt(out, in));
					VDVERIFY(VDPixmapBlt(output, out));

					for(int y=0; y<size; ++y) {
						const uint32 *sp = (const uint32 *)vdptroffset(src.data, src.pitch*y);
						uint32 *dp = (uint32 *)vdptroffset(output.data, output.pitch*y);

						for(int x=0; x<size; ++x) {
							const uint32 spx = sp[x];
							const uint32 dpx = dp[x];

							const int re = (int)((spx>>16)&0xff) - (int)((dpx>>16)&0xff);
							const int ge = (int)((spx>> 8)&0xff) - (int)((dpx>> 8)&0xff);
							const int be = (int)((spx    )&0xff) - (int)((dpx    )&0xff);

							if (abs(re) > 1 || abs(ge) > 1 || abs(be) > 1) {
								printf("        Failed: %s -> %s\n", VDPixmapGetInfo(srcformat).name, VDPixmapGetInfo(dstformat).name);
								printf("            (%d,%d) %08lx != %08lx\n", x, y, spx, dpx);
								VDASSERT(false);
								goto failed;
							}
						}
					}
	failed:;
				}
			}
		}
	}
	return 0;
}

