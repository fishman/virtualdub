#include <windows.h>
#include <ddraw.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "displaydrv.h"

#define VDDEBUG_DISP (void)sizeof printf
//#define VDDEBUG_DISP VDDEBUG

#if 0
	#define DEBUG_LOG(x) VDLog(kVDLogInfo, VDStringW(L##x))
#else
	#define DEBUG_LOG(x)
#endif

void VDDitherImage(VDPixmap& dst, const VDPixmap& src, const uint8 *pLogPal);

///////////////////////////////////////////////////////////////////////////////////////////////////

class IVDDirectDrawClient {
public:
	virtual void DirectDrawShutdown() = 0;
	virtual void DirectDrawPrimaryRestored() = 0;
};

class IVDDirectDrawManager {
public:
	virtual IDirectDraw2 *GetDDraw() = 0;
	virtual const DDCAPS& GetCaps() = 0;
	virtual IDirectDrawSurface2 *GetPrimary() = 0;
	virtual const DDSURFACEDESC& GetPrimaryDesc() = 0;
	virtual bool Restore() = 0;
};

class VDDirectDrawManager : public IVDDirectDrawManager {
public:
	bool Init(IVDDirectDrawClient *pClient);
	void Shutdown(IVDDirectDrawClient *pClient);

	IDirectDraw2 *GetDDraw() { return mpdd; }
	const DDCAPS& GetCaps() { return mCaps; }

	IDirectDrawSurface2 *GetPrimary() { return mpddsPrimary; }
	const DDSURFACEDESC& GetPrimaryDesc() { return mPrimaryDesc; }

	bool Restore();

protected:
	bool InitPrimary();
	void ShutdownPrimary();


	int mInitCount;

	HMODULE					mhmodDD;

	IDirectDraw2			*mpdd;
	IDirectDrawSurface2		*mpddsPrimary;

	DDSURFACEDESC			mPrimaryDesc;
	DDCAPS					mCaps;

	typedef vdfastvector<IVDDirectDrawClient *> tClients;
	tClients mClients;
};

bool VDDirectDrawManager::Init(IVDDirectDrawClient *pClient) {
	if (mInitCount) {
		++mInitCount;
		mClients.push_back(pClient);
		return true;
	}

	mhmodDD = LoadLibrary("ddraw");

	if (!mhmodDD)
		return false;

	do {
		typedef HRESULT (WINAPI *tpDirectDrawCreate)(GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter);
		tpDirectDrawCreate pDirectDrawCreate = (tpDirectDrawCreate)GetProcAddress(mhmodDD, "DirectDrawCreate");

		if (!pDirectDrawCreate)
			break;

		IDirectDraw *pdd;
		HRESULT hr;

		// create DirectDraw object
		if (FAILED(pDirectDrawCreate(NULL, &pdd, NULL))) {
			DEBUG_LOG("VideoDriver/DDraw: Couldn't create DirectDraw2 object\n");
			break;
		}

		// query up to IDirectDraw2 (DirectX 3)
		hr = pdd->QueryInterface(IID_IDirectDraw2, (void **)&mpdd);
		pdd->Release();

		if (FAILED(hr))
			break;

		// get caps
		memset(&mCaps, 0, sizeof mCaps);
		mCaps.dwSize = sizeof(DDCAPS);
		hr = mpdd->GetCaps(&mCaps, NULL);
		if (FAILED(hr)) {
			DEBUG_LOG("VideoDriver/DDraw: Couldn't get caps\n");
			break;
		}

		// set cooperative level
		hr = mpdd->SetCooperativeLevel(NULL, DDSCL_NORMAL);
		if (FAILED(hr)) {
			DEBUG_LOG("VideoDriver/DDraw: Couldn't set cooperative level\n");
			break;
		}

		// attempt to create primary surface
		if (!InitPrimary())
			break;

		mInitCount = 1;
		mClients.push_back(pClient);
		return true;
	} while(false);

	Shutdown(NULL);
	return false;
}

bool VDDirectDrawManager::InitPrimary() {
	do {
		// attempt to create primary surface
		DDSURFACEDESC ddsdPri = {sizeof(DDSURFACEDESC)};
		IDirectDrawSurface *pdds;

		ddsdPri.dwFlags				= DDSD_CAPS;
		ddsdPri.ddsCaps.dwCaps		= DDSCAPS_PRIMARYSURFACE;
		ddsdPri.ddpfPixelFormat.dwSize	= sizeof(DDPIXELFORMAT);

		if (FAILED(mpdd->CreateSurface(&ddsdPri, &pdds, NULL))) {
			DEBUG_LOG("VideoDriver/DDraw: Couldn't create primary surface\n");
			break;
		}

		// query up to IDirectDrawSurface2 (DX3)
		HRESULT hr = pdds->QueryInterface(IID_IDirectDrawSurface2, (void **)&mpddsPrimary);
		pdds->Release();

		if (FAILED(hr))
			break;

		// We cannot call GetSurfaceDesc() on the Primary as it causes the Vista beta 2
		// DWM to freak out.
		if (FAILED(mpdd->GetDisplayMode(&ddsdPri))) {
			DEBUG_LOG("VideoDriver/DDraw: Couldn't get primary desc\n");
			break;
		}

		mPrimaryDesc = ddsdPri;

		return true;
	} while(false);

	ShutdownPrimary();
	return false;
}

