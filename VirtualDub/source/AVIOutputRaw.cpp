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

#include "stdafx.h"

#include <windows.h>
#include <vfw.h>

#include <vd2/system/error.h>

#include "AVIOutputRaw.h"

extern uint32 VDPreferencesGetFileAsyncDefaultMode();

//////////////////////////////////////////////////////////////////////
//
// AVIAudioOutputStreamRaw
//
//////////////////////////////////////////////////////////////////////

class AVIAudioOutputStreamRaw : public AVIOutputStream {
public:
	AVIAudioOutputStreamRaw(AVIOutputRaw *pParent);

	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples);
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples);
	void partialWrite(const void *pBuffer, uint32 cbBuffer);
	void partialWriteEnd();

protected:
	AVIOutputRaw *const mpParent;
};

AVIAudioOutputStreamRaw::AVIAudioOutputStreamRaw(AVIOutputRaw *pParent) : mpParent(pParent) {
}

void AVIAudioOutputStreamRaw::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	mpParent->write(pBuffer, cbBuffer);
}

void AVIAudioOutputStreamRaw::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {

}

void AVIAudioOutputStreamRaw::partialWrite(const void *pBuffer, uint32 cbBuffer) {
	mpParent->write(pBuffer, cbBuffer);
}

void AVIAudioOutputStreamRaw::partialWriteEnd() {
}

//////////////////////////////////////////////////////////////////////
//
// AVIOutputRaw
//
//////////////////////////////////////////////////////////////////////

AVIOutputRaw::AVIOutputRaw() {
	mBytesWritten		= 0;
	mBufferSize			= 65536;
}

AVIOutputRaw::~AVIOutputRaw() {
}

IVDMediaOutputStream *AVIOutputRaw::createVideoStream() {
	return NULL;
}

IVDMediaOutputStream *AVIOutputRaw::createAudioStream() {
	VDASSERT(!audioOut);
	if (!(audioOut = new_nothrow AVIAudioOutputStreamRaw(this)))
		throw MyMemoryError();
	return audioOut;
}

bool AVIOutputRaw::init(const wchar_t *pwszFile) {
	mpFileAsync = VDCreateFileAsync((IVDFileAsync::Mode)VDPreferencesGetFileAsyncDefaultMode());
	mpFileAsync->Open(pwszFile, 2, mBufferSize >> 1);

	mBytesWritten = 0;
	return true;
}

void AVIOutputRaw::finalize() {
	if (!mpFileAsync->IsOpen())
		return;

	mpFileAsync->FastWriteEnd();
	mpFileAsync->Truncate(mBytesWritten);
	mpFileAsync->Close();
}

void AVIOutputRaw::write(const void *pBuffer, uint32 cbBuffer) {
	mpFileAsync->FastWrite(pBuffer, cbBuffer);
	mBytesWritten += cbBuffer;
}
