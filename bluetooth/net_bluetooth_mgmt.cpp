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

#include "net_bluetooth_mgmt.h"

#include <fcntl.h>
#include <log/log.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// Definitions imported from <linux/net/bluetooth/bluetooth.h>
#define BTPROTO_HCI 1

// Definitions imported from <linux/net/bluetooth/hci_sock.h>
#define HCI_CHANNEL_USER 1
#define HCI_CHANNEL_CONTROL 3
#define HCI_DEV_NONE 0xffff

struct sockaddr_hci {
  sa_family_t hci_family;
  unsigned short hci_dev;
  unsigned short hci_channel;
};

// Definitions imported from <linux/net/bluetooth/mgmt.h>
#define MGMT_OP_READ_INDEX_LIST 0x0003
#define MGMT_EV_INDEX_ADDED 0x0004
#define MGMT_EV_CMD_COMPLETE 0x0001
#define MGMT_PKT_SIZE_MAX 1024
#define MGMT_INDEX_NONE 0xFFFF
#define WRITE_NO_INTR(fn) \
  do {                  \
  } while ((fn) == -1 && errno == EINTR)

struct mgmt_pkt {
  uint16_t opcode;
  uint16_t index;
  uint16_t len;
  uint8_t data[MGMT_PKT_SIZE_MAX];
} __attribute__((packed));

struct mgmt_ev_read_index_list {
  uint16_t opcode;
  uint8_t status;
  uint16_t num_controllers;
  uint16_t index[];
} __attribute__((packed));

