LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_INC := $(PWD)/../install/include
LOCAL_LIB := $(PWD)/../install/lib

$(call import-add-path,$(LOCAL_PATH)/../..)

LOCAL_MODULE    := inferno_os_app
LOCAL_SRC_FILES := clutter_main.c native-audio.c clutter-utils.c
LOCAL_LDLIBS    := -llog -landroid -lEGL -lGLESv1_CM -lz -lOpenSLES \
    -L$(LOCAL_LIB)	\
    -lgdk_pixbuf-2.0	\
    -lclutter-1.0	\
    -ljson-glib-1.0	\
    -lsoup-2.4		\
    -latk-1.0		\
    -lcogl-pango	\
    -lpangocairo-1.0	\
    -lpangoft2-1.0	\
    -lpango-1.0		\
    -lfontconfig	\
    -lxml2		\
    -lcairo-gobject	\
    -lcairo		\
    -lpixman-1		\
    -lpng15		\
    -ljpeg		\
    -lfreetype		\
    -lcogl		\
    -lgio-2.0		\
    -lgobject-2.0	\
    -lgmodule-2.0	\
    -lgthread-2.0	\
    -lglib-android-1.0	\
    -lglib-2.0		\
    -liconv		\
    -lffi		\
    -lintl		\
    \
    -L$(PWD)/../inferno-os/Android/arm/lib \
    -l9			\
    $(PWD)/../inferno-os/Android/arm/lib/libfreetype.a	\
    -lemu		\
    -linterp		\
    -llodepng		\
    -lbio		\
    -ldraw		\
    -ldynld		\
    -lkeyring		\
    -lmath		\
    -lmemlayer		\
    -lmemdraw		\
    -lsec		\
    -ltk		\
    $(PWD)/../inferno-os/Android/arm/lib/libfreetype.a	\
    -lmp		\
    -l9			\
    -lemu		\



#    -L$(PWD)/../inferno-os/Android/arm/lib \
#    -l9			\
#    $(PWD)/../inferno-os/Android/arm/lib/libfreetype.a	\
#    -lemu		\
#    -linterp		\
#    -llodepng		\
#    -lbio		\
#    -ldraw		\
#    -ldynld		\
#    -lkeyring		\
#    -lmath		\
#    -lmemlayer		\
#    -lmemdraw		\
#    -lsec		\
#    -ltk		\
#    $(PWD)/../inferno-os/Android/arm/lib/libfreetype.a	\
#    -lmp		\
#    -l9			\
#    -lemu		\

#    -lfreetype		\

#    $(PWD)/../inferno-os/Android/arm/lib/lib9.a		\

LOCAL_STATIC_LIBRARIES := mx gdk-pixbuf clutter json-glib soup atk cogl-pango pangocairo pangoft2 pango fontconfig libxml2 cairo-gobject cairo pixman png jpeg freetype cogl gio gobject gmodule gthread glib-android glib iconv libffi gettext
LOCAL_ARM_MODE := arm
LOCAL_CFLAGS := 				\
	-Wall					\
	-DCOGL_ENABLE_EXPERIMENTAL_API		\
	-I$(LOCAL_INC)				\
	-I$(LOCAL_INC)/mx-2.0			\
	-I$(LOCAL_INC)/json-glib-1.0		\
	-I$(LOCAL_INC)/atk-1.0			\
	-I$(LOCAL_INC)/pango-1.0		\
	-I$(LOCAL_INC)/cairo			\
	-I$(LOCAL_INC)/glib-2.0			\
	-I$(LOCAL_INC)/glib-android-1.0		\
	-I$(LOCAL_INC)/clutter-1.0		\
	-I$(LOCAL_INC)/cogl			\
	-I$(LOCAL_INC)/../lib/glib-2.0/include	\
	-I$(LOCAL_PATH)/fake-android-application	\
	-I$(ANDROID_NDK_DIR)/sources/android/native_app_glue	\
	$(NULL)

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
#$(call import-module,install)
