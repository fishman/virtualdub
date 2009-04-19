//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"
#include "FilterFrame.h"
#include "FilterFrameManualSource.h"
#include "FilterFrameRequest.h"

class VDFilterFrameRequest;

VDFilterFrameManualSource::VDFilterFrameManualSource() {
	mLayout.format = 0;
}

VDFilterFrameManualSource::~VDFilterFrameManualSource() {
}

void VDFilterFrameManualSource::SetAllocator(VDFilterFrameAllocator *pAllocator) {
	mpAllocator = pAllocator;
}

bool VDFilterFrameManualSource::CreateRequest(sint64 outputFrame, bool writable, IVDFilterFrameClientRequest **req) {
	vdrefptr<VDFilterFrameRequest> r;
	bool cached = false;
	bool newRequest = false;

	if (!mFrameQueueWaiting.GetRequest(outputFrame, ~r) && !mFrameQueueInProgress.GetRequest(outputFrame, ~r)) {
		newRequest = true;
		mFrameQueueWaiting.CreateRequest(~r);

		VDFilterFrameRequestTiming timing;
		timing.mSourceFrame = outputFrame;
		timing.mOutputFrame = outputFrame;
		r->SetTiming(timing);

		vdrefptr<VDFilterFrameBuffer> buf;

		if (mFrameCache.Lookup(outputFrame, ~buf)) {
			r->SetResultBuffer(buf);
			cached = true;
		}
	}

	vdrefptr<IVDFilterFrameClientRequest> creq;
	r->CreateClient(writable, ~creq);

	if (cached)
		r->MarkComplete(true);
	else if (newRequest)
		mFrameQueueWaiting.Add(r);

	*req = creq.release();
	return true;
}

bool VDFilterFrameManualSource::GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex) {
	sourceIndex = 0;
	sourceFrame = outputFrame;
	return true;
}

sint64 VDFilterFrameManualSource::GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source) {
	return source == this ? outputFrame : -1;
}

sint64 VDFilterFrameManualSource::GetSourceFrame(sint64 outputFrame) {
	return outputFrame;
}

sint64 VDFilterFrameManualSource::GetNearestUniqueFrame(sint64 outputFrame) {
	return outputFrame;
}

bool VDFilterFrameManualSource::PeekNextRequestFrame(VDPosition& pos) {
	vdrefptr<VDFilterFrameRequest> req;
	if (!mFrameQueueWaiting.PeekNextRequest(~req))
		return false;

	pos = req->GetTiming().mOutputFrame;
	return true;
}

bool VDFilterFrameManualSource::GetNextRequest(VDFilterFrameRequest **ppReq) {
	vdrefptr<VDFilterFrameRequest> req;
	if (!mFrameQueueWaiting.GetNextRequest(~req))
		return false;

	mFrameQueueInProgress.Add(req);

	*ppReq = req.release();
	return true;
}

bool VDFilterFrameManualSource::AllocateRequestBuffer(VDFilterFrameRequest *req) {
	vdrefptr<VDFilterFrameBuffer> buf;
	if (!mpAllocator->Allocate(~buf))
		return false;

	req->SetResultBuffer(buf);
	return true;
}

void VDFilterFrameManualSource::CompleteRequest(VDFilterFrameRequest *req, bool cache) {
	if (cache) {
		VDFilterFrameBuffer *buf = req->GetResultBuffer();
		if (buf)
			mFrameCache.Add(buf, req->GetTiming().mOutputFrame);
	}

	VDVERIFY(mFrameQueueInProgress.Remove(req));
}
