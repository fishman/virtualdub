//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#ifndef f_VIDEODISPLAYDRIVERS_H
#define f_VIDEODISPLAYDRIVERS_H

#include <windows.h>
#include <vd2/system/vectors.h>
#include <vd2/Kasumi/pixmap.h>

struct VDVideoDisplaySourceInfo {
	VDPixmap	pixmap;
	int			bpp;
	int			bpr;
	void		*pSharedObject;
	ptrdiff_t	sharedOffset;
	bool		bAllowConversion;
	bool		bPersistent;
	bool		bInterlaced;
};


class IVDVideoDisplayMinidriver {
public:
	enum UpdateMode {
		kModeNone		= 0x00000000,
		kModeEvenField	= 0x00000001,
		kModeOddField	= 0x00000002,
		kModeAllFields	= 0x00000003,
		kModeFieldMask	= 0x00000003,
		kModeVSync		= 0x00000004,
		kModeAll		= 0x00000007
	};

	enum FilterMode {
		kFilterAnySuitable,
		kFilterPoint,
		kFilterBilinear,
		kFilterBicubic,
		kFilterModeCount
	};

	virtual ~IVDVideoDisplayMinidriver() {}

	virtual bool Init(HWND hwnd, const VDVideoDisplaySourceInfo& info) = 0;
	virtual void Shutdown() = 0;

	virtual bool ModifySource(const VDVideoDisplaySourceInfo& info) = 0;

	virtual bool IsValid() = 0;
	virtual void SetFilterMode(FilterMode mode) = 0;

	virtual bool Tick(int id) = 0;
	virtual bool Resize() = 0;
	virtual bool Update(UpdateMode) = 0;
	virtual void Refresh(UpdateMode) = 0;
	virtual bool Paint(HDC hdc, const RECT& rClient, UpdateMode lastUpdateMode) = 0;

	virtual bool SetSubrect(const vdrect32 *r) = 0;
	virtual void SetLogicalPalette(const uint8 *pLogicalPalette) = 0;
};

IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverOpenGL();
IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverDirectDraw();
IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverGDI();
IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverDX9();

#endif
