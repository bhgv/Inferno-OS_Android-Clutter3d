.DEFAULT_GOAL := help
PYTHON ?= python3
INFERNO_BUNDLE_RUNTIME ?= 0
INFERNO_ABI ?= armeabi-v7a
INFERNO_OBJTYPE = $(if $(filter arm64-v8a,$(INFERNO_ABI)),arm64,$(if $(filter x86_64,$(INFERNO_ABI)),amd64,arm))
INFERNO_NM ?= nm
INFERNO_OBJCOPY ?= arm-linux-androideabi-objcopy
INFERNO_PRIVATE_LIB = $(CURDIR)/build/inferno-private-lib/$(INFERNO_OBJTYPE)
INFERNO_ROOT ?= $(CURDIR)/inferno-os

.PHONY: inferno-assets
inferno-assets:
	$(PYTHON) tools/stage-inferno.py --source "$(INFERNO_ROOT)" --output "$(CURDIR)/app/assets/inferno" $(if $(filter 1,$(INFERNO_BUNDLE_RUNTIME)),,--disabled)

.PHONY: inferno-libs
inferno-libs:
	$(PYTHON) tools/namespace-inferno-freetype.py --source "$(INFERNO_ROOT)/Android/$(INFERNO_OBJTYPE)/lib" --output "$(INFERNO_PRIVATE_LIB)" --nm "$(INFERNO_NM)" --objcopy "$(INFERNO_OBJCOPY)"

QUIET_ = @
QUIET = $(QUIET_$(V))

help:
	@echo "Usage: make [ ohand-app | ohand-pc | clean ]"

FAKE_C = $(PWD)/fake-android-application/fake-android-application.c
FAKE_H = $(PWD)/fake-android-application/fake-android-application.h
FAKE_OBJS = $(patsubst %.c,%.o,$(FAKE_C))
#FAKE_CFLAGS = `pkg-config mx-2.0 libsoup-2.4 gdk-pixbuf-2.0 --cflags` -D_PC_VERSION=1 -include $(FAKE_H) -I ohand/jni
#FAKE_LIBS = `pkg-config mx-2.0 libsoup-2.4 gdk-pixbuf-2.0 --libs` -lm
FAKE_CFLAGS = `pkg-config libsoup-2.4 gdk-pixbuf-2.0 --cflags` -D_PC_VERSION=1 -include $(FAKE_H) -I app/jni
FAKE_LIBS = `pkg-config libsoup-2.4 gdk-pixbuf-2.0 --libs` -lm

$(FAKE_OBJS): $(FAKE_C)
	@echo "  CC     `basename $@`"
	$(QUIET)$(CC) -Wall -g -c -o $@ $< $(FAKE_CFLAGS)

# OHand

ohand-app:
	cd $(PWD)/ohand && ndk-build
	cd $(PWD)/ohand && ant debug
	adb install -r $(PWD)/ohand/bin/NativeActivity-debug.apk

ohand-app-full:
	android update project -p $(PWD)/ohand --target 1
	cd $(PWD)/ohand && ndk-build
	cd $(PWD)/ohand && ant debug
	adb install -r $(PWD)/ohand/bin/NativeActivity-debug.apk

inferno-full: inferno-assets inferno-libs
#	export NDK_MODULE_PATH=$(PWD)
	android update project -p $(PWD)/app --target 1
	cd $(PWD)/app && ndk-build APP_ABI=$(INFERNO_ABI) INFERNO_LIB="$(INFERNO_PRIVATE_LIB)" INFERNO_ROOT="$(INFERNO_ROOT)" INFERNO_BUNDLE_RUNTIME=$(INFERNO_BUNDLE_RUNTIME)
	cd $(PWD)/app && ant debug
#	adb install -r $(PWD)/ohand/bin/NativeActivity-debug.apk

inferno: inferno-assets inferno-libs
#	export NDK_MODULE_PATH=$(PWD)
	cd $(PWD)/app && ndk-build APP_ABI=$(INFERNO_ABI) INFERNO_LIB="$(INFERNO_PRIVATE_LIB)" INFERNO_ROOT="$(INFERNO_ROOT)" INFERNO_BUNDLE_RUNTIME=$(INFERNO_BUNDLE_RUNTIME)
	cd $(PWD)/app && ant debug
#	adb install -r $(PWD)/ohand/bin/NativeActivity-debug.apk

OHAND_FILES = $(shell cat $(PWD)/ohand/jni/Android.mk | grep LOCAL_SRC_FILES | cut -d = -f 2)
OHAND_SOURCES = $(addprefix $(PWD)/ohand/jni/, $(OHAND_FILES))

ohand-pc: $(OHAND_SOURCES) $(FAKE_C) $(FAKE_H)
	@echo $(OHAND_SOURCES)
	$(CC) -Wall -g -D_PC_VERSION=1 -include $(FAKE_H) $(FAKE_CFLAGS) $(OHAND_SOURCES) $(FAKE_C) -I$(PWD)/ohand/jni -o ohand-pc $(FAKE_LIBS)
