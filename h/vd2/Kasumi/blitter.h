#ifndef f_VD2_KASUMI_BLITTER_H
#define f_VD2_KASUMI_BLITTER_H

#include <vd2/system/vectors.h>

struct VDPixmap;

class IVDPixmapBlitter {
public:
	virtual ~IVDPixmapBlitter() {}
	virtual void Blit(const VDPixmap& dst, const VDPixmap& src) = 0;
	virtual void Blit(const VDPixmap& dst, const vdrect32 *rDst, const VDPixmap& src) = 0;
};

IVDPixmapBlitter *VDPixmapCreateBlitter(const VDPixmap& dst, const VDPixmap& src);

#endif
