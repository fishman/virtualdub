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

#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>
#include "filters.h"
#include "f_convolute.h"

#define USE_ASM

extern "C" void asm_sharpen_run(
		void *dst,
		void *src,
		unsigned long width,
		unsigned long height,
		unsigned long srcstride,
		unsigned long dststride,
		long a_mult,
		long b_mult);

#define C_TOPOK		(1)
#define C_BOTTOMOK	(2)
#define C_LEFTOK	(4)
#define C_RIGHTOK	(8)

void inline conv_add(long& rt, long& gt, long& bt, unsigned long dv, long m) {
	bt += m*(0xFF & (dv));
	gt += m*(0xFF & (dv>>8));
	rt += m*(0xFF & (dv>>16));
}

void inline conv_add2(long& rt, long& gt, long& bt, unsigned long dv) {
	bt += 0xFF & (dv);
	gt += 0xFF & (dv>>8);
	rt += 0xFF & (dv>>16);
}

static unsigned long __fastcall do_conv(unsigned long *data, long *m, long sflags, long pit) {
	long rt=0, gt=0, bt=0;

	if (sflags & C_TOPOK) {
		if (sflags & (C_LEFTOK))		conv_add2(rt, gt, bt, data[        -1]);
		else							conv_add2(rt, gt, bt, data[         0]);
										conv_add2(rt, gt, bt, data[         0]);
		if (sflags & (C_RIGHTOK))		conv_add2(rt, gt, bt, data[        +1]);
		else							conv_add2(rt, gt, bt, data[         0]);
	} else {
		if (sflags & (C_LEFTOK))		conv_add2(rt, gt, bt, data[(pit>>2)-1]);
		else							conv_add2(rt, gt, bt, data[(pit>>2)  ]);
										conv_add2(rt, gt, bt, data[(pit>>2)  ]);
		if (sflags & (C_RIGHTOK))		conv_add2(rt, gt, bt, data[(pit>>2)+1]);
		else							conv_add2(rt, gt, bt, data[(pit>>2)  ]);
	}
	if (sflags & (C_LEFTOK))			conv_add2(rt, gt, bt, data[(pit>>2)-1]);
	else								conv_add2(rt, gt, bt, data[(pit>>2)  ]);
	if (sflags & (C_RIGHTOK))			conv_add2(rt, gt, bt, data[(pit>>2)+1]);
	else								conv_add2(rt, gt, bt, data[(pit>>2)  ]);
	if (sflags & C_BOTTOMOK) {
		if (sflags & (C_LEFTOK))		conv_add2(rt, gt, bt, data[(pit>>1)-1]);
		else							conv_add2(rt, gt, bt, data[(pit>>1)  ]);
										conv_add2(rt, gt, bt, data[(pit>>1)  ]);
		if (sflags & (C_RIGHTOK))		conv_add2(rt, gt, bt, data[(pit>>1)+1]);
		else							conv_add2(rt, gt, bt, data[(pit>>1)  ]);
	} else {
		if (sflags & (C_LEFTOK))		conv_add2(rt, gt, bt, data[(pit>>2)-1]);
		else							conv_add2(rt, gt, bt, data[(pit>>2)  ]);
										conv_add2(rt, gt, bt, data[(pit>>2)  ]);
		if (sflags & (C_RIGHTOK))		conv_add2(rt, gt, bt, data[(pit>>2)+1]);
		else							conv_add2(rt, gt, bt, data[(pit>>2)  ]);
	}

	rt = rt*m[0]+m[9];
	gt = gt*m[0]+m[9];
	bt = bt*m[0]+m[9];

	conv_add(rt, gt, bt, data[(pit>>2)  ],m[4]);

	rt>>=8;	if (rt<0) rt=0; else if (rt>255) rt=255;
	gt>>=8;	if (gt<0) gt=0; else if (gt>255) gt=255;
	bt>>=8;	if (bt<0) bt=0; else if (bt>255) bt=255;

	return (unsigned long)((rt<<16) | (gt<<8) | (bt));
}

#ifndef USE_ASM
static inline unsigned long do_conv2(unsigned long *data, long *m, long pit) {
	long rt=0, gt=0, bt=0;

	conv_add2(rt, gt, bt, data[        -1]);
	conv_add2(rt, gt, bt, data[         0]);
	conv_add2(rt, gt, bt, data[        +1]);
	conv_add2(rt, gt, bt, data[(pit>>2)-1]);
	conv_add2(rt, gt, bt, data[(pit>>2)+1]);
	conv_add2(rt, gt, bt, data[(pit>>1)-1]);
	conv_add2(rt, gt, bt, data[(pit>>1)  ]);
	conv_add2(rt, gt, bt, data[(pit>>1)+1]);
	rt = rt*m[0]+m[9];
	gt = gt*m[0]+m[9];
	bt = bt*m[0]+m[9];

	conv_add(rt, gt, bt, data[(pit>>2)  ], m[4]);

	rt>>=8;	if (rt<0) rt=0; else if (rt>255) rt=255;
	gt>>=8;	if (gt<0) gt=0; else if (gt>255) gt=255;
	bt>>=8;	if (bt<0) bt=0; else if (bt>255) bt=255;

	return (unsigned long)((rt<<16) | (gt<<8) | (bt));
}
#endif

