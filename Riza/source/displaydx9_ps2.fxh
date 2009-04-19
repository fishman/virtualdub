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

#ifndef DISPLAYDX9_PS2_FXH
#define DISPLAYDX9_PS2_FXH

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Pixel shader 2.0 paths
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void VertexShaderPointBilinear_2_0(VertexInput IN, out float4 oPos : POSITION, out float2 oT0 : TEXCOORD0, out float2 oT1 : TEXCOORD1) {
	oPos = IN.pos;
	oT0 = IN.uv;
	oT1 = IN.uv2 * vd_vpsize.xy / 16.0f;
}

float4 PixelShaderPointBilinear_2_0(float2 t0 : TEXCOORD0, float2 ditherUV : TEXCOORD1) : COLOR0 {
	float3 c = tex2D(samp0, t0).rgb;
			 	
	return float4(c, 0) * (254.0f / 255.0f) + tex2D(samp1, ditherUV) / 256.0f;
};

technique point_2_0 {
	pass p0 <
		bool vd_clippos = true;
	> {
		VertexShader = compile vs_2_0 VertexShaderPointBilinear_2_0();
		PixelShader = compile ps_2_0 PixelShaderPointBilinear_2_0();

		MinFilter[0] = Point;
		MagFilter[0] = Point;
		MipFilter[0] = Point;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		Texture[0] = <vd_srctexture>;

		MinFilter[1] = Point;
		MagFilter[1] = Point;
		MipFilter[1] = Point;
		AddressU[1] = Wrap;
		AddressV[1] = Wrap;
		Texture[1] = <vd_dithertexture>;

		AlphaBlendEnable = false;
	}
}

technique bilinear_2_0 {
	pass p0 <
		bool vd_clippos = true;
	> {
		VertexShader = compile vs_2_0 VertexShaderPointBilinear_2_0();
		PixelShader = compile ps_2_0 PixelShaderPointBilinear_2_0();

		MinFilter[0] = Linear;
		MagFilter[0] = Linear;
		MipFilter[0] = Linear;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		Texture[0] = <vd_srctexture>;

		MinFilter[1] = Point;
		MagFilter[1] = Point;
		MipFilter[1] = Point;
		AddressU[1] = Wrap;
		AddressV[1] = Wrap;
		Texture[1] = <vd_dithertexture>;

		AlphaBlendEnable = false;
	}
}

struct VertexOutputBicubic_2_0 {
	float4	pos		: POSITION;
	float2	uvfilt	: TEXCOORD0;
	float2	uvsrc0	: TEXCOORD1;
	float2	uvsrc1	: TEXCOORD2;
	float2	uvsrc2	: TEXCOORD3;
};

VertexOutputBicubic_2_0 VertexShaderBicubic_2_0_A(VertexInput IN) {
	VertexOutputBicubic_2_0 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = IN.uv2.x * vd_vpsize.x * vd_interphtexsize.w;
	OUT.uvfilt.y = 0;

	OUT.uvsrc0 = IN.uv + float2(-1.5f, vd_fieldinfo.y)*vd_texsize.wz;
	OUT.uvsrc1 = IN.uv + float2( 0.0f, vd_fieldinfo.y)*vd_texsize.wz;
	OUT.uvsrc2 = IN.uv + float2(+1.5f, vd_fieldinfo.y)*vd_texsize.wz;
	
	return OUT;
}

VertexOutputBicubic_2_0 VertexShaderBicubic_2_0_B(VertexInput IN, out float2 ditherUV : TEXCOORD4) {
	VertexOutputBicubic_2_0 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = IN.uv2.y * vd_vpsize.y * vd_interpvtexsize.w;
	OUT.uvfilt.y = 0;
	
	float2 uv = IN.uv2 * float2(vd_vpsize.x, vd_srcsize.y) * vd_tempsize.wz;
	OUT.uvsrc0 = uv + float2(0, -1.5f)*vd_tempsize.wz;
	OUT.uvsrc1 = uv + float2(0,  0.0f)*vd_tempsize.wz;
	OUT.uvsrc2 = uv + float2(0, +1.5f)*vd_tempsize.wz;
	
	ditherUV = IN.uv2 * vd_vpsize.xy / 16.0f;
	
	return OUT;
}

