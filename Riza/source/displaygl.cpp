#include <windows.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/math.h>
#include <vd2/system/w32assist.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Riza/opengl.h>
#include "displaydrv.h"

#define VDDEBUG_DISP (void)sizeof printf
//#define VDDEBUG_DISP VDDEBUG

///////////////////////////////////////////////////////////////////////////

#include <GL/GL.h>
#include <vd2/external/glATI.h>			// I feel dirty using this, but I can't figure out the license for the SGI glext.h pointed to by NVIDIA.

// from OpenGL 1.2 EXT_packed_pixels

#ifndef GL_UNSIGNED_SHORT_5_6_5
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif
#ifndef GL_UNSIGNED_SHORT_5_6_5_REV
#define GL_UNSIGNED_SHORT_5_6_5_REV 0x8364
#endif
#ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
#define GL_UNSIGNED_SHORT_1_5_5_5_REV 0x8366
#endif

///////////////////////////////////////////////////////////////////////////

class VDVideoTextureTilePatternOpenGL {
public:
	struct TileInfo {
		float	mInvU;
		float	mInvV;
		int		mSrcX;
		int		mSrcY;
		int		mSrcW;
		int		mSrcH;
		bool	mbLeft;
		bool	mbTop;
		bool	mbRight;
		bool	mbBottom;
		GLuint	mTextureID;
	};

	VDVideoTextureTilePatternOpenGL() : mbPhase(false) {}
	void Init(VDOpenGLBinding *pgl, int w, int h, bool bPackedPixelsSupported, bool bEdgeClampSupported);
	void Shutdown(VDOpenGLBinding *pgl);

	void ReinitFiltering(VDOpenGLBinding *pgl, IVDVideoDisplayMinidriver::FilterMode mode);

	bool IsInited() const { return !mTextures.empty(); }

	void Flip();
	void GetTileInfo(TileInfo*& pInfo, int& nTiles);

protected:
	int			mTextureTilesW;
	int			mTextureTilesH;
	int			mTextureSize;
	double		mTextureSizeInv;
	int			mTextureLastW;
	int			mTextureLastH;
	double		mTextureLastWInvPow2;
	double		mTextureLastHInvPow2;
	vdfastvector<GLuint>	mTextures;
	vdfastvector<TileInfo>	mTileInfo;

	bool		mbPackedPixelsSupported;
	bool		mbEdgeClampSupported;
	bool		mbPhase;
};

