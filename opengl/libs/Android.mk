LOCAL_PATH:= $(call my-dir)

###############################################################################
# Build META EGL library
#

egl.cfg_config_module :=
# OpenGL drivers config file
ifneq ($(BOARD_EGL_CFG),)

include $(CLEAR_VARS)
LOCAL_MODULE := egl.cfg
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/lib/egl
LOCAL_SRC_FILES := ../../../../$(BOARD_EGL_CFG)
include $(BUILD_PREBUILT)
egl.cfg_config_module := $(LOCAL_MODULE)
endif

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 	       \
	EGL/egl_tls.cpp        \
	EGL/egl_cache.cpp      \
	EGL/egl_display.cpp    \
	EGL/egl_object.cpp     \
	EGL/egl.cpp 	       \
	EGL/eglApi.cpp 	       \
	EGL/getProcAddress.cpp.arm \
	EGL/Loader.cpp 	       \
#

LOCAL_SHARED_LIBRARIES += libbinder libcutils libutils liblog libui
LOCAL_MODULE:= libEGL
LOCAL_LDFLAGS += -Wl,--exclude-libs=ALL
LOCAL_SHARED_LIBRARIES += libdl
# we need to access the private Bionic header <bionic_tls.h>
LOCAL_C_INCLUDES += bionic/libc/private

LOCAL_CFLAGS += -DLOG_TAG=\"libEGL\"
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES
LOCAL_CFLAGS += -fvisibility=hidden

ifeq ($(BOARD_ALLOW_EGL_HIBERNATION),true)
  LOCAL_CFLAGS += -DBOARD_ALLOW_EGL_HIBERNATION
endif
ifneq ($(MAX_EGL_CACHE_ENTRY_SIZE),)
  LOCAL_CFLAGS += -DMAX_EGL_CACHE_ENTRY_SIZE=$(MAX_EGL_CACHE_ENTRY_SIZE)
endif

ifneq ($(MAX_EGL_CACHE_KEY_SIZE),)
  LOCAL_CFLAGS += -DMAX_EGL_CACHE_KEY_SIZE=$(MAX_EGL_CACHE_KEY_SIZE)
endif

ifneq ($(MAX_EGL_CACHE_SIZE),)
  LOCAL_CFLAGS += -DMAX_EGL_CACHE_SIZE=$(MAX_EGL_CACHE_SIZE)
endif

ifneq ($(filter address,$(SANITIZE_TARGET)),)
  LOCAL_CFLAGS_32 += -DEGL_WRAPPER_DIR=\"/$(TARGET_COPY_OUT_DATA)/lib\"
  LOCAL_CFLAGS_64 += -DEGL_WRAPPER_DIR=\"/$(TARGET_COPY_OUT_DATA)/lib64\"
endif

LOCAL_REQUIRED_MODULES := $(egl.cfg_config_module)
egl.cfg_config_module :=

include $(BUILD_SHARED_LIBRARY)

###############################################################################
# Build the wrapper OpenGL ES 1.x library
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	GLES_CM/gl.cpp.arm 	\
#

LOCAL_CLANG := false
LOCAL_SHARED_LIBRARIES += libcutils liblog libEGL
LOCAL_MODULE:= libGLESv1_CM

LOCAL_SHARED_LIBRARIES += libdl
# we need to access the private Bionic header <bionic_tls.h>
LOCAL_C_INCLUDES += bionic/libc/private

LOCAL_CFLAGS += -DLOG_TAG=\"libGLESv1\"
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES
LOCAL_CFLAGS += -fvisibility=hidden

# TODO: This is to work around b/20093774. Remove after root cause is fixed
LOCAL_LDFLAGS_arm += -Wl,--hash-style,both

include $(BUILD_SHARED_LIBRARY)


###############################################################################
# Build the wrapper OpenGL ES 2.x library
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	GLES2/gl2.cpp   \
#

LOCAL_CLANG := false
LOCAL_ARM_MODE := arm
LOCAL_SHARED_LIBRARIES += libcutils libutils liblog libEGL
LOCAL_MODULE:= libGLESv2

LOCAL_SHARED_LIBRARIES += libdl
# we need to access the private Bionic header <bionic_tls.h>
LOCAL_C_INCLUDES += bionic/libc/private

LOCAL_CFLAGS += -DLOG_TAG=\"libGLESv2\"
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES
LOCAL_CFLAGS += -fvisibility=hidden

# TODO: This is to work around b/20093774. Remove after root cause is fixed
LOCAL_LDFLAGS_arm += -Wl,--hash-style,both

include $(BUILD_SHARED_LIBRARY)

###############################################################################
# Build the wrapper OpenGL ES 3.x library (this is just different name for v2)
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	GLES2/gl2.cpp   \
#

LOCAL_CLANG := false
LOCAL_ARM_MODE := arm
LOCAL_SHARED_LIBRARIES += libcutils libutils liblog libEGL
LOCAL_MODULE:= libGLESv3
LOCAL_SHARED_LIBRARIES += libdl
# we need to access the private Bionic header <bionic_tls.h>
LOCAL_C_INCLUDES += bionic/libc/private

LOCAL_CFLAGS += -DLOG_TAG=\"libGLESv3\"
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES
LOCAL_CFLAGS += -fvisibility=hidden

# TODO: This is to work around b/20093774. Remove after root cause is fixed
LOCAL_LDFLAGS_arm += -Wl,--hash-style,both

include $(BUILD_SHARED_LIBRARY)

###############################################################################
# Build the ETC1 host static library
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	ETC1/etc1.cpp 	\
#

LOCAL_MODULE:= libETC1
LOCAL_MODULE_HOST_OS := darwin linux windows

include $(BUILD_HOST_STATIC_LIBRARY)

###############################################################################
# Build the ETC1 device library
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	ETC1/etc1.cpp 	\
#

LOCAL_MODULE:= libETC1

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
