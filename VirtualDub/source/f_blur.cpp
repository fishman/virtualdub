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
#include <vd2/system/vdalloc.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/VDXFrame/VideoFilter.h>

#include "Effect.h"
#include "e_blur.h"

class VDVFilterBlurBase : public VDXVideoFilter {
public:
	uint32 GetParams();
	void End();
	void Run();

protected:
	vdautoptr<VEffect> mpEffect;
};

uint32 VDVFilterBlurBase::GetParams() {
	fa->dst.offset = fa->src.offset;
	return 0;
}

void VDVFilterBlurBase::End() {
	mpEffect = NULL;
}

void VDVFilterBlurBase::Run() {
	if (mpEffect)
		mpEffect->run((VBitmap *)&fa->dst);
}

///////////////////////////////////////////////////////////////////////////////

class VDVFilterBlur : public VDVFilterBlurBase {
public:
	void Start();
};

void VDVFilterBlur::Start() {
	mpEffect = VCreateEffectBlur((VBitmap *)&fa->dst);
	if (!mpEffect)
		ff->ExceptOutOfMemory();
}

///////////////////////////////////////////////////////////////////////////////

class VDVFilterBlurHi : public VDVFilterBlurBase {
public:
	void Start();
};

void VDVFilterBlurHi::Start() {
	mpEffect = VCreateEffectBlurHi((VBitmap *)&fa->dst);
	if (!mpEffect)
		ff->ExceptOutOfMemory();
}

///////////////////////////////////////////////////////////////////////////////

extern const VDXFilterDefinition filterDef_blur = VDXVideoFilterDefinition<VDVFilterBlur>(
		NULL,
		"blur",
		"Applies a radius-1 gaussian blur to the image.");

extern const VDXFilterDefinition filterDef_blurhi = VDXVideoFilterDefinition<VDVFilterBlurHi>(
		NULL,
		"blur more",
		"Applies a radius-2 gaussian blur to the image.");

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)