void VDVideoTextureTilePatternOpenGL::Init(VDOpenGLBinding *pgl, int w, int h, bool bPackedPixelsSupported, bool bEdgeClampSupported) {
	mbPackedPixelsSupported		= bPackedPixelsSupported;
	mbEdgeClampSupported		= bEdgeClampSupported;

	GLint maxsize;
	pgl->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxsize);

	mTextureSize	= maxsize;
	mTextureSizeInv	= 1.0 / maxsize;
	mTextureTilesW	= (w  - 1 + (maxsize-2)) / (maxsize - 1);
	mTextureTilesH	= (h - 1 + (maxsize-2)) / (maxsize - 1);

	int ntiles = mTextureTilesW * mTextureTilesH;
	int xlast = w - (mTextureTilesW - 1)*(maxsize - 1);
	int ylast = h - (mTextureTilesH - 1)*(maxsize - 1);
	int xlasttex = 1;
	int ylasttex = 1;

	while(xlasttex < xlast)
		xlasttex += xlasttex;
	while(ylasttex < ylast)
		ylasttex += ylasttex;

	int largestW = mTextureSize;
	int largestH = mTextureSize;

	if (mTextureTilesW == 1)
		largestW = xlasttex;
	if (mTextureTilesH == 1)
		largestH = ylasttex;

	mTextureLastW = xlast;
	mTextureLastH = ylast;
	mTextureLastWInvPow2	= 1.0 / xlasttex;
	mTextureLastHInvPow2	= 1.0 / ylasttex;

	mTextures.resize(ntiles*2);
	pgl->glGenTextures(ntiles*2, mTextures.data());

	vdautoblockptr zerobuffer(malloc(4 * largestW * largestH));
	memset(zerobuffer, 0, 4 * largestW * largestH);

	pgl->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	pgl->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);


	int tile = 0;
	for(int y = 0; y < mTextureTilesH; ++y) {
		for(int x = 0; x < mTextureTilesW; ++x, ++tile) {
			int w = (x==mTextureTilesW-1) ? xlasttex : maxsize;
			int h = (y==mTextureTilesH-1) ? ylasttex : maxsize;

			for(int offset=0; offset<2; ++offset) {
				pgl->glBindTexture(GL_TEXTURE_2D, mTextures[tile*2+offset]);

				if (mbEdgeClampSupported) {
					pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_EXT);
					pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_EXT);
				} else {
					pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
					pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

					static const float black[4]={0.f,0.f,0.f,0.f};
					pgl->glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, black);
				}

				if (w==maxsize && h==maxsize)
					pgl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
				else
					pgl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, zerobuffer.get());
			}

			TileInfo info;

			info.mInvU	= 1.0f / w;
			info.mInvV	= 1.0f / h;
			info.mSrcX	= mTextureSize * x;
			info.mSrcY	= mTextureSize * y;
			info.mSrcW	= (x==mTextureTilesW-1) ? xlast : mTextureSize;
			info.mSrcH	= (y==mTextureTilesH-1) ? ylast : mTextureSize;
			info.mbLeft		= x<=0;
			info.mbTop		= y<=0;
			info.mbRight	= (x==mTextureTilesW-1);
			info.mbBottom	= (y==mTextureTilesH-1);

			mTileInfo.push_back(info);
		}
	}

	Flip();
}

void VDVideoTextureTilePatternOpenGL::Shutdown(VDOpenGLBinding *pgl) {
	if (mTextures.empty()) {
		int nTextures = mTextures.size();

		pgl->glDeleteTextures(nTextures, mTextures.data());
		vdfastvector<GLuint>().swap(mTextures);
	}
	vdfastvector<TileInfo>().swap(mTileInfo);
}

void VDVideoTextureTilePatternOpenGL::ReinitFiltering(VDOpenGLBinding *pgl, IVDVideoDisplayMinidriver::FilterMode mode) {
	const size_t nTextures = mTextures.size();
	for(size_t i=0; i<nTextures; ++i) {
		pgl->glBindTexture(GL_TEXTURE_2D, mTextures[i]);

		if (mode == IVDVideoDisplayMinidriver::kFilterPoint)
			pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		else
			pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		if (mode == IVDVideoDisplayMinidriver::kFilterPoint)
			pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		else
			pgl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}

void VDVideoTextureTilePatternOpenGL::Flip() {
	mbPhase = !mbPhase;
	size_t nTiles = mTileInfo.size();
	for(size_t i=0; i<nTiles; ++i)
		mTileInfo[i].mTextureID = mTextures[2*i+mbPhase];
}

void VDVideoTextureTilePatternOpenGL::GetTileInfo(TileInfo*& pInfo, int& nTiles) {
	pInfo = &mTileInfo[0];
	nTiles = mTileInfo.size();
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayMinidriverOpenGL : public VDVideoDisplayMinidriver {
public:
	VDVideoDisplayMinidriverOpenGL();
	~VDVideoDisplayMinidriverOpenGL();

	bool Init(HWND hwnd, const VDVideoDisplaySourceInfo& info);
	void Shutdown();

	bool ModifySource(const VDVideoDisplaySourceInfo& info);

	bool IsValid() { return mbValid; }
	void SetFilterMode(FilterMode mode);

	bool Resize();
	bool Update(UpdateMode);
	void Refresh(UpdateMode);
	bool Paint(HDC hdc, const RECT& rClient, UpdateMode mode) { return true; }

protected:
	void Upload(const VDPixmap& source, VDVideoTextureTilePatternOpenGL& texPattern);

	static ATOM VDVideoDisplayMinidriverOpenGL::Register();
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	bool OnOpenGLInit();
	void OnDestroy();
	void OnPaint();

	
	HWND		mhwnd;
	HWND		mhwndOGL;
	bool		mbValid;
	bool		mbFirstPresent;
	bool		mbVerticalFlip;

	FilterMode	mPreferredFilter;
	VDVideoTextureTilePatternOpenGL		mTexPattern[2];
	VDVideoDisplaySourceInfo			mSource;

	VDOpenGLBinding	mGL;
};

#define MYWM_OGLINIT		(WM_USER + 0x180)

IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverOpenGL() {
	return new VDVideoDisplayMinidriverOpenGL;
}

VDVideoDisplayMinidriverOpenGL::VDVideoDisplayMinidriverOpenGL()
	: mhwndOGL(0)
	, mbValid(false)
	, mbFirstPresent(false)
	, mPreferredFilter(kFilterAnySuitable)
{
	memset(&mSource, 0, sizeof mSource);
}

VDVideoDisplayMinidriverOpenGL::~VDVideoDisplayMinidriverOpenGL() {
}

bool VDVideoDisplayMinidriverOpenGL::Init(HWND hwnd, const VDVideoDisplaySourceInfo& info) {
	mSource = info;
	mhwnd = hwnd;

	// OpenGL doesn't allow upside-down texture uploads, so we simply
	// upload the surface inverted and then reinvert on display.
	mbVerticalFlip = false;
	if (mSource.pixmap.pitch < 0) {
		mSource.pixmap.data = (char *)mSource.pixmap.data + mSource.pixmap.pitch*(mSource.pixmap.h - 1);
		mSource.pixmap.pitch = -mSource.pixmap.pitch;
		mbVerticalFlip = true;
	}

	RECT r;
	GetClientRect(mhwnd, &r);

	static ATOM wndClass = Register();

	if (!mGL.Init())
		return false;

	// We have to create a separate window because the NVIDIA driver subclasses the
	// window and doesn't unsubclass it even after the OpenGL context is deleted.
	// If we use the main window instead then the app will bomb the moment we unload
	// OpenGL.

	mhwndOGL = CreateWindowEx(WS_EX_TRANSPARENT, (LPCSTR)wndClass, "", WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, r.right, r.bottom, mhwnd, NULL, VDGetLocalModuleHandleW32(), this);
	if (mhwndOGL) {
		if (SendMessage(mhwndOGL, MYWM_OGLINIT, 0, 0)) {
			mbValid = false;
			mbFirstPresent = true;
			return true;
		}

		DestroyWindow(mhwndOGL);
		mhwndOGL = 0;
	}

	return false;
}

void VDVideoDisplayMinidriverOpenGL::Shutdown() {
	if (mhwndOGL) {
		DestroyWindow(mhwndOGL);
		mhwndOGL = NULL;
	}

	mGL.Shutdown();
	mbValid = false;
}

bool VDVideoDisplayMinidriverOpenGL::ModifySource(const VDVideoDisplaySourceInfo& info) {
	if (!mGL.IsInited())
		return false;

	if (info.pixmap.w == mSource.pixmap.w && info.pixmap.h == mSource.pixmap.h && info.pixmap.format == mSource.pixmap.format && info.bInterlaced == mSource.bInterlaced) {
		mSource = info;
		// OpenGL doesn't allow upside-down texture uploads, so we simply
		// upload the surface inverted and then reinvert on display.
		mbVerticalFlip = false;
		if (mSource.pixmap.pitch < 0) {
			mSource.pixmap.data = (char *)mSource.pixmap.data + mSource.pixmap.pitch*(mSource.pixmap.h - 1);
			mSource.pixmap.pitch = -mSource.pixmap.pitch;
			mbVerticalFlip = true;
		}
		return true;
	}

	return false;
}

void VDVideoDisplayMinidriverOpenGL::SetFilterMode(FilterMode mode) {
	if (mPreferredFilter == mode)
		return;

	mPreferredFilter = mode;

	if (mhwndOGL) {
		if (HDC hdc = GetDC(mhwndOGL)) {
			if (mGL.Begin(hdc)) {
				mTexPattern[0].ReinitFiltering(&mGL, mode);
				mTexPattern[1].ReinitFiltering(&mGL, mode);
				mGL.wglMakeCurrent(NULL, NULL);
			}
		}
	}
}

bool VDVideoDisplayMinidriverOpenGL::Resize() {
	if (mhwndOGL) {
		RECT r;

		GetClientRect(mhwnd, &r);
		SetWindowPos(mhwndOGL, 0, 0, 0, r.right, r.bottom, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
	}

	return true;
}

bool VDVideoDisplayMinidriverOpenGL::Update(UpdateMode mode) {
	if (!mGL.IsInited())
		return false;

	if (!mSource.pixmap.data)
		return false;

	if (HDC hdc = GetDC(mhwndOGL)) {
		if (mGL.Begin(hdc)) {
			VDASSERT(mGL.glGetError() == GL_NO_ERROR);

			if (mSource.bInterlaced) {
				uint32 fieldmode = (mode & kModeFieldMask);

				if (fieldmode == kModeAllFields || fieldmode == kModeEvenField) {
					VDPixmap evenFieldSrc(mSource.pixmap);

					evenFieldSrc.h = (evenFieldSrc.h+1) >> 1;
					evenFieldSrc.pitch += evenFieldSrc.pitch;

					Upload(evenFieldSrc, mTexPattern[0]);
				}
				if (fieldmode == kModeAllFields || fieldmode == kModeOddField) {
					VDPixmap oddFieldSrc(mSource.pixmap);

					oddFieldSrc.data = (char *)oddFieldSrc.data + oddFieldSrc.pitch;
					oddFieldSrc.h = (oddFieldSrc.h+1) >> 1;
					oddFieldSrc.pitch += oddFieldSrc.pitch;

					Upload(oddFieldSrc, mTexPattern[1]);
				}
			} else {
				Upload(mSource.pixmap, mTexPattern[0]);
			}

			VDASSERT(mGL.glGetError() == GL_NO_ERROR);

			mGL.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			mGL.End();
		}

		mbValid = true;

		ReleaseDC(mhwndOGL, hdc);
	}

	return true;
}

void VDVideoDisplayMinidriverOpenGL::Refresh(UpdateMode) {
	if (mbValid) {
		InvalidateRect(mhwndOGL, NULL, FALSE);
		UpdateWindow(mhwndOGL);
	}
}

void VDVideoDisplayMinidriverOpenGL::Upload(const VDPixmap& source, VDVideoTextureTilePatternOpenGL& texPattern) {
	VDVideoTextureTilePatternOpenGL::TileInfo *pTiles;
	int nTiles;

	mGL.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	switch(source.format) {
	case nsVDPixmap::kPixFormat_XRGB1555:
	case nsVDPixmap::kPixFormat_RGB565:
		mGL.glPixelStorei(GL_UNPACK_ROW_LENGTH, source.pitch >> 1);
		break;
	case nsVDPixmap::kPixFormat_RGB888:
		mGL.glPixelStorei(GL_UNPACK_ROW_LENGTH, source.pitch / 3);
		break;
	case nsVDPixmap::kPixFormat_XRGB8888:
		mGL.glPixelStorei(GL_UNPACK_ROW_LENGTH, source.pitch >> 2);
		break;
	}

	texPattern.Flip();
	texPattern.GetTileInfo(pTiles, nTiles);

	for(int tileno=0; tileno<nTiles; ++tileno) {
		VDVideoTextureTilePatternOpenGL::TileInfo& tile = *pTiles++;

		mGL.glBindTexture(GL_TEXTURE_2D, tile.mTextureID);

		switch(source.format) {
		case nsVDPixmap::kPixFormat_XRGB1555:
			mGL.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGRA_EXT, GL_UNSIGNED_SHORT_1_5_5_5_REV, (const char *)source.data + (source.pitch*tile.mSrcY + tile.mSrcX*2));
			break;
		case nsVDPixmap::kPixFormat_RGB565:
			mGL.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGR_EXT, GL_UNSIGNED_SHORT_5_6_5_REV, (const char *)source.data + (source.pitch*tile.mSrcY + tile.mSrcX*2));
			break;
		case nsVDPixmap::kPixFormat_RGB888:
			mGL.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGR_EXT, GL_UNSIGNED_BYTE, (const char *)source.data + (source.pitch*tile.mSrcY + tile.mSrcX*3));
			break;
		case nsVDPixmap::kPixFormat_XRGB8888:
			mGL.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (const char *)source.data + (source.pitch*tile.mSrcY + tile.mSrcX*4));
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////

ATOM VDVideoDisplayMinidriverOpenGL::Register() {
	WNDCLASS wc;

	wc.style			= CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= StaticWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= sizeof(VDVideoDisplayMinidriverOpenGL *);
	wc.hInstance		= VDGetLocalModuleHandleW32();
	wc.hIcon			= 0;
	wc.hCursor			= 0;
	wc.hbrBackground	= 0;
	wc.lpszMenuName		= 0;
	wc.lpszClassName	= "phaeronOpenGLVideoDisplay";

	return RegisterClass(&wc);
}

LRESULT CALLBACK VDVideoDisplayMinidriverOpenGL::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDVideoDisplayMinidriverOpenGL *pThis = (VDVideoDisplayMinidriverOpenGL *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		pThis = (VDVideoDisplayMinidriverOpenGL *)((LPCREATESTRUCT)lParam)->lpCreateParams;
		SetWindowLongPtr(hwnd, 0, (DWORD_PTR)pThis);
		pThis->mhwndOGL = hwnd;
		break;
	}

	return pThis ? pThis->WndProc(msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT VDVideoDisplayMinidriverOpenGL::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case MYWM_OGLINIT:
		return OnOpenGLInit();
	case WM_DESTROY:
		OnDestroy();
		break;
	case WM_PAINT:
		OnPaint();
		return 0;
	case WM_NCHITTEST:
		return HTTRANSPARENT;
	}

	return DefWindowProc(mhwndOGL, msg, wParam, lParam);
}

bool VDVideoDisplayMinidriverOpenGL::OnOpenGLInit() {
	if (HDC hdc = GetDC(mhwndOGL)) {
		if (mGL.Attach(hdc, 8, 0, 0, 0, false)) {
			if (mGL.Begin(hdc)) {
				VDDEBUG_DISP("VideoDisplay: OpenGL version string: [%s]\n", mGL.glGetString(GL_VERSION));

				const GLubyte *pExtensions = mGL.glGetString(GL_EXTENSIONS);

				vdfastvector<char> extstr(strlen((const char *)pExtensions)+1);
				std::copy(pExtensions, pExtensions + extstr.size(), extstr.data());

				char *s = extstr.data();

				bool bPackedPixelsSupported = false;
				bool bEdgeClampSupported = false;

				while(const char *tok = strtok(s, " ")) {
					if (!strcmp(tok, "GL_EXT_packed_pixels"))
						bPackedPixelsSupported = true;
					else if (!strcmp(tok, "GL_EXT_texture_edge_clamp"))
						bEdgeClampSupported = true;
					s = NULL;
				}

				if (mSource.bInterlaced) {
					mTexPattern[0].Init(&mGL, mSource.pixmap.w, (mSource.pixmap.h+1)>>1, bPackedPixelsSupported, bEdgeClampSupported);
					mTexPattern[1].Init(&mGL, mSource.pixmap.w, mSource.pixmap.h>>1, bPackedPixelsSupported, bEdgeClampSupported);
					mTexPattern[1].ReinitFiltering(&mGL, mPreferredFilter);
				} else
					mTexPattern[0].Init(&mGL, mSource.pixmap.w, mSource.pixmap.h, bPackedPixelsSupported, bEdgeClampSupported);
				mTexPattern[0].ReinitFiltering(&mGL, mPreferredFilter);

				VDASSERT(mGL.glGetError() == GL_NO_ERROR);

				mGL.End();
				ReleaseDC(mhwndOGL, hdc);

				VDDEBUG_DISP("VideoDisplay: Using OpenGL for %dx%d display.\n", mSource.pixmap.w, mSource.pixmap.h);
				return true;
			}
			mGL.Detach();
		}

		ReleaseDC(mhwndOGL, hdc);
	}

	return false;
}

void VDVideoDisplayMinidriverOpenGL::OnDestroy() {
	if (mGL.IsInited()) {
		if (mTexPattern[0].IsInited() || mTexPattern[1].IsInited()) {
			if (HDC hdc = GetDC(mhwndOGL)) {
				if (mGL.Begin(hdc)) {
					mTexPattern[0].Shutdown(&mGL);
					mTexPattern[1].Shutdown(&mGL);
					mGL.End();
				}
			}
		}

		mGL.Detach();
	}
}

void VDVideoDisplayMinidriverOpenGL::OnPaint() {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhwndOGL, &ps);

	if (!hdc)
		return;

	RECT r;
	GetClientRect(mhwndOGL, &r);

	if (mGL.Begin(hdc)) {
		mGL.glViewport(0, 0, r.right, r.bottom);

		if (mColorOverride) {
			mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			mGL.glClearColor(
				(float)(mColorOverride & 0x00ff0000) / (float)0x00ff0000,
				(float)(mColorOverride & 0x0000ff00) / (float)0x0000ff00,
				(float)(mColorOverride & 0x000000ff) / (float)0x000000ff,
				0.0f);
			mGL.glClear(GL_COLOR_BUFFER_BIT);
		} else {
			mGL.glMatrixMode(GL_PROJECTION);
			mGL.glLoadIdentity();
			mGL.glMatrixMode(GL_MODELVIEW);

			mGL.glDisable(GL_ALPHA_TEST);
			mGL.glDisable(GL_DEPTH_TEST);
			mGL.glDisable(GL_STENCIL_TEST);
			mGL.glDisable(GL_BLEND);
			mGL.glDisable(GL_CULL_FACE);
			mGL.glEnable(GL_DITHER);
			mGL.glEnable(GL_TEXTURE_2D);

			VDVideoTextureTilePatternOpenGL::TileInfo *pTiles;
			int nTiles;

			if (mSource.bInterlaced) {
				const int dstH = r.bottom;

				const double viewmat[16]={
					2.0 / mSource.pixmap.w,
					0,
					0,
					0,

					0,
					(mbVerticalFlip ? +2.0 : -2.0) / dstH,
					0,
					0,

					0,
					0,
					0,
					0.0,

					-1.0,
					(mbVerticalFlip ? -1.0 : +1.0),
					-0.5,
					+1.0,
				};

				mGL.glLoadMatrixd(viewmat);

				for(int field=0; field<2; ++field) {
					mTexPattern[field].GetTileInfo(pTiles, nTiles);

					for(int tileno=0; tileno<nTiles; ++tileno) {
						VDVideoTextureTilePatternOpenGL::TileInfo& tile = *pTiles++;		

						double	xbase	= tile.mSrcX;
						double	ybase	= tile.mSrcY;

						int		w = tile.mSrcW;
						int		h = tile.mSrcH;
						double	iw = tile.mInvU;
						double	ih = tile.mInvV;

						double	px1 = tile.mbLeft ? 0 : 0.5;
						double	py1 = tile.mbTop ? 0 : 0.5;
						double	px2 = w - (tile.mbRight ? 0 : 0.5f);
						double	py2 = h - (tile.mbBottom ? 0 : 0.5f);

						double	u1 = iw * px1;
						double	u2 = iw * px2;

						ih *= mSource.pixmap.h / (double)dstH * 0.5;

						mGL.glBindTexture(GL_TEXTURE_2D, tile.mTextureID);

						int ytop	= VDRoundToInt(ceil(((ybase + py1)*2 + field - 0.5) * (dstH / (double)mSource.pixmap.h) - 0.5));
						int ybottom	= VDRoundToInt(ceil(((ybase + py2)*2 + field - 0.5) * (dstH / (double)mSource.pixmap.h) - 0.5));

						if ((ytop^field) & 1)
							++ytop;

						mGL.glBegin(GL_QUADS);
						mGL.glColor4d(1.0f, 1.0f, 1.0f, 1.0f);

						for(int ydst = ytop; ydst < ybottom; ydst += 2) {
							mGL.glTexCoord2d(u1, (ydst  -ybase-field+0.5)*ih);		mGL.glVertex2d(xbase + px1, ydst);
							mGL.glTexCoord2d(u1, (ydst+1-ybase-field+0.5)*ih);		mGL.glVertex2d(xbase + px1, ydst+1);
							mGL.glTexCoord2d(u2, (ydst+1-ybase-field+0.5)*ih);		mGL.glVertex2d(xbase + px2, ydst+1);
							mGL.glTexCoord2d(u2, (ydst  -ybase-field+0.5)*ih);		mGL.glVertex2d(xbase + px2, ydst);
						}

						mGL.glEnd();
					}
				}
			} else {
				const double viewmat[16]={
					2.0 / mSource.pixmap.w,
					0,
					0,
					0,

					0,
					(mbVerticalFlip ? +2.0 : -2.0) / mSource.pixmap.h,
					0,
					0,

					0,
					0,
					0,
					0.0,

					-1.0,
					(mbVerticalFlip ? -1.0 : +1.0),
					-0.5,
					+1.0,
				};

				mGL.glLoadMatrixd(viewmat);

				mTexPattern[0].GetTileInfo(pTiles, nTiles);
				for(int tileno=0; tileno<nTiles; ++tileno) {
					VDVideoTextureTilePatternOpenGL::TileInfo& tile = *pTiles++;		

					double	xbase	= tile.mSrcX;
					double	ybase	= tile.mSrcY;

					int		w = tile.mSrcW;
					int		h = tile.mSrcH;
					double	iw = tile.mInvU;
					double	ih = tile.mInvV;

					double	px1 = tile.mbLeft ? 0 : 0.5;
					double	py1 = tile.mbTop ? 0 : 0.5;
					double	px2 = w - (tile.mbRight ? 0 : 0.5f);
					double	py2 = h - (tile.mbBottom ? 0 : 0.5f);

					double	u1 = iw * px1;
					double	v1 = ih * py1;
					double	u2 = iw * px2;
					double	v2 = ih * py2;

					mGL.glBindTexture(GL_TEXTURE_2D, tile.mTextureID);

					mGL.glBegin(GL_QUADS);
					mGL.glColor4d(1.0f, 1.0f, 1.0f, 1.0f);
					mGL.glTexCoord2d(u1, v1);		mGL.glVertex2d(xbase + px1, ybase + py1);
					mGL.glTexCoord2d(u1, v2);		mGL.glVertex2d(xbase + px1, ybase + py2);
					mGL.glTexCoord2d(u2, v2);		mGL.glVertex2d(xbase + px2, ybase + py2);
					mGL.glTexCoord2d(u2, v1);		mGL.glVertex2d(xbase + px2, ybase + py1);
					mGL.glEnd();
				}
			}
		}

		VDASSERT(mGL.glGetError() == GL_NO_ERROR);

		mGL.glFlush();

		SwapBuffers(hdc);

		// Workaround for Windows Vista DWM composition chain not updating.
		if (mbFirstPresent) {
			SetWindowPos(mhwndOGL, NULL, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER|SWP_FRAMECHANGED);
			SetWindowPos(mhwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER|SWP_FRAMECHANGED);
			mbFirstPresent = false;
		}

		mGL.End();
	}

	EndPaint(mhwndOGL, &ps);
}
