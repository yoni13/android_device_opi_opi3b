#
# Copyright (C) 2021-2022 KonstaKANG
#
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/opi/opi3b

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := cortex-a55

# # Bluetooth
BOARD_HAVE_BLUETOOTH := true

# Bootloader
TARGET_NO_BOOTLOADER := true

# # Camera
# BOARD_LIBCAMERA_IPAS := rpi/pisp
# BOARD_LIBCAMERA_PIPELINES := rpi/pisp
# BOARD_LIBCAMERA_USES_MESON_BUILD := true

# Display
TARGET_SCREEN_DENSITY := 213
# Metadata
BOARD_USES_METADATA_PARTITION := true
BOARD_METADATAIMAGE_PARTITION_SIZE := 16777216  # 16MB is standard
BOARD_METADATAIMAGE_FILE_SYSTEM_TYPE := ext4

# !------- TODO: need to change to panfrost
# Graphics
BOARD_MESA3D_BUILD_LIBGBM := true
BOARD_MESA3D_USES_MESON_BUILD := true
BOARD_MESA3D_GALLIUM_DRIVERS := panfrost,kmsro
BOARD_MESA3D_VULKAN_DRIVERS := panfrost

# Kernel
BOARD_CUSTOM_BOOTIMG := true
BOARD_CUSTOM_BOOTIMG_MK := $(DEVICE_PATH)/mkbootimg.mk
TARGET_NO_KERNEL_OVERRIDE := true

# Use prebuilt kernel image packaged in device tree
BOARD_KERNEL_IMAGE_NAME := Image
TARGET_PREBUILT_KERNEL := $(DEVICE_PATH)-kernel/$(BOARD_KERNEL_IMAGE_NAME)
BOARD_KERNEL_CMDLINE := console=ttyS2,1500000 no_console_suspend root=/dev/ram0 rootwait androidboot.hardware=opi3b video=HDMI-A-1:1920x1080@60
BOARD_KERNEL_CMDLINE += androidboot.console=ttyFIQ0

# Manifest
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE := $(DEVICE_PATH)/framework_compatibility_matrix.xml
DEVICE_MANIFEST_FILE := $(DEVICE_PATH)/manifest.xml
PRODUCT_MANIFEST_FILES := $(DEVICE_PATH)/product_manifest.xml
#DEVICE_MATRIX_FILE := $(DEVICE_PATH)/compatibility_matrix.xml

# Partition sizes
BOARD_FLASH_BLOCK_SIZE := 4096
BOARD_BOOTIMAGE_PARTITION_SIZE := 134217728 # 128M
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 2684354560 # 2560M
BOARD_USERDATAIMAGE_PARTITION_SIZE := 4294967296 # 4G
BOARD_VENDORIMAGE_PARTITION_SIZE := 268435456 # 256M
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4
TARGET_USERIMAGES_SPARSE_EXT_DISABLED := true
TARGET_USERIMAGES_USE_EXT4 := true


# !-----TODO : need to check dependencies
# Platform
TARGET_BOARD_PLATFORM := Rockchip

TARGET_BOOTLOADER_BOARD_NAME := Rockchip

# Properties
TARGET_VENDOR_PROP += $(DEVICE_PATH)/vendor.prop

# Recovery
TARGET_NO_RECOVERY := true

# SELinux
BOARD_SEPOLICY_DIRS += device/opi/opi3b/sepolicy
BOARD_KERNEL_CMDLINE += androidboot.selinux=permissive
SELINUX_IGNORE_NEVERALLOWS := true

# Treble
TARGET_COPY_OUT_VENDOR := vendor

# Virtualization
BOARD_KERNEL_CMDLINE += androidboot.hypervisor.vm.supported=1


# Wifi
BOARD_WLAN_DEVICE := unisoc
BOARD_HOSTAPD_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
WPA_SUPPLICANT_VERSION := VER_0_8_X
BOARD_WIFI_VENDOR_HAL := libwifi-hal-uni

# Disable features that crash the Unisoc VIF allocation
WIFI_HIDL_FEATURE_DISABLE_AP_MAC_RANDOMIZATION := true
WIFI_HIDL_FEATURE_AWARE := false
WIFI_HIDL_FEATURE_DUAL_INTERFACE := false

# Unisoc specific
WIFI_DRIVER_MODULE_NAME := "sprdwl_ng"
WIFI_DRIVER_MODULE_PATH := "/vendor/lib/modules/sprdwl_ng.ko"
WIFI_DRIVER_FW_PATH_PARAM := "/sys/module/sprdwl_ng/parameters/firmware_path"

# Temporary for Wi-Fi bring-up: allow shipping .ko via PRODUCT_COPY_FILES.
BUILD_BROKEN_ELF_PREBUILT_PRODUCT_COPY_FILES := true
