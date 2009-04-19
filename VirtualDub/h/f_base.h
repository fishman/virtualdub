//	VDShader - Custom shader video filter for VirtualDub
//	Copyright (C) 2007 Avery Lee
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

#ifndef f_VD2_F_BASE_H
#define f_VD2_F_BASE_H

#include <stdlib.h>
#include <stddef.h>
#include <new>

#include <vd2/plugin/vdvideofilt.h>

class VDVFilterDialog {
public:
	VDVFilterDialog();

protected:
	HWND	mhdlg;

	LRESULT Show(HINSTANCE hInst, LPCTSTR templName, HWND parent);
	static INT_PTR CALLBACK StaticDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
};

///////////////////////////////////////////////////////////////////////////
//
//	class VDVFilter
//
//	This class handles most of the grimy work of creating the interface
//	between your filter and VirtualDub.

class VDVFilterBase {
public:
	VDVFilterBase();
	virtual ~VDVFilterBase();

	void SetHooks(VDXFilterActivation *fa, const VDXFilterFunctions *ff);

	// linkage routines

	virtual bool Init();
	virtual long GetParams()=0;
	virtual void Start();
	virtual bool Run() = 0;
	virtual void End();
	virtual bool Configure(VDXHWND hwnd);
	virtual void GetSettingString(char *buf, int maxlen);
	virtual void GetScriptString(char *buf, int maxlen);
	virtual int Serialize(char *buf, int maxbuf);
	virtual int Deserialize(const char *buf, int maxbuf);

	static void __cdecl FilterDeinit   (VDXFilterActivation *fa, const VDXFilterFunctions *ff);
	static int  __cdecl FilterRun      (const VDXFilterActivation *fa, const VDXFilterFunctions *ff);
	static long __cdecl FilterParam    (VDXFilterActivation *fa, const VDXFilterFunctions *ff);
	static int  __cdecl FilterConfig   (VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hWnd);
	static int  __cdecl FilterStart    (VDXFilterActivation *fa, const VDXFilterFunctions *ff);
	static int  __cdecl FilterEnd      (VDXFilterActivation *fa, const VDXFilterFunctions *ff);
	static bool __cdecl FilterScriptStr(VDXFilterActivation *fa, const VDXFilterFunctions *, char *, int);
	static void __cdecl FilterString2  (const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxlen);
	static int  __cdecl FilterSerialize    (VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxbuf);
	static void __cdecl FilterDeserialize  (VDXFilterActivation *fa, const VDXFilterFunctions *ff, const char *buf, int maxbuf);

	// member variables
	VDXFilterActivation *fa;
	const VDXFilterFunctions *ff;

	enum { sScriptMethods = 0 };
};

///////////////////////////////////////////////////////////////////////////
//
//	class VDVFilter
//
//	This is the class you should derive your filter from -- pass your class
//	as the template parameter.  It is critical that this be the most
//	derived name for your class, as it is the one used for placement new
//	and placement delete!
//
template<class T>
class VDVFilter : public VDVFilterBase {
public:
	static int  __cdecl FilterInit     (VDXFilterActivation *fa, const VDXFilterFunctions *ff);
	static void __cdecl FilterCopy         (VDXFilterActivation *fa, const VDXFilterFunctions *ff, void *dst);
};

template<class T>
int  __cdecl VDVFilter<T>::FilterInit     (VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	T *pThis = new(fa->filter_data) T;
	pThis->SetHooks(fa, ff);

	try {
		if (!pThis->Init()) {
			pThis->~T();
			return 1;
		}

		return 0;
	} catch(...) {
		pThis->~T();
		throw;
	}
}

template<class T>
void __cdecl VDVFilter<T>::FilterCopy         (VDXFilterActivation *fa, const VDXFilterFunctions *ff, void *dst) {
	new(dst) T(*static_cast<T *>(reinterpret_cast<VDVFilterBase *>(fa->filter_data)));
	((T *)dst)->ff = ff;
}

template<class T>
class VDVFilterScriptObjectAdapter {
public:
	static const VDXScriptObject sScriptObject;
};

template<class T>
const VDXScriptObject VDVFilterScriptObjectAdapter<T>::sScriptObject = {
	NULL, const_cast<ScriptFunctionDef *>(T::sScriptMethods), NULL
};

///////////////////////////////////////////////////////////////////////////
//
//	class VDVFilterDefinition
//
//	This template creates the FilterDefinition structure for you based on
//	your filter class.
//
template<class T>
class VDVFilterDefinition : public VDXFilterDefinition {
public:
	VDVFilterDefinition(const char *pszAuthor, const char *pszName, const char *pszDescription) {
		name			= pszName;
		desc			= pszDescription;
		maker			= pszAuthor;
		private_data	= NULL;
		inst_data_size	= sizeof(T);

		initProc		= T::FilterInit;
		deinitProc		= T::FilterDeinit;
		runProc			= T::FilterRun;
		paramProc		= T::FilterParam;
		configProc		= T::FilterConfig;
		stringProc		= NULL;
		startProc		= T::FilterStart;
		endProc			= T::FilterEnd;

		script_obj		= T::sScriptMethods ? const_cast<CScriptObject *>(&VDVFilterScriptObjectAdapter<T>::sScriptObject) : 0;
		fssProc			= T::FilterScriptStr;

		stringProc2		= T::FilterString2;
		serializeProc	= T::FilterSerialize;
		deserializeProc	= T::FilterDeserialize;
		copyProc		= T::FilterCopy;

		prefetchProc	= NULL;
	}
};

#endif
