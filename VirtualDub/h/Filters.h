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

#ifndef f_FILTERS_H
#define f_FILTERS_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <malloc.h>

#include <windows.h>

#include <list>
#include <vector>
#include <vd2/system/list.h>
#include <vd2/system/error.h>
#include <vd2/system/VDString.h>
#include <vd2/system/refcount.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/VDLib/win32/DIBSection.h>
#include <vd2/VDLib/win32/FileMapping.h>
#include <vd2/VDLib/ParameterCurve.h>
#include "VBitmap.h"
#include "FilterSystem.h"
#include "filter.h"
#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "gui.h"

//////////////////

class IVDVideoDisplay;
class IVDPositionControl;
struct VDWaveFormat;
class VDTimeline;
struct VDXWaveFormat;
struct VDXFilterDefinition;
class VDExternalModule;

///////////////////

VDXWaveFormat *VDXCopyWaveFormat(const VDXWaveFormat *pFormat);

///////////////////

class FilterDefinitionInstance : public ListNode2<FilterDefinitionInstance> {
public:
	FilterDefinitionInstance(VDExternalModule *pfm);
	~FilterDefinitionInstance();

	void Assign(const FilterDefinition& def, int len);

	const FilterDefinition& Attach();
	void Detach();

	const FilterDefinition& GetDef() const { return mDef; }
	VDExternalModule	*GetModule() const { return mpExtModule; }

	const VDStringA&	GetName() const { return mName; }
	const VDStringA&	GetAuthor() const { return mAuthor; }
	const VDStringA&	GetDescription() const { return mDescription; }

protected:
	VDExternalModule	*mpExtModule;
	FilterDefinition	mDef;
	VDAtomicInt			mRefCount;
	VDStringA			mName;
	VDStringA			mAuthor;
	VDStringA			mDescription;
};

//////////

extern List			g_listFA;

extern FilterSystem	filters;

VDXFilterDefinition *FilterAdd(VDXFilterModule *fm, VDXFilterDefinition *pfd, int fd_len);
void				FilterAddBuiltin(const VDXFilterDefinition *pfd);
void				FilterRemove(VDXFilterDefinition *fd);

struct FilterBlurb {
	FilterDefinitionInstance	*key;
	VDStringA					name;
	VDStringA					author;
	VDStringA					description;
};

void				FilterEnumerateFilters(std::list<FilterBlurb>& blurbs);


LONG FilterGetSingleValue(HWND hWnd, LONG cVal, LONG lMin, LONG lMax, char *title, IVDXFilterPreview2 *ifp2, void (*pUpdateFunction)(long value, void *data), void *pUpdateFunctionData);

#endif
