#ifndef f_VD2_KASUMI_TRIBLT_H
#define f_VD2_KASUMI_TRIBLT_H

#include <vd2/system/vdstl.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vector>

struct VDTriBltVertex {
	float x, y, z, u, v;
};

struct VDTriColorVertex {
	float x, y, z, r, g, b, a;
};

enum VDTriBltFilterMode {
	kTriBltFilterPoint,
	kTriBltFilterBilinear,
	kTriBltFilterTrilinear,
	kTriBltFilterCount
};

bool VDPixmapTriFill(VDPixmap& dst, uint32 c,
					const VDTriBltVertex *pVertices, int nVertices,
					const int *pIndices, const int nIndices,
					const float pTransform[16] = NULL);

bool VDPixmapTriFill(VDPixmap& dst,
					const VDTriColorVertex *pVertices, int nVertices,
					const int *pIndices, const int nIndices,
					const float pTransform[16] = NULL);

bool VDPixmapTriBlt(VDPixmap& dst, const VDPixmap *const *pSources, int nMipmaps,
					const VDTriBltVertex *pVertices, int nVertices,
					const int *pIndices, const int nIndices,
					VDTriBltFilterMode filterMode,
					bool border = false,
					const float pTransform[16] = NULL);

class VDPixmapTextureMipmapChain {
public:
	VDPixmapTextureMipmapChain(const VDPixmap& src, bool wrap=false, int maxlevels = 16);

	const VDPixmap *const *Mips() const { return mMipMaps.data(); }
	int Levels() const { return mMipMaps.size(); }

protected:
	std::vector<VDPixmapBuffer>		mBuffers;
	vdfastvector<const VDPixmap *>	mMipMaps;
};

#endif
