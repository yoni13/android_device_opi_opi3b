#
# Copyright (C) 2021-2023 KonstaKANG
#
# SPDX-License-Identifier: Apache-2.0
#

PRODUCT_MAKEFILES := \
    $(LOCAL_DIR)/lineage_opi3b_tv.mk

COMMON_LUNCH_CHOICES := \
    lineage_opi3b_tv-userdebug \
    lineage_opi3b_tv-user \
    lineage_opi3b_tv-eng # PLS dont use, we rely on WITH_DEXPREOPT to boot!
