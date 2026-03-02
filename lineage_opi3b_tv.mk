#
# Copyright (C) 2021-2023 KonstaKANG
# Copyright (C) 2025 Venkata Atchuta Bheemeswara Sarma Darbha
#
# SPDX-License-Identifier: Apache-2.0
#

PRODUCT_IS_ATV := true

PRODUCT_AAPT_PREF_CONFIG := tvdpi
PRODUCT_CHARACTERISTICS := tv

# Inherit some common AOSP stuff
$(call inherit-product, device/google/atv/products/atv_base.mk)

# Inherit some common Lineage stuff
$(call inherit-product, vendor/lineage/config/common_full_tv.mk)

# Inherit device configuration
$(call inherit-product, device/opi/opi3b/device.mk)

PRODUCT_COMPRESSED_APEX := false
PRODUCT_DEFAULT_APEX_PAYLOAD_TYPE := ext4

# Bluetooth
PRODUCT_VENDOR_PROPERTIES += \
    bluetooth.device.class_of_device=34,4,36

# Overlays
PRODUCT_PACKAGES += \
    AndroidTvOpiOverlay \
    SettingsProviderTvOpiOverlay \
    WifiOpiOverlay

# $(call inherit-product, vendor/gapps_tv/arm64/arm64-vendor.mk)
# Device identifier. This must come after all inclusions.
PRODUCT_DEVICE := opi3b
PRODUCT_NAME := lineage_opi3b_tv
PRODUCT_BRAND := Orange
PRODUCT_MODEL := Pi 3B
PRODUCT_MANUFACTURER := Orange Pi
