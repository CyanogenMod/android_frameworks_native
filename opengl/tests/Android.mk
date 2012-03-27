dirs := \
	angeles \
	configdump \
	EGLTest \
	fillrate \
	filter \
	finish \
	gl2_basic \
	gl2_copyTexImage \
	gl2_yuvtex \
	gl_basic \
	gl_perf \
	gl_yuvtex \
	gralloc \
	hwc \
	include \
	lib \
	linetex \
	swapinterval \
	textures \
	tritex \

ifneq ($(TARGET_BUILD_PDK), true)
dirs += \
	gl2_cameraeye \
	gl2_java \
	gl2_jni \
	gldual \
	gl_jni \
	gl_perfapp \
	lighting1709 \
	testFramerate \
	testLatency \
	testPauseResume \
	testViewport \

endif

include $(call all-named-subdir-makefiles, $(dirs))
