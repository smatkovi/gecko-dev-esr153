/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "modules/video_capture/sfos/video_capture_sfos.h"

#include <cstring>
#include <new>
#include <utility>

#include "api/make_ref_counted.h"
#include "api/video/i420_buffer.h"
#include "common_video/include/video_frame_buffer.h"
#include "libyuv/convert.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace videocapturemodule {

rtc::scoped_refptr<VideoCaptureModule> VideoCaptureImpl::Create(
    const char* device_unique_id_utf8) {
  auto implementation = rtc::make_ref_counted<VideoCaptureModuleSFOS>();
  if (implementation->Init(device_unique_id_utf8) != 0) {
    return nullptr;
  }
  return implementation;
}

rtc::scoped_refptr<VideoCaptureModule> VideoCaptureImpl::Create(
    VideoCaptureOptions* /* options */,
    const char* device_unique_id_utf8) {
  return Create(device_unique_id_utf8);
}

VideoCaptureModuleSFOS::~VideoCaptureModuleSFOS() {
  if (camera_) {
    // stopCapture() is the provider's synchronous callback-quiescence point.
    // Keep the listener valid until it returns so an in-flight callback can
    // finish while this object is still alive.
    camera_->stopCapture();
    camera_->setListener(nullptr);
  }
}

int32_t VideoCaptureModuleSFOS::Init(const char* device_unique_id_utf8) {
  if (!device_unique_id_utf8) {
    return -1;
  }

  RTC_DCHECK_RUN_ON(&api_checker_);
  const size_t name_length = std::strlen(device_unique_id_utf8);
  if (name_length >= kVideoCaptureUniqueNameLength) {
    return -1;
  }

  _deviceUniqueId = new (std::nothrow) char[name_length + 1];
  if (!_deviceUniqueId) {
    return -1;
  }
  std::memcpy(_deviceUniqueId, device_unique_id_utf8, name_length + 1);

  gecko::camera::CameraManager* manager = gecko_camera_manager();
  if (!manager || !manager->openCamera(device_unique_id_utf8, camera_) ||
      !camera_) {
    return -1;
  }

  gecko::camera::CameraInfo info{};
  if (!camera_->getInfo(info)) {
    camera_.reset();
    return -1;
  }

  sensor_mount_angle_ = info.mountAngle;
  if (sensor_mount_angle_ % 90 != 0) {
    RTC_LOG(LS_WARNING) << "Invalid camera mount angle "
                        << sensor_mount_angle_;
    sensor_mount_angle_ = 0;
  }
  camera_->setListener(this);
  return 0;
}

int32_t VideoCaptureModuleSFOS::StartCapture(
    const VideoCaptureCapability& capability) {
  RTC_DCHECK_RUN_ON(&api_checker_);
  if (!camera_ || capability.width <= 0 || capability.height <= 0 ||
      capability.maxFPS <= 0) {
    return -1;
  }

  if (camera_->captureStarted()) {
    if (capability == _requestedCapability) {
      return 0;
    }
    if (!camera_->stopCapture()) {
      return -1;
    }
  }

  UpdateCaptureRotation();

  gecko::camera::CameraCapability camera_capability{};
  camera_capability.width = static_cast<unsigned int>(capability.width);
  camera_capability.height = static_cast<unsigned int>(capability.height);
  camera_capability.fps = static_cast<unsigned int>(capability.maxFPS);
  if (!camera_->startCapture(camera_capability)) {
    return -1;
  }

  _requestedCapability = capability;
  _requestedCapability.videoType = VideoType::kI420;
  return 0;
}

int32_t VideoCaptureModuleSFOS::StopCapture() {
  RTC_DCHECK_RUN_ON(&api_checker_);
  if (!camera_) {
    return -1;
  }
  if (!camera_->captureStarted()) {
    return 0;
  }
  return camera_->stopCapture() ? 0 : -1;
}

bool VideoCaptureModuleSFOS::CaptureStarted() {
  RTC_DCHECK_RUN_ON(&api_checker_);
  return camera_ && camera_->captureStarted();
}

