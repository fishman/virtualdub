#include "test.h"
#include <vd2/system/vdalloc.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "../../Kasumi/h/uberblit.h"

DEFINE_TEST(Uberblit) {
	using namespace nsVDPixmap;

	// test primary color conversion
	const int size = 8;
	VDPixmapBuffer src(size, size, kPixFormat_XRGB8888);
	VDPixmapBuffer output(size, size, kPixFormat_XRGB8888);

	for(int srcformat = nsVDPixmap::kPixFormat_XRGB1555; srcformat < nsVDPixmap::kPixFormat_Max_Standard; ++srcformat) {
		if (srcformat == kPixFormat_YUV444_XVYU)
			continue;

		for(int dstformat = nsVDPixmap::kPixFormat_XRGB1555; dstformat < nsVDPixmap::kPixFormat_Max_Standard; ++dstformat) {
			if (dstformat == kPixFormat_YUV444_XVYU)
				continue;

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

			vdautoptr<IVDPixmapBlitter> blit1(VDPixmapCreateBlitter(in, src));
			vdautoptr<IVDPixmapBlitter> blit2(VDPixmapCreateBlitter(out, in));
			vdautoptr<IVDPixmapBlitter> blit3(VDPixmapCreateBlitter(output, out));

			for(int v=0; v<maxtest; ++v) {
				VDMemset32Rect(src.data, src.pitch, kColors[v], size, size);

				blit1->Blit(in, src);
				in.validate();
				blit2->Blit(out, in);
				out.validate();
				blit3->Blit(output, out);
				output.validate();

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
	return 0;
}
