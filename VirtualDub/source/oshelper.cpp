//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2007 Avery Lee
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

#include <windows.h>
#include <shellapi.h>
#include <vd2/system/VDString.h>
#include <vd2/system/filesys.h>
#include <vd2/system/zip.h>
#include <vd2/system/Error.h>
#include <vd2/system/w32assist.h>
#include "oshelper.h"
#include "version.h"

extern const char g_szError[];

void Draw3DRect(HDC hDC, LONG x, LONG y, LONG dx, LONG dy, BOOL inverted) {
	HPEN hPenOld;

	hPenOld = (HPEN)SelectObject(hDC, GetStockObject(inverted ? WHITE_PEN : BLACK_PEN));
	MoveToEx(hDC, x, y+dy-1, NULL);
	LineTo(hDC, x+dx-1, y+dy-1);
	LineTo(hDC, x+dx-1, y);
	DeleteObject(SelectObject(hDC, GetStockObject(inverted ? BLACK_PEN : WHITE_PEN)));
	MoveToEx(hDC, x, y+dy-1, NULL);
	LineTo(hDC, x, y);
	LineTo(hDC, x+dx-1, y);
	DeleteObject(SelectObject(hDC, hPenOld));
}

// We follow MAME32's lead and put our keys in:
//
//	HKEY_CURRENT_USER\Software\Freeware\VirtualDub\

HKEY OpenConfigKey(const char *szKeyName) {
	char temp[MAX_PATH]="Software\\Freeware\\VirtualDub";
	HKEY hkey;

	if (szKeyName) {
		strcat(temp, "\\");
		strcat(temp, szKeyName);
	}

	return RegOpenKeyEx(HKEY_CURRENT_USER, temp, 0, KEY_ALL_ACCESS, &hkey)==ERROR_SUCCESS
			? hkey
			: NULL;
}

HKEY CreateConfigKey(const char *szKeyName) {
	char temp[MAX_PATH]="Software\\Freeware\\VirtualDub";
	HKEY hkey;
	DWORD dwDisposition;

	if (szKeyName) {
		strcat(temp, "\\");
		strcat(temp, szKeyName);
	}

	return RegCreateKeyEx(HKEY_CURRENT_USER, temp, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, &dwDisposition)==ERROR_SUCCESS
			? hkey
			: NULL;
}

BOOL DeleteConfigValue(const char *szKeyName, const char *szValueName) {
	HKEY hkey;
	BOOL success;

	if (!(hkey = OpenConfigKey(szKeyName)))
		return FALSE;

	success = (RegDeleteValue(hkey, szValueName) == ERROR_SUCCESS);

	RegCloseKey(hkey);

	return success;
}

BOOL QueryConfigString(const char *szKeyName, const char *szValueName, char *lpBuffer, int cbBuffer) {
	HKEY hkey;
	BOOL success;
	DWORD type;

	if (!(hkey = OpenConfigKey(szKeyName)))
		return FALSE;

	success = (ERROR_SUCCESS == RegQueryValueEx(hkey, szValueName, 0, &type, (LPBYTE)lpBuffer, (LPDWORD)&cbBuffer));

	RegCloseKey(hkey);

	return success;
}

DWORD QueryConfigBinary(const char *szKeyName, const char *szValueName, char *lpBuffer, int cbBuffer) {
	HKEY hkey;
	BOOL success;
	DWORD type;
	DWORD size = cbBuffer;

	if (!(hkey = OpenConfigKey(szKeyName)))
		return 0;

	success = (ERROR_SUCCESS == RegQueryValueEx(hkey, szValueName, 0, &type, (LPBYTE)lpBuffer, (LPDWORD)&size));

	RegCloseKey(hkey);

	return success ? size : 0;
}

BOOL QueryConfigDword(const char *szKeyName, const char *szValueName, DWORD *lpdwData) {
	HKEY hkey;
	BOOL success;
	DWORD type;
	DWORD size = sizeof(DWORD);

	if (!(hkey = OpenConfigKey(szKeyName)))
		return 0;

	success = (ERROR_SUCCESS == RegQueryValueEx(hkey, szValueName, 0, &type, (LPBYTE)lpdwData, (LPDWORD)&size));

	RegCloseKey(hkey);

	return success;
}