int32_t VideoCaptureModuleSFOS::CaptureSettings(
    VideoCaptureCapability& settings) {
  RTC_DCHECK_RUN_ON(&api_checker_);
  settings = _requestedCapability;
  return 0;
}

void VideoCaptureModuleSFOS::onCameraFrame(
    std::shared_ptr<gecko::camera::GraphicBuffer> graphic_buffer) {
  if (!graphic_buffer ||
      graphic_buffer->imageFormat != gecko::camera::ImageFormat::YCbCr) {
    RTC_LOG(LS_ERROR) << "Invalid camera image format";
    return;
  }

  auto frame = graphic_buffer->mapYCbCr();
  if (!frame || !frame->y || !frame->cb || !frame->cr || !frame->width ||
      !frame->height || !frame->chromaStep) {
    RTC_LOG(LS_ERROR) << "Could not map camera frame";
    return;
  }

  const int chroma_width = (frame->width + 1) / 2;
  const int minimum_chroma_stride =
      (chroma_width - 1) * frame->chromaStep + 1;
  if (frame->yStride < frame->width ||
      frame->cStride < minimum_chroma_stride) {
    RTC_LOG(LS_ERROR) << "Invalid camera frame strides";
    return;
  }

  const uint64_t timestamp_us = frame->timestampUs;
  rtc::scoped_refptr<I420BufferInterface> buffer;
  if (frame->chromaStep == 1) {
    buffer = WrapI420Buffer(
        frame->width, frame->height, frame->y, frame->yStride, frame->cb,
        frame->cStride, frame->cr, frame->cStride,
        [frame = std::move(frame)]() {});
  } else if (frame->chromaStep == 2) {
    auto owned_buffer = I420Buffer::Create(frame->width, frame->height);
    const int result = libyuv::Android420ToI420(
        frame->y, frame->yStride, frame->cb, frame->cStride, frame->cr,
        frame->cStride, frame->chromaStep, owned_buffer->MutableDataY(),
        owned_buffer->StrideY(), owned_buffer->MutableDataU(),
        owned_buffer->StrideU(), owned_buffer->MutableDataV(),
        owned_buffer->StrideV(), frame->width, frame->height);
    if (result != 0) {
      RTC_LOG(LS_ERROR) << "Could not convert camera frame to I420";
      return;
    }
    buffer = std::move(owned_buffer);
  } else {
    RTC_LOG(LS_ERROR) << "Unsupported camera chroma step "
                      << frame->chromaStep;
    return;
  }

  Clock* clock = Clock::GetRealTimeClockRaw();
  int64_t capture_time_ms = clock->CurrentNtpInMilliseconds();
  if (timestamp_us) {
    const int64_t frame_time_ms =
        static_cast<int64_t>(timestamp_us / 1000);
    const int64_t now_ms = clock->TimeInMilliseconds();
    // Production droid camera timestamps use Android's monotonic clock, which
    // shares WebRTC's epoch. Fall back to callback time for other providers.
    if (frame_time_ms >= now_ms - 60'000 && frame_time_ms <= now_ms + 1'000) {
      capture_time_ms =
          clock->ConvertTimestampToNtpTimeInMilliseconds(frame_time_ms);
    }
  }
  if (IncomingVideoBuffer(buffer, capture_time_ms) != 0) {
    RTC_LOG(LS_ERROR) << "Could not deliver camera frame";
  }
}

void VideoCaptureModuleSFOS::onCameraError(std::string error_description) {
  RTC_LOG(LS_ERROR) << "Camera error: " << error_description;
}

void VideoCaptureModuleSFOS::UpdateCaptureRotation() {
  VideoRotation rotation = kVideoRotation_0;
  if (RotationFromDegrees(static_cast<int>(sensor_mount_angle_ % 360),
                          &rotation) != 0) {
    RTC_LOG(LS_WARNING) << "Could not convert camera mount angle";
  }
  RTC_LOG(LS_INFO) << "Camera sensor mount angle=" << sensor_mount_angle_;
  SetCaptureRotation(rotation);
}

}  // namespace videocapturemodule
}  // namespace webrtc
