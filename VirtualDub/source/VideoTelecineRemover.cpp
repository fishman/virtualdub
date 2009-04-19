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

#include "VBitmap.h"
#include <vd2/system/error.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>

#include "VideoTelecineRemover.h"

#if defined(_MSC_VER) && defined(_M_IX86)
	#pragma warning(disable: 4799)	// warning C4799: function 'computeScanImprovementMMX' has no EMMS instruction
#endif

class CVideoTelecineRemover: public VideoTelecineRemover {
public:
	CVideoTelecineRemover(int w, int h, bool decomb, int offset, bool invertedPolarity);
	~CVideoTelecineRemover();

	void ProcessIn(const VDPixmap& in, VDPosition srcFrame, VDPosition timelineFrame);
	bool ProcessOut(const VDPixmap& out, VDPosition& srcFrame, VDPosition& timelineFrame);

private:
	char *	pMemBlock;
	long	nCombVals[10][2];
	VDPosition	mSourceFrameNums[10];
	VDPosition	mTimelineFrameNums[10];
	VBitmap	vb;
	int		nCurrentIn, nCurrentOut;
	int		nCombOffset1, nCombOffset2;
	int		nNewCombOffset1, nNewCombOffset2;
	int		nLag;
	bool	fInvertPolarity, fNewPolarity, fDropMode, fNewDropMode,
			fDecomb, fOffsetLocked;

	VDPixmapBuffer	mTempBuffer;
	VDPixmapBuffer	mTempBuffer2;
};

VideoTelecineRemover *CreateVideoTelecineRemover(int w, int h, bool fDecomb, int iOffset, bool fInvertedPolarity) {
	return new CVideoTelecineRemover(w, h, fDecomb, iOffset, fInvertedPolarity);
}


CVideoTelecineRemover::CVideoTelecineRemover(int w, int h, bool fDecomb, int iOffset, bool fInvertedPolarity) {
	vb = VBitmap(NULL, w, h, 24);

	if (!(pMemBlock = new char[vb.size * 10]))
		throw MyMemoryError();

	memset(pMemBlock, 0, vb.size * 10);

	if (fDecomb) {
		nCurrentIn = 0;
		nLag = 10;
	} else {
		nCurrentIn = 4;
		nLag = 6;
	}
	nCurrentOut = 9;

	if (iOffset<0) {
		nCombOffset1 = nNewCombOffset1 = -1;
		nCombOffset2 = nNewCombOffset2 = -1;
		fDropMode = fNewDropMode = true;
		fOffsetLocked = false;
	} else {
		fDropMode = false;
		nCombOffset1 = iOffset;
		nCombOffset2 = (iOffset+1) % 5;
		fInvertPolarity = fInvertedPolarity;
		fOffsetLocked = true;
	}
	this->fDecomb = fDecomb;

	memset(nCombVals, 0, sizeof nCombVals);
}

VideoTelecineRemover::~VideoTelecineRemover() {
}

CVideoTelecineRemover::~CVideoTelecineRemover() {
	delete[] pMemBlock;
}

static inline int sq(int d) {
	return d*d;
}

#ifdef _M_IX86
#if 0

