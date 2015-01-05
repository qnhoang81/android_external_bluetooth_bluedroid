LOCAL_PATH := $(call my-dir)

ifneq ($(BOARD_HAVE_BLUETOOTH_MRVL),)

include $(CLEAR_VARS)

BDROID_DIR := $(TOP_DIR)external/bluetooth/bluedroid

LOCAL_SRC_FILES := \
		bt_vendor_mrvl.c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/include \
        $(BDROID_DIR)/hci/include \
		$(TOP_DIR)vendor/marvell/generic/libMarvellWireless

LOCAL_SHARED_LIBRARIES := \
        libcutils

LOCAL_SHARED_LIBRARIES += libMarvellWireless

LOCAL_MODULE := libbt-vendor
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_OWNER := marvell
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_CFLAGS += -Werror
#include $(LOCAL_PATH)/vnd_buildcfg.mk

include $(BUILD_SHARED_LIBRARY)

ifeq ($(TARGET_PRODUCT), full_maguro)
    include $(LOCAL_PATH)/conf/samsung/maguro/Android.mk
endif
ifeq ($(TARGET_PRODUCT), full_crespo)
    include $(LOCAL_PATH)/conf/samsung/crespo/Android.mk
endif
ifeq ($(TARGET_PRODUCT), full_crespo4g)
    include $(LOCAL_PATH)/conf/samsung/crespo4g/Android.mk
endif
ifeq ($(TARGET_PRODUCT), full_wingray)
    include $(LOCAL_PATH)/conf/moto/wingray/Android.mk
endif

endif # BOARD_HAVE_BLUETOOTH_MRVL

