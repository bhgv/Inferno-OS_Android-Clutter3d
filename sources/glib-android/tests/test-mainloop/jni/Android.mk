LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := test-main-loop
LOCAL_SRC_FILES := main.c
LOCAL_LDLIBS    := -llog -landroid -lEGL -lGLESv1_CM
LOCAL_STATIC_LIBRARIES := android_native_app_glue gobject gmodule gthread glib-android glib iconv
LOCAL_ARM_MODE := arm
LOCAL_CFLAGS := 				\
	-Wall					\
	-DG_LOG_DOMAIN=\"TestMainLoop\"		\
	$(NULL)

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
$(call import-module,glib)