static long __declspec(naked) computeScanImprovementMMX(Pixel8 *src1, Pixel8 *src2, PixOffset pitch, PixDim w) {
	__asm {
		push		ebx

		mov			eax,[esp+8]
		mov			ecx,[esp+16]
		mov			edx,[esp+12]
		mov			ebx,[esp+20]

		pxor		mm4,mm4
xloop:
		movd		mm0,[eax]
		pxor		mm7,mm7

		movd		mm1,[eax+ecx*2]
		punpcklbw	mm0,mm7

		movd		mm2,[eax+ecx]
		punpcklbw	mm1,mm7

		movd		mm3,[edx]
		punpcklbw	mm2,mm7			;mm2 = pB

		paddw		mm0,mm1			;mm0 = pA + pC
		paddw		mm2,mm2			;mm2 = 2*pB

		punpcklbw	mm3,mm7			;mm3 = pD
		psubw		mm2,mm0			;mm2 = 2*pB - (pA+pC)

		paddw		mm3,mm3			;mm3 = 2*pD
		pmaddwd		mm2,mm2			;mm2 = sq(2*pB - (pA+pC))

		psubw		mm3,mm0			;mm3 = 2*pD - (pA+pC)
		add			eax,4

		pmaddwd		mm3,mm3			;mm3 = sq(2*pD - (pA+pC))
		add			edx,4

		paddd		mm4,mm2
		dec			ebx

		;
		;

		psubd		mm4,mm3
		jne			xloop

		movd		eax,mm4
		psrlq		mm4,32
		movd		ecx,mm4
		add			eax,ecx

		pop			ebx
		ret
	}
}
#else
static long __declspec(naked) computeScanImprovementMMX(Pixel8 *src1, Pixel8 *src2, PixOffset pitch, PixDim w) {
	__asm {
		push		ebx

		mov			eax,[esp+8]
		mov			ecx,[esp+16]
		mov			edx,[esp+12]
		mov			ebx,[esp+20]

		pxor		mm6,mm6
xloop:
		movd		mm0,[eax]
		pxor		mm7,mm7

		movd		mm1,[eax+ecx*2]
		punpcklbw	mm0,mm7			;mm0 = pA

		movd		mm2,[eax+ecx]
		punpcklbw	mm1,mm7			;mm1 = pC

		movd		mm3,[edx]
		punpcklbw	mm2,mm7			;mm2 = pB

		movd		mm4,[edx+ecx*2]
		punpcklbw	mm3,mm7			;mm3 = pD

		movd		mm5,[edx+ecx]
		punpcklbw	mm4,mm7			;mm4 = pF

		punpcklbw	mm5,mm7			;mm5 = pE
		paddw		mm0,mm1			;mm0 = pA + pC

		paddw		mm3,mm4			;mm3 = pD + pF
		paddw		mm5,mm5			;mm5 = 2*pE

		paddw		mm2,mm2			;mm2 = 2*pB
		psubw		mm3,mm5			;mm3 = pD + pF - 2*pE

		psubw		mm5,mm0			;mm5 = 2*pE - (pA + pC)
		pmaddwd		mm3,mm3			;mm3 = sq(pD + pF - 2*pE)	[mm3 --- ---]

		psubw		mm0,mm2			;mm0 = pA + pC - 2*pB
		pmaddwd		mm5,mm5			;mm5 = sq(pA + pC - 2*pE)	[mm5 mm3 ---]

		pmaddwd		mm0,mm0			;mm0 = sq(pA + pC - 2*pB)	[mm0 mm5 mm3]
		add			eax,4

		paddd		mm6,mm3
		add			edx,4

		psubd		mm6,mm5
		dec			ebx

		paddd		mm6,mm0
		jne			xloop

		movd		eax,mm6
		psrlq		mm6,32
		movd		ecx,mm6
		add			eax,ecx

		pop			ebx
		ret
	}
}
#endif
#endif

static long computeScanImprovement(Pixel8 *src1, Pixel8 *src2, PixOffset pitch, PixDim w) {
	long imp = 0;

	// now using original intended algorithm, plus checking if the second frame is also combed
	// so it doesn't easily take the frame to be dropped as the frame to be decombed. 
	// Check below to see fix...  This actually works very well! Beats any commercial
	// software out there!
	//
	// Without the fix, the reason the broken algorithm sorta worked is because, in the 
	// sequence [A1/A2] [A1/B2], if A2 sorta looked like B2, it would actually take the 
	// right offset... but when scene changes a lot, this does not work at all. 
	//
	// Samuel Audet <guardia@cam.órg>

	w = -w;
	do {
		int rA = src1[0];
		int rB = src1[pitch];
		int rC = src1[pitch*2];
		int rD = src2[0];
		int rE = src2[pitch];
		int rF = src2[pitch*2];

		imp += sq(rA + rC - 2*rB)		// combing in current frame
            + sq(rD + rF - 2*rE)		// combing in second frame
			- sq(rA + rC - 2*rE);		// combing in merged frame

		src1++;
		src2++;
	} while(++w);

	return imp;
}