BOOL SetConfigString(const char *szKeyName, const char *szValueName, const char *lpBuffer) {
	HKEY hkey;
	BOOL success;

	if (!(hkey = CreateConfigKey(szKeyName)))
		return FALSE;

	success = (ERROR_SUCCESS == RegSetValueEx(hkey, szValueName, 0, REG_SZ, (LPBYTE)lpBuffer, strlen(lpBuffer)+1));

	RegCloseKey(hkey);

	return success;
}

BOOL SetConfigBinary(const char *szKeyName, const char *szValueName, const char *lpBuffer, int cbBuffer) {
	HKEY hkey;
	BOOL success;

	if (!(hkey = CreateConfigKey(szKeyName)))
		return FALSE;

	success = (ERROR_SUCCESS == RegSetValueEx(hkey, szValueName, 0, REG_BINARY, (LPBYTE)lpBuffer, cbBuffer));

	RegCloseKey(hkey);

	return success;
}

BOOL SetConfigDword(const char *szKeyName, const char *szValueName, DWORD dwData) {
	HKEY hkey;
	BOOL success;

	if (!(hkey = CreateConfigKey(szKeyName)))
		return FALSE;

	success = (ERROR_SUCCESS == RegSetValueEx(hkey, szValueName, 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD)));

	RegCloseKey(hkey);

	return success;
}

///////////////////////////////////////////////////////////////////////////
//
//	help support
//
///////////////////////////////////////////////////////////////////////////

VDStringW VDGetHelpPath() {
	return VDMakePath(VDGetProgramPath().c_str(), L"VirtualDub.chm");
}

void VDShowHelp(HWND hwnd, const wchar_t *filename) {
	try {
		VDStringW helpFile(VDGetHelpPath());

		if (!VDDoesPathExist(helpFile.c_str()))
			throw MyError("Cannot find help file: %ls", helpFile.c_str());

		// If we're on Windows NT, check for the ADS and/or network drive.
		if (VDIsWindowsNT()) {
			VDStringW helpFileADS(helpFile);
			helpFileADS += L":Zone.Identifier";
			if (VDDoesPathExist(helpFileADS.c_str())) {
				int rv = MessageBox(hwnd, "VirtualDub has detected that its help file, VirtualDub.chm, has an Internet Explorer download location marker on it. This may prevent the help file from being displayed properly, resulting in \"Action canceled\" errors being displayed. Would you like to remove it?", "VirtualDub warning", MB_YESNO|MB_ICONEXCLAMATION);

				if (rv == IDYES)
					DeleteFileW(helpFileADS.c_str());
			}
		}

		if (filename) {
			helpFile.append(L"::/");
			helpFile.append(filename);
		}

		VDStringW helpCommand(VDStringW(L"\"hh.exe\" \"") + helpFile + L'"');

		PROCESS_INFORMATION pi;
		BOOL retval;

		// CreateProcess will actually modify the string that it gets, soo....
		if (VDIsWindowsNT()) {
			STARTUPINFOW si = {sizeof(STARTUPINFOW)};
			std::vector<wchar_t> tempbufW(helpCommand.size() + 1, 0);
			helpCommand.copy(&tempbufW[0], tempbufW.size());
			retval = CreateProcessW(NULL, &tempbufW[0], NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &si, &pi);
		} else {
			STARTUPINFOA si = {sizeof(STARTUPINFOA)};
			VDStringA strA(VDTextWToA(helpCommand));
			std::vector<char> tempbufA(strA.size() + 1, 0);
			strA.copy(&tempbufA[0], tempbufA.size());
			retval = CreateProcessA(NULL, &tempbufA[0], NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &si, &pi);
		}

		if (retval) {
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		} else
			throw MyWin32Error("Cannot launch HTML Help: %%s", GetLastError());
	} catch(const MyError& e) {
		e.post(hwnd, g_szError);
	}
}

