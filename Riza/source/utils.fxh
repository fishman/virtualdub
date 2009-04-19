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

#ifndef UTILS_FXH
#define UTILS_FXH

float4 ConvertYCbCrToRGB(float y, float cb, float cr) {
	const float3 kCoeffCr = { 1.596f, -0.813f, 0 };
	const float3 kCoeffCb = { 0, -0.391f, 2.018f };
	const float kCoeffY = 1.164f;
	const float kBiasY = -16.0f / 255.0f;
	const float kBiasC = -128.0f / 255.0f;

	float4 result = y * kCoeffY;
	result.rgb += kCoeffCr * cr;
	result.rgb += kCoeffCb * cb;
	result.rgb += kCoeffY * kBiasY + (kCoeffCr + kCoeffCb) * kBiasC;	
	
	return result;
}

float4 ConvertYCbCrToRGB_709(float y, float cb, float cr) {
	const float3 kCoeffCr = { 1.793f, -0.533f, 0 };
	const float3 kCoeffCb = { 0, -0.213f, 2.112f };
	const float kCoeffY = 1.164f;
	const float kBiasY = -16.0f / 255.0f;
	const float kBiasC = -128.0f / 255.0f;

	float4 result = y * kCoeffY;
	result.rgb += kCoeffCr * cr;
	result.rgb += kCoeffCb * cb;
	result.rgb += kCoeffY * kBiasY + (kCoeffCr + kCoeffCb) * kBiasC;	
	
	return result;
}

#endif