void CVideoTelecineRemover::ProcessIn(const VDPixmap& in, VDPosition srcFrame, VDPosition timelineFrame) {
	Pixel8 *src1, *src2;
	PixDim h;
	sint64 field1=0, field2=0;

	vb.data = (Pixel *)(pMemBlock + vb.pitch*vb.h * nCurrentIn);

	VDPixmapBlt(VDAsPixmap(vb), in);

	mSourceFrameNums[nCurrentIn] = srcFrame;
	mTimelineFrameNums[nCurrentIn] = timelineFrame;

	if (fOffsetLocked) {
		if (++nCurrentIn == 10)
			nCurrentIn = 0;
		return;
	}

	// We skip the top and bottom 8 lines -- they're usually full of head noise.

	{
		long longs = (vb.w*3)/4;

		h = (vb.h-2)/2 - 8;
		src1 = (Pixel8 *)(pMemBlock + vb.pitch*(vb.h * nCurrentIn + 8));
		src2 = (Pixel8 *)(pMemBlock + vb.pitch*(vb.h * ((nCurrentIn+9)%10) + 8));

#ifdef _M_IX86
		if (MMX_enabled) {
			do {
				field1 += computeScanImprovementMMX(src1, src2, vb.pitch, longs);
				field2 += computeScanImprovementMMX(src1+vb.pitch, src2+vb.pitch, vb.pitch, longs);

				src1 += vb.pitch*2;
				src2 += vb.pitch*2;
			} while(--h);

			__asm emms
		} else
#endif
		{
			do {
				field1 += computeScanImprovement(src1, src2, vb.pitch, longs*4);
				field2 += computeScanImprovement(src1+vb.pitch, src2+vb.pitch, vb.pitch, longs*4);

				src1 += vb.pitch*2;
				src2 += vb.pitch*2;
			} while(--h);
		}
	}

	if (field1 < 0)
		field1 = 0;

	if (field2 < 0)
		field2 = 0;

	VDDEBUG("%16d %16d\n", (long)sqrt((double)field1), (long)sqrt((double)field2));

	nCombVals[(nCurrentIn+9)%10][0] = (long)sqrt((double)field1);
	nCombVals[(nCurrentIn+9)%10][1] = (long)sqrt((double)field2);

	if (++nCurrentIn == 10)
		nCurrentIn = 0;

	if (nCurrentIn == 0 || nCurrentIn == 5) {
		int i;
		long best_score = 0;
		int best_offset = -1;
		bool best_polarity = false;

		for(i=0; i<5; i++) {
			long v1 = nCombVals[(nCurrentIn+4+i)%10][0];
			long v2 = nCombVals[(nCurrentIn+4+i)%10][1];

			if (v1 > best_score) {
				best_offset = (i+4)%5;
				best_polarity = true;
				best_score = v1;
			}

			if (v2 > best_score) {
				best_offset = (i+4)%5;
				best_polarity = false;
				best_score = v2;
			}
		}

//		VDDEBUG("----------- %d %d [%d %d]\n", best_offset, best_polarity, nNewCombOffset1, fNewPolarity);

		fDropMode = fNewDropMode;
		nCombOffset1 = nNewCombOffset1;
		nCombOffset2 = nNewCombOffset2;
		fInvertPolarity = fNewPolarity;

		if (best_offset == -1) {
			fNewDropMode = true;
			nNewCombOffset1 = 0;
			nNewCombOffset2 = 1;
		} else {
			fNewDropMode = false;
			nNewCombOffset1 = best_offset;
			nNewCombOffset2 = (best_offset+1) % 5;
			fNewPolarity = best_polarity;
		}
	}
}

