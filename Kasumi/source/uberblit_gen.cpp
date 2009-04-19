#include <vd2/system/vdalloc.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "uberblit.h"
#include "uberblit_gen.h"
#include "uberblit_fill.h"
#include "uberblit_input.h"
#include "uberblit_resample.h"
#include "uberblit_ycbcr.h"
#include "uberblit_rgb.h"
#include "uberblit_pal.h"

#ifdef VD_CPU_X86
	#include "uberblit_ycbcr_x86.h"
	#include "uberblit_rgb_x86.h"
#endif

void VDPixmapGenerate(void *dst, ptrdiff_t pitch, sint32 bpr, sint32 height, IVDPixmapGen *gen, int genIndex) {
	for(sint32 y=0; y<height; ++y) {
		memcpy(dst, gen->GetRow(y, genIndex), bpr);
		vdptrstep(dst, pitch);
	}
	VDCPUCleanupExtensions();
}

void VDPixmapGenerateFast(void *dst, ptrdiff_t pitch, sint32 height, IVDPixmapGen *gen) {
	for(sint32 y=0; y<height; ++y) {
		gen->ProcessRow(dst, y);
		vdptrstep(dst, pitch);
	}
	VDCPUCleanupExtensions();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VDPixmapUberBlitter::VDPixmapUberBlitter() {
}

VDPixmapUberBlitter::~VDPixmapUberBlitter() {
	while(!mGenerators.empty()) {
		delete mGenerators.back();
		mGenerators.pop_back();
	}
}

void VDPixmapUberBlitter::Blit(const VDPixmap& dst, const VDPixmap& src) {
	Blit(dst, NULL, src);
}

void VDPixmapUberBlitter::Blit(const VDPixmap& dst, const vdrect32 *rDst, const VDPixmap& src) {
	for(Sources::const_iterator it(mSources.begin()), itEnd(mSources.end()); it!=itEnd; ++it) {
		const SourceEntry& se = *it;
		const void *p;
		ptrdiff_t pitch;

		switch(se.mSrcPlane) {
			case 0:
				p = src.data;
				pitch = src.pitch;
				break;
			case 1:
				p = src.data2;
				pitch = src.pitch2;
				break;
			case 2:
				p = src.data3;
				pitch = src.pitch3;
				break;
			default:
				VDASSERT(false);
				break;
		}

		se.mpSrc->SetSource((const char *)p + pitch*se.mSrcY + se.mSrcX, pitch, src.palette);
	}

	if (mOutputs[2].mpSrc)
		Blit3(dst, rDst);
	else
		Blit(dst, rDst);
}

void VDPixmapUberBlitter::Blit(const VDPixmap& dst, const vdrect32 *rDst) {
	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(dst.format);

	mOutputs[0].mpSrc->AddWindowRequest(0, 0);
	mOutputs[0].mpSrc->Start();

	void *p = dst.data;
	int w = -(-dst.w >> formatInfo.qwbits);
	int h = -(-dst.h >> formatInfo.qhbits);

	if (rDst) {
		int x1 = rDst->left;
		int y1 = rDst->top;
		int x2 = rDst->right;
		int y2 = rDst->bottom;

		VDASSERT(x1 >= 0 && y1 >= 0 && x2 <= w && y2 <= h && x2 >= x1 && y2 >= y1);

		if (x2 < x1 || y2 < y1)
			return;

		p = vdptroffset(dst.data, dst.pitch * y1 + x1 * formatInfo.qsize);
		w = x2 - x1;
		h = y2 - y1;
	}

	uint32 bpr = formatInfo.qsize * w;

	if (mOutputs[0].mSrcIndex == 0)
		VDPixmapGenerateFast(p, dst.pitch, h, mOutputs[0].mpSrc);
	else
		VDPixmapGenerate(p, dst.pitch, bpr, h, mOutputs[0].mpSrc, mOutputs[0].mSrcIndex);
}

void VDPixmapUberBlitter::Blit3(const VDPixmap& px, const vdrect32 *rDst) {
	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(px.format);
	IVDPixmapGen *gen = mOutputs[1].mpSrc;
	int idx = mOutputs[1].mSrcIndex;
	IVDPixmapGen *gen1 = mOutputs[2].mpSrc;
	int idx1 = mOutputs[2].mSrcIndex;
	IVDPixmapGen *gen2 = mOutputs[0].mpSrc;
	int idx2 = mOutputs[0].mSrcIndex;

	gen->AddWindowRequest(0, 0);
	gen->Start();
	gen1->AddWindowRequest(0, 0);
	gen1->Start();
	gen2->AddWindowRequest(0, 0);
	gen2->Start();

	uint32 auxstep = 0x80000000UL >> formatInfo.auxhbits;
	uint32 auxaccum = 0;

	auxstep += auxstep;

	uint32 height = -(-px.h >> formatInfo.qhbits);
	uint32 bpr = formatInfo.qsize * -(-px.w >> formatInfo.qwbits);
	uint32 bpr2 = -(-px.w >> formatInfo.auxwbits);
	uint8 *dst = (uint8 *)px.data;
	uint8 *dst2 = (uint8 *)px.data2;
	uint8 *dst3 = (uint8 *)px.data3;
	ptrdiff_t pitch = px.pitch;
	ptrdiff_t pitch2 = px.pitch2;
	ptrdiff_t pitch3 = px.pitch3;
	uint32 y2 = 0;
	for(uint32 y=0; y<height; ++y) {
		memcpy(dst, gen->GetRow(y, idx), bpr);
		vdptrstep(dst, pitch);

		if (!auxaccum) {
			memcpy(dst2, gen1->GetRow(y2, idx1), bpr2);
			vdptrstep(dst2, pitch2);
			memcpy(dst3, gen2->GetRow(y2, idx2), bpr2);
			vdptrstep(dst3, pitch3);
			++y2;
		}

		auxaccum += auxstep;
	}

	VDCPUCleanupExtensions();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
VDPixmapUberBlitterGenerator::VDPixmapUberBlitterGenerator() {
}

VDPixmapUberBlitterGenerator::~VDPixmapUberBlitterGenerator() {
	while(!mGenerators.empty()) {
		delete mGenerators.back();
		mGenerators.pop_back();
	}
}

void VDPixmapUberBlitterGenerator::swap(int index) {
	std::swap(mStack.back(), (&mStack.back())[-index]);
}

void VDPixmapUberBlitterGenerator::dup() {
	mStack.push_back(mStack.back());
}

void VDPixmapUberBlitterGenerator::pop() {
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::ldsrc(int srcIndex, int srcPlane, int x, int y, uint32 w, uint32 h, uint32 type, uint32 bpr) {
	VDPixmapGenSrc *src = new VDPixmapGenSrc;

	src->Init(w, h, type, bpr);

	mGenerators.push_back(src);
	mStack.push_back(StackEntry(src, 0));

	SourceEntry se;
	se.mpSrc = src;
	se.mSrcIndex = srcIndex;
	se.mSrcPlane = srcPlane;
	se.mSrcX = x;
	se.mSrcY = y;
	mSources.push_back(se);
}

void VDPixmapUberBlitterGenerator::ldconst(uint8 fill, uint32 bpr, uint32 w, uint32 h, uint32 type) {
	VDPixmapGenFill8 *src = new VDPixmapGenFill8;

	src->Init(fill, bpr, w, h, type);

	mGenerators.push_back(src);
	mStack.push_back(StackEntry(src, 0));
}

void VDPixmapUberBlitterGenerator::extract_8in16(int offset, uint32 w, uint32 h) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_8In16 *src = new VDPixmapGen_8In16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, offset, w, h);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::extract_8in32(int offset, uint32 w, uint32 h) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_8In32 *src = new VDPixmapGen_8In32;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, offset, w, h);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::swap_8in16(uint32 w, uint32 h, uint32 bpr) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_Swap8In16 *src = new VDPixmapGen_Swap8In16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, w, h, bpr);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_Pal1_to_8888(int srcIndex) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_Pal1_To_X8R8G8B8 *src = new VDPixmapGen_Pal1_To_X8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	mStack.push_back(StackEntry(src, 0));

	SourceEntry se;
	se.mpSrc = src;
	se.mSrcIndex = srcIndex;
	se.mSrcPlane = 0;
	se.mSrcX = 0;
	se.mSrcY = 0;
	mSources.push_back(se);
}

