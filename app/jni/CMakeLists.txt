

cmake_minimum_required(VERSION 3.4.1)

option(INFERNO_BUNDLE_RUNTIME "Provision bundled runtime into /sdcard/Inferno" OFF)




set(CLUTTER "clutter" CACHE STRING "Native renderer selection")


set(CMAKE_VERBOSE_MAKEFILE on)


get_filename_component(INFERNO_OS_PROJ_DIR
                       ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)


# configure import libs
set(INFERNO_ROOT "${INFERNO_OS_PROJ_DIR}/inferno-os" CACHE PATH "Inferno source/library tree")
set(CLUTTER_PREFIX "${INFERNO_OS_PROJ_DIR}/install" CACHE PATH "Matching Clutter/Cogl ABI installation")
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(INFERNO_OBJTYPE arm)
elseif(ANDROID_ABI STREQUAL "arm64-v8a")
    set(INFERNO_OBJTYPE arm64)
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(INFERNO_OBJTYPE amd64)
else()
    message(FATAL_ERROR "Unsupported Inferno ABI: ${ANDROID_ABI}")
endif()
set(infrlib_DIR "${INFERNO_ROOT}/Android/${INFERNO_OBJTYPE}/lib")
# Clutter and Inferno carry distinct FreeType implementations; never mix their symbols.
if(CLUTTER STREQUAL "clutter")
    find_program(INFERNO_PYTHON python3)
    if(NOT INFERNO_PYTHON)
        message(FATAL_ERROR "Python 3 is required for private Inferno library preparation")
    endif()
    set(INFERNO_SOURCE_LIB_DIR "${infrlib_DIR}")
    set(infrlib_DIR "${CMAKE_CURRENT_BINARY_DIR}/inferno-private-lib")
    file(GLOB INFERNO_SOURCE_ARCHIVES "${INFERNO_SOURCE_LIB_DIR}/*.a")
    set(INFERNO_PRIVATE_ARCHIVES)
    foreach(name emu lib9 libbio libdraw libdynld libfreetype libinterp libkeyring libmath libmemdraw libmemlayer libmp libprefab libsec libstyx libtk liblodepng)
        list(APPEND INFERNO_PRIVATE_ARCHIVES "${infrlib_DIR}/${name}.a")
    endforeach()
    add_custom_command(OUTPUT "${infrlib_DIR}/ready.stamp"
        BYPRODUCTS ${INFERNO_PRIVATE_ARCHIVES}
        COMMAND "${INFERNO_PYTHON}" "${INFERNO_OS_PROJ_DIR}/tools/namespace-inferno-freetype.py"
            --source "${INFERNO_SOURCE_LIB_DIR}" --output "${infrlib_DIR}"
            --nm "${CMAKE_NM}" --objcopy "${CMAKE_OBJCOPY}"
        DEPENDS ${INFERNO_SOURCE_ARCHIVES} "${INFERNO_OS_PROJ_DIR}/tools/namespace-inferno-freetype.py"
        VERBATIM)
    add_custom_target(inferno_private_freetype DEPENDS "${infrlib_DIR}/ready.stamp")
endif()

link_directories("${CLUTTER_PREFIX}/lib")

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
		${CLUTTER_PREFIX}/include
		${CLUTTER_PREFIX}/include/mx-2.0
		${CLUTTER_PREFIX}/include/json-glib-1.0
		${CLUTTER_PREFIX}/include/atk-1.0
		${CLUTTER_PREFIX}/include/pango-1.0
		${CLUTTER_PREFIX}/include/cairo
		${CLUTTER_PREFIX}/include/glib-2.0
		${CLUTTER_PREFIX}/lib/glib-2.0/include
		${CLUTTER_PREFIX}/include/glib-android-1.0
		${CLUTTER_PREFIX}/include/clutter-1.0
		${CLUTTER_PREFIX}/include/cogl
	)



	add_library(inferno_os_app SHARED
		clutter_main.c
		clutter-utils.c
		inferno_storage.c
		inferno_viewport.c
	)
else()
	add_library(inferno_os_app SHARED
		fb_main.cpp
	)
endif()


add_library(native-audio STATIC native-audio.c)
if(CLUTTER STREQUAL "clutter")
    add_dependencies(inferno_os_app inferno_private_freetype)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set_property(TARGET inferno_os_app APPEND_STRING PROPERTY LINK_FLAGS " -fno-lto -fno-use-linker-plugin")
    endif()
endif()


if(INFERNO_BUNDLE_RUNTIME)
    target_compile_definitions(inferno_os_app PRIVATE INFERNO_BUNDLE_RUNTIME=1)
endif()

# now build app's shared lib
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -fPIC -Wall -DCOGL_ENABLE_EXPERIMENTAL_API")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fPIC -fPIE")

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate -fPIC -fPIE"
)
# -flto -Wl,--allow-multiple-definition


# add lib dependencies

if( "${CLUTTER}" STREQUAL "clutter")

target_link_libraries(inferno_os_app 
    -Wl,--wrap=libinit
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
    intl
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