bool VDDirectDrawManager::Restore() {
	if (mpddsPrimary) {
		if (SUCCEEDED(mpddsPrimary->IsLost()))
			return true;

		VDDEBUG_DISP("VDDirectDraw: Primary surface restore requested.\n");

		HRESULT hr = mpddsPrimary->Restore();

		if (FAILED(hr)) {
			VDDEBUG_DISP("VDDirectDraw: Primary surface restore failed -- tearing down DirectDraw!\n");

			for(tClients::iterator it(mClients.begin()), itEnd(mClients.end()); it!=itEnd; ++it) {
				IVDDirectDrawClient *pClient = *it;

				pClient->DirectDrawShutdown();
			}

			if (!mInitCount) {
				VDDEBUG_DISP("VDDirectDraw: All clients vacated.\n");
				return false;
			}

			Shutdown(NULL);
			if (!Init(NULL)) {
				VDDEBUG_DISP("VDDirectDraw: Couldn't resurrect DirectDraw!\n");
				return false;
			}
		}
	} else {
		if (!InitPrimary())
			return false;
	}

	VDDEBUG_DISP("VDDirectDraw: Primary surface restore complete.\n");
	for(tClients::iterator it(mClients.begin()), itEnd(mClients.end()); it!=itEnd; ++it) {
		IVDDirectDrawClient *pClient = *it;

		pClient->DirectDrawPrimaryRestored();
	}

	return true;
}

void VDDirectDrawManager::ShutdownPrimary() {
	if (mpddsPrimary) {
		mpddsPrimary->Release();
		mpddsPrimary = 0;
	}
}

void VDDirectDrawManager::Shutdown(IVDDirectDrawClient *pClient) {
	if (pClient) {
		tClients::iterator it(std::find(mClients.begin(), mClients.end(), pClient));

		if (it != mClients.end()) {
			*it = mClients.back();
			mClients.pop_back();
		}

		if (--mInitCount)
			return;
	}

	ShutdownPrimary();

	if (mpdd) {
		mpdd->Release();
		mpdd = 0;
	}

	if (mhmodDD) {
		FreeLibrary(mhmodDD);
		mhmodDD = 0;
	}	
}

VDDirectDrawManager g_ddman;

IVDDirectDrawManager *VDInitDirectDraw(IVDDirectDrawClient *pClient) {
	VDASSERT(pClient);
	return g_ddman.Init(pClient) ? &g_ddman : NULL;
}