#ifdef _M_IX86
static void __declspec(naked) DeBlendMMX_32_24(void *dst, void *src1, void *src2, void *src3, void *src4, long w3, long h, long spitch, long dmodulo) {
	static const sint64 one = 0x0001000100010001i64;
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			ebp,[esp+24+16]
		mov			edi,[esp+4+16]
		mov			eax,[esp+8+16]
		mov			ebx,[esp+12+16]
		mov			ecx,[esp+16+16]
		mov			edx,[esp+20+16]
		sub			eax,ebp
		sub			ebx,ebp
		sub			ecx,ebp
		sub			edx,ebp
		pxor		mm7,mm7
yloop:
		mov			ebp,[esp+24+16]
xloop:
		movd		mm0,[eax+ebp]
		movd		mm1,[ebx+ebp]
		movd		mm2,[ecx+ebp]
		movd		mm3,[edx+ebp]
		punpcklbw	mm0,mm7			;mm0 = A = a
		punpcklbw	mm1,mm7			;mm1 = B = (a+b)/2
		punpcklbw	mm2,mm7			;mm2 = C = (b+c)/2
		punpcklbw	mm3,mm7			;mm3 = D = c
		paddw		mm0,mm3			;mm0 = A+D = a+c
		paddw		mm1,mm2			;mm1 = B+C = (a+b)/2 + (b+c)/2 = b + (a+c)/2
		paddw		mm0,one
		psrlw		mm0,1			;mm0 = (a+c)/2
		psubw		mm1,mm0			;mm1 = [B1][R0][G0][B0] *done

		movd		mm0,[eax+ebp+4]
		movd		mm4,[ebx+ebp+4]
		movd		mm2,[ecx+ebp+4]
		movd		mm3,[edx+ebp+4]
		punpcklbw	mm0,mm7			;mm0 = A = a
		punpcklbw	mm4,mm7			;mm4 = B = (a+b)/2
		punpcklbw	mm2,mm7			;mm2 = C = (b+c)/2
		punpcklbw	mm3,mm7			;mm3 = D = c
		paddw		mm0,mm3			;mm0 = A+D = a+c
		paddw		mm4,mm2			;mm4 = B+C = (a+b)/2 + (b+c)/2 = b + (a+c)/2
		paddw		mm0,one
		psrlw		mm0,1			;mm0 = (a+c)/2
		psubw		mm4,mm0			;mm4 = [G2][B2][R1][G1]

		movd		mm0,[eax+ebp+8]
		movd		mm5,[ebx+ebp+8]
		movd		mm2,[ecx+ebp+8]
		movd		mm3,[edx+ebp+8]
		punpcklbw	mm0,mm7			;mm0 = A = a
		punpcklbw	mm5,mm7			;mm1 = B = (a+b)/2
		punpcklbw	mm2,mm7			;mm2 = C = (b+c)/2
		punpcklbw	mm3,mm7			;mm3 = D = c
		paddw		mm0,mm3			;mm0 = A+D = a+c
		paddw		mm5,mm2			;mm1 = B+C = (a+b)/2 + (b+c)/2 = b + (a+c)/2
		paddw		mm0,one
		psrlw		mm0,1			;mm0 = (a+c)/2
		psubw		mm5,mm0			;mm5 = [R3][G3][B3][R2]

		movq		mm6,mm4			;mm6 = [G2][B2][R1][G1]
		psrlq		mm4,32			;mm4 = [  ][  ][G2][B2]

		psllq		mm6,16			;mm6 = [B2][R1][G1][  ]
		movq		mm2,mm1			;mm2 = [B1][R0][G0][B0]

		movq		mm0,mm5			;mm0 = [R3][G3][B3][R2]
		psrlq		mm2,48			;mm2 = [  ][  ][  ][B1]

		psllq		mm5,32			;mm5 = [B3][R2][  ][  ]
		por			mm2,mm6			;mm2 = [B2][R1][G1][B1] *done

		por			mm4,mm5			;mm4 = [B3][R2][G2][B2] *done
		packuswb	mm1,mm2			;mm0 = [B2][R1][G1][B1][B1][R0][G0][B0]

		psrlq		mm0,16			;mm5 = [  ][R3][G3][B3] *done

		movq		[edi+0],mm1
		packuswb	mm4,mm0			;mm4 = [  ][R3][G3][B3][B3][R2][G2][B2]

		movq		[edi+8],mm4

		add			edi,16

		add			ebp,12
		jne			xloop

		mov			ebp,[esp+32+16]
		add			edi,[esp+36+16]
		add			eax,ebp
		add			ebx,ebp
		add			ecx,ebp
		add			edx,ebp

		dec			dword ptr [esp+28+16]
		jne			yloop
		emms

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
	}
}
#endif