float4 PixelShaderBicubic_2_0_A(VertexOutputBicubic_2_0 IN) : COLOR0 {		
	float4 weights = tex2D(samp0, IN.uvfilt) * float4(0.5, 1.0, -0.25, -0.25) + float4(0, 127.0f / 255.0f, 0, 0);
		
	float4 c = tex2D(samp2, IN.uvsrc1) * weights.g
			 + tex2D(samp1, IN.uvsrc0) * weights.b
			 + tex2D(samp1, IN.uvsrc2) * weights.a
			 + tex2D(samp1, IN.uvsrc1) * weights.r;
			 
	return c;
}

float4 PixelShaderBicubic_2_0_B(VertexOutputBicubic_2_0 IN, float2 ditherUV : TEXCOORD4) : COLOR0 {
	float4 weights = tex2D(samp0, IN.uvfilt) * float4(0.5, 1.0, -0.25, -0.25) + float4(0, 127.0f / 255.0f, 0, 0);
	
	float3 c = tex2D(samp2, IN.uvsrc1).rgb * weights.g
			 + tex2D(samp1, IN.uvsrc0).rgb * weights.b
			 + tex2D(samp1, IN.uvsrc2).rgb * weights.a
			 + tex2D(samp1, IN.uvsrc1).rgb * weights.r;
			 			 
	return float4(c * (254.0f / 255.0f), 0) + tex2D(samp3, ditherUV) / 256.0f;
}

technique bicubic_2_0 {
	pass horiz <
		string vd_target="temp";
		string vd_viewport="out, src";
	> {
		VertexShader = compile vs_2_0 VertexShaderBicubic_2_0_A();
		PixelShader = compile ps_2_0 PixelShaderBicubic_2_0_A();
		
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
	}
	
	pass vert <
		string vd_target="";
		string vd_viewport="out,out";
	> {
		VertexShader = compile vs_2_0 VertexShaderBicubic_2_0_B();
		PixelShader = compile ps_2_0 PixelShaderBicubic_2_0_B();
		Texture[0] = <vd_interpvtexture>;
		Texture[1] = <vd_temptexture>;
		Texture[2] = <vd_temptexture>;
		
		Texture[3] = <vd_dithertexture>;
		AddressU[3] = Wrap;
		AddressV[3] = Wrap;
		MipFilter[3] = None;
		MinFilter[3] = Point;
		MagFilter[3] = Point;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	UYVY/YUY2 to RGB -- pixel shader 2.0
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void VS_UYVY_to_RGB_2_0(
	float4 pos : POSITION,
	float2 uv : TEXCOORD0,
	float2 uv2 : TEXCOORD1,
	out float4 oPos : POSITION,
	out float2 oT0 : TEXCOORD0,
	out float2 oT1 : TEXCOORD1,
	out float2 oT2 : TEXCOORD2)
{
	oPos = pos;
	oT0 = uv;
	oT1 = oT0 + vd_texsize.wz * float2(0.25, 0);
	oT2.x = vd_srcsize.x * uv2.x / 16.0f;
	oT2.y = 0;
}

float4 PS_UYVY_to_RGB_2_0(float2 uvY : TEXCOORD0, float2 uvC : TEXCOORD1, float2 uvSelect : TEXCOORD2) : COLOR0 {
	float4 pxY = tex2D(samp0, uvY);
	float4 pxC = tex2D(samp1, uvC);
	float4 pxSelect = tex2D(samp2, uvSelect);

	float y = lerp(pxY.g, pxY.a, pxSelect.a);
	float cr = pxC.r;
	float cb = pxC.b;
	
	return ConvertYCbCrToRGB(y, cb, cr);
}

technique uyvy_to_rgb_2_0 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_UYVY_to_RGB_2_0();
		PixelShader = compile ps_2_0 PS_UYVY_to_RGB_2_0();
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_srctexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
		
		Texture[2] = <vd_hevenoddtexture>;
		AddressU[2] = Wrap;
		AddressV[2] = Clamp;
		MinFilter[2] = Point;
		MagFilter[2] = Point;
	}
}

