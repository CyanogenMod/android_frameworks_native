LOCAL_PATH:= $(call my-dir)

#
# Build the software OpenGL ES library
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	egl.cpp                     \
	state.cpp		            \
	texture.cpp		            \
    Tokenizer.cpp               \
    TokenManager.cpp            \
    TextureObjectManager.cpp    \
    BufferObjectManager.cpp     \
	array.cpp.arm		        \
	fp.cpp.arm		            \
	light.cpp.arm		        \
	matrix.cpp.arm		        \
	mipmap.cpp.arm		        \
	primitives.cpp.arm	        \
	vertex.cpp.arm

LOCAL_CFLAGS += -DLOG_TAG=\"libagl\"
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES
LOCAL_CFLAGS += -fvisibility=hidden

LOCAL_SHARED_LIBRARIES := libcutils libhardware libutils liblog libpixelflinger libETC1 libui

LOCAL_SRC_FILES_arm += fixed_asm.S iterators.S
LOCAL_CFLAGS_arm += -fstrict-aliasing

ifndef ARCH_MIPS_REV6
LOCAL_SRC_FILES_mips += arch-mips/fixed_asm.S
endif
LOCAL_CFLAGS_mips += -fstrict-aliasing
# The graphics code can generate division by zero
LOCAL_CFLAGS_mips += -mno-check-zero-division

# we need to access the private Bionic header <bionic_tls.h>
LOCAL_C_INCLUDES += bionic/libc/private

LOCAL_MODULE_RELATIVE_PATH := egl
LOCAL_MODULE:= libGLES_android

include $(BUILD_SHARED_LIBRARY)
