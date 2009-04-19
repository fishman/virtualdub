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
#include "resource.h"
#include <vd2/system/strutil.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/Kasumi/pixmaputils.h>

class VDVFilterConvertFormatConfigDialog : public VDDialogFrameW32 {
public:
	VDVFilterConvertFormatConfigDialog(int format);

	int GetFormat() const { return mFormat; }

	bool OnLoaded();
	void OnDataExchange(bool write);

protected:
	int mFormat;
};

VDVFilterConvertFormatConfigDialog::VDVFilterConvertFormatConfigDialog(int format)
	: VDDialogFrameW32(IDD_FILTER_CONVERTFORMAT)
	, mFormat(format)
{
}

bool VDVFilterConvertFormatConfigDialog::OnLoaded() {
	LBAddString(IDC_FORMATS, L"32-bit RGB");
	LBAddString(IDC_FORMATS, L"4:4:4 planar YCbCr (YV24)");
	LBAddString(IDC_FORMATS, L"4:2:2 planar YCbCr (YV16)");
	LBAddString(IDC_FORMATS, L"4:2:0 planar YCbCr (YV12)");
	LBAddString(IDC_FORMATS, L"4:1:1 planar YCbCr");
	LBAddString(IDC_FORMATS, L"4:1:0 planar YCbCr (YVU9)");
	LBAddString(IDC_FORMATS, L"4:2:2 interleaved YCbCr (UYVY)");
	LBAddString(IDC_FORMATS, L"4:2:2 interleaved YCbCr (YUY2)");

	SetFocusToControl(IDC_FORMATS);

	VDDialogFrameW32::OnLoaded();
	return true;
}

void VDVFilterConvertFormatConfigDialog::OnDataExchange(bool write) {
	if (write) {
		switch(LBGetSelectedIndex(IDC_FORMATS)) {
			case 0:
			default:
				mFormat = nsVDXPixmap::kPixFormat_XRGB8888;
				break;
			case 1:
				mFormat = nsVDXPixmap::kPixFormat_YUV444_Planar;
				break;
			case 2:
				mFormat = nsVDXPixmap::kPixFormat_YUV422_Planar;
				break;
			case 3:
				mFormat = nsVDXPixmap::kPixFormat_YUV420_Planar;
				break;
			case 4:
				mFormat = nsVDXPixmap::kPixFormat_YUV411_Planar;
				break;
			case 5:
				mFormat = nsVDXPixmap::kPixFormat_YUV410_Planar;
				break;
			case 6:
				mFormat = nsVDXPixmap::kPixFormat_YUV422_UYVY;
				break;
			case 7:
				mFormat = nsVDXPixmap::kPixFormat_YUV422_YUYV;
				break;
		}
	} else {
		switch(mFormat) {
			case nsVDXPixmap::kPixFormat_XRGB8888:
			default:
				LBSetSelectedIndex(IDC_FORMATS, 0);
				break;

			case nsVDXPixmap::kPixFormat_YUV444_Planar:
				LBSetSelectedIndex(IDC_FORMATS, 1);
				break;

			case nsVDXPixmap::kPixFormat_YUV422_Planar:
				LBSetSelectedIndex(IDC_FORMATS, 2);
				break;

			case nsVDXPixmap::kPixFormat_YUV420_Planar:
				LBSetSelectedIndex(IDC_FORMATS, 3);
				break;

			case nsVDXPixmap::kPixFormat_YUV411_Planar:
				LBSetSelectedIndex(IDC_FORMATS, 4);
				break;

			case nsVDXPixmap::kPixFormat_YUV410_Planar:
				LBSetSelectedIndex(IDC_FORMATS, 5);
				break;

			case nsVDXPixmap::kPixFormat_YUV422_UYVY:
				LBSetSelectedIndex(IDC_FORMATS, 6);
				break;

			case nsVDXPixmap::kPixFormat_YUV422_YUYV:
				LBSetSelectedIndex(IDC_FORMATS, 7);
				break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

class VDVFilterConvertFormat : public VDXVideoFilter {
public:
	VDVFilterConvertFormat();

	uint32 GetParams();
	void Run();

	bool Configure(VDXHWND hwnd);

	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	void ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	int mFormat;
};

VDVFilterConvertFormat::VDVFilterConvertFormat()
	: mFormat(nsVDXPixmap::kPixFormat_XRGB8888)
{
}

uint32 VDVFilterConvertFormat::GetParams() {
	if (fa->src.mpPixmapLayout->format != mFormat)
		return FILTERPARAM_NOT_SUPPORTED;

	return FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void VDVFilterConvertFormat::Run() {
}

bool VDVFilterConvertFormat::Configure(VDXHWND hwnd) {
	VDVFilterConvertFormatConfigDialog dlg(mFormat);

	if (!dlg.ShowDialog((VDGUIHandle)hwnd))
		return false;

	mFormat = dlg.GetFormat();
	return true;
}

void VDVFilterConvertFormat::GetSettingString(char *buf, int maxlen) {
	const char *formatname;

	switch(mFormat) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
		default:
			formatname = "RGB32";
			break;

		case nsVDXPixmap::kPixFormat_YUV444_Planar:
			formatname = "YV24";
			break;

		case nsVDXPixmap::kPixFormat_YUV422_Planar:
			formatname = "YV16";
			break;

		case nsVDXPixmap::kPixFormat_YUV420_Planar:
			formatname = "YV12";
			break;

		case nsVDXPixmap::kPixFormat_YUV411_Planar:
			formatname = "YUV411";
			break;

		case nsVDXPixmap::kPixFormat_YUV410_Planar:
			formatname = "YVU9";
			break;

		case nsVDXPixmap::kPixFormat_YUV422_UYVY:
			formatname = "UYVY";
			break;

		case nsVDXPixmap::kPixFormat_YUV422_YUYV:
			formatname = "YUY2";
			break;
	}

	_snprintf(buf, maxlen, " (%s)", formatname);
}

void VDVFilterConvertFormat::GetScriptString(char *buf, int maxlen) {
	_snprintf(buf, maxlen, "Config(%d)", mFormat);
}

void VDVFilterConvertFormat::ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	mFormat = argv[0].asInt();

	switch(mFormat) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV410_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV:
			break;

		default:
			mFormat = nsVDXPixmap::kPixFormat_XRGB8888;
			break;
	}
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVFilterConvertFormat)
VDXVF_DEFINE_SCRIPT_METHOD(VDVFilterConvertFormat, ScriptConfig, "i")
VDXVF_END_SCRIPT_METHODS()

extern VDXFilterDefinition filterDef_convertformat = VDXVideoFilterDefinition<VDVFilterConvertFormat>(NULL, "convert format", "Converts video to a different color space or color encoding.");

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)