float4 PS_HDYC_to_RGB_2_0(float2 uvY : TEXCOORD0, float2 uvC : TEXCOORD1, float2 uvSelect : TEXCOORD2) : COLOR0 {
	float4 pxY = tex2D(samp0, uvY);
	float4 pxC = tex2D(samp1, uvC);
	float4 pxSelect = tex2D(samp2, uvSelect);

	float y = lerp(pxY.g, pxY.a, pxSelect.a);
	float cr = pxC.r;
	float cb = pxC.b;
	
	return ConvertYCbCrToRGB_709(y, cb, cr);
}

technique hdyc_to_rgb_2_0 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_UYVY_to_RGB_2_0();
		PixelShader = compile ps_2_0 PS_HDYC_to_RGB_2_0();
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_srctexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
		
		Texture[2] = <vd_hevenoddtexture>;
		AddressU[2] = Wrap;
		AddressV[2] = Clamp;
		MinFilter[2] = Point;
		MagFilter[2] = Point;
	}
}

float4 PS_YUY2_to_RGB_2_0(float2 uvY : TEXCOORD0, float2 uvC : TEXCOORD1, float2 uvSelect : TEXCOORD2) : COLOR0 {
	float4 pxY = tex2D(samp0, uvY);
	float4 pxC = tex2D(samp1, uvC);
	float4 pxSelect = tex2D(samp2, uvSelect);

	float y = lerp(pxY.b, pxY.r, pxSelect.a);
	float cr = pxC.a;
	float cb = pxC.g;
	
	return ConvertYCbCrToRGB(y, cb, cr);
}

technique yuy2_to_rgb_2_0 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_UYVY_to_RGB_2_0();
		PixelShader = compile ps_2_0 PS_YUY2_to_RGB_2_0();
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_srctexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
		
		Texture[2] = <vd_hevenoddtexture>;
		AddressU[2] = Wrap;
		AddressV[2] = Clamp;
		MinFilter[2] = Point;
		MagFilter[2] = Point;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	NV12 to RGB -- pixel shader 2.0
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void VS_NV12_to_RGB_2_0(
	float4 pos : POSITION,
	float2 uv : TEXCOORD0,
	float2 uv2 : TEXCOORD1,
	out float4 oPos : POSITION,
	out float2 oT0 : TEXCOORD0,
	out float2 oT1 : TEXCOORD1)
{
	oPos = pos;
	oT0 = uv;
	oT1 = oT0 + vd_texsize.wz * float2(0.25, 0);
}

float4 PS_NV12_to_RGB_2_0(float2 uvY : TEXCOORD0, float2 uvC : TEXCOORD1) : COLOR0 {
	float4 pxY = tex2D(samp0, uvY);
	float4 pxC = tex2D(samp1, uvC);
	
	return ConvertYCbCrToRGB(pxY.g, pxC.g, pxC.a);
}

technique nv12_to_rgb_2_0 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_NV12_to_RGB_2_0();
		PixelShader = compile ps_2_0 PS_NV12_to_RGB_2_0();
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_src2atexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	YCbCr to RGB -- pixel shader 2.0
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void VS_YCbCr_to_RGB_2_0(
	float4 pos : POSITION,
	float2 uv : TEXCOORD0,
	float2 uv2 : TEXCOORD1,
	out float4 oPos : POSITION,
	out float2 oT0 : TEXCOORD0,
	out float2 oT1 : TEXCOORD1,
	uniform float2 scale,
	uniform float2 offset)
{
	oPos = pos;
	oT0 = uv;
	oT1 = (uv2 * scale * vd_srcsize.xy + offset) * vd_tex2size.wz;
}

float4 PS_YCbCr_to_RGB_2_0(float2 uvY : TEXCOORD0, float2 uvC : TEXCOORD1) : COLOR0 {
	float y = tex2D(samp0, uvY).b;
	float cb = tex2D(samp1, uvC).b;
	float cr = tex2D(samp2, uvC).b;
		
	return ConvertYCbCrToRGB(y, cb, cr);
}

technique yvu9_to_rgb_2_0 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_YCbCr_to_RGB_2_0(float2(0.25, 0.25), float2(0, 0));
		PixelShader = compile ps_2_0 PS_YCbCr_to_RGB_2_0();
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_src2atexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
		
		Texture[2] = <vd_src2btexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MinFilter[2] = Linear;
		MagFilter[2] = Linear;
	}
}

