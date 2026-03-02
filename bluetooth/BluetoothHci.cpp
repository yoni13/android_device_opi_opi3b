/*
 * Copyright (C) 2022 The Android Open Source Project
 * Copyright (C) 2024 KonstaKANG
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.bluetooth.service.opi"

#include "BluetoothHci.h"

#include "log/log.h"

using namespace ::android::hardware::bluetooth::hci;
using namespace ::android::hardware::bluetooth::async;
using aidl::android::hardware::bluetooth::Status;

namespace aidl::android::hardware::bluetooth::impl {

void OnDeath(void* cookie);

class BluetoothDeathRecipient {
 public:
  BluetoothDeathRecipient(BluetoothHci* hci) : mHci(hci) {}

  ~BluetoothDeathRecipient() {
    if (clientDeathRecipient_ != nullptr) {
      AIBinder_DeathRecipient_delete(clientDeathRecipient_);
    }
  }

  void LinkToDeath(const std::shared_ptr<IBluetoothHciCallbacks>& cb) {
    mCb = cb;
    if (clientDeathRecipient_ == nullptr) {
      clientDeathRecipient_ = AIBinder_DeathRecipient_new(OnDeath);
    }
    auto linkToDeathReturnStatus = AIBinder_linkToDeath(
        mCb->asBinder().get(), clientDeathRecipient_, this /* cookie */);
    LOG_ALWAYS_FATAL_IF(linkToDeathReturnStatus != STATUS_OK,
                        "Unable to link to death recipient");
  }

  void UnlinkToDeath(const std::shared_ptr<IBluetoothHciCallbacks>& cb) {
    LOG_ALWAYS_FATAL_IF(cb != mCb, "Unable to unlink mismatched pointers");
    if (mCb != nullptr && clientDeathRecipient_ != nullptr) {
      AIBinder_unlinkToDeath(mCb->asBinder().get(), clientDeathRecipient_, this /* cookie */);
    }
  }

  void serviceDied() {
    if (mCb != nullptr && !AIBinder_isAlive(mCb->asBinder().get())) {
      ALOGE("Bluetooth remote service has died");
    } else {
      ALOGE("BluetoothDeathRecipient::serviceDied called but service not dead");
      return;
    }
    {
      std::lock_guard<std::mutex> guard(mHasDiedMutex);
      has_died_ = true;
    }
    mHci->close();
  }
  BluetoothHci* mHci;
  std::shared_ptr<IBluetoothHciCallbacks> mCb;
  AIBinder_DeathRecipient* clientDeathRecipient_;
  bool getHasDied() {
    std::lock_guard<std::mutex> guard(mHasDiedMutex);
    return has_died_;
  }

 private:
  std::mutex mHasDiedMutex;
  bool has_died_{false};
};

void OnDeath(void* cookie) {
  auto* death_recipient = static_cast<BluetoothDeathRecipient*>(cookie);
  death_recipient->serviceDied();
}

BluetoothHci::BluetoothHci() {
  mDeathRecipient = std::make_shared<BluetoothDeathRecipient>(this);
}