static int sharpen_run(const FilterActivation *fa, const FilterFunctions *ff) {
	unsigned long w,h;
	unsigned long *src = (unsigned long *)fa->src.data, *dst = (unsigned long *)fa->dst.data;
	unsigned long pitch = fa->src.pitch;
	long *m = ((ConvoluteFilterData *)(fa->filter_data))->m;

	src -= pitch>>2;

	*dst++ = do_conv(src++, m, C_BOTTOMOK | C_RIGHTOK, pitch);
	w = fa->src.w-2;
	do { *dst++ = do_conv(src++, m, C_BOTTOMOK | C_LEFTOK | C_RIGHTOK, pitch); } while(--w);
	*dst++ = do_conv(src++, m, C_BOTTOMOK | C_LEFTOK, pitch);

	src += fa->src.modulo>>2;
	dst += fa->dst.modulo>>2;

#ifdef USE_ASM
	asm_sharpen_run(
			dst+1,
			src+1,
			fa->src.w-2,
			fa->src.h-2,
			fa->src.pitch,
			fa->dst.pitch,
			m[0],
			m[4]);
#endif

	h = fa->src.h-2;
	do {
		*dst++ = do_conv(src++, m, C_TOPOK | C_BOTTOMOK | C_RIGHTOK, pitch);
#ifdef USE_ASM
		src += fa->src.w-2;
		dst += fa->src.w-2;
#else
		w = fa->src.w-2;
		do { *dst++ = do_conv2(src++, m, pitch); } while(--w);
#endif
		*dst++ = do_conv(src++, m, C_TOPOK | C_BOTTOMOK | C_LEFTOK, pitch);

		src += fa->src.modulo>>2;
		dst += fa->dst.modulo>>2;
	} while(--h);

	*dst++ = do_conv(src++, m, C_TOPOK | C_RIGHTOK, pitch);
	w = fa->src.w-2;
	do { *dst++ = do_conv(src++, m, C_TOPOK | C_LEFTOK | C_RIGHTOK, pitch); } while(--w);
	*dst++ = do_conv(src++, m, C_TOPOK | C_LEFTOK, pitch);

	return 0;
}

static long sharpen_param(FilterActivation *fa, const FilterFunctions *ff) {
	return FILTERPARAM_SWAP_BUFFERS;
}

static void sharpen_update(long value, void *pThis) {
	ConvoluteFilterData *cfd = (ConvoluteFilterData *)pThis;
	for(int i=0; i<9; i++)
		if (i==4) cfd->m[4] = 256+8*value; else cfd->m[i]=-value;
	cfd->bias = -value*4;
}

static int sharpen_init(FilterActivation *fa, const FilterFunctions *ff) {
	sharpen_update(16, fa->filter_data);
	return 0;
}

static int sharpen_config(FilterActivation *fa, const FilterFunctions *ff, VDXHWND hWnd) {
	ConvoluteFilterData *cfd;
	LONG lv;

	if (!fa->filter_data) {
		if (!(fa->filter_data = (void *)new ConvoluteFilterData)) return 0;
		memset(fa->filter_data, 0, sizeof ConvoluteFilterData);
		((ConvoluteFilterData *)fa->filter_data)->m[4] = 256;
	}
	cfd = (ConvoluteFilterData *)fa->filter_data;

	lv = FilterGetSingleValue((HWND)hWnd, -cfd->m[0], 0, 64, "sharpen", fa->ifp, sharpen_update, cfd);

	sharpen_update(lv, fa->filter_data);
	return 0;
}

static void sharpen_string(const FilterActivation *fa, const FilterFunctions *ff, char *buf) {
	ConvoluteFilterData *cfd = (ConvoluteFilterData *)fa->filter_data;

	wsprintf(buf, " (by %ld)", -cfd->bias/4);
}

static void sharpen_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;
	int lv = argv[0].asInt();

	ConvoluteFilterData *cfd = (ConvoluteFilterData *)fa->filter_data;

	for(int i=0; i<9; i++)
		if (i==4) cfd->m[4] = 256+8*lv; else cfd->m[i]=-lv;

	cfd->bias = -lv*4;
}

static ScriptFunctionDef sharpen_func_defs[]={
	{ (ScriptFunctionPtr)sharpen_script_config, "Config", "0i" },
	{ NULL },
};

static CScriptObject sharpen_obj={
	NULL, sharpen_func_defs
};

static bool sharpen_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	ConvoluteFilterData *cfd = (ConvoluteFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d)", -cfd->bias/4);

	return true;
}

FilterDefinition filterDef_sharpen={
	0,0,NULL,
	"sharpen",
	"Enhances contrast between adjacent elements in an image.\n\n[Assembly optimized] [MMX optimized]",
	NULL,NULL,
	sizeof(ConvoluteFilterData),
	sharpen_init,
	NULL,
	sharpen_run,
	sharpen_param,
	sharpen_config,
	sharpen_string,
	NULL,
	NULL,

	&sharpen_obj,
	sharpen_script_line,
};