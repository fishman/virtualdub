//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2005 Avery Lee
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
//

#ifndef f_VIDEODISPLAYDRIVERDX9_H
#define f_VIDEODISPLAYDRIVERDX9_H

#include <windows.h>
#include <d3d9.h>

#include <vd2/system/vdstl.h>

namespace nsVDVideoDisplayDriverDX9 {
	enum {
		kVertexBufferSize	= 4096,						// in vertices
		kIndexBufferSize	= kVertexBufferSize*3/2		// in indices
	};

	struct Vertex {
		float x, y, z;
		uint32 diffuse;
		float u0, v0, u1, v1, u2, v2, u3, v3, u4, v4;

		Vertex(float x_, float y_, float z_, uint32 c_, uint32 d_, float u0_, float v0_, float u1_=0.f, float v1_=0.f) : x(x_), y(y_), z(z_), diffuse(c_), u0(u0_), v0(v0_), u1(u1_), v1(v1_)
			, u2(0), v2(0), u3(0), v3(0), u4(0), v4(0) {}

		inline void SetFF1(float x_, float y_, float u0_, float v0_) {
			x = x_;
			y = y_;
			z = 0.f;
			diffuse = 0xffffffff;
			u0 = u0_;
			v0 = v0_;
			u1 = v1 = u2 = v2 = u3 = v3 = u4 = v4 = 0.f;
		}

		inline void SetFF2(float x_, float y_, uint32 c_, float u0_, float v0_, float u1_, float v1_) {
			x = x_;
			y = y_;
			z = 0.f;
			diffuse = c_;
			u0 = u0_;
			v0 = v0_;
			u1 = u1_;
			v1 = v1_;
			u2 = v2 = u3 = v3 = u4 = v4 = 0.f;
		}

		inline void SetFF3(float x_, float y_, float u0_, float v0_, float u1_, float v1_, float u2_, float v2_) {
			x = x_;
			y = y_;
			z = 0.f;
			diffuse = 0xffffffff;
			u0 = u0_;
			v0 = v0_;
			u1 = u1_;
			v1 = v1_;
			u2 = u2_;
			v2 = v2_;
			u3 = v3 = u4 = v4 = 0.f;
		}

		inline void SetPS1_4(float x_, float y_, float u0_, float v0_, float u1_, float v1_, float u2_, float v2_,
								float u3_, float v3_, float u4_, float v4_) {
			x = x_;
			y = y_;
			z = 0.f;
			diffuse = 0xffffffff;
			u0 = u0_;
			v0 = v0_;
			u1 = u1_;
			v1 = v1_;
			u2 = u2_;
			v2 = v2_;
			u3 = u3_;
			v3 = v3_;
			u4 = u4_;
			v4 = v4_;
		}
	};
};

class VDVideoDisplayDX9Client : public vdlist<VDVideoDisplayDX9Client>::node {
public:
	virtual void OnPreDeviceReset() = 0;
};

class VDVideoDisplayDX9Manager {
public:
	enum CubicMode {
		kCubicNotInitialized,
		kCubicNotPossible,		// Your card is LAME
		kCubicUseFF2Path,		// Use fixed-function, 2 stage path (GeForce2/GeForce4MX - 8 passes)
		kCubicUseFF3Path,		// Use fixed-function, 3 stage path (RADEON 7xxx - 4 passes)
		kCubicUsePS1_1Path,		// Use programmable, 3 stage path (GeForce3/GeForce4 - 4 passes)
		kCubicUsePS1_4Path,		// Use programmable, 5 stage path (RADEON 85xx+/GeForceFX+ - 2 passes)
		kMaxCubicMode = kCubicUsePS1_4Path
	};

	struct SurfaceInfo {
		int		mWidth;
		int		mHeight;
		float	mInvWidth;
		float	mInvHeight;
	};

	VDVideoDisplayDX9Manager();
	~VDVideoDisplayDX9Manager();

	bool Attach(VDVideoDisplayDX9Client *pClient);
	void Detach(VDVideoDisplayDX9Client *pClient);

	bool InitTempRTT(int index);
	void ShutdownTempRTT(int index);

	CubicMode InitBicubic();
	void ShutdownBicubic();

	const D3DCAPS9& GetCaps() const { return mDevCaps; }
	IDirect3D9 *GetD3D() const { return mpD3D; }
	IDirect3DDevice9 *GetDevice() const { return mpD3DDevice; }
	IDirect3DIndexBuffer9 *GetIndexBuffer() const { return mpD3DIB; }
	IDirect3DVertexBuffer9 *GetVertexBuffer() const { return mpD3DVB; }
	const D3DPRESENT_PARAMETERS& GetPresentParms() const { return mPresentParms; }
	UINT GetAdapter() const { return mAdapter; }
	D3DDEVTYPE GetDeviceType() const { return mDevType; }

	IDirect3DSurface9	*GetRenderTarget() const { return mpD3DRTMain; }
	IDirect3DTexture9	*GetTempRTT(int index) const { return mpD3DRTTs[index]; }
	int GetMainRTWidth() const { return mPresentParms.BackBufferWidth; }
	int GetMainRTHeight() const { return mPresentParms.BackBufferHeight; }
	const SurfaceInfo& GetTempRTTInfo(int index) const { return mRTTSurfaceInfo[index]; }
	IDirect3DTexture9	*GetFilterTexture() const { return mpD3DFilterTexture; }

	IDirect3DPixelShader9	*GetHPixelShader() const { return mpD3DPixelShader1; }
	IDirect3DPixelShader9	*GetVPixelShader() const { return mpD3DPixelShader2; }

	bool		Reset();
	bool		CheckDevice();

	void		AdjustTextureSize(int& w, int& h);
	void		ClearRenderTarget(IDirect3DTexture9 *pTexture);

	void		ResetBuffers();
	nsVDVideoDisplayDriverDX9::Vertex *	LockVertices(unsigned vertices);
	void		UnlockVertices();
	uint16 *	LockIndices(unsigned indices);
	void		UnlockIndices();
	HRESULT		DrawArrays(D3DPRIMITIVETYPE type, UINT vertStart, UINT primCount);
	HRESULT		DrawElements(D3DPRIMITIVETYPE type, UINT vertStart, UINT vertCount, UINT idxStart, UINT primCount);
	HRESULT		Present(const RECT *srcRect, HWND hwndDest, bool vsync);

	HRESULT DisableTextureStage(UINT stage);
	HRESULT SetTextureStageOp(UINT stage, DWORD color1, DWORD colorop, DWORD color2, DWORD alpha1, DWORD alphaop, DWORD alpha2, DWORD output = D3DTA_CURRENT);

	int GetBicubicShaderStages(CubicMode mode);
	bool Is3DCardLame();
	bool ValidateBicubicShader(CubicMode mode);
	HRESULT SetBicubicShader(CubicMode mode, int stage);

	void MakeCubic4Texture(uint32 *texture, ptrdiff_t pitch, double A, CubicMode mode);
protected:
	bool Init();
	bool InitVRAMResources();
	void ShutdownVRAMResources();
	void Shutdown();

	HMODULE				mhmodDX9;
	IDirect3D9			*mpD3D;
	IDirect3DDevice9	*mpD3DDevice;
	IDirect3DTexture9	*mpD3DFilterTexture;
	IDirect3DSurface9	*mpD3DRTMain;
	IDirect3DTexture9	*mpD3DRTTs[2];
	int					mRTTRefCounts[2];
	SurfaceInfo			mRTTSurfaceInfo[2];
	UINT				mAdapter;
	D3DDEVTYPE			mDevType;

	bool				mbDeviceValid;

	IDirect3DVertexBuffer9	*mpD3DVB;
	IDirect3DIndexBuffer9	*mpD3DIB;
	uint32					mVertexBufferPt;
	uint32					mVertexBufferLockSize;
	uint32					mIndexBufferPt;
	uint32					mIndexBufferLockSize;

	IDirect3DPixelShader9	*mpD3DPixelShader1;
	IDirect3DPixelShader9	*mpD3DPixelShader2;

	D3DCAPS9				mDevCaps;
	D3DPRESENT_PARAMETERS	mPresentParms;

	CubicMode			mCubicMode;
	int					mCubicRefCount;
	bool				mbCubicTempRTTAllocated;

	int					mRefCount;

	vdlist<VDVideoDisplayDX9Client>	mClients;
};

VDVideoDisplayDX9Manager *VDInitDisplayDX9(VDVideoDisplayDX9Client *pClient);
void VDDeinitDisplayDX9(VDVideoDisplayDX9Manager *p, VDVideoDisplayDX9Client *pClient);

#endif
