LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE    := libhidota
LOCAL_SRC_FILES :=  hidserv.c sdp.c androhid.c

LOCAL_CFLAGS:= -DSTORAGEDIR=\"/data/misc/bluetoothd\"
	
LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH)/../../../../external/bluetooth/bluez/btio \
	$(LOCAL_PATH)/../../../../external/bluetooth/bluez/lib \
	$(LOCAL_PATH)/../../../../external/bluetooth/bluez/src \
	$(LOCAL_PATH)/../../../../external/bluetooth/bluez/gdbus \
	$(LOCAL_PATH)/../../../../external/bluetooth/bluez/common \
	$(call include-path-for, glib) \
	$(call include-path-for, dbus)
LOCAL_SHARED_LIBRARIES := \
	libbluetoothd \
	libbluetooth \
	libbtio \
	libcutils \
	libdbus \
	libexpat \
	libglib 
LOCAL_DEFAULT_CPP_EXTENSION := cpp

include $(BUILD_SHARED_LIBRARY)
