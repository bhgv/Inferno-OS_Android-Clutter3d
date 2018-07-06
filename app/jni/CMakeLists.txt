
SET(CMAKE_C_COMPILER "gcc-4.8")
SET(CMAKE_CXX_COMPILER "g++-4.8")

cmake_minimum_required(VERSION 3.4.1)



set(CLUTTER "clutter")


set(CMAKE_VERBOSE_MAKEFILE on)


get_filename_component(INFERNO_OS_PROJ_DIR
                       ${CMAKE_CURRENT_SOURCE_DIR}/../../../.. ABSOLUTE)


# configure import libs
set(infrlib_DIR ${INFERNO_OS_PROJ_DIR}/inferno-os/Android/arm/lib)

add_library(emu STATIC IMPORTED)
set_target_properties(emu PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/emu.a)

add_library(lib9 STATIC IMPORTED)
set_target_properties(lib9 PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/lib9.a)

add_library(libbio STATIC IMPORTED)
set_target_properties(libbio PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libbio.a)

add_library(libdraw STATIC IMPORTED)
set_target_properties(libdraw PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libdraw.a)

add_library(libdynld STATIC IMPORTED)
set_target_properties(libdynld PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libdynld.a)

add_library(libfreetype STATIC IMPORTED)
set_target_properties(libfreetype PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libfreetype.a)

add_library(libinterp STATIC IMPORTED)
set_target_properties(libinterp PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libinterp.a)

add_library(libkeyring STATIC IMPORTED)
set_target_properties(libkeyring PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libkeyring.a)

add_library(libmath STATIC IMPORTED)
set_target_properties(libmath PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libmath.a)

add_library(libmemdraw STATIC IMPORTED)
set_target_properties(libmemdraw PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libmemdraw.a)

add_library(libmemlayer STATIC IMPORTED)
set_target_properties(libmemlayer PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libmemlayer.a)

add_library(libmp STATIC IMPORTED)
set_target_properties(libmp PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libmp.a)

add_library(libprefab STATIC IMPORTED)
set_target_properties(libprefab PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libprefab.a)

add_library(libsec STATIC IMPORTED)
set_target_properties(libsec PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libsec.a)

add_library(libstyx STATIC IMPORTED)
set_target_properties(libstyx PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libstyx.a)

add_library(libtk STATIC IMPORTED)
set_target_properties(libtk PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/libtk.a)

add_library(liblodepng STATIC IMPORTED)
set_target_properties(liblodepng PROPERTIES IMPORTED_LOCATION
    ${infrlib_DIR}/liblodepng.a)


# build native_app_glue as a static lib

include_directories(${ANDROID_NDK}/sources/android/native_app_glue)

add_library(native_app_glue STATIC
	${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)


if( "${CLUTTER}" STREQUAL "clutter")
	include_directories( 
#		${ANDROID_NDK}/sources/android/native_app_glue
		${INFERNO_OS_PROJ_DIR}/ext/include 
		${INFERNO_OS_PROJ_DIR}/ext/include 
		${INFERNO_OS_PROJ_DIR}/ext/include 
		${INFERNO_OS_PROJ_DIR}/ext/include 
		${INFERNO_OS_PROJ_DIR}/ext/include 
		${INFERNO_OS_PROJ_DIR}/ext/include/mx-2.0 
		${INFERNO_OS_PROJ_DIR}/ext/include/json-glib-1.0 
		${INFERNO_OS_PROJ_DIR}/ext/include/atk-1.0 
		${INFERNO_OS_PROJ_DIR}/ext/include/pango-1.0 
		${INFERNO_OS_PROJ_DIR}/ext/include/cairo 
		${INFERNO_OS_PROJ_DIR}/ext/include/glib-2.0 
		${INFERNO_OS_PROJ_DIR}/ext/include/glib-android-1.0 
		${INFERNO_OS_PROJ_DIR}/ext/include/clutter-1.0 
		${INFERNO_OS_PROJ_DIR}/ext/include/cogl 
		${INFERNO_OS_PROJ_DIR}/ext/fake-android-application
	)

	add_library(fake_android_application STATIC
		${INFERNO_OS_PROJ_DIR}/ext/fake-android-application/fake-android-application.c)

	add_library(inferno_os_app SHARED
		clutter_main.c
	)
else()
	add_library(inferno_os_app SHARED
		fb_main.cpp
	)
endif()


add_library(native-audio STATIC native-audio.c)


# now build app's shared lib
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -fPIC -Wall -DCOGL_ENABLE_EXPERIMENTAL_API")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fPIC -fPIE")

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate -fPIC -fPIE -L${infrlib_DIR} -L${INFERNO_OS_PROJ_DIR}/ext/lib"
)
# -flto -Wl,--allow-multiple-definition

# add lib dependencies

if( "${CLUTTER}" STREQUAL "clutter")

target_link_libraries(inferno_os_app 
    native-audio 
    android 
    log 
    OpenSLES 
    m native_app_glue m
    
    emu
    libinterp
    liblodepng
    libbio
    libdraw
    libdynld
    libkeyring
    libmath
    libmemlayer
    libmemdraw
    libsec
    libtk
    libfreetype
    libmp
    lib9
    emu

    native-audio


    mx-2.0 
    gdk_pixbuf-2.0 
    clutter-1.0 
    json-glib-1.0 
    soup-2.4 
    atk-1.0 
    cogl-pango 
    pangocairo-1.0 
    pangoft2-1.0 
    pango-1.0 
    fontconfig 
    xml2 
    cairo-gobject 
    cairo
    pixman-1 
    png15 
    jpeg 
    freetype 
    cogl 
    gio-2.0 
    gobject-2.0 
    gmodule-2.0 
    gthread-2.0 
    glib-android-1.0 
    glib-2.0 
    iconv 
    ffi 
#    gettext

    EGL 
    GLESv1_CM 
    z
    )
#    -Wl,--start-group
#     -Wl,--end-group

else()

target_link_libraries(inferno_os_app 
    native-audio 
    android 
    log 
    OpenSLES 
    m native_app_glue m
    
    emu
    libinterp
    liblodepng
    libbio
    libdraw
    libdynld
    libkeyring
    libmath
    libmemlayer
    libmemdraw
    libsec
    libtk
    libfreetype
    libmp
    lib9
    emu

    native-audio
    )
#    -Wl,--start-group
#     -Wl,--end-group

endif()

