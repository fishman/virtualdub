//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2008 Avery Lee
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

#ifndef DISPLAYDX9_PS14_FXH
#define DISPLAYDX9_PS14_FXH

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Pixel shader 1.4 bicubic path - 5 texture stages, 2 passes (NVIDIA GeForceFX+, ATI RADEON 8500+)
//
////////////////////////////////////////////////////////////////////////////////////////////////////

struct VertexOutputBicubic1_4 {
	float4	pos		: POSITION;
	float2	uvfilt	: TEXCOORD0;
	float2	uvsrc0	: TEXCOORD1;
	float2	uvsrc1	: TEXCOORD2;
	float2	uvsrc2	: TEXCOORD3;
	float2	uvsrc3	: TEXCOORD4;
};

VertexOutputBicubic1_4 VertexShaderBicubic1_4A(VertexInput IN) {
	VertexOutputBicubic1_4 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = IN.uv2.x * vd_vpsize.x * vd_interphtexsize.w;
	OUT.uvfilt.y = 0;

	OUT.uvsrc0 = IN.uv + float2(-1.5f, vd_fieldinfo.y)*vd_texsize.wz;
	OUT.uvsrc1 = IN.uv + float2( 0.0f, vd_fieldinfo.y)*vd_texsize.wz;
	OUT.uvsrc2 = IN.uv + float2( 0.0f, vd_fieldinfo.y)*vd_texsize.wz;
	OUT.uvsrc3 = IN.uv + float2(+1.5f, vd_fieldinfo.y)*vd_texsize.wz;
	
	return OUT;
}

VertexOutputBicubic1_4 VertexShaderBicubic1_4B(VertexInput IN) {
	VertexOutputBicubic1_4 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = IN.uv2.y * vd_vpsize.y * vd_interpvtexsize.w;
	OUT.uvfilt.y = 0;
	
	float2 uv = IN.uv2 * float2(vd_vpsize.x, vd_srcsize.y) * vd_tempsize.wz;
	OUT.uvsrc0 = uv + float2(0, -1.5f)*vd_tempsize.wz;
	OUT.uvsrc1 = uv + float2(0,  0.0f)*vd_tempsize.wz;
	OUT.uvsrc2 = uv + float2(0,  0.0f)*vd_tempsize.wz;
	OUT.uvsrc3 = uv + float2(0, +1.5f)*vd_tempsize.wz;
	
	return OUT;
}

pixelshader bicubic1_4_ps = asm {
	ps_1_4
	texld r0, t0
	texld r1, t1
	texld r2, t2
	texld r3, t3
	texld r4, t4
	mad_x4 r2, r2, r0_bias.g, r2
	mad r2, r1, -r0.b, r2
	mad_d2 r2, r4, -r0.a, r2
	mad_d2 r0, r3, r0.r, r2
};

technique bicubic1_4 {
	pass horiz <
		string vd_target="temp";
		string vd_viewport="out, src";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubic1_4A();
		PixelShader = <bicubic1_4_ps>;
		
		Texture[0] = <vd_interphtexture>;
		AddressU[0] = Wrap;
		AddressV[0] = Clamp;
		MipFilter[0] = None;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_srctexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MipFilter[1] = None;
		MinFilter[1] = Point;
		MagFilter[1] = Point;
		
		Texture[2] = <vd_srctexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MipFilter[2] = None;
		MinFilter[2] = Linear;
		MagFilter[2] = Linear;
		
		Texture[3] = <vd_srctexture>;
		AddressU[3] = Clamp;
		AddressV[3] = Clamp;		
		MipFilter[3] = None;
		MinFilter[3] = Point;
		MagFilter[3] = Point;
		
		Texture[4] = <vd_srctexture>;
		AddressU[4] = Clamp;
		AddressV[4] = Clamp;		
		MipFilter[4] = None;
		MinFilter[4] = Point;
		MagFilter[4] = Point;
	}
	
	pass vert <
		string vd_target="";
		string vd_viewport="out,out";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubic1_4B();
		PixelShader = <bicubic1_4_ps>;
		Texture[0] = <vd_interpvtexture>;
		Texture[1] = <vd_temptexture>;
		Texture[2] = <vd_temptexture>;
		Texture[3] = <vd_temptexture>;
		Texture[4] = <vd_temptexture>;
	}
}

#endif
