/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MODULES_VIDEO_CAPTURE_SFOS_DEVICE_INFO_SFOS_H_
#define MODULES_VIDEO_CAPTURE_SFOS_DEVICE_INFO_SFOS_H_

#include <sys/types.h>

#include <string>
#include <vector>

#include <geckocamera.h>

#include "modules/video_capture/device_info_impl.h"

namespace webrtc {
namespace videocapturemodule {

class DeviceInfoSFOS final : public DeviceInfoImpl {
 public:
  DeviceInfoSFOS();
  ~DeviceInfoSFOS() override = default;

  uint32_t NumberOfDevices() override;
  int32_t Refresh() override;
  int32_t GetDeviceName(uint32_t device_number,
                        char* device_name_utf8,
                        uint32_t device_name_length,
                        char* device_unique_id_utf8,
                        uint32_t device_unique_id_utf8_length,
                        char* product_unique_id_utf8,
                        uint32_t product_unique_id_utf8_length,
                        pid_t* pid,
                        bool* device_is_placeholder) override;
  int32_t DisplayCaptureSettingsDialogBox(const char* device_unique_id_utf8,
                                          const char* dialog_title_utf8,
                                          void* parent_window,
                                          uint32_t position_x,
                                          uint32_t position_y) override;

 private:
  int32_t Init() override;
  int32_t CreateCapabilityMap(const char* device_unique_id_utf8) override;
  bool RefreshCameraList() RTC_EXCLUSIVE_LOCKS_REQUIRED(_apiLock);

  gecko::camera::CameraManager* camera_manager_ = nullptr;
  std::vector<gecko::camera::CameraInfo> camera_list_
      RTC_GUARDED_BY(_apiLock);
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_SFOS_DEVICE_INFO_SFOS_H_