technique yv12_to_rgb_2_0 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_YCbCr_to_RGB_2_0(float2(0.5, 0.5), float2(-0.25, 0));
		PixelShader = compile ps_2_0 PS_YCbCr_to_RGB_2_0();
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_src2atexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
		
		Texture[2] = <vd_src2btexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MinFilter[2] = Linear;
		MagFilter[2] = Linear;
	}
}

technique yv16_to_rgb_2_0 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_YCbCr_to_RGB_2_0(float2(0.5, 1), float2(-0.25, 0));
		PixelShader = compile ps_2_0 PS_YCbCr_to_RGB_2_0();
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_src2atexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
		
		Texture[2] = <vd_src2btexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MinFilter[2] = Linear;
		MagFilter[2] = Linear;
	}
}

technique yv24_to_rgb_2_0 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_YCbCr_to_RGB_2_0(float2(1, 1), float2(0, 0));
		PixelShader = compile ps_2_0 PS_YCbCr_to_RGB_2_0();
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_src2atexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
		
		Texture[2] = <vd_src2btexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MinFilter[2] = Linear;
		MagFilter[2] = Linear;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	YCbCr (v210) to RGB -- pixel shader 2.0
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void VS_v210_to_RGB_2_0_A(
	float4 pos : POSITION,
	float2 uv : TEXCOORD0,
	float2 uv2 : TEXCOORD1,
	out float4 oPos : POSITION,
	out float stipple : TEXCOORD0,
	out float2 oT0 : TEXCOORD1,
	uniform float stippleOffset,
	uniform float2 offset0)
{
	oPos = pos;
	stipple = (uv2.x * vd_srcsize.x - stippleOffset) / 6.0f;
	
	float2 uvbase = uv2 * vd_srcsize.xy * float2(4.0f/6.0f, 1.0f) + 0.1f;
	oT0 = (uvbase + offset0) * vd_texsize.wz;
}

void VS_v210_to_RGB_2_0_B(
	float4 pos : POSITION,
	float2 uv : TEXCOORD0,
	float2 uv2 : TEXCOORD1,
	out float4 oPos : POSITION,
	out float stipple : TEXCOORD0,
	out float2 oT0 : TEXCOORD1,
	out float2 oT1 : TEXCOORD2,
	uniform float stippleOffset,
	uniform float2 offset0,
	uniform float2 offset1)
{
	oPos = pos;
	stipple = (uv2.x * vd_srcsize.x - stippleOffset) / 6.0f;

	float2 uvbase = uv2 * vd_srcsize.xy * float2(4.0f/6.0f, 1.0f) + 0.1f;
	oT0 = (uvbase + offset0) * vd_texsize.wz;
	oT1 = (uvbase + offset1) * vd_texsize.wz;
}

void VS_v210_to_RGB_2_0_C(
	float4 pos : POSITION,
	float2 uv : TEXCOORD0,
	float2 uv2 : TEXCOORD1,
	out float4 oPos : POSITION,
	out float stipple : TEXCOORD0,
	out float2 oT0 : TEXCOORD1,
	out float2 oT1 : TEXCOORD2,
	out float2 oT2 : TEXCOORD3,
	uniform float stippleOffset,
	uniform float2 offset0,
	uniform float2 offset1,
	uniform float2 offset2)
{
	oPos = pos;
	stipple = (uv2.x * vd_srcsize.x - stippleOffset) / 6.0f;

	float2 uvbase = uv2 * vd_srcsize.xy * float2(4.0f/6.0f, 1.0f) + 0.1f;
	oT0 = (uvbase + offset0) * vd_texsize.wz;
	oT1 = (uvbase + offset1) * vd_texsize.wz;
	oT2 = (uvbase + offset2) * vd_texsize.wz;
}

//           A   B   G   R
// dword 0: XX Cr0  Y0 Cb0
// dword 1: XX  Y2 Cb2  Y1
// dword 2: XX Cb4  Y3 Cr2
// dword 3: XX  Y5 Cr4  Y4
// dword 4: XX Cr6  Y6 Cb6

float4 PS_v210_to_RGB_2_0_pass0(float stipple : TEXCOORD0, float2 t0 : TEXCOORD1) : COLOR0 {
	clip((1.0f / 6.0f) - frac(stipple));

	float4 d0 = tex2D(samp0, t0);
		
	return ConvertYCbCrToRGB(d0.g, d0.r, d0.b);
}

