#ifndef f_VD2_VDLIB_DIALOG_H
#define f_VD2_VDLIB_DIALOG_H

#ifdef _MSC_VER
#pragma once
#endif

#include <vd2/system/vdstl.h>
#include <vd2/system/win32/miniwindows.h>

class IVDUIDropFileList {
public:
	virtual bool GetFileName(int index, VDStringW& fileName) = 0;
};

class VDDialogFrameW32 {
public:
	VDZHWND GetWindowHandle() const { return mhdlg; }

	bool	Create(VDGUIHandle hwndParent);
	void	Destroy();

	void	Show();
	void	Hide();

	sintptr ShowDialog(VDGUIHandle hwndParent);

protected:
	VDDialogFrameW32(uint32 dlgid);

	void End(sintptr result);

	VDZHWND GetControl(uint32 id);

	void SetFocusToControl(uint32 id);
	void EnableControl(uint32 id, bool enabled);

	void SetCaption(uint32 id, const wchar_t *format);

	void SetControlText(uint32 id, const wchar_t *s);
	void SetControlTextF(uint32 id, const wchar_t *format, ...);

	uint32 GetControlValueUint32(uint32 id);
	double GetControlValueDouble(uint32 id);
	VDStringW GetControlValueString(uint32 id);

	void ExchangeControlValueBoolCheckbox(bool write, uint32 id, bool& val);
	void ExchangeControlValueDouble(bool write, uint32 id, const wchar_t *format, double& val, double minVal, double maxVal);
	void ExchangeControlValueString(bool write, uint32 id, VDStringW& s);

	void CheckButton(uint32 id, bool checked);
	bool IsButtonChecked(uint32 id);

	void BeginValidation();
	bool EndValidation();

	void FailValidation(uint32 id);
	void SignalFailedValidation(uint32 id);

	// listbox
	sint32 LBGetSelectedIndex(uint32 id);
	void LBSetSelectedIndex(uint32 id, sint32 idx);
	void LBAddString(uint32 id, const wchar_t *s);
	void LBAddStringF(uint32 id, const wchar_t *format, ...);

	// trackbar
	sint32 TBGetValue(uint32 id);
	void TBSetValue(uint32 id, sint32 value);
	void TBSetRange(uint32 id, sint32 minval, sint32 maxval);

protected:
	virtual VDZINT_PTR DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);
	virtual void OnDataExchange(bool write);
	virtual bool OnLoaded();
	virtual bool OnOK();
	virtual bool OnCancel();
	virtual void OnSize();
	virtual void OnDestroy();
	virtual bool OnTimer(uint32 id);
	virtual bool OnCommand(uint32 id, uint32 extcode);
	virtual void OnDropFiles(VDZHDROP hDrop);
	virtual void OnDropFiles(IVDUIDropFileList *dropFileList);
	virtual void OnHScroll(uint32 id, int code);
	virtual void OnVScroll(uint32 id, int code);
	virtual bool PreNCDestroy();

	bool	mbValidationFailed;
	bool	mbIsModal;
	VDZHWND	mhdlg;

private:
	static VDZINT_PTR VDZCALLBACK StaticDlgProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

	const char *mpDialogResourceName;
	uint32	mFailedId;
};

class VDDialogResizerW32 {
public:
	VDDialogResizerW32();
	~VDDialogResizerW32();

	enum {
		kAnchorX	= 0x01,
		kAnchorW	= 0x02,
		kAnchorY	= 0x04,
		kAnchorH	= 0x08,

		kL		= 0,
		kC		= kAnchorW,
		kR		= kAnchorX,
		kHMask	= 0x03,

		kT		= 0,
		kM		= kAnchorH,
		kB		= kAnchorY,
		kVMask	= 0x0C,

		kTL		= kT | kL,
		kTR		= kT | kR,
		kTC		= kT | kC,
		kML		= kM | kL,
		kMR		= kM | kR,
		kMC		= kM | kC,
		kBL		= kB | kL,
		kBR		= kB | kR,
		kBC		= kB | kC,
	};

	void Init(VDZHWND hwnd);
	void Relayout();
	void Relayout(int width, int height);
	void Add(uint32 id, int alignment);

protected:
	struct ControlEntry {
		VDZHWND	mhwnd;
		int		mAlignment;
		sint32	mX;
		sint32	mY;
		sint32	mW;
		sint32	mH;
	};

	VDZHWND	mhwndBase;
	int		mWidth;
	int		mHeight;

	typedef vdfastvector<ControlEntry> Controls;
	Controls mControls;
};

#endif