void VDPixmapUberBlitterGenerator::conv_Pal2_to_8888(int srcIndex) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_Pal2_To_X8R8G8B8 *src = new VDPixmapGen_Pal2_To_X8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	mStack.push_back(StackEntry(src, 0));

	SourceEntry se;
	se.mpSrc = src;
	se.mSrcIndex = srcIndex;
	se.mSrcPlane = 0;
	se.mSrcX = 0;
	se.mSrcY = 0;
	mSources.push_back(se);
}

void VDPixmapUberBlitterGenerator::conv_Pal4_to_8888(int srcIndex) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_Pal4_To_X8R8G8B8 *src = new VDPixmapGen_Pal4_To_X8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	mStack.push_back(StackEntry(src, 0));

	SourceEntry se;
	se.mpSrc = src;
	se.mSrcIndex = srcIndex;
	se.mSrcPlane = 0;
	se.mSrcX = 0;
	se.mSrcY = 0;
	mSources.push_back(se);
}

void VDPixmapUberBlitterGenerator::conv_Pal8_to_8888(int srcIndex) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_Pal8_To_X8R8G8B8 *src = new VDPixmapGen_Pal8_To_X8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	mStack.push_back(StackEntry(src, 0));

	SourceEntry se;
	se.mpSrc = src;
	se.mSrcIndex = srcIndex;
	se.mSrcPlane = 0;
	se.mSrcX = 0;
	se.mSrcY = 0;
	mSources.push_back(se);
}