void VDShutdownDirectDraw(IVDDirectDrawClient *pClient) {
	VDASSERT(pClient);
	g_ddman.Shutdown(pClient);
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayMinidriverDirectDraw : public VDVideoDisplayMinidriver, protected IVDDirectDrawClient {
public:
	VDVideoDisplayMinidriverDirectDraw(bool enableOverlays);
	~VDVideoDisplayMinidriverDirectDraw();

	bool Init(HWND hwnd, const VDVideoDisplaySourceInfo& info);
	void Shutdown();

	bool ModifySource(const VDVideoDisplaySourceInfo& info);

	bool IsValid();

	bool Tick(int id);
	bool Resize();
	bool Update(UpdateMode);
	void Refresh(UpdateMode);
	bool Paint(HDC hdc, const RECT& rClient, UpdateMode mode);
	bool SetSubrect(const vdrect32 *r);
	void SetLogicalPalette(const uint8 *pLogicalPalette) { mpLogicalPalette = pLogicalPalette; }

protected:
	enum {
		kOverlayUpdateTimerId = 200
	};

	void DirectDrawShutdown() {
		Shutdown();
		mbReset = true;
	}

	void DirectDrawPrimaryRestored() {
		memset(&mLastDisplayRect, 0, sizeof mLastDisplayRect);
	}

	bool InitOverlay();
	bool InitOffscreen();
	void ShutdownDisplay();
	bool UpdateOverlay(bool force);
	void InternalRefresh(const RECT& rClient, UpdateMode mode);
	bool InternalBlt(IDirectDrawSurface2 *&pDest, RECT *prDst, RECT *prSrc, UpdateMode mode);

	HWND		mhwnd;
	IVDDirectDrawManager	*mpddman;
	IDirectDrawClipper	*mpddc;
	IDirectDrawSurface2	*mpddsBitmap;
	IDirectDrawSurface2	*mpddsOverlay;
	int			mPrimaryFormat;
	int			mPrimaryW;
	int			mPrimaryH;
	const uint8 *mpLogicalPalette;

	RECT		mLastDisplayRect;
	UINT		mOverlayUpdateTimer;

	COLORREF	mChromaKey;
	unsigned	mRawChromaKey;

	bool		mbReset;
	bool		mbValid;
	bool		mbFirstFrame;
	bool		mbRepaintOnNextUpdate;
	bool		mbSwapChromaPlanes;
	bool		mbUseSubrect;
	vdrect32	mSubrect;

	bool		mbEnableOverlays;

	DDCAPS		mCaps;
	VDVideoDisplaySourceInfo	mSource;
};

IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverDirectDraw(bool enableOverlays) {
	return new VDVideoDisplayMinidriverDirectDraw(enableOverlays);
}

VDVideoDisplayMinidriverDirectDraw::VDVideoDisplayMinidriverDirectDraw(bool enableOverlays)
	: mhwnd(0)
	, mpddman(0)
	, mpddc(0)
	, mpddsBitmap(0)
	, mpddsOverlay(0)
	, mpLogicalPalette(NULL)
	, mOverlayUpdateTimer(0)
	, mbReset(false)
	, mbValid(false)
	, mbFirstFrame(false)
	, mbRepaintOnNextUpdate(false)
	, mbUseSubrect(false)
	, mbEnableOverlays(enableOverlays)
{
	memset(&mSource, 0, sizeof mSource);
}

VDVideoDisplayMinidriverDirectDraw::~VDVideoDisplayMinidriverDirectDraw() {
}

bool VDVideoDisplayMinidriverDirectDraw::Init(HWND hwnd, const VDVideoDisplaySourceInfo& info) {
	switch(info.pixmap.format) {
	case nsVDPixmap::kPixFormat_Pal8:
	case nsVDPixmap::kPixFormat_XRGB1555:
	case nsVDPixmap::kPixFormat_RGB565:
	case nsVDPixmap::kPixFormat_RGB888:
	case nsVDPixmap::kPixFormat_XRGB8888:
	case nsVDPixmap::kPixFormat_YUV422_YUYV:
	case nsVDPixmap::kPixFormat_YUV422_UYVY:
	case nsVDPixmap::kPixFormat_YUV444_Planar:
	case nsVDPixmap::kPixFormat_YUV422_Planar:
	case nsVDPixmap::kPixFormat_YUV420_Planar:
	case nsVDPixmap::kPixFormat_YUV411_Planar:
	case nsVDPixmap::kPixFormat_YUV410_Planar:
	case nsVDPixmap::kPixFormat_Y8:
		break;
	default:
		return false;
	}

	mhwnd	= hwnd;
	mSource	= info;

	do {
		mpddman = VDInitDirectDraw(this);
		if (!mpddman)
			break;

		// The Windows Vista DWM has a bug where it allows you to create an overlay surface even
		// though you'd never be able to display it -- so we have to detect the DWM and force
		// overlays off.
		bool allowOverlay = mbEnableOverlays && !mbUseSubrect;

		if (mbEnableOverlays) {
			HMODULE hmodDwmApi = LoadLibraryA("dwmapi");
			if (hmodDwmApi) {
				typedef HRESULT (WINAPI *tpDwmIsCompositionEnabled)(BOOL *);

				tpDwmIsCompositionEnabled pDwmIsCompositionEnabled = (tpDwmIsCompositionEnabled)GetProcAddress(hmodDwmApi, "DwmIsCompositionEnabled");
				if (pDwmIsCompositionEnabled) {
					BOOL enabled;
					HRESULT hr = pDwmIsCompositionEnabled(&enabled);

					if (SUCCEEDED(hr) && enabled)
						allowOverlay = false;
				}

				FreeLibrary(hmodDwmApi);
			}
		}

		mCaps = mpddman->GetCaps();

		const DDSURFACEDESC& ddsdPri = mpddman->GetPrimaryDesc();

		mPrimaryW = ddsdPri.dwWidth;
		mPrimaryH = ddsdPri.dwHeight;

		// Interestingly enough, if another app goes full-screen, it's possible for us to lose
		// the device and have a failed Restore() between InitOverlay() and InitOffscreen().
		if ((allowOverlay && InitOverlay()) || (mpddman && InitOffscreen()))
			return true;

	} while(false);

	Shutdown();
	return false;
}

bool VDVideoDisplayMinidriverDirectDraw::InitOverlay() {
	DWORD dwFourCC;
	int minw = 1;
	int minh = 1;

	mbSwapChromaPlanes = false;
	switch(mSource.pixmap.format) {
	case nsVDPixmap::kPixFormat_YUV422_YUYV:
		dwFourCC = MAKEFOURCC('Y', 'U', 'Y', '2');
		minw = 2;
		break;

	case nsVDPixmap::kPixFormat_YUV422_UYVY:
		dwFourCC = MAKEFOURCC('U', 'Y', 'V', 'Y');
		minw = 2;
		break;

	case nsVDPixmap::kPixFormat_YUV420_Planar:
		dwFourCC = MAKEFOURCC('Y', 'V', '1', '2');
		mbSwapChromaPlanes = true;
		minw = 2;
		minh = 2;
		break;

	case nsVDPixmap::kPixFormat_YUV422_Planar:
		dwFourCC = MAKEFOURCC('Y', 'V', '1', '6');
		mbSwapChromaPlanes = true;
		minw = 2;
		break;

	case nsVDPixmap::kPixFormat_YUV410_Planar:
		dwFourCC = MAKEFOURCC('Y', 'V', 'U', '9');
		mbSwapChromaPlanes = true;
		minw = 4;
		minh = 4;
		break;

	case nsVDPixmap::kPixFormat_Y8:
		dwFourCC = MAKEFOURCC('Y', '8', ' ', ' ');
		mbSwapChromaPlanes = true;
		break;

	default:
		return false;
	}

	do {
		// create overlay surface
		DDSURFACEDESC ddsdOff = {sizeof(DDSURFACEDESC)};

		ddsdOff.dwFlags						= DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
		ddsdOff.dwWidth						= (mSource.pixmap.w + minw - 1) & -minw;
		ddsdOff.dwHeight					= (mSource.pixmap.h + minh - 1) & -minh;
		ddsdOff.ddsCaps.dwCaps				= DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
		ddsdOff.ddpfPixelFormat.dwSize		= sizeof(DDPIXELFORMAT);
		ddsdOff.ddpfPixelFormat.dwFlags		= DDPF_FOURCC;
		ddsdOff.ddpfPixelFormat.dwFourCC	= dwFourCC;

		if (mCaps.dwCaps & DDCAPS_ALIGNSIZESRC) {
			ddsdOff.dwWidth += mCaps.dwAlignSizeSrc - 1;
			ddsdOff.dwWidth -= ddsdOff.dwWidth % mCaps.dwAlignSizeSrc;
		}

		IDirectDrawSurface *pdds;
		HRESULT hr = mpddman->GetDDraw()->CreateSurface(&ddsdOff, &pdds, NULL);

		if (FAILED(hr)) {
			DEBUG_LOG("VideoDisplay/DDraw: Overlay surface creation failed\n");
			break;
		}

		hr = pdds->QueryInterface(IID_IDirectDrawSurface2, (void **)&mpddsOverlay);
		pdds->Release();

		if (FAILED(hr))
			break;

		// Do not allow colorkey if the primary surface is paletted, as we may not be able
		// to reliably choose the correct color.
		mChromaKey = 0;

		if (!(mpddman->GetPrimaryDesc().ddpfPixelFormat.dwFlags & (DDPF_PALETTEINDEXED8|DDPF_PALETTEINDEXED4))) {
			if (mCaps.dwCKeyCaps & DDCKEYCAPS_DESTOVERLAY) {
				const DDSURFACEDESC& ddsdPri = mpddman->GetPrimaryDesc();

				mRawChromaKey = ddsdPri.ddpfPixelFormat.dwGBitMask & ~(ddsdPri.ddpfPixelFormat.dwGBitMask >> 1);
				mChromaKey = RGB(0,128,0);
			}
		}

		mOverlayUpdateTimer = SetTimer(mhwnd, kOverlayUpdateTimerId, 100, NULL);
		memset(&mLastDisplayRect, 0, sizeof mLastDisplayRect);

		VDDEBUG_DISP("VideoDisplay: Using DirectDraw overlay for %dx%d %s display.\n", mSource.pixmap.w, mSource.pixmap.h, VDPixmapGetInfo(mSource.pixmap.format).name);
		DEBUG_LOG("VideoDisplay/DDraw: Overlay surface creation successful\n");

		mbRepaintOnNextUpdate = true;
		mbValid = false;

		if (!UpdateOverlay(false))
			break;

		return true;
	} while(false);

	ShutdownDisplay();
	return false;
}

bool VDVideoDisplayMinidriverDirectDraw::InitOffscreen() {
	HRESULT hr;

	do {
		const DDPIXELFORMAT& pf = mpddman->GetPrimaryDesc().ddpfPixelFormat;

		// determine primary surface pixel format
		if (pf.dwFlags & DDPF_PALETTEINDEXED8) {
			mPrimaryFormat = nsVDPixmap::kPixFormat_Pal8;
			VDDEBUG_DISP("VideoDisplay/DirectDraw: Display is 8-bit paletted.\n");
		} else if (pf.dwFlags & DDPF_RGB) {
			if (   pf.dwRGBBitCount == 16 && pf.dwRBitMask == 0x7c00 && pf.dwGBitMask == 0x03e0 && pf.dwBBitMask == 0x001f) {
				mPrimaryFormat = nsVDPixmap::kPixFormat_XRGB1555;
				VDDEBUG_DISP("VideoDisplay/DirectDraw: Display is 16-bit xRGB (1-5-5-5).\n");
			} else if (pf.dwRGBBitCount == 16 && pf.dwRBitMask == 0xf800 && pf.dwGBitMask == 0x07e0 && pf.dwBBitMask == 0x001f) {
				mPrimaryFormat = nsVDPixmap::kPixFormat_RGB565;
				VDDEBUG_DISP("VideoDisplay/DirectDraw: Display is 16-bit RGB (5-6-5).\n");
			} else if (pf.dwRGBBitCount == 24 && pf.dwRBitMask == 0xff0000 && pf.dwGBitMask == 0x00ff00 && pf.dwBBitMask == 0x0000ff) {
				mPrimaryFormat = nsVDPixmap::kPixFormat_RGB888;
				VDDEBUG_DISP("VideoDisplay/DirectDraw: Display is 24-bit RGB (8-8-8).\n");
			} else if (pf.dwRGBBitCount == 32 && pf.dwRBitMask == 0xff0000 && pf.dwGBitMask == 0x00ff00 && pf.dwBBitMask == 0x0000ff) {
				mPrimaryFormat = nsVDPixmap::kPixFormat_XRGB8888;
				VDDEBUG_DISP("VideoDisplay/DirectDraw: Display is 32-bit xRGB (8-8-8-8).\n");
			} else
				break;
		} else
			break;

		if (mPrimaryFormat != mSource.pixmap.format) {
			if (!mSource.bAllowConversion) {
				VDDEBUG_DISP("VideoDisplay/DirectDraw: Display is not compatible with source and conversion is disallowed.\n");
				return false;
			}
		}

		// attempt to create clipper
		if (FAILED(mpddman->GetDDraw()->CreateClipper(0, &mpddc, 0)))
			break;

		if (FAILED(mpddc->SetHWnd(0, mhwnd)))
			break;

		// create bitmap surface
		DDSURFACEDESC ddsdOff = {sizeof(DDSURFACEDESC)};

		ddsdOff.dwFlags					= DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
		ddsdOff.dwWidth					= mSource.pixmap.w;
		ddsdOff.dwHeight				= mSource.pixmap.h;
		ddsdOff.ddsCaps.dwCaps			= DDSCAPS_OFFSCREENPLAIN;
		ddsdOff.ddpfPixelFormat			= pf;

		IDirectDrawSurface *pdds = NULL;

		// if the source is persistent, try to create the surface directly into system memory
		if (mSource.bPersistent) {
			mSource.bPersistent = false;

#if 0		// doesn't work in DX3 -- need DX7 interfaces to create client surfaces
			if (mPrimaryFormat == mSource.format) {
				DDSURFACEDESC ddsdOff2(ddsdOff);

				ddsdOff2.dwFlags			= DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_PITCH | DDSD_LPSURFACE;
				ddsdOff2.lpSurface			= (void *)mSource.data;
				ddsdOff2.lPitch				= mSource.pitch;
				ddsdOff2.ddsCaps.dwCaps		= DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
				if (SUCCEEDED(mpddman->GetDDraw()->CreateSurface(&ddsdOff2, &pdds, NULL))) {
					DEBUG_LOG("VideoDriver/DDraw: Created surface directly in system memory (lucky!)\n");
					mSource.bPersistent = true;
				}
			}
#endif
		}

		if (!pdds && FAILED(mpddman->GetDDraw()->CreateSurface(&ddsdOff, &pdds, NULL))) {
			DEBUG_LOG("VideoDriver/DDraw: Couldn't create offscreen surface\n");
			break;
		}

		hr = pdds->QueryInterface(IID_IDirectDrawSurface2, (void **)&mpddsBitmap);
		pdds->Release();

		if (FAILED(hr))
			break;

		mChromaKey = 0;
		mbValid = false;
		mbRepaintOnNextUpdate = false;
		mbFirstFrame = true;

		DEBUG_LOG("VideoDriver/DDraw: Offscreen initialization successful\n");
		VDDEBUG_DISP("VideoDisplay: Using DirectDraw offscreen surface for %dx%d %s display.\n", mSource.pixmap.w, mSource.pixmap.h, VDPixmapGetInfo(mSource.pixmap.format).name);
		return true;
	} while(false); 

	ShutdownDisplay();
	return false;
}

void VDVideoDisplayMinidriverDirectDraw::ShutdownDisplay() {
	if (mpddc) {
		mpddc->Release();
		mpddc = 0;
	}

	if (mpddsBitmap) {
		mpddsBitmap->Release();
		mpddsBitmap = 0;
	}

	if (mpddsOverlay) {
		mpddsOverlay->Release();
		mpddsOverlay = 0;
	}

	mbValid = false;
}

void VDVideoDisplayMinidriverDirectDraw::Shutdown() {
	ShutdownDisplay();
	
	if (mpddman) {
		VDShutdownDirectDraw(this);
		mpddman = NULL;
	}
}

bool VDVideoDisplayMinidriverDirectDraw::ModifySource(const VDVideoDisplaySourceInfo& info) {
	if (!mpddsBitmap && !mpddsOverlay)
		return false;

	if (mSource.pixmap.w == info.pixmap.w && mSource.pixmap.h == info.pixmap.h && mSource.pixmap.format == info.pixmap.format) {
		mSource = info;
		return true;
	}

	return false;
}

bool VDVideoDisplayMinidriverDirectDraw::IsValid() {
	return mbValid && ((mpddsOverlay && DD_OK == mpddsOverlay->IsLost()) || (mpddsBitmap && DD_OK == mpddsBitmap->IsLost()));
}

bool VDVideoDisplayMinidriverDirectDraw::Tick(int id) {
	if (id == kOverlayUpdateTimerId) {
		RECT r;
		GetClientRect(mhwnd, &r);
		MapWindowPoints(mhwnd, NULL, (LPPOINT)&r, 2);

		if (memcmp(&r, &mLastDisplayRect, sizeof(RECT)))
			Resize();
	}

	return !mbReset;
}

bool VDVideoDisplayMinidriverDirectDraw::Resize() {
	if (mpddsOverlay)
		UpdateOverlay(false);

	return !mbReset;
}

bool VDVideoDisplayMinidriverDirectDraw::UpdateOverlay(bool force) {
	do {
		RECT rDst0;

		GetClientRect(mhwnd, &rDst0);
		MapWindowPoints(mhwnd, NULL, (LPPOINT)&rDst0, 2);

		// destination clipping
		RECT rDst = rDst0;
		const int dstw = rDst.right - rDst.left;
		const int dsth = rDst.bottom - rDst.top;

		if (rDst.left < 0)
			rDst.left = 0;

		if (rDst.top < 0)
			rDst.top = 0;

		if (rDst.right > mPrimaryW)
			rDst.right = mPrimaryW;

		if (rDst.bottom > mPrimaryH)
			rDst.bottom = mPrimaryH;

		if (rDst.bottom <= rDst.top || rDst.right <= rDst.left)
			break;

		// source clipping
		RECT rSrc = {
			(rDst.left   - rDst0.left) * mSource.pixmap.w / dstw,
			(rDst.top    - rDst0.top ) * mSource.pixmap.h / dsth,
			(rDst.right  - rDst0.left) * mSource.pixmap.w / dstw,
			(rDst.bottom - rDst0.top ) * mSource.pixmap.h / dsth,
		};

		// source alignment
		if (mCaps.dwCaps & DDCAPS_ALIGNBOUNDARYSRC) {
			int align = mCaps.dwAlignBoundarySrc;
			rSrc.left -= rSrc.left % align;
		}

		if (mCaps.dwCaps & DDCAPS_ALIGNSIZESRC) {
			int w = rSrc.right - rSrc.left;

			w -= w % mCaps.dwAlignSizeSrc;

			rSrc.right = rSrc.left + w;
		}

		// destination alignment
		if (mCaps.dwCaps & DDCAPS_ALIGNBOUNDARYDEST) {
			int align = mCaps.dwAlignBoundaryDest;

			rDst.left += align-1;
			rDst.left -= rDst.left % align;
		}

		if (mCaps.dwCaps & DDCAPS_ALIGNSIZEDEST) {
			int w = rDst.right - rDst.left;

			w -= w % mCaps.dwAlignSizeDest;

			if (w <= 0)
				break;

			rDst.right = rDst.left + w;
		}

		DWORD dwFlags = DDOVER_SHOW | DDOVER_DDFX;
		DDOVERLAYFX ddfx = {sizeof(DDOVERLAYFX)};

		if (mChromaKey) {
			dwFlags |= DDOVER_KEYDESTOVERRIDE;
			ddfx.dckDestColorkey.dwColorSpaceLowValue = mRawChromaKey;
			ddfx.dckDestColorkey.dwColorSpaceHighValue = mRawChromaKey;
		}

		if (mCaps.dwFXCaps & DDFXCAPS_OVERLAYARITHSTRETCHY)
			ddfx.dwFlags |= DDOVERFX_ARITHSTRETCHY;

		IDirectDrawSurface2 *pDest = mpddman->GetPrimary();
		HRESULT hr = mpddsOverlay->UpdateOverlay(&rSrc, pDest, &rDst, dwFlags, &ddfx);

		if (FAILED(hr)) {
			mbValid = false;
			memset(&mLastDisplayRect, 0, sizeof mLastDisplayRect);

			// NVIDIA ForceWare 96.85 for Vista allows us to create multiple overlays,
			// but attempting to show more than one gives DDERR_NOTAVAILABLE.
			if (hr != DDERR_SURFACELOST)
				return false;

			if (FAILED(mpddsOverlay->Restore()))
				return false;

			if (FAILED(pDest->IsLost()) && mpddman->Restore())
				return false;
		} else
			mLastDisplayRect = rDst0;
		return !mbReset;
	} while(false);

	mpddsOverlay->UpdateOverlay(NULL, mpddman->GetPrimary(), NULL, DDOVER_HIDE, NULL);
	return !mbReset;
}

bool VDVideoDisplayMinidriverDirectDraw::Update(UpdateMode mode) {
	if (!mSource.pixmap.data)
		return false;

	HRESULT hr;
	DDSURFACEDESC ddsd = { sizeof(DDSURFACEDESC) };

	ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	
	static DWORD dwLockFlags = GetVersion() & 0x80000000 ? DDLOCK_WRITEONLY | DDLOCK_NOSYSLOCK | DDLOCK_WAIT : DDLOCK_WRITEONLY | DDLOCK_WAIT;

	IDirectDrawSurface2 *pTarget = mpddsBitmap ? mpddsBitmap : mpddsOverlay;

	if (!pTarget)
		return false;

	// When NView reverts between dual-display modes, we can get a DDERR_SURFACELOST on which
	// Restore() succeeds, but the next lock still fails. We insert a safety counter here to
	// prevent a hang.
	for(int retries=0; retries<5; ++retries) {
		hr = pTarget->Lock(NULL, &ddsd, dwLockFlags, 0);

		if (SUCCEEDED(hr))
			break;

		if (hr != DDERR_SURFACELOST)
			break;

		mbValid = false;
		memset(&mLastDisplayRect, 0, sizeof mLastDisplayRect);

		if (!mpddman->Restore())
			break;

		hr = pTarget->Restore();
		if (FAILED(hr))
			break;
	}

	if (FAILED(hr)) {
		mbValid = false;
		memset(&mLastDisplayRect, 0, sizeof mLastDisplayRect);
		return false;
	}

	VDPixmap source(mSource.pixmap);

	char *dst = (char *)ddsd.lpSurface;
	ptrdiff_t dstpitch = ddsd.lPitch;

	uint32 fieldmode = mode & kModeFieldMask;

	VDPixmap dstbm = { dst, NULL, ddsd.dwWidth, ddsd.dwHeight, dstpitch, mPrimaryFormat };

	if (mpddsOverlay)
		dstbm.format = source.format;

	const VDPixmapFormatInfo& dstinfo = VDPixmapGetInfo(dstbm.format);

	if (dstinfo.auxbufs >= 1) {
		const int qw = -(-dstbm.w >> dstinfo.qwbits);
		const int qh = -(-dstbm.h >> dstinfo.qhbits);

		VDASSERT((qw << dstinfo.qwbits) == dstbm.w);
		VDASSERT((qh << dstinfo.qhbits) == dstbm.h);

		dstbm.data2		= (char *)dstbm.data + dstpitch * qh;
		dstbm.pitch2	= dstpitch >> dstinfo.auxwbits;

		if (dstinfo.auxbufs >= 2) {
			dstbm.data3 = (char *)dstbm.data2 + dstbm.pitch2 * -(-dstbm.h >> dstinfo.auxhbits);
			dstbm.pitch3 = dstbm.pitch2;
		}

		if (mbSwapChromaPlanes) {
			std::swap(dstbm.data2, dstbm.data3);
			std::swap(dstbm.pitch2, dstbm.pitch3);
		}
	}

	if (mSource.bInterlaced && fieldmode != kModeAllFields) {
		const VDPixmapFormatInfo& srcformat = VDPixmapGetInfo(source.format);
		const VDPixmapFormatInfo& dstformat = VDPixmapGetInfo(dstbm.format);

		if (!srcformat.qhbits && !dstformat.qhbits && !srcformat.auxhbits && !dstformat.auxhbits) {
			if (fieldmode == kModeOddField) {
				source.h >>= 1;

				vdptrstep(source.data, source.pitch);
				switch(srcformat.auxbufs) {
					case 2:	vdptrstep(source.data3, source.pitch3);
					case 1:	vdptrstep(source.data2, source.pitch2);
				}

				dstbm.h >>= 1;
				vdptrstep(dstbm.data, dstbm.pitch);
				switch(dstformat.auxbufs) {
					case 2:	vdptrstep(dstbm.data3, dstbm.pitch3);
					case 1:	vdptrstep(dstbm.data2, dstbm.pitch2);
				}
			} else {
				source.h = (source.h + 1) >> 1;
				dstbm.h = (dstbm.h + 1) >> 1;
			}

			source.pitch += source.pitch;
			switch(dstformat.auxbufs) {
				case 2:	source.pitch3 += source.pitch3;
				case 1:	source.pitch2 += source.pitch2;
			}

			dstbm.pitch += dstbm.pitch;
			switch(dstformat.auxbufs) {
				case 2:	dstbm.pitch3 += dstbm.pitch3;
				case 1:	dstbm.pitch2 += dstbm.pitch2;
			}
		}
	}

	bool dither = false;
	if (dstbm.format == nsVDPixmap::kPixFormat_Pal8 && dstbm.format != source.format) {
		switch(source.format) {
			case nsVDPixmap::kPixFormat_XRGB1555:
			case nsVDPixmap::kPixFormat_RGB565:
			case nsVDPixmap::kPixFormat_RGB888:
			case nsVDPixmap::kPixFormat_XRGB8888:
				dither = true;
				break;
		}
	}

	if (dither)
		VDDitherImage(dstbm, source, mpLogicalPalette);
	else
		VDPixmapBlt(dstbm, source);
	
	hr = pTarget->Unlock(0);

	mbValid = SUCCEEDED(hr);

	return !mbReset;
}

void VDVideoDisplayMinidriverDirectDraw::Refresh(UpdateMode mode) {
	if (mbValid) {
		if (mpddsOverlay) {
			Tick(kOverlayUpdateTimerId);
			if (mbRepaintOnNextUpdate) {
				InvalidateRect(mhwnd, NULL, TRUE);
				mbRepaintOnNextUpdate = false;
			}
		} else {
			RECT r;
			GetClientRect(mhwnd, &r);
			InternalRefresh(r, mode);

			// Workaround for Windows Vista DWM not adding window to composition tree immediately
			if (mbFirstFrame) {
				mbFirstFrame = false;
				SetWindowPos(mhwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
			}
		}
	}
}

bool VDVideoDisplayMinidriverDirectDraw::Paint(HDC hdc, const RECT& rClient, UpdateMode mode) {
	if (mpddsOverlay) {
		if (mChromaKey) {
			SetBkColor(hdc, mChromaKey);
			ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rClient, "", 0, NULL);
		}
	} else {
		InternalRefresh(rClient, mode);
	}

	// Workaround for Windows Vista DWM not adding window to composition tree immediately
	if (mbFirstFrame) {
		mbFirstFrame = false;
		SetWindowPos(mhwnd, NULL, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
	}

	return !mbReset;
}

bool VDVideoDisplayMinidriverDirectDraw::SetSubrect(const vdrect32 *r) {
	if (mpddsOverlay)
		return false;

	if (r) {
		mbUseSubrect = true;
		mSubrect = *r;
	} else
		mbUseSubrect = false;

	return true;
}

void VDVideoDisplayMinidriverDirectDraw::InternalRefresh(const RECT& rClient, UpdateMode mode) {
	RECT rDst = rClient;

	// DirectX doesn't like null rects.
	if (rDst.right <= rDst.left || rDst.bottom <= rDst.top)
		return;

	MapWindowPoints(mhwnd, NULL, (LPPOINT)&rDst, 2);

	IDirectDrawSurface2 *pDest = mpddman->GetPrimary();

	pDest->SetClipper(mpddc);

	if (mColorOverride) {
		// convert color to primary surface format
		VDPixmap srcpx;
		srcpx.data = &mColorOverride;
		srcpx.pitch = 0;
		srcpx.w = 1;
		srcpx.h = 1;
		srcpx.format = nsVDPixmap::kPixFormat_XRGB8888;

		VDPixmap dstpx;
		uint32 tmpbuf;
		dstpx.data = &tmpbuf;
		dstpx.pitch = 0;
		dstpx.w = 1;
		dstpx.h = 1;
		dstpx.format = mPrimaryFormat;

		VDPixmapBlt(dstpx, srcpx);

		DDBLTFX fx = {sizeof(DDBLTFX)};
		fx.dwFillColor = tmpbuf;

		for(int i=0; i<5; ++i) {
			HRESULT hr = pDest->Blt(&rDst, NULL, NULL, DDBLT_WAIT | DDBLT_COLORFILL, &fx);

			if (SUCCEEDED(hr))
				break;

			if (hr != DDERR_SURFACELOST)
				break;

			if (FAILED(pDest->IsLost())) {
				pDest->SetClipper(NULL);
				pDest = NULL;

				if (!mpddman->Restore())
					return;

				if (mbReset)
					return;

				pDest = mpddman->GetPrimary();
				pDest->SetClipper(mpddc);
			}
		}

		return;
	}

	if (!mSource.bInterlaced) {
		if (mbUseSubrect) {
			RECT rSrc = { mSubrect.left, mSubrect.top, mSubrect.right, mSubrect.bottom };
			InternalBlt(pDest, &rDst, &rSrc, mode);
		} else
			InternalBlt(pDest, &rDst, NULL, mode);
	} else {
		const VDPixmap& source = mSource.pixmap;
		vdrect32 r;
		if (mbUseSubrect)
			r = mSubrect;
		else
			r.set(0, 0, source.w, source.h);

		const uint32 fieldmode = mode & kModeFieldMask;

		uint32 vinc		= (r.height() << 16) / rClient.bottom;
		uint32 vaccum	= (vinc >> 1) + (r.top << 16);
		uint32 vtlimit	= (((source.h + 1) >> 1) << 17) - 1;
		int fieldbase	= (fieldmode == kModeOddField ? 1 : 0);
		int ystep		= (fieldmode == kModeAllFields) ? 1 : 2;

		vaccum += vinc*fieldbase;
		vinc *= ystep;

		for(int y = fieldbase; y < rClient.bottom; y += ystep) {
			int v;

			if (y & 1) {
				uint32 vt = vaccum < 0x8000 ? 0 : vaccum - 0x8000;

				v = (y&1) + ((vt>>16) & ~1);
			} else {
				uint32 vt = vaccum + 0x8000;

				if (vt > vtlimit)
					vt = vtlimit;

				v = (vt>>16) & ~1;
			}

			RECT rDstTemp = { rDst.left, rDst.top+y, rDst.right, rDst.top+y+1 };
			RECT rSrcTemp = { r.left, v, r.width(), v+1 };

			if (!InternalBlt(pDest, &rDstTemp, &rSrcTemp, mode))
				break;

			vaccum += vinc;
		}
	}
	
	if (pDest)
		pDest->SetClipper(NULL);
}

bool VDVideoDisplayMinidriverDirectDraw::InternalBlt(IDirectDrawSurface2 *&pDest, RECT *prDst, RECT *prSrc, UpdateMode mode) {
	HRESULT hr;

	for(;;) {
		// DDBLTFX_NOTEARING is ignored by DirectDraw in 2K/XP.

		if (mode & kModeVSync) {
			IDirectDraw2 *pDD = mpddman->GetDDraw();
			DWORD maxScan = 0;
			
			for(;;) {
				DWORD scan;
				hr = pDD->GetScanLine(&scan);

				// Check if GetScanLine() failed -- it will do so if we're in VBlank.
				if (FAILED(hr))
					break;

				// Check if we are outside of the danger zone.
				if ((int)scan < prDst->top || (int)scan >= prDst->bottom)
					break;

				// Check if we have looped around, which may mean the system is too
				// busy to poll the beam reliably.
				if (scan < maxScan)
					break;

				// We're in the danger zone. If the delta is greater than one tenth of the
				// display, do a sleep.
				if ((prDst->bottom - (int)scan) * 10 >= (int)mpddman->GetPrimaryDesc().dwHeight)
					::Sleep(1);

				maxScan = scan;
			}
		}

		hr = pDest->Blt(prDst, mpddsBitmap, prSrc, DDBLT_ASYNC | DDBLT_WAIT, NULL);

		if (SUCCEEDED(hr))
			break;

		if (hr != DDERR_SURFACELOST)
			break;

		if (FAILED(mpddsBitmap->IsLost())) {
			mpddsBitmap->Restore();
			mbValid = false;
			break;
		}

		if (FAILED(pDest->IsLost())) {
			pDest->SetClipper(NULL);
			pDest = NULL;

			if (!mpddman->Restore())
				return false;

			if (mbReset)
				return false;

			pDest = mpddman->GetPrimary();
			pDest->SetClipper(mpddc);
		}
	}

	return SUCCEEDED(hr);
}