namespace aidl::android::hardware::bluetooth::impl {

// Wait indefinitely for the selected HCI interface to be enabled in the
// bluetooth driver.
int NetBluetoothMgmt::waitHciDev(int hci_interface) {
  ALOGI("waiting for hci interface %d", hci_interface);

  int ret = -1;
  struct mgmt_pkt cmd;
  struct pollfd pollfd;
  struct sockaddr_hci hci_addr = {
      .hci_family = AF_BLUETOOTH,
      .hci_dev = HCI_DEV_NONE,
      .hci_channel = HCI_CHANNEL_CONTROL,
  };

  // Open and bind a socket to the bluetooth control interface in the
  // kernel driver, used to send control commands and receive control
  // events.
  int fd = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
  if (fd < 0) {
    ALOGE("unable to open raw bluetooth socket: %s", strerror(errno));
    return -1;
  }

  if (bind(fd, (struct sockaddr*)&hci_addr, sizeof(hci_addr)) < 0) {
    ALOGE("unable to bind bluetooth control channel: %s", strerror(errno));
    ::close(fd);
    return -1;
  }

  // Send the control command [Read Index List].
  cmd = {
      .opcode = MGMT_OP_READ_INDEX_LIST,
      .index = MGMT_INDEX_NONE,
      .len = 0,
  };

  if (write(fd, &cmd, 6) != 6) {
    ALOGE("error writing mgmt command: %s", strerror(errno));
    ::close(fd);
    return -1;
  }

  // Poll the control socket waiting for the command response,
  // and subsequent [Index Added] events. The loops continue
  // for at most 30 seconds until the selected hci interface is detected.
  pollfd = {.fd = fd, .events = POLLIN};

  int timeout_ms = 30000; // 30s timeout
  int wait_ms = 1000;    // check every 1s

  while (timeout_ms > 0) {
    ret = poll(&pollfd, 1, wait_ms);

    // Poll interrupted, try again.
    if (ret == -1 && (errno == EINTR || errno == EAGAIN)) {
      continue;
    }

    // Poll failure, abandon.
    if (ret == -1) {
      ALOGE("poll error: %s", strerror(errno));
      break;
    }

    // Poll timeout, check elapsed time.
    if (ret == 0) {
        timeout_ms -= wait_ms;
        continue;
    }

    // Spurious wakeup, try again.
    if ((pollfd.revents & POLLIN) == 0) {
      continue;
    }

    // Read the next control event.
    struct mgmt_pkt ev {};
    ret = read(fd, &ev, sizeof(ev));
    if (ret < 0) {
      ALOGE("error reading mgmt event: %s", strerror(errno));
      break;
    }

    // Received [Read Index List] command response.
    if (ev.opcode == MGMT_EV_CMD_COMPLETE) {
      struct mgmt_ev_read_index_list* data =
          (struct mgmt_ev_read_index_list*)ev.data;

      // Prefer the exact hci_interface
      for (int i = 0; i < data->num_controllers; i++) {
        if (data->index[i] == hci_interface) {
          ALOGI("hci interface %d found", data->index[i]);
          ret = data->index[i];
          goto end;
        }
      }

      // Accept a larger one if we can't find the exact one
      for (int i = 0; i < data->num_controllers; i++) {
        if (data->index[i] >= hci_interface) {
          ALOGI("hci interface %d found", data->index[i]);
          ret = data->index[i];
          goto end;
        }
      }
    }

    // Received [Index Added] event.
    if (ev.opcode == MGMT_EV_INDEX_ADDED && ev.index == hci_interface) {
      ALOGI("hci interface %d added", hci_interface);
      ret = hci_interface;
      goto end;
    }
  }

  ALOGE("timeout waiting for hci interface %d", hci_interface);
  ret = -1;

end:
  ::close(fd);
  return ret;
}

int NetBluetoothMgmt::rfKill(int block) {
  char rfkill_type[64];
  char rfkill_state[64];
  char type[16];
  int fd, size, i;
  char on = (block) ? '0' : '1';

  for (i = 0; i < 16; i++) {
    snprintf(rfkill_type, sizeof(rfkill_type), "/sys/class/rfkill/rfkill%d/type", i);
    fd = open(rfkill_type, O_RDONLY);
    if (fd < 0) continue;

    memset(type, 0, sizeof(type));
    size = read(fd, &type, sizeof(type) - 1);
    ::close(fd);

    if (size > 0) {
      ALOGI("Found rfkill%d type: %s", i, type);
      if (strstr(type, "bluetooth")) {
        snprintf(rfkill_state, sizeof(rfkill_state), "/sys/class/rfkill/rfkill%d/state", i);
        fd = open(rfkill_state, O_WRONLY);
        if (fd >= 0) {
          ALOGI("Unblocking bluetooth rfkill%d (%c)", i, on);
          WRITE_NO_INTR(write(fd, &on, 1));
          ::close(fd);
        } else {
          ALOGW("Skipping rfkill%d unblock: %s", i, strerror(errno));
        }
      }
    }
  }
  return 0;
}

int NetBluetoothMgmt::openHci(int hci_interface) {
  ALOGI("opening hci interface %d", hci_interface);

  // Unblock Bluetooth.
  rfKill(0); 

  // Wait for the HCI interface to complete initialization or to come online.
  int hci = waitHciDev(hci_interface);
  if (hci < 0) {
    ALOGE("hci interface %d not found", hci_interface);
    return -1;
  }

  // Open the raw HCI socket.
  int fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
  if (fd < 0) {
    ALOGE("unable to open raw bluetooth socket: %s", strerror(errno));
    return -1;
  }

  struct sockaddr_hci hci_addr = {
      .hci_family = AF_BLUETOOTH,
      .hci_dev = static_cast<uint16_t>(hci),
      .hci_channel = HCI_CHANNEL_USER,
  };

  // Bind the socket to the selected interface.
  if (bind(fd, (struct sockaddr*)&hci_addr, sizeof(hci_addr)) < 0) {
    ALOGE("unable to bind bluetooth user channel: %s", strerror(errno));
    ::close(fd);
    return -1;
  }

  ALOGI("hci interface %d ready", hci);
  bt_fd_ = fd;

  // Settle delay and quick drain of the buffer to clear early UART noise.
  usleep(100000);
  int flags = fcntl(bt_fd_, F_GETFL, 0);
  fcntl(bt_fd_, F_SETFL, flags | O_NONBLOCK);
  unsigned char junk[1024];
  while (read(bt_fd_, junk, sizeof(junk)) > 0) ;
  fcntl(bt_fd_, F_SETFL, flags);

  return fd;
}

void NetBluetoothMgmt::closeHci() {
  if (bt_fd_ != -1) {
    ::close(bt_fd_);
    bt_fd_ = -1;
  }
}

}  // namespace aidl::android::hardware::bluetooth::impl