bool IsFilenameOnFATVolume(const wchar_t *pszFilename) {
	VDStringW rootPath(VDFileGetRootPath(pszFilename));

	if (VDIsWindowsNT()) {
		DWORD dwMaxComponentLength;
		DWORD dwFSFlags;
		wchar_t szFilesystem[MAX_PATH];

		if (!GetVolumeInformationW(rootPath.c_str(),
				NULL, 0,		// Volume name buffer
				NULL,			// Serial number buffer
				&dwMaxComponentLength,
				&dwFSFlags,
				szFilesystem,
				MAX_PATH))
			return false;

		return !_wcsnicmp(szFilesystem, L"FAT", 3);
	} else {
		DWORD dwMaxComponentLength;
		DWORD dwFSFlags;
		char szFilesystem[MAX_PATH];

		if (!GetVolumeInformationA(VDTextWToA(rootPath).c_str(),
				NULL, 0,		// Volume name buffer
				NULL,			// Serial number buffer
				&dwMaxComponentLength,
				&dwFSFlags,
				szFilesystem,
				sizeof szFilesystem))
			return false;

		return !_strnicmp(szFilesystem, "FAT", 3);
	}
}

HWND APIENTRY VDGetAncestorW95(HWND hwnd, UINT gaFlags) {
	switch(gaFlags) {
	case GA_PARENT:
		return GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD ? GetParent(hwnd) : NULL;
	case GA_ROOT:
		while(GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD)
			hwnd = GetParent(hwnd);
		return hwnd;
	case GA_ROOTOWNER:
		while(HWND hwndParent = GetParent(hwnd))
			hwnd = hwndParent;
		return hwnd;
	default:
		VDNEVERHERE;
		return NULL;
	}
}

HWND APIENTRY VDGetAncestorW98(HWND hwnd, UINT gaFlags);

namespace {
	HWND APIENTRY VDGetAncestorAutodetect(HWND hwnd, UINT gaFlags);

	typedef HWND (APIENTRY *tpGetAncestor)(HWND, UINT);
	tpGetAncestor g_pVDGetAncestor = VDGetAncestorAutodetect;
	tpGetAncestor g_pVDGetAncestorRaw = VDGetAncestorW95;

	HWND APIENTRY VDGetAncestorAutodetect(HWND hwnd, UINT gaFlags) {
		tpGetAncestor ga = (tpGetAncestor)GetProcAddress(GetModuleHandle("user32"), "GetAncestor");

		if (!ga)
			ga = VDGetAncestorW95;
		else if (!VDIsWindowsNT()) {
			g_pVDGetAncestorRaw = ga;
			ga = VDGetAncestorW98;
		}

		g_pVDGetAncestor = ga;
		return ga(hwnd, gaFlags);
	}
}

HWND APIENTRY VDGetAncestorW98(HWND hwnd, UINT gaFlags) {
	// Believe it or not, HWND_MESSAGE works under Windows 98 -- and if you call GetAncestor()
	// on such a window, user32 crashes in 16-bit code. :(
	if (gaFlags == GA_ROOT && GetParent(hwnd) == NULL)
		return hwnd;

	return g_pVDGetAncestorRaw(hwnd, gaFlags);
}

HWND VDGetAncestorW32(HWND hwnd, UINT gaFlags) {
	return g_pVDGetAncestor(hwnd, gaFlags);
}

