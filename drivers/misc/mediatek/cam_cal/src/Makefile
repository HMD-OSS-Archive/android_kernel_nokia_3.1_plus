#
# Copyright (C) 2018 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
                    -I$(MTK_CUSTOM_PATH)/imgsensor/src/$(CONFIG_MTK_PLATFORM)/camera_hw \
                    -I$(MTK_CUSTOM_PATH)/cam_cal/inc
subdir-ccflags-y += -I$(srctree)/drivers/i2c/busses/
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))


obj-y += $(MTK_PLATFORM)/

LEGACY_PLATFORM = mt6572 mt6580 mt6582 mt6735 mt6752 mt6795 mt6755
CURRENT_PLATFORM = $(subst ",,$(CONFIG_MTK_PLATFORM))

ifneq (, $(findstring $(CURRENT_PLATFORM), $(LEGACY_PLATFORM)))
#$(info cam_cal: use legacy folder)
obj-y += legacy/$(subst ",,$(CONFIG_MTK_PLATFORM))/
else
# $(info cam_cal: use common folder)
#obj-y += eeprom_driver.o
#obj-y += cam_cal_list.o
endif
#$(info cam_cal: makefile end)


