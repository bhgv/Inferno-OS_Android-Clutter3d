LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := test-cogl-hello
LOCAL_SRC_FILES := main.c
LOCAL_LDLIBS    := -llog -landroid -lEGL -lGLESv1_CM
LOCAL_STATIC_LIBRARIES := cogl android_native_app_glue glib-android gobject gmodule gthread glib iconv intl
LOCAL_ARM_MODE := arm
LOCAL_CFLAGS := 				\
	-DG_LOG_DOMAIN=\"TestCoglHello\"	\
	-DCOGL_ENABLE_EXPERIMENTAL_2_0_API	\
	-g3 -O0					\
	$(NULL)

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
$(call import-module,gettext)
$(call import-module,libiconv)
$(call import-module,glib)
$(call import-module,glib-android)
$(call import-module,cogl)