VDStringW VDLoadStringW32(UINT uID, bool doSubstitutions) {
	// Credit for this function goes to Raymond Chen, who described how
	// to directly access string resources in his blog.

	VDStringW str;

	HRSRC hrsrc = FindResourceEx(NULL, RT_STRING, MAKEINTRESOURCE(1 + (uID >> 4)), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
	if (hrsrc) {
		HGLOBAL hglob = LoadResource(NULL, hrsrc);
		if (hglob) {
			LPCWSTR pTable = (LPCWSTR)LockResource(hglob);
			if (pTable) {
				uID &= 15;

				while(uID--)
					pTable += 1 + (UINT)*pTable;

				str.assign(pTable+1, (UINT)*pTable);
			}
			// UnlockResource() is a NOP in Win32.
		}
		// FreeResource() is a NOP in Win32.
	}

	if (doSubstitutions)
		VDSubstituteStrings(str);

	return str;
}

void VDSubstituteStrings(VDStringW& s) {
	VDStringW::size_type posLast = 0;
	VDStringW::size_type pos = s.find('$');
	if (pos == VDStringW::npos)
		return;

	VDStringW t;

	for(;;) {
		t.append(s, posLast, pos - posLast);

		if (pos == VDStringW::npos || pos+1 >= s.size())
			break;

		wchar_t c = s[pos+1];
		switch(c) {
			case L'b':
				t.append_sprintf(L"%d", version_num);
				break;

			case L'n':
				t.append(VD_PROGRAM_NAMEW);
				break;

			case L'v':
				t.append(VD_PROGRAM_VERSIONW);
				break;

			case L's':
				t.append(VD_PROGRAM_SPECIAL_BUILDW);
				break;

			case L'c':
				t.append(VD_PROGRAM_CONFIGW);
				break;

			case L'C':
				t.append(VD_PROGRAM_GENERIC_CONFIGW);
				break;

			case L'p':
				t.append(VD_PROGRAM_PLATFORM_NAMEW);
				break;

			case L'k':
				#if VD_COMPILER_MSVC >= 1500
					#if VD_CPU_AMD64
						t.append(L"Microsoft Visual Studio 2008 for AMD64");
					#else
						t.append(L"Microsoft Visual Studio 2008 for X86");
					#endif
				#elif VD_COMPILER_MSVC >= 1400
					#if VD_CPU_AMD64
						#if VD_COMPILER_MSVC_VC8_DDK
							t.append(L"Microsoft Visual C++ 8.0 for AMD64 (DDK version)");
						#elif VD_COMPILER_MSVC_VC8_PSDK
							t.append(L"Microsoft Visual C++ 8.0 for AMD64 (PSDK version)");
						#else
							t.append(L"Microsoft Visual Studio 2005 for AMD64");
						#endif
					#else
						t.append(L"Microsoft Visual Studio 2005 for X86");
					#endif
				#else
					t.append("Unknown compiler");
				#endif
				break;
		}

		posLast = pos + 2;
		pos = s.find('$', posLast);
	}

	s = t;
}

///////////////////////////////////////////////////////////////////////////

void LaunchURL(const char *pURL) {
	ShellExecute(NULL, "open", pURL, NULL, NULL, SW_SHOWNORMAL);
}

///////////////////////////////////////////////////////////////////////////

static void ExitWindowsExDammit(UINT uFlags, DWORD dwReserved) {
	if (!(GetVersion()&0x80000000)) {
		HANDLE h;

		if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &h)) {
			LUID luid;

			if (LookupPrivilegeValue(NULL, "SeShutdownPrivilege", &luid)) {
				TOKEN_PRIVILEGES tp;
				tp.PrivilegeCount = 1;
				tp.Privileges[0].Luid = luid;
				tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

				AdjustTokenPrivileges(h, FALSE, &tp, 0, NULL, NULL);
			}

			CloseHandle(h);
		}
	}

	ExitWindowsEx(uFlags, dwReserved);
}

void VDInitiateSystemShutdown() {
	// In theory, this is an illegal combination of flags, but it
	// seems to be necessary to properly power off both Windows 98
	// and Windows XP.  In particular, Windows 98 just logs off if
	// you try EWX_POWEROFF.  Joy.
	ExitWindowsExDammit(EWX_SHUTDOWN|EWX_POWEROFF|EWX_FORCEIFHUNG, 0);
}

///////////////////////////////////////////////////////////////////////////

