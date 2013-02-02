LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	BatteryService.cpp \
	CorrectedGyroSensor.cpp \
    Fusion.cpp \
    GravitySensor.cpp \
    LinearAccelerationSensor.cpp \
    OrientationSensor.cpp \
    RotationVectorSensor.cpp \
    RotationVectorSensor2.cpp \
    SensorDevice.cpp \
    SensorFusion.cpp \
    SensorInterface.cpp \
    SensorService.cpp \

# Legacy virtual sensors used in combination from accelerometer & magnetometer.
LOCAL_SRC_FILES += \
	legacy/SecondOrderLowPassFilter.cpp \
	legacy/LegacyGravitySensor.cpp \
	legacy/LegacyLinearAccelerationSensor.cpp \
	legacy/LegacyRotationVectorSensor.cpp


LOCAL_CFLAGS:= -DLOG_TAG=\"SensorService\"

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libhardware \
	libhardware_legacy \
	libutils \
	liblog \
	libbinder \
	libui \
	libgui

ifneq ($(BOARD_SYSFS_LIGHT_SENSOR),)
    LOCAL_CFLAGS += -DSYSFS_LIGHT_SENSOR=\"$(BOARD_SYSFS_LIGHT_SENSOR)\"
endif

LOCAL_MODULE:= libsensorservice

include $(BUILD_SHARED_LIBRARY)
