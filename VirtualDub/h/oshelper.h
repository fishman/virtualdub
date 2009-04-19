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

#ifndef f_OSHELPER_H
#define f_OSHELPER_H

#include <windows.h>

#include <vd2/system/VDString.h>

void Draw3DRect(HDC hDC, LONG x, LONG y, LONG dx, LONG dy, BOOL inverted);

HKEY OpenConfigKey(const char *szKeyName);
HKEY CreateConfigKey(const char *szKeyName);
BOOL DeleteConfigValue(const char *szKeyName, const char *szValueName);
BOOL QueryConfigString(const char *szKeyName, const char *szValueName, char *lpBuffer, int cbBuffer);
DWORD QueryConfigBinary(const char *szKeyName, const char *szValueName, char *lpBuffer, int cbBuffer);
BOOL QueryConfigDword(const char *szKeyName, const char *szValueName, DWORD *lpdwData);
BOOL SetConfigString(const char *szKeyName, const char *szValueName, const char *lpBuffer);
BOOL SetConfigBinary(const char *szKeyName, const char *szValueName, const char *lpBuffer, int cbBuffer);
BOOL SetConfigDword(const char *szKeyName, const char *szValueName, DWORD dwData);

void VDShowHelp(HWND hwnd, const wchar_t *filename = 0);

bool IsFilenameOnFATVolume(const wchar_t *pszFilename);

HWND VDGetAncestorW32(HWND hwnd, UINT gaFlags);
VDStringW VDLoadStringW32(UINT uID, bool doSubstitutions);
void VDSubstituteStrings(VDStringW& s);

void LaunchURL(const char *pURL);

enum VDSystemShutdownMode
{
	kVDSystemShutdownMode_Shutdown,
	kVDSystemShutdownMode_Hibernate,
	kVDSystemShutdownMode_Sleep
};

bool VDInitiateSystemShutdown(VDSystemShutdownMode mode);

class VDCPUUsageReader {
public:
	VDCPUUsageReader();
	~VDCPUUsageReader();

	void Init();
	void Shutdown();

	int read();

private:
	bool fNTMethod;
	HKEY hkeyKernelCPU;

	unsigned __int64 kt_last;
	unsigned __int64 ut_last;
	unsigned __int64 st_last;
};

void VDEnableSampling(bool bEnable);

struct VDSamplingAutoProfileScope {
	VDSamplingAutoProfileScope() {
		VDEnableSampling(true);
	}
	~VDSamplingAutoProfileScope() {
		VDEnableSampling(false);
	}
};

#endif