bool VDEnableCPUTracking() {
	HKEY hOpen;
	DWORD cbData;
	DWORD dwType;
	LPBYTE pByte;
	DWORD rc;

	bool fSuccess = true;

    if ( (rc = RegOpenKeyEx(HKEY_DYN_DATA,"PerfStats\\StartStat", 0,
					KEY_READ, &hOpen)) == ERROR_SUCCESS) {

		// query to get data size
		if ( (rc = RegQueryValueEx(hOpen,"KERNEL\\CPUUsage",NULL,&dwType,
				NULL, &cbData )) == ERROR_SUCCESS) {

			pByte = (LPBYTE)allocmem(cbData);

			rc = RegQueryValueEx(hOpen,"KERNEL\\CPUUsage",NULL,&dwType, pByte,
                              &cbData );

			freemem(pByte);
		} else
			fSuccess = false;

		RegCloseKey(hOpen);
	} else
		fSuccess = false;

	return fSuccess;
}

bool VDDisableCPUTracking() {
	HKEY hOpen;
	DWORD cbData;
	DWORD dwType;
	LPBYTE pByte;
	DWORD rc;

	bool fSuccess = true;

    if ( (rc = RegOpenKeyEx(HKEY_DYN_DATA,"PerfStats\\StopStat", 0,
					KEY_READ, &hOpen)) == ERROR_SUCCESS) {

		// query to get data size
		if ( (rc = RegQueryValueEx(hOpen,"KERNEL\\CPUUsage",NULL,&dwType,
				NULL, &cbData )) == ERROR_SUCCESS) {

			pByte = (LPBYTE)allocmem(cbData);

			rc = RegQueryValueEx(hOpen,"KERNEL\\CPUUsage",NULL,&dwType, pByte,
                              &cbData );

			freemem(pByte);
		} else
			fSuccess = false;

		RegCloseKey(hOpen);
	} else
		fSuccess = false;

	return fSuccess;
}

VDCPUUsageReader::VDCPUUsageReader()
	: hkeyKernelCPU(NULL)
{
}

VDCPUUsageReader::~VDCPUUsageReader() {
	Shutdown();
}

void VDCPUUsageReader::Init() {
	FILETIME ftCreate, ftExit;

	hkeyKernelCPU = NULL;
	fNTMethod = false;

	if (GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, (FILETIME *)&kt_last, (FILETIME *)&ut_last)) {

		// Using Windows NT/2000 method
		GetSystemTimeAsFileTime((FILETIME *)&st_last);

		fNTMethod = true;

	} else {

		// Using Windows 95/98 method

		HKEY hkey;

		if (VDEnableCPUTracking()) {

			if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_DYN_DATA, "PerfStats\\StatData", 0, KEY_READ, &hkey)) {
				hkeyKernelCPU = hkey;
			} else
				VDDisableCPUTracking();
		}
	}
}

void VDCPUUsageReader::Shutdown() {
	if (hkeyKernelCPU) {
		RegCloseKey(hkeyKernelCPU);
		VDDisableCPUTracking();
	}
}

int VDCPUUsageReader::read() {

	if (hkeyKernelCPU) {
		DWORD type;
		DWORD dwUsage;
		DWORD size = sizeof dwUsage;

		if (ERROR_SUCCESS == RegQueryValueEx(hkeyKernelCPU, "KERNEL\\CPUUsage", 0, &type, (LPBYTE)&dwUsage, (LPDWORD)&size))
			return (int)dwUsage;
		
		return -1;
	} else if (fNTMethod) {
		FILETIME ftCreate, ftExit;
		unsigned __int64 kt, st, ut;
		int cpu;

		GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, (FILETIME *)&kt, (FILETIME *)&ut);
		GetSystemTimeAsFileTime((FILETIME *)&st);

		if (st == st_last)
			return 100;
		else
			cpu = (int)((100 * (kt + ut - kt_last - ut_last) + (st - st_last)/2) / (st - st_last));

		kt_last = kt;
		ut_last = ut;
		st_last = st;

		return cpu;
	}

	return -1;
}
