/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MODULES_VIDEO_CAPTURE_SFOS_VIDEO_CAPTURE_SFOS_H_
#define MODULES_VIDEO_CAPTURE_SFOS_VIDEO_CAPTURE_SFOS_H_

#include <memory>
#include <string>

#include <geckocamera.h>

#include "modules/video_capture/video_capture_impl.h"

namespace webrtc {
namespace videocapturemodule {

class VideoCaptureModuleSFOS : public VideoCaptureImpl,
                               public gecko::camera::CameraListener {
 public:
  VideoCaptureModuleSFOS() = default;
  ~VideoCaptureModuleSFOS() override;

  int32_t Init(const char* device_unique_id_utf8);
  int32_t StartCapture(const VideoCaptureCapability& capability) override;
  int32_t StopCapture() override;
  bool CaptureStarted() override;
  int32_t CaptureSettings(VideoCaptureCapability& settings) override;

  void onCameraFrame(
      std::shared_ptr<gecko::camera::GraphicBuffer> buffer) override;
  void onCameraError(std::string error_description) override;

 private:
  void UpdateCaptureRotation();

  unsigned int sensor_mount_angle_ = 0;
  std::shared_ptr<gecko::camera::Camera> camera_;
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_SFOS_VIDEO_CAPTURE_SFOS_H_
