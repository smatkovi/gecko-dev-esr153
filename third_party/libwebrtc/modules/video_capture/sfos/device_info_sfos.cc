/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "modules/video_capture/sfos/device_info_sfos.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "mozilla/Preferences.h"
#include "modules/video_capture/video_capture_impl.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace videocapturemodule {
namespace {

constexpr char kMaxWidthPref[] = "media.getusermedia.camera.max_width";
constexpr char kMaxHeightPref[] = "media.getusermedia.camera.max_height";

bool CopyString(const std::string& source,
                char* destination,
                uint32_t destination_length) {
  if (!destination || destination_length <= source.size()) {
    return false;
  }
  std::memset(destination, 0, destination_length);
  std::memcpy(destination, source.data(), source.size());
  return true;
}

}  // namespace

VideoCaptureModule::DeviceInfo* VideoCaptureImpl::CreateDeviceInfo() {
  return new DeviceInfoSFOS();
}

VideoCaptureModule::DeviceInfo* VideoCaptureImpl::CreateDeviceInfo(
    VideoCaptureOptions* /* options */) {
  return new DeviceInfoSFOS();
}

DeviceInfoSFOS::DeviceInfoSFOS() { Init(); }

int32_t DeviceInfoSFOS::Init() {
  camera_manager_ = gecko_camera_manager();
  return camera_manager_ ? 0 : -1;
}

bool DeviceInfoSFOS::RefreshCameraList() {
  camera_list_.clear();
  if (!camera_manager_) {
    return false;
  }

  const int camera_count = camera_manager_->getNumberOfCameras();
  for (int i = 0; i < camera_count; ++i) {
    gecko::camera::CameraInfo info{};
    if (camera_manager_->getCameraInfo(i, info)) {
      camera_list_.push_back(std::move(info));
    }
  }

  std::stable_sort(
      camera_list_.begin(), camera_list_.end(),
      [](const gecko::camera::CameraInfo& first,
         const gecko::camera::CameraInfo& second) {
        const bool first_is_front =
            first.facing == gecko::camera::GECKO_CAMERA_FACING_FRONT;
        const bool second_is_front =
            second.facing == gecko::camera::GECKO_CAMERA_FACING_FRONT;
        return first_is_front > second_is_front;
      });
  return true;
}

uint32_t DeviceInfoSFOS::NumberOfDevices() {
  MutexLock lock(&_apiLock);
  RefreshCameraList();
  return static_cast<uint32_t>(camera_list_.size());
}

int32_t DeviceInfoSFOS::Refresh() {
  MutexLock lock(&_apiLock);
  return RefreshCameraList() ? 0 : -1;
}

int32_t DeviceInfoSFOS::GetDeviceName(
    uint32_t device_number,
    char* device_name_utf8,
    uint32_t device_name_length,
    char* device_unique_id_utf8,
    uint32_t device_unique_id_utf8_length,
    char* product_unique_id_utf8,
    uint32_t product_unique_id_utf8_length,
    pid_t* pid,
    bool* device_is_placeholder) {
  MutexLock lock(&_apiLock);
  if (!RefreshCameraList() || device_number >= camera_list_.size()) {
    return -1;
  }

  const auto& info = camera_list_[device_number];
  if (!CopyString(info.name, device_name_utf8, device_name_length) ||
      !CopyString(info.id, device_unique_id_utf8,
                  device_unique_id_utf8_length) ||
      (product_unique_id_utf8 &&
       !CopyString(info.provider, product_unique_id_utf8,
                   product_unique_id_utf8_length))) {
    RTC_LOG(LS_ERROR) << "Camera name buffer is too small";
    return -1;
  }

  if (pid) {
    *pid = 0;
  }
  if (device_is_placeholder) {
    *device_is_placeholder = false;
  }
  return 0;
}

int32_t DeviceInfoSFOS::CreateCapabilityMap(
    const char* device_unique_id_utf8) {
  if (!camera_manager_ || !device_unique_id_utf8) {
    return -1;
  }

  const size_t name_length = std::strlen(device_unique_id_utf8);
  if (name_length >= kVideoCaptureUniqueNameLength) {
    RTC_LOG(LS_ERROR) << "Camera device name is too long";
    return -1;
  }

  std::vector<gecko::camera::CameraCapability> camera_capabilities;
  if (!camera_manager_->queryCapabilities(device_unique_id_utf8,
                                          camera_capabilities)) {
    return -1;
  }

  char* updated_name = static_cast<char*>(
      std::realloc(_lastUsedDeviceName, name_length + 1));
  if (!updated_name) {
    return -1;
  }
  _lastUsedDeviceName = updated_name;
  _lastUsedDeviceNameLength = static_cast<uint32_t>(name_length);
  std::memcpy(_lastUsedDeviceName, device_unique_id_utf8, name_length + 1);

  const uint32_t max_width =
      mozilla::Preferences::GetUint(kMaxWidthPref, 640);
  const uint32_t max_height =
      mozilla::Preferences::GetUint(kMaxHeightPref, 480);

  _captureCapabilities.clear();
  for (const auto& camera_capability : camera_capabilities) {
    if (!camera_capability.width || !camera_capability.height ||
        !camera_capability.fps || camera_capability.width > max_width ||
        camera_capability.height > max_height) {
      continue;
    }

    VideoCaptureCapability capability;
    capability.width = camera_capability.width;
    capability.height = camera_capability.height;
    capability.maxFPS = camera_capability.fps;
    capability.videoType = VideoType::kI420;
    _captureCapabilities.push_back(capability);
  }

  RTC_LOG(LS_INFO) << "Capability map for camera " << device_unique_id_utf8
                   << " has " << _captureCapabilities.size() << " entries";
  return static_cast<int32_t>(_captureCapabilities.size());
}

int32_t DeviceInfoSFOS::DisplayCaptureSettingsDialogBox(
    const char* /* device_unique_id_utf8 */,
    const char* /* dialog_title_utf8 */,
    void* /* parent_window */,
    uint32_t /* position_x */,
    uint32_t /* position_y */) {
  return -1;
}

}  // namespace videocapturemodule
}  // namespace webrtc
