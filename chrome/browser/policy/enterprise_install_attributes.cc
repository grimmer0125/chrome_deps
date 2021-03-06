// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/enterprise_install_attributes.h"

#include <utility>

#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/policy/proto/install_attributes.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace policy {

namespace {
// Constants for the possible device modes that can be stored in the lockbox.
const char kConsumerDeviceMode[] = "consumer";
const char kEnterpiseDeviceMode[] = "enterprise";
const char kKioskDeviceMode[] = "kiosk";
const char kUnknownDeviceMode[] = "unknown";

// Field names in the lockbox.
const char kAttrEnterpriseDeviceId[] = "enterprise.device_id";
const char kAttrEnterpriseDomain[] = "enterprise.domain";
const char kAttrEnterpriseMode[] = "enterprise.mode";
const char kAttrEnterpriseOwned[] = "enterprise.owned";
const char kAttrEnterpriseUser[] = "enterprise.user";

// Translates DeviceMode constants to strings used in the lockbox.
std::string GetDeviceModeString(DeviceMode mode) {
  switch (mode) {
    case DEVICE_MODE_CONSUMER:
      return kConsumerDeviceMode;
    case DEVICE_MODE_ENTERPRISE:
      return kEnterpiseDeviceMode;
    case DEVICE_MODE_KIOSK:
      return kKioskDeviceMode;
    case DEVICE_MODE_PENDING:
    case DEVICE_MODE_NOT_SET:
      break;
  }
  NOTREACHED() << "Invalid device mode: " << mode;
  return kUnknownDeviceMode;
}

// Translates strings used in the lockbox to DeviceMode values.
DeviceMode GetDeviceModeFromString(
    const std::string& mode) {
  if (mode == kConsumerDeviceMode)
    return DEVICE_MODE_CONSUMER;
  else if (mode == kEnterpiseDeviceMode)
    return DEVICE_MODE_ENTERPRISE;
  else if (mode == kKioskDeviceMode)
    return DEVICE_MODE_KIOSK;
  NOTREACHED() << "Unknown device mode string: " << mode;
  return DEVICE_MODE_NOT_SET;
}

bool ReadMapKey(const std::map<std::string, std::string>& map,
                const std::string& key,
                std::string* value) {
  std::map<std::string, std::string>::const_iterator entry = map.find(key);
  if (entry == map.end())
    return false;

  *value = entry->second;
  return true;
}

}  // namespace

// Cache file name.
const FilePath::CharType EnterpriseInstallAttributes::kCacheFilePath[] =
    FILE_PATH_LITERAL("/var/run/lockbox/install_attributes.pb");

EnterpriseInstallAttributes::EnterpriseInstallAttributes(
    chromeos::CryptohomeLibrary* cryptohome)
    : cryptohome_(cryptohome),
      device_locked_(false),
      registration_mode_(DEVICE_MODE_PENDING) {}

void EnterpriseInstallAttributes::ReadCacheFile(const FilePath& cache_file) {
  if (device_locked_ || !file_util::PathExists(cache_file))
    return;

  device_locked_ = true;

  char buf[16384];
  int len = file_util::ReadFile(cache_file, buf, sizeof(buf));
  if (len == -1 || len >= static_cast<int>(sizeof(buf))) {
    PLOG(ERROR) << "Failed to read " << cache_file.value();
    return;
  }

  cryptohome::SerializedInstallAttributes install_attrs_proto;
  if (!install_attrs_proto.ParseFromArray(buf, len)) {
    LOG(ERROR) << "Failed to parse install attributes cache";
    return;
  }

  google::protobuf::RepeatedPtrField<
      const cryptohome::SerializedInstallAttributes::Attribute>::iterator entry;
  std::map<std::string, std::string> attr_map;
  for (entry = install_attrs_proto.attributes().begin();
       entry != install_attrs_proto.attributes().end();
       ++entry) {
    // The protobuf values unfortunately contain terminating null characters, so
    // we have to sanitize the value here.
    attr_map.insert(std::make_pair(entry->name(),
                                   std::string(entry->value().c_str())));
  }

  DecodeInstallAttributes(attr_map);
}

void EnterpriseInstallAttributes::ReadImmutableAttributes() {
  if (device_locked_)
    return;

  if (cryptohome_ && cryptohome_->InstallAttributesIsReady()) {
    registration_mode_ = DEVICE_MODE_NOT_SET;
    if (!cryptohome_->InstallAttributesIsInvalid() &&
        !cryptohome_->InstallAttributesIsFirstInstall()) {
      device_locked_ = true;

      static const char* kEnterpriseAttributes[] = {
        kAttrEnterpriseDeviceId,
        kAttrEnterpriseDomain,
        kAttrEnterpriseMode,
        kAttrEnterpriseOwned,
        kAttrEnterpriseUser,
      };
      std::map<std::string, std::string> attr_map;
      for (size_t i = 0; i < arraysize(kEnterpriseAttributes); ++i) {
        std::string value;
        if (cryptohome_->InstallAttributesGet(kEnterpriseAttributes[i], &value))
          attr_map[kEnterpriseAttributes[i]] = value;
      }

      DecodeInstallAttributes(attr_map);
    }
  }
}

