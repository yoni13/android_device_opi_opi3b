#
# Copyright (C) 2021-2022 KonstaKANG
#
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/opi/opi3b
KERNEL_PATH := device/opi/opi3b-kernel
SIZE := $(100*1024)
BOOT:= $(PRODUCT_OUT)/boot.img

OPI_BOOT_OUT := $(PRODUCT_OUT)/opiboot

$(OPI_BOOT_OUT): $(INSTALLED_RAMDISK_TARGET) \
	$(KERNEL_PATH)/Image \
	$(KERNEL_PATH)/rk3566-orangepi-3b-v1.1.dtb \
	$(KERNEL_PATH)/boot.cmd
	mkdir -p $(OPI_BOOT_OUT)
	cp $(KERNEL_PATH)/Image $(OPI_BOOT_OUT)
	cp $(KERNEL_PATH)/rk3566-orangepi-3b-v1.1.dtb $(OPI_BOOT_OUT)
	prebuilts/tools-lineage/linux-x86/bin/mkimage -A arm64 -O linux -T script -C none \
		-n "Boot Script" \
		-d $(KERNEL_PATH)/boot.cmd \
		$(OPI_BOOT_OUT)/boot.scr
	prebuilts/tools-lineage/linux-x86/bin/mkimage -A arm64 -O linux -T ramdisk -C none \
		-n "Android Ramdisk" \
		-d $(PRODUCT_OUT)/ramdisk.img \
		$(OPI_BOOT_OUT)/uRamdisk

$(INSTALLED_BOOTIMAGE_TARGET): $(OPI_BOOT_OUT) $(HOST_OUT_EXECUTABLES)/newfs_msdos $(HOST_OUT_EXECUTABLES)/mcopy
	$(call pretty,"Target boot image: $@")

	$(HOST_OUT_EXECUTABLES)/newfs_msdos -F 16 -L boot -C 128m -S 512 -h 255 -u 63 -s 262144 -o 0 $@
	$(HOST_OUT_EXECUTABLES)/mcopy -s -i $@ $(OPI_BOOT_OUT)/* ::