bool CVideoTelecineRemover::ProcessOut(const VDPixmap& out, VDPosition& srcFrame, VDPosition& timelineFrame) {

	if (++nCurrentOut >= 10)
		nCurrentOut = 0;

	if (nLag) {
		--nLag;
		return false;
	}

	// Input frames:	[A1/A2] [A1/B2] [B1/C2] [C1/C2] [D1/D2]
	// Action:			copy    decomb  drop    copy    copy

	if ((nCurrentOut == nCombOffset1 || nCurrentOut == nCombOffset1+5) && !fDropMode) {
		// First combed frame; reconstruct.

		if (fDecomb) {
			VDPixmap pxin, pxout;

			const VDPixmap *temp = &out;

			switch(out.format) {
				case nsVDPixmap::kPixFormat_XRGB1555:
				case nsVDPixmap::kPixFormat_RGB565:
				case nsVDPixmap::kPixFormat_RGB888:
				case nsVDPixmap::kPixFormat_XRGB8888:
				case nsVDPixmap::kPixFormat_Y8:
				case nsVDPixmap::kPixFormat_YUV422_UYVY:
				case nsVDPixmap::kPixFormat_YUV422_YUYV:
				case nsVDPixmap::kPixFormat_YUV444_Planar:
				case nsVDPixmap::kPixFormat_YUV422_Planar:
				case nsVDPixmap::kPixFormat_YUV411_Planar:
					break;

				default:
					mTempBuffer.init(vb.w, vb.h, nsVDPixmap::kPixFormat_YUV444_Planar);
					temp = &mTempBuffer;
			}

			// Copy bottom field.
			int bottomFieldFrame = (nCurrentOut + (fInvertPolarity?1:0))%10;
			pxin.data		= pMemBlock + vb.pitch * (vb.h*bottomFieldFrame + (vb.h - 2));
			pxin.pitch		= -vb.pitch * 2;
			pxin.w			= vb.w;
			pxin.h			= vb.h >> 1;
			pxin.format		= nsVDPixmap::kPixFormat_RGB888;

			pxout = *temp;
			pxout.data		= vdptroffset(temp->data, temp->pitch);
			pxout.data2		= vdptroffset(temp->data2, temp->pitch2);
			pxout.data3		= vdptroffset(temp->data3, temp->pitch3);
			pxout.pitch		= temp->pitch * 2;
			pxout.pitch2	= temp->pitch2 * 2;
			pxout.pitch3	= temp->pitch3 * 2;
			pxout.h			= temp->h >> 1;

			VDPixmapBlt(pxout, pxin);

			// Copy top field.
			int topFieldFrame = (nCurrentOut+(fInvertPolarity?0:1))%10;
			pxin.data		= pMemBlock + vb.pitch * (vb.h*topFieldFrame + (vb.h - 1));
			pxin.h			= (vb.h + 1) >> 1;

			pxout.data		= temp->data;
			pxout.data2		= temp->data2;
			pxout.data3		= temp->data3;
			pxout.h			= (temp->h + 1) >> 1;

			VDPixmapBlt(pxout, pxin);

			if (temp != &out)
				VDPixmapBlt(out, *temp);
		} else {
//			VDDEBUG("Recon: %d %d\n", nCurrentIn, nCurrentOut);

#ifdef _M_IX86
			const VDPixmap *pxsrc;

			if (out.format != nsVDPixmap::kPixFormat_XRGB8888) {
				mTempBuffer2.init(vb.w, vb.h, nsVDPixmap::kPixFormat_XRGB8888);
				pxsrc = &mTempBuffer2;
			} else
				pxsrc = &out;

			VBitmap vbout(VDAsVBitmap(*pxsrc));

			DeBlendMMX_32_24(
					vbout.data,
					pMemBlock + vb.pitch * (vb.h * ((nCurrentOut+9)%10)),
					pMemBlock + vb.pitch * (vb.h *   nCurrentOut       ),
					pMemBlock + vb.pitch * (vb.h * ((nCurrentOut+1)%10)),
					pMemBlock + vb.pitch * (vb.h * ((nCurrentOut+2)%10)),
					-vb.w*3,
					vb.h,
					vb.pitch,
					vbout.pitch - vb.w * 4);

			if (out.format != nsVDPixmap::kPixFormat_XRGB8888)
				VDPixmapBlt(out, *pxsrc);
#endif

#pragma vdpragma_TODO("Need scalar version of this for AMD64")
		}

		srcFrame = mSourceFrameNums[nCurrentOut];
		timelineFrame = mTimelineFrameNums[nCurrentOut];
		return true;

	} else if (nCurrentOut == nCombOffset2 || nCurrentOut == nCombOffset2+5) {
		// Second combed frame; drop.
		return false;
	} else {
		// Uncombed, unduplicated frame.

		vb.data = (Pixel *)(pMemBlock + vb.pitch*vb.h * nCurrentOut);

		VDPixmapBlt(out, VDAsPixmap(vb));

		srcFrame = mSourceFrameNums[nCurrentOut];
		timelineFrame = mTimelineFrameNums[nCurrentOut];
		return true;
	}
}
