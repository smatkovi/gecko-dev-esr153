/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoFrameUtils.h"
#include "api/video/video_frame.h"
#include "mozilla/ShmemPool.h"

#ifdef MOZ_ENABLE_WEBRTC_GECKOCAMERA
#  include <algorithm>

#  include "libyuv/rotate.h"
#endif

namespace mozilla {

#ifdef MOZ_ENABLE_WEBRTC_GECKOCAMERA
namespace {

struct PackedI420Layout {
  int width;
  int height;
  int strideY;
  int strideUV;
  size_t sizeY;
  size_t sizeU;
  size_t sizeV;

  size_t TotalSize() const { return sizeY + sizeU + sizeV; }
};

PackedI420Layout GetPackedI420Layout(
    const webrtc::VideoFrame& aVideoFrame) {
  auto i420 = aVideoFrame.video_frame_buffer()->ToI420();
  int width = i420->width();
  int height = i420->height();
  MOZ_RELEASE_ASSERT(width > 0 && height > 0);

  if (aVideoFrame.rotation() == webrtc::kVideoRotation_90 ||
      aVideoFrame.rotation() == webrtc::kVideoRotation_270) {
    std::swap(width, height);
  }

  const int strideY = width;
  const int strideUV = (width + 1) / 2;
  const size_t chromaHeight = (static_cast<size_t>(height) + 1) / 2;
  return {width,
          height,
          strideY,
          strideUV,
          static_cast<size_t>(height) * strideY,
          chromaHeight * strideUV,
          chromaHeight * strideUV};
}

}  // namespace
#endif

uint32_t VideoFrameUtils::TotalRequiredBufferSize(
    const webrtc::VideoFrame& aVideoFrame) {
#ifdef MOZ_ENABLE_WEBRTC_GECKOCAMERA
  size_t size = GetPackedI420Layout(aVideoFrame).TotalSize();
#else
  auto i420 = aVideoFrame.video_frame_buffer()->ToI420();
  auto height = i420->height();
  size_t size = height * i420->StrideY() +
                ((height + 1) / 2) * i420->StrideU() +
                ((height + 1) / 2) * i420->StrideV();
#endif
  MOZ_RELEASE_ASSERT(size < std::numeric_limits<uint32_t>::max());
  return static_cast<uint32_t>(size);
}

void VideoFrameUtils::InitFrameBufferProperties(
    const webrtc::VideoFrame& aVideoFrame,
    camera::VideoFrameProperties& aDestProps) {
  aDestProps.captureTime() = TimeStamp::Now();

  // The VideoFrameBuffer image data stored in the accompanying buffer
  // the buffer is at least this size of larger.
  aDestProps.bufferSize() = TotalRequiredBufferSize(aVideoFrame);

  aDestProps.rtpTimeStamp() = aVideoFrame.rtp_timestamp();
  aDestProps.ntpTimeMs() = aVideoFrame.ntp_time_ms();
  aDestProps.renderTimeMs() = aVideoFrame.render_time_ms();
  aDestProps.rotation() = aVideoFrame.rotation();

#ifdef MOZ_ENABLE_WEBRTC_GECKOCAMERA
  const auto layout = GetPackedI420Layout(aVideoFrame);
  aDestProps.width() = layout.width;
  aDestProps.height() = layout.height;
  aDestProps.yStride() = layout.strideY;
  aDestProps.uStride() = layout.strideUV;
  aDestProps.vStride() = layout.strideUV;
  aDestProps.yAllocatedSize() = layout.sizeY;
  aDestProps.uAllocatedSize() = layout.sizeU;
  aDestProps.vAllocatedSize() = layout.sizeV;
#else
  auto i420 = aVideoFrame.video_frame_buffer()->ToI420();
  auto height = i420->height();
  aDestProps.yAllocatedSize() = height * i420->StrideY();
  aDestProps.uAllocatedSize() = ((height + 1) / 2) * i420->StrideU();
  aDestProps.vAllocatedSize() = ((height + 1) / 2) * i420->StrideV();

  aDestProps.width() = i420->width();
  aDestProps.height() = height;

  aDestProps.yStride() = i420->StrideY();
  aDestProps.uStride() = i420->StrideU();
  aDestProps.vStride() = i420->StrideV();
#endif
}

void VideoFrameUtils::CopyVideoFrameBuffers(uint8_t* aDestBuffer,
                                            const size_t aDestBufferSize,
                                            const webrtc::VideoFrame& aFrame) {
  size_t aggregateSize = TotalRequiredBufferSize(aFrame);

  MOZ_ASSERT(aDestBufferSize >= aggregateSize);
  auto i420 = aFrame.video_frame_buffer()->ToI420();
  const size_t sourceSize =
      static_cast<size_t>(i420->height()) * i420->StrideY() +
      ((static_cast<size_t>(i420->height()) + 1) / 2) * i420->StrideU() +
      ((static_cast<size_t>(i420->height()) + 1) / 2) * i420->StrideV();

  // If planes are ordered YUV and contiguous then do a single copy.
  const bool is_contiguous =
      (i420->DataY() != nullptr) &&
      // Check that the three planes are ordered.
      (i420->DataY() < i420->DataU()) && (i420->DataU() < i420->DataV()) &&
      // Check that the last plane ends at firstPlane[totalsize].
      (&i420->DataY()[sourceSize] ==
       &i420->DataV()[((i420->height() + 1) / 2) * i420->StrideV()]);

#ifdef MOZ_ENABLE_WEBRTC_GECKOCAMERA
  const auto layout = GetPackedI420Layout(aFrame);
  const bool is_tightly_packed =
      i420->StrideY() == layout.strideY &&
      i420->StrideU() == layout.strideUV &&
      i420->StrideV() == layout.strideUV;
  if (is_contiguous && is_tightly_packed &&
      aFrame.rotation() == webrtc::kVideoRotation_0) {
#else
  if (is_contiguous) {
#endif
    memcpy(aDestBuffer, i420->DataY(), aggregateSize);
    return;
  }

#ifdef MOZ_ENABLE_WEBRTC_GECKOCAMERA
  libyuv::RotationMode rotation_mode;

  switch (aFrame.rotation()) {
    case webrtc::kVideoRotation_90:
      rotation_mode = libyuv::kRotate90;
      break;
    case webrtc::kVideoRotation_180:
      rotation_mode = libyuv::kRotate180;
      break;
    case webrtc::kVideoRotation_270:
      rotation_mode = libyuv::kRotate270;
      break;
    case webrtc::kVideoRotation_0:
    default:
      rotation_mode = libyuv::kRotate0;
      break;
  }

  const size_t offset_u = layout.sizeY;
  const size_t offset_v = offset_u + layout.sizeU;

  MOZ_RELEASE_ASSERT(
      libyuv::I420Rotate(
          i420->DataY(), i420->StrideY(), i420->DataU(), i420->StrideU(),
          i420->DataV(), i420->StrideV(), aDestBuffer, layout.strideY,
          &aDestBuffer[offset_u], layout.strideUV, &aDestBuffer[offset_v],
          layout.strideUV, i420->width(), i420->height(), rotation_mode) == 0);
#else
  // Copy each plane
  size_t offset = 0;
  size_t size;
  auto height = i420->height();
  size = height * i420->StrideY();
  memcpy(&aDestBuffer[offset], i420->DataY(), size);
  offset += size;
  size = ((height + 1) / 2) * i420->StrideU();
  memcpy(&aDestBuffer[offset], i420->DataU(), size);
  offset += size;
  size = ((height + 1) / 2) * i420->StrideV();
  memcpy(&aDestBuffer[offset], i420->DataV(), size);
#endif
}

void VideoFrameUtils::CopyVideoFrameBuffers(
    ShmemBuffer& aDestShmem, const webrtc::VideoFrame& aVideoFrame) {
  CopyVideoFrameBuffers(aDestShmem.Get().get<uint8_t>(),
                        aDestShmem.Get().Size<uint8_t>(), aVideoFrame);
}

}  // namespace mozilla