EnterpriseInstallAttributes::LockResult EnterpriseInstallAttributes::LockDevice(
    const std::string& user,
    DeviceMode device_mode,
    const std::string& device_id) {
  CHECK_NE(device_mode, DEVICE_MODE_PENDING);
  CHECK_NE(device_mode, DEVICE_MODE_NOT_SET);

  std::string domain = gaia::ExtractDomainName(user);

  // Check for existing lock first.
  if (device_locked_) {
    return !registration_domain_.empty() && domain == registration_domain_ ?
        LOCK_SUCCESS : LOCK_WRONG_USER;
  }

  if (!cryptohome_ || !cryptohome_->InstallAttributesIsReady())
    return LOCK_NOT_READY;

  // Clearing the TPM password seems to be always a good deal.
  if (cryptohome_->TpmIsEnabled() &&
      !cryptohome_->TpmIsBeingOwned() &&
      cryptohome_->TpmIsOwned()) {
    cryptohome_->TpmClearStoredPassword();
  }

  // Make sure we really have a working InstallAttrs.
  if (cryptohome_->InstallAttributesIsInvalid()) {
    LOG(ERROR) << "Install attributes invalid.";
    return LOCK_BACKEND_ERROR;
  }

  if (!cryptohome_->InstallAttributesIsFirstInstall())
    return LOCK_WRONG_USER;

  std::string mode = GetDeviceModeString(device_mode);

  // Set values in the InstallAttrs and lock it.
  if (!cryptohome_->InstallAttributesSet(kAttrEnterpriseOwned, "true") ||
      !cryptohome_->InstallAttributesSet(kAttrEnterpriseUser, user) ||
      !cryptohome_->InstallAttributesSet(kAttrEnterpriseDomain, domain) ||
      !cryptohome_->InstallAttributesSet(kAttrEnterpriseMode, mode) ||
      !cryptohome_->InstallAttributesSet(kAttrEnterpriseDeviceId, device_id)) {
    LOG(ERROR) << "Failed writing attributes";
    return LOCK_BACKEND_ERROR;
  }

  if (!cryptohome_->InstallAttributesFinalize() ||
      cryptohome_->InstallAttributesIsFirstInstall()) {
    LOG(ERROR) << "Failed locking.";
    return LOCK_BACKEND_ERROR;
  }

  ReadImmutableAttributes();
  if (GetRegistrationUser() != user) {
    LOG(ERROR) << "Locked data doesn't match";
    return LOCK_BACKEND_ERROR;
  }

  return LOCK_SUCCESS;
}

bool EnterpriseInstallAttributes::IsEnterpriseDevice() {
  return device_locked_ && !registration_user_.empty();
}

std::string EnterpriseInstallAttributes::GetRegistrationUser() {
  if (!device_locked_)
    return std::string();

  return registration_user_;
}

std::string EnterpriseInstallAttributes::GetDomain() {
  if (!IsEnterpriseDevice())
    return std::string();

  return registration_domain_;
}

std::string EnterpriseInstallAttributes::GetDeviceId() {
  if (!IsEnterpriseDevice())
    return std::string();

  return registration_device_id_;
}

DeviceMode EnterpriseInstallAttributes::GetMode() {
  return registration_mode_;
}

void EnterpriseInstallAttributes::DecodeInstallAttributes(
    const std::map<std::string, std::string>& attr_map) {
  std::string enterprise_owned;
  std::string enterprise_user;
  if (ReadMapKey(attr_map, kAttrEnterpriseOwned, &enterprise_owned) &&
      ReadMapKey(attr_map, kAttrEnterpriseUser, &enterprise_user) &&
      enterprise_owned == "true" &&
      !enterprise_user.empty()) {
    registration_user_ = gaia::CanonicalizeEmail(enterprise_user);

    // Initialize the mode to the legacy enterprise mode here and update
    // below if more information is present.
    registration_mode_ = DEVICE_MODE_ENTERPRISE;

    // If we could extract basic setting we should try to extract the
    // extended ones too. We try to set these to defaults as good as
    // as possible if present, which could happen for device enrolled in
    // pre 19 revisions of the code, before these new attributes were added.
    if (ReadMapKey(attr_map, kAttrEnterpriseDomain, &registration_domain_))
      registration_domain_ = gaia::CanonicalizeDomain(registration_domain_);
    else
      registration_domain_ = gaia::ExtractDomainName(registration_user_);

    ReadMapKey(attr_map, kAttrEnterpriseDeviceId, &registration_device_id_);

    std::string mode;
    if (ReadMapKey(attr_map, kAttrEnterpriseMode, &mode))
      registration_mode_ = GetDeviceModeFromString(mode);
  } else if (enterprise_user.empty() && enterprise_owned != "true") {
    // |registration_user_| is empty on consumer devices.
    registration_mode_ = DEVICE_MODE_CONSUMER;
  }
}

}  // namespace policy
