#!/bin/bash

#
# Copyright (C) 2025 Venkata Atchuta Bheemeswara Sarma Darbha
#
# SPDX-License-Identifier: Apache-2.0
#

exit_with_error() {
  echo $@
  exit 1
}



# Check required env vars
if [ -z ${TARGET_PRODUCT} ]; then
  exit_with_error "TARGET_PRODUCT environment variable is not set. Run lunch first."
fi

if [ -z ${ANDROID_PRODUCT_OUT} ]; then
  exit_with_error "ANDROID_PRODUCT_OUT environment variable is not set. Run lunch first."
fi

# Check required images
for PARTITION in "boot" "system" "vendor"; do
  if [ ! -f ${ANDROID_PRODUCT_OUT}/${PARTITION}.img ]; then
    exit_with_error "Partition image not found: ${PARTITION}.img. Run 'make ${PARTITION}image' first."
  fi
done

UBOOT_COMBINED=device/opi/opi3b-kernel/u-boot-rockchip.bin
IDBLOADER_BIN=device/opi/opi3b-kernel/idbloader.img
UBOOT_ITB_BIN=device/opi/opi3b-kernel/u-boot.itb

# Image metadata
VERSION=OrangePiAOSP
DATE=$(date +%Y%m%d)
TARGET=$(echo ${TARGET_PRODUCT} | sed 's/^aosp_//')
IMGNAME=${VERSION}-${DATE}-${TARGET}.img
IMGSIZE=14848MiB

# Partition layout (512-byte sectors)
# Keep first 16MiB free for Rockchip bootloader blobs.
P1_START=32768
P1_SIZE=128M
P2_START=294912
P2_SIZE=2560M
P3_START=5537792
P3_SIZE=256M
P4_START=6062080
P4_SIZE=64M
P5_START=6193152
P5_SIZE=4M
P6_START=6201344

# Avoid overwriting existing image
if [ -f ${ANDROID_PRODUCT_OUT}/${IMGNAME} ]; then
  rm ${ANDROID_PRODUCT_OUT}/${IMGNAME}
  echo "Old image deleted"
fi

echo "Creating image file ${ANDROID_PRODUCT_OUT}/${IMGNAME}..."
sudo fallocate -l ${IMGSIZE} ${ANDROID_PRODUCT_OUT}/${IMGNAME}
sync

# Write Rockchip bootloader in raw sectors.
# Preferred layout:
#   idbloader.img @ sector 64
#   u-boot.itb    @ sector 16384
# Fallback:
#   u-boot-rockchip.bin @ sector 64
if [ -f ${IDBLOADER_BIN} ] && [ -f ${UBOOT_ITB_BIN} ]; then
  echo "Writing idbloader to sector 64..."
  sudo dd if=${IDBLOADER_BIN} of=${ANDROID_PRODUCT_OUT}/${IMGNAME} seek=64 bs=512 conv=notrunc
  echo "Writing u-boot.itb to sector 16384..."
  sudo dd if=${UBOOT_ITB_BIN} of=${ANDROID_PRODUCT_OUT}/${IMGNAME} seek=16384 bs=512 conv=notrunc
elif [ -f ${UBOOT_COMBINED} ]; then
  echo "Writing u-boot-rockchip.bin to sector 64..."
  sudo dd if=${UBOOT_COMBINED} of=${ANDROID_PRODUCT_OUT}/${IMGNAME} seek=64 bs=512 conv=notrunc
else
  exit_with_error "Missing bootloader files. Provide either {idbloader.img + u-boot.itb} or u-boot-rockchip.bin in device/opi/opi3b-kernel"
fi
sync

# Create partitions
echo "Creating partitions..."
(
echo g
echo n
echo 1
echo ${P1_START}
echo +${P1_SIZE}
echo n
echo 2
echo ${P2_START}
echo +${P2_SIZE}
echo n
echo 3
echo ${P3_START}
echo +${P3_SIZE}
echo n
echo 4
echo ${P4_START}
echo +${P4_SIZE}
echo n
echo 5
echo ${P5_START}
echo +${P5_SIZE}
echo n
echo 6
echo ${P6_START}
echo
echo w
) | sudo fdisk ${ANDROID_PRODUCT_OUT}/${IMGNAME}
sync

# Mount loop device
LOOPDEV=$(sudo kpartx -av ${ANDROID_PRODUCT_OUT}/${IMGNAME} | awk 'NR==1{ sub(/p[0-9]$/, "", $3); print $3 }')
if [ -z ${LOOPDEV} ]; then
  exit_with_error "Unable to find loop device!"
fi
echo "Image mounted as /dev/${LOOPDEV}"
sleep 1

# Write images to partitions
echo "Copying boot..."
sudo dd if=${ANDROID_PRODUCT_OUT}/boot.img of=/dev/mapper/${LOOPDEV}p1 bs=1M

echo "Copying system..."
sudo dd if=${ANDROID_PRODUCT_OUT}/system.img of=/dev/mapper/${LOOPDEV}p2 bs=1M
echo "Copying vendor..."
sudo dd if=${ANDROID_PRODUCT_OUT}/vendor.img of=/dev/mapper/${LOOPDEV}p3 bs=1M

# Create metadata + userdata partitions
echo "Creating metadata..."
sudo mkfs.ext4 /dev/mapper/${LOOPDEV}p4 -I 512 -L metadata

echo "Initializing misc..."
sudo dd if=/dev/zero of=/dev/mapper/${LOOPDEV}p5 bs=1M count=4 conv=fsync

echo "Creating userdata..."
sudo mkfs.ext4 /dev/mapper/${LOOPDEV}p6 -I 512 -L userdata
sync

# Unmount loop device
sudo kpartx -d "/dev/${LOOPDEV}"
sudo chown ${USER}:${USER} ${ANDROID_PRODUCT_OUT}/${IMGNAME}

echo "✅ Done! Created ${ANDROID_PRODUCT_OUT}/${IMGNAME} ready to flash with u-boot pre-installed."
exit 0