ndk::ScopedAStatus BluetoothHci::initialize(
    const std::shared_ptr<IBluetoothHciCallbacks>& cb) {
  ALOGI(__func__);

  if (cb == nullptr) {
    ALOGE("cb == nullptr! -> Unable to call initializationComplete(ERR)");
    return ndk::ScopedAStatus::fromServiceSpecificError(STATUS_BAD_VALUE);
  }

  HalState old_state = HalState::READY;
  {
    std::lock_guard<std::mutex> guard(mStateMutex);
    if (mState != HalState::READY) {
      old_state = mState;
    } else {
      mState = HalState::INITIALIZING;
    }
  }

  if (old_state != HalState::READY) {
    ALOGE("initialize: Unexpected State %d", static_cast<int>(old_state));
    close();
    cb->initializationComplete(Status::ALREADY_INITIALIZED);
    return ndk::ScopedAStatus::ok();
  }

  mCb = cb;
  mDeathRecipient->LinkToDeath(mCb);

  management_.reset(new NetBluetoothMgmt);
  mFd = management_->openHci();
  if (mFd < 0) {
    management_.reset();

    ALOGE("Unable to open Linux interface (hci0). Calling initializationComplete(UNABLE_TO_OPEN_INTERFACE)");
    mState = HalState::READY;
    cb->initializationComplete(Status::UNABLE_TO_OPEN_INTERFACE);
    return ndk::ScopedAStatus::ok();
  }

  mH4 = std::make_shared<H4Protocol>(
      mFd,
      [](const std::vector<uint8_t>& /* raw_command */) {
        LOG_ALWAYS_FATAL("Unexpected command!");
      },
      [cb](const std::vector<uint8_t>& raw_acl) {
        cb->aclDataReceived(raw_acl);
      },
      [cb](const std::vector<uint8_t>& raw_sco) {
        cb->scoDataReceived(raw_sco);
      },
      [cb](const std::vector<uint8_t>& raw_event) {
        cb->hciEventReceived(raw_event);
      },
      [cb](const std::vector<uint8_t>& raw_iso) {
        cb->isoDataReceived(raw_iso);
      },
      [this]() {
        ALOGI("HCI socket device disconnected");
        mFdWatcher.StopWatchingFileDescriptors();
      });
  mFdWatcher.WatchFdForNonBlockingReads(mFd,
                                        [this](int) { mH4->OnDataReady(); });

  {
    std::lock_guard<std::mutex> guard(mStateMutex);
    mState = HalState::ONE_CLIENT;
  }

  ALOGI("initialization complete. Sending Status::SUCCESS to client.");
  auto status = cb->initializationComplete(Status::SUCCESS);
  if (!status.isOk()) {
    ALOGE("Error sending initializationComplete callback!");
    close();
    return ndk::ScopedAStatus::fromServiceSpecificError(STATUS_FAILED_TRANSACTION);
  }

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BluetoothHci::close() {
  ALOGI(__func__);
  {
    std::lock_guard<std::mutex> guard(mStateMutex);
    if (mState != HalState::ONE_CLIENT && mState != HalState::INITIALIZING) {
      ALOGI("Already closed or not initialized");
      return ndk::ScopedAStatus::ok();
    }
    mState = HalState::CLOSING;
  }

  mFdWatcher.StopWatchingFileDescriptors();

  if (management_) {
    management_->closeHci();
  }

  {
    std::lock_guard<std::mutex> guard(mStateMutex);
    mState = HalState::READY;
    mH4 = nullptr;
  }
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BluetoothHci::sendHciCommand(
    const std::vector<uint8_t>& packet) {
  // Log the first few bytes of the command to identify it (OpCode)
  if (packet.size() >= 2) {
    ALOGD("HCI CMD: %02x %02x", packet[0], packet[1]);
  }
  return send(PacketType::COMMAND, packet);
}

ndk::ScopedAStatus BluetoothHci::sendAclData(
    const std::vector<uint8_t>& packet) {
  ALOGV("HCI ACL: size %zu", packet.size());
  return send(PacketType::ACL_DATA, packet);
}

ndk::ScopedAStatus BluetoothHci::sendScoData(
    const std::vector<uint8_t>& packet) {
  return send(PacketType::SCO_DATA, packet);
}

ndk::ScopedAStatus BluetoothHci::sendIsoData(
    const std::vector<uint8_t>& packet) {
  return send(PacketType::ISO_DATA, packet);
}

ndk::ScopedAStatus BluetoothHci::send(PacketType type,
    const std::vector<uint8_t>& v) {
  if (v.empty()) {
    ALOGE("Packet is empty, no data was found to be sent");
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  std::lock_guard<std::mutex> guard(mStateMutex);
  if (mH4 == nullptr) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
  }

  mH4->Send(type, v);
  return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::bluetooth::impl
