		segment	.rdata, align=16

correct	dq			0000800000008000h

		segment	.text

		%include	"a_triblt.inc"

;--------------------------------------------------------------------------
	global	_vdasm_triblt_span_bilinear_mmx
_vdasm_triblt_span_bilinear_mmx:
		push		ebp
		push		edi
		push		esi
		push		ebx
		mov			edi,[esp+4+16]
		mov			edx,[edi+texinfo.dst]
		mov			ebp,[edi+texinfo.w]
		shl			ebp,2
		mov			ebx,[edi+texinfo.mips+mipmap.bits]
		add			edx,ebp
		mov			esi,[edi+texinfo.mips+mipmap.pitch]
		neg			ebp
		movd		mm6,[edi+texinfo.mips+mipmap.uvmul]
		pxor		mm7,mm7
		mov			edi,[edi+texinfo.src]
.xloop:
		movq		mm4,[edi]
		movq		mm0,mm4
		psrld		mm0,16
		movq		mm5,mm4
		packssdw	mm0,mm0
		pmaddwd		mm0,mm6
		add			edi,8
		punpcklwd	mm4,mm4
		punpckldq	mm4,mm4
		movd		ecx,mm0
		add			ecx,ebx
		psrlw		mm4,1
		movd		mm0,dword [ecx]
		movd		mm1,dword [ecx+4]
		punpcklbw	mm0,mm7
		movd		mm2,dword [ecx+esi]
		punpcklbw	mm1,mm7
		movd		mm3,dword [ecx+esi+4]
		punpcklbw	mm2,mm7
		punpcklbw	mm3,mm7
		psubw		mm1,mm0
		psubw		mm3,mm2
		paddw		mm1,mm1
		paddw		mm3,mm3
		pmulhw		mm1,mm4
		pmulhw		mm3,mm4
		punpckhwd	mm5,mm5
		punpckldq	mm5,mm5
		paddw		mm0,mm1
		psrlw		mm5,1
		paddw		mm2,mm3
		psubw		mm2,mm0
		paddw		mm2,mm2
		pmulhw		mm2,mm5
		paddw		mm0,mm2
		packuswb	mm0,mm0
		movd		dword [edx+ebp],mm0
		add			ebp,4
		jnc			.xloop
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		emms
		ret
		
;--------------------------------------------------------------------------
	global	_vdasm_triblt_span_trilinear_mmx
_vdasm_triblt_span_trilinear_mmx:
		push		ebp
		push		edi
		push		esi
		push		ebx
		mov			esi,[esp+4+16]
		mov			edx,[esi+texinfo.dst]
		mov			ebp,[esi+texinfo.w]
		shl			ebp,2
		add			edx,ebp
		neg			ebp
		mov			edi,[esi+texinfo.src]
		pxor		mm7,mm7
.xloop:
		movd		mm6,[edi+mipspan.u]
		punpckldq	mm6,[edi+mipspan.v]
		mov			eax,[edi+mipspan.lambda]
		shr			eax,4
		and			eax,byte -16
		movd		mm2,eax
		psrlq		mm2,4
		psrld		mm6,mm2
		paddd		mm6,[correct]

		;fetch mipmap 1
		mov			ebx,[esi+eax+mipmap.pitch]
		movd		mm1,[esi+eax+mipmap.uvmul]
		movq		mm4,mm6
		movq		mm0,mm6
		psrld		mm0,16
		packssdw	mm0,mm0
		pmaddwd		mm0,mm1
		movq		mm5,mm4
		punpcklwd	mm4,mm4
		punpckldq	mm4,mm4
		punpckhwd	mm5,mm5
		punpckldq	mm5,mm5
		movd		ecx,mm0
		add			ecx,[esi+eax+mipmap.bits]
		psrlw		mm4,1
		movd		mm0,dword [ecx]
		movd		mm1,dword [ecx+4]
		punpcklbw	mm0,mm7
		movd		mm2,dword [ecx+ebx]
		punpcklbw	mm1,mm7
		movd		mm3,dword [ecx+ebx+4]
		punpcklbw	mm2,mm7
		punpcklbw	mm3,mm7
		psubw		mm1,mm0
		psubw		mm3,mm2
		paddw		mm1,mm1
		paddw		mm3,mm3
		pmulhw		mm1,mm4
		pmulhw		mm3,mm4
		paddw		mm0,mm1
		psrlw		mm5,1
		paddw		mm2,mm3
		psubw		mm2,mm0
		paddw		mm2,mm2
		pmulhw		mm2,mm5
		paddw		mm0,mm2

		;fetch mipmap 2
		mov			ebx,[esi+eax+16+mipmap.pitch]
		movd		mm1,[esi+eax+16+mipmap.uvmul]
		paddd		mm6,[correct]
		psrld		mm6,1
		movq		mm4,mm6
		psrld		mm6,16
		packssdw	mm6,mm6
		pmaddwd		mm6,mm1
		movq		mm5,mm4
		punpcklwd	mm4,mm4
		punpckldq	mm4,mm4
		punpckhwd	mm5,mm5
		punpckldq	mm5,mm5
		movd		ecx,mm6
		add			ecx,[esi+eax+16+mipmap.bits]
		psrlw		mm4,1
		movd		mm6,dword [ecx]
		movd		mm1,dword [ecx+4]
		punpcklbw	mm6,mm7
		movd		mm2,dword [ecx+ebx]
		punpcklbw	mm1,mm7
		movd		mm3,dword [ecx+ebx+4]
		punpcklbw	mm2,mm7
		punpcklbw	mm3,mm7
		psubw		mm1,mm6
		psubw		mm3,mm2
		paddw		mm1,mm1
		paddw		mm3,mm3
		pmulhw		mm1,mm4
		pmulhw		mm3,mm4
		paddw		mm6,mm1
		psrlw		mm5,1
		paddw		mm2,mm3
		psubw		mm2,mm6
		paddw		mm2,mm2
		pmulhw		mm2,mm5
		paddw		mm6,mm2

		;blend mips
		movd		mm1,[edi+mipspan.lambda]
		punpcklwd	mm1,mm1
		punpckldq	mm1,mm1
		psllw		mm1,8
		psrlq		mm1,1
		psubw		mm6,mm0
		paddw		mm6,mm6
		pmulhw		mm6,mm1
		paddw		mm0,mm6
		packuswb	mm0,mm0

		movd		dword [edx+ebp],mm0
		add			edi, mipspan_size
		add			ebp,4
		jnc			.xloop
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		emms
		ret

		end