float4 PS_v210_to_RGB_2_0_pass1(float stipple : TEXCOORD0, float2 t0 : TEXCOORD1, float2 t1 : TEXCOORD2, float2 t2 : TEXCOORD3) : COLOR0 {
	clip((1.0f / 6.0f) - frac(stipple));

	float4 d0 = tex2D(samp0, t0);
	float4 d1 = tex2D(samp0, t1);
	float4 d2 = tex2D(samp0, t2);
		
	return ConvertYCbCrToRGB(d1.r, 0.5f*(d0.r + d1.g), 0.5f*(d0.b + d2.r));
}

float4 PS_v210_to_RGB_2_0_pass2(float stipple : TEXCOORD0, float2 t0 : TEXCOORD1, float2 t1 : TEXCOORD2) : COLOR0 {
	clip((1.0f / 6.0f) - frac(stipple));

	float4 d1 = tex2D(samp0, t0);
	float4 d2 = tex2D(samp0, t1);
		
	return ConvertYCbCrToRGB(d1.b, d1.g, d2.r);
}

float4 PS_v210_to_RGB_2_0_pass3(float stipple : TEXCOORD0, float2 t0 : TEXCOORD1, float2 t1 : TEXCOORD2, float2 t2 : TEXCOORD3) : COLOR0 {
	clip((1.0f / 6.0f) - frac(stipple));

	float4 d1 = tex2D(samp0, t0);
	float4 d2 = tex2D(samp0, t1);
	float4 d3 = tex2D(samp0, t2);
		
	return ConvertYCbCrToRGB(d2.g, 0.5f*(d1.g + d2.b), 0.5f*(d2.r + d3.g));
}

float4 PS_v210_to_RGB_2_0_pass4(float stipple : TEXCOORD0, float2 t0 : TEXCOORD1, float2 t1 : TEXCOORD2) : COLOR0 {
	clip((1.0f / 6.0f) - frac(stipple));

	float4 d2 = tex2D(samp0, t0);
	float4 d3 = tex2D(samp0, t1);
		
	return ConvertYCbCrToRGB(d3.r, d2.b, d3.g);
}

float4 PS_v210_to_RGB_2_0_pass5(float stipple : TEXCOORD0, float2 t0 : TEXCOORD1, float2 t1 : TEXCOORD2, float2 t2 : TEXCOORD3) : COLOR0 {
	clip((1.0f / 6.0f) - frac(stipple));

	float4 d2 = tex2D(samp0, t0);
	float4 d3 = tex2D(samp0, t1);
	float4 d4 = tex2D(samp0, t2);
		
	return ConvertYCbCrToRGB(d3.b, 0.5f*(d2.b + d4.r), 0.5f*(d3.g + d4.b));
}

technique v210_to_rgb_2_0 {
	// pass 0 - base offset 0
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_v210_to_RGB_2_0_A(0, 0);
		PixelShader = compile ps_2_0 PS_v210_to_RGB_2_0_pass0();
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_srctexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Point;
		MagFilter[1] = Point;
		
		Texture[2] = <vd_srctexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MinFilter[2] = Point;
		MagFilter[2] = Point;
	}
	
	// pass 1 - base offset 1
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_v210_to_RGB_2_0_C(1, -1, 0, 1);
		PixelShader = compile ps_2_0 PS_v210_to_RGB_2_0_pass1();
	}
	
	// pass 2 - base offset 1
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_v210_to_RGB_2_0_B(2, 0, 1);
		PixelShader = compile ps_2_0 PS_v210_to_RGB_2_0_pass2();
	}
	
	// pass 3 - base offset 2
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_v210_to_RGB_2_0_C(3, -1, 0, 1);
		PixelShader = compile ps_2_0 PS_v210_to_RGB_2_0_pass3();
	}
	
	// pass 4 - base offset 3
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_v210_to_RGB_2_0_B(4, -1, 0);
		PixelShader = compile ps_2_0 PS_v210_to_RGB_2_0_pass4();
	}
	
	// pass 5 - base offset 3
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_2_0 VS_v210_to_RGB_2_0_C(5, -1, 0, 1);
		PixelShader = compile ps_2_0 PS_v210_to_RGB_2_0_pass5();
	}	
}

#endif