void VDPixmapUberBlitterGenerator::pointh(float xoffset, float xfactor, uint32 w) {
	StackEntry *args = &mStack.back();

	if (xoffset != 0.0f || xfactor != 1.0f) {
		VDPixmapGenResampleRow *src = new VDPixmapGenResampleRow;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, w, xoffset, xfactor, nsVDPixmap::kFilterPoint, 0, false);

		mGenerators.push_back(src);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::pointv(float yoffset, float yfactor, uint32 h) {
	StackEntry *args = &mStack.back();

	if (yoffset != 0.0f || yfactor != 1.0f) {
		VDPixmapGenResampleCol *src = new VDPixmapGenResampleCol;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, h, yoffset, yfactor, nsVDPixmap::kFilterPoint, 0, false);

		mGenerators.push_back(src);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::linearh(float xoffset, float xfactor, uint32 w, bool interpOnly) {
	StackEntry *args = &mStack.back();

	if (xoffset != 0.0f || xfactor != 1.0f) {
		VDPixmapGenResampleRow *src = new VDPixmapGenResampleRow;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, w, xoffset, xfactor, nsVDPixmap::kFilterLinear, 0, interpOnly);

		mGenerators.push_back(src);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::linearv(float yoffset, float yfactor, uint32 h, bool interpOnly) {
	StackEntry *args = &mStack.back();

	if (yoffset != 0.0f || yfactor != 1.0f) {
		VDPixmapGenResampleCol *src = new VDPixmapGenResampleCol;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, h, yoffset, yfactor, nsVDPixmap::kFilterLinear, 0, interpOnly);

		mGenerators.push_back(src);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::linear(float xoffset, float xfactor, uint32 w, float yoffset, float yfactor, uint32 h) {
	linearh(xoffset, xfactor, w, false);
	linearv(yoffset, yfactor, h, false);
}

void VDPixmapUberBlitterGenerator::cubich(float xoffset, float xfactor, uint32 w, float splineFactor, bool interpOnly) {
	StackEntry *args = &mStack.back();

	if (xoffset != 0.0f || xfactor != 1.0f) {
		VDPixmapGenResampleRow *src = new VDPixmapGenResampleRow;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, w, xoffset, xfactor, nsVDPixmap::kFilterCubic, splineFactor, interpOnly);

		mGenerators.push_back(src);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::cubicv(float yoffset, float yfactor, uint32 h, float splineFactor, bool interpOnly) {
	StackEntry *args = &mStack.back();

	if (yoffset != 0.0f || yfactor != 1.0f) {
		VDPixmapGenResampleCol *src = new VDPixmapGenResampleCol;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, h, yoffset, yfactor, nsVDPixmap::kFilterCubic, splineFactor, interpOnly);

		mGenerators.push_back(src);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::cubic(float xoffset, float xfactor, uint32 w, float yoffset, float yfactor, uint32 h, float splineFactor) {
	cubich(xoffset, xfactor, w, splineFactor, false);
	cubicv(yoffset, yfactor, h, splineFactor, false);
}

void VDPixmapUberBlitterGenerator::lanczos3h(float xoffset, float xfactor, uint32 w) {
	StackEntry *args = &mStack.back();

	if (xoffset != 0.0f || xfactor != 1.0f) {
		VDPixmapGenResampleRow *src = new VDPixmapGenResampleRow;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, w, xoffset, xfactor, nsVDPixmap::kFilterLanczos3, 0, false);

		mGenerators.push_back(src);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::lanczos3v(float yoffset, float yfactor, uint32 h) {
	StackEntry *args = &mStack.back();

	if (yoffset != 0.0f || yfactor != 1.0f) {
		VDPixmapGenResampleCol *src = new VDPixmapGenResampleCol;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, h, yoffset, yfactor, nsVDPixmap::kFilterLanczos3, 0, false);

		mGenerators.push_back(src);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::lanczos3(float xoffset, float xfactor, uint32 w, float yoffset, float yfactor, uint32 h) {
	lanczos3h(xoffset, xfactor, w);
	lanczos3v(yoffset, yfactor, h);
}

void VDPixmapUberBlitterGenerator::conv_555_to_8888() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_X1R5G5B5_To_X8R8G8B8 *src = MMX_enabled ? new VDPixmapGen_X1R5G5B5_To_X8R8G8B8_MMX : new VDPixmapGen_X1R5G5B5_To_X8R8G8B8;
#else
	VDPixmapGen_X1R5G5B5_To_X8R8G8B8 *src = new VDPixmapGen_X1R5G5B5_To_X8R8G8B8;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_565_to_8888() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_R5G6B5_To_X8R8G8B8 *src = MMX_enabled ? new VDPixmapGen_R5G6B5_To_X8R8G8B8_MMX : new VDPixmapGen_R5G6B5_To_X8R8G8B8;
#else
	VDPixmapGen_R5G6B5_To_X8R8G8B8 *src = new VDPixmapGen_R5G6B5_To_X8R8G8B8;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_888_to_8888() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_R8G8B8_To_A8R8G8B8 *src = MMX_enabled ? new VDPixmapGen_R8G8B8_To_X8R8G8B8_MMX : new VDPixmapGen_R8G8B8_To_A8R8G8B8;
#else
	VDPixmapGen_R8G8B8_To_A8R8G8B8 *src = new VDPixmapGen_R8G8B8_To_A8R8G8B8;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_8_to_32F() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_8_To_32F *src = new VDPixmapGen_8_To_32F;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_8888_to_X32F() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X8R8G8B8_To_X32B32G32R32F *src = new VDPixmapGen_X8R8G8B8_To_X32B32G32R32F;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_8888_to_555() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_X8R8G8B8_To_X1R5G5B5 *src = MMX_enabled ? new VDPixmapGen_X8R8G8B8_To_X1R5G5B5_MMX : new VDPixmapGen_X8R8G8B8_To_X1R5G5B5;
#else
	VDPixmapGen_X8R8G8B8_To_X1R5G5B5 *src = new VDPixmapGen_X8R8G8B8_To_X1R5G5B5;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_555_to_565() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_X1R5G5B5_To_R5G6B5 *src = MMX_enabled ? new VDPixmapGen_X1R5G5B5_To_R5G6B5_MMX : new VDPixmapGen_X1R5G5B5_To_R5G6B5;
#else
	VDPixmapGen_X1R5G5B5_To_R5G6B5 *src = new VDPixmapGen_X1R5G5B5_To_R5G6B5;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_565_to_555() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_R5G6B5_To_X1R5G5B5 *src = MMX_enabled ? new VDPixmapGen_R5G6B5_To_X1R5G5B5_MMX : new VDPixmapGen_R5G6B5_To_X1R5G5B5;
#else
	VDPixmapGen_R5G6B5_To_X1R5G5B5 *src = new VDPixmapGen_R5G6B5_To_X1R5G5B5;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_8888_to_565() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_X8R8G8B8_To_R5G6B5 *src = MMX_enabled ? new VDPixmapGen_X8R8G8B8_To_R5G6B5_MMX : new VDPixmapGen_X8R8G8B8_To_R5G6B5;
#else
	VDPixmapGen_X8R8G8B8_To_R5G6B5 *src = new VDPixmapGen_X8R8G8B8_To_R5G6B5;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_8888_to_888() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_X8R8G8B8_To_R8G8B8 *src = MMX_enabled ? new VDPixmapGen_X8R8G8B8_To_R8G8B8_MMX : new VDPixmapGen_X8R8G8B8_To_R8G8B8;
#else
	VDPixmapGen_X8R8G8B8_To_R8G8B8 *src = new VDPixmapGen_X8R8G8B8_To_R8G8B8;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_32F_to_8() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_32F_To_8 *src = new VDPixmapGen_32F_To_8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_X32F_to_8888() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X32B32G32R32F_To_X8R8G8B8 *src = new VDPixmapGen_X32B32G32R32F_To_X8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::convd_8888_to_555() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X8R8G8B8_To_X1R5G5B5_Dithered *src = new VDPixmapGen_X8R8G8B8_To_X1R5G5B5_Dithered;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::convd_8888_to_565() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X8R8G8B8_To_R5G6B5_Dithered *src = new VDPixmapGen_X8R8G8B8_To_R5G6B5_Dithered;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::convd_32F_to_8() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_32F_To_8_Dithered *src = new VDPixmapGen_32F_To_8_Dithered;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::convd_X32F_to_8888() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X32B32G32R32F_To_X8R8G8B8_Dithered *src = new VDPixmapGen_X32B32G32R32F_To_X8R8G8B8_Dithered;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::interleave_B8G8_R8G8() {
	StackEntry *args = &mStack.back() - 2;
	VDPixmapGen_B8x3_To_G8R8_G8B8 *src = new VDPixmapGen_B8x3_To_G8R8_G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::interleave_G8B8_G8R8() {
	StackEntry *args = &mStack.back() - 2;
	VDPixmapGen_B8x3_To_R8G8_B8G8 *src = new VDPixmapGen_B8x3_To_R8G8_B8G8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::interleave_X8R8G8B8() {
	StackEntry *args = &mStack.back() - 2;
	VDPixmapGen_B8x3_To_X8R8G8B8 *src = new VDPixmapGen_B8x3_To_X8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::ycbcr601_to_rgb32() {
	StackEntry *args = &mStack.back() - 2;

#ifdef VD_CPU_X86
	VDPixmapGenYCbCr601ToRGB32 *src = MMX_enabled ? new VDPixmapGenYCbCr601ToRGB32_MMX : new VDPixmapGenYCbCr601ToRGB32;
#else
	VDPixmapGenYCbCr601ToRGB32 *src = new VDPixmapGenYCbCr601ToRGB32;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::ycbcr709_to_rgb32() {
	StackEntry *args = &mStack.back() - 2;

	VDPixmapGenYCbCr709ToRGB32 *src = new VDPixmapGenYCbCr709ToRGB32;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::rgb32_to_ycbcr601() {
	StackEntry *args = &mStack.back();
	VDPixmapGenRGB32ToYCbCr601 *src = new VDPixmapGenRGB32ToYCbCr601;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::rgb32_to_ycbcr709() {
	StackEntry *args = &mStack.back();
	VDPixmapGenRGB32ToYCbCr709 *src = new VDPixmapGenRGB32ToYCbCr709;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

IVDPixmapBlitter *VDPixmapUberBlitterGenerator::create() {
	vdautoptr<VDPixmapUberBlitter> blitter(new VDPixmapUberBlitter);

	int numStackEntries = (int)mStack.size();

	for(int i=0; i<3; ++i) {
		if (i < numStackEntries) {
			blitter->mOutputs[i].mpSrc = mStack[i].mpSrc;
			blitter->mOutputs[i].mSrcIndex = mStack[i].mSrcIndex;
		} else {
			blitter->mOutputs[i].mpSrc = NULL;
			blitter->mOutputs[i].mSrcIndex = 0;
		}
	}

	mStack.clear();

	blitter->mGenerators.swap(mGenerators);
	blitter->mSources.swap(mSources);
	return blitter.release();
}
