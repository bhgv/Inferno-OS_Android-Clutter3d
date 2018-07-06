/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_VERSION_H__
#define __COGL_VERSION_H__

#include <cogl/cogl-defines.h>

/**
 * SECTION:cogl-version
 * @short_description: Macros for determining the version of Cogl being used
 *
 * Cogl offers a set of macros for checking the version of the library
 * at compile time.
 *
 * Since: 1.12.0
 */

/**
 * COGL_VERSION_MAJOR:
 *
 * The major version of the Cogl library (1, if %COGL_VERSION is 1.2.3)
 *
 * Since: 1.12.0
 */
#define COGL_VERSION_MAJOR COGL_VERSION_MAJOR_INTERNAL

/**
 * COGL_VERSION_MINOR:
 *
 * The minor version of the Cogl library (2, if %COGL_VERSION is 1.2.3)
 *
 * Since: 1.12.0
 */
#define COGL_VERSION_MINOR COGL_VERSION_MINOR_INTERNAL

/**
 * COGL_VERSION_MICRO:
 *
 * The micro version of the Cogl library (3, if %COGL_VERSION is 1.2.3)
 *
 * Since: 1.12.0
 */
#define COGL_VERSION_MICRO COGL_VERSION_MICRO_INTERNAL

/**
 * COGL_VERSION_STRING:
 *
 * The full version of the Cogl library, in string form (suited for
 * string concatenation)
 *
 * Since: 1.12.0
 */
#define COGL_VERSION_STRING COGL_VERSION_STRING_INTERNAL

/* Macros to handle compacting a 3-component version number into an
 * int for quick comparison. This assumes all of the components are <=
 * 1023 and that an int is >= 31 bits */
#define COGL_VERSION_COMPONENT_BITS 10
#define COGL_VERSION_MAX_COMPONENT_VALUE        \
  ((1 << COGL_VERSION_COMPONENT_BITS) - 1)

/**
 * COGL_VERSION:
 *
 * The Cogl version encoded into a single integer using the
 * COGL_VERSION_ENCODE() macro. This can be used for quick comparisons
 * with particular versions.
 *
 * Since: 1.12.0
 */
#define COGL_VERSION                            \
  COGL_VERSION_ENCODE (COGL_VERSION_MAJOR,      \
                       COGL_VERSION_MINOR,      \
                       COGL_VERSION_MICRO)

/**
 * COGL_VERSION_ENCODE:
 * @major: The major part of a version number
 * @minor: The minor part of a version number
 * @micro: The micro part of a version number
 *
 * Encodes a 3 part version number into a single integer. This can be
 * used to compare the Cogl version. For example if there is a known
 * bug in Cogl versions between 1.3.2 and 1.3.4 you could use the
 * following code to provide a workaround:
 *
 * |[
 * #if COGL_VERSION >= COGL_VERSION_ENCODE (1, 3, 2) && \
 *     COGL_VERSION <= COGL_VERSION_ENCODE (1, 3, 4)
 *   /<!-- -->* Do the workaround *<!-- -->/
 * #endif
 * ]|
 *
 * Since: 1.12.0
 */
#define COGL_VERSION_ENCODE(major, minor, micro)        \
  (((major) << (COGL_VERSION_COMPONENT_BITS * 2)) |     \
   ((minor) << COGL_VERSION_COMPONENT_BITS)             \
   | (micro))

/**
 * COGL_VERSION_GET_MAJOR:
 * @version: An encoded version number
 *
 * Extracts the major part of an encoded version number.
 *
 * Since: 1.12.0
 */
#define COGL_VERSION_GET_MAJOR(version)                 \
  (((version) >> (COGL_VERSION_COMPONENT_BITS * 2))     \
   & COGL_VERSION_MAX_COMPONENT_VALUE)

/**
 * COGL_VERSION_GET_MINOR:
 * @version: An encoded version number
 *
 * Extracts the minor part of an encoded version number.
 *
 * Since: 1.12.0
 */
#define COGL_VERSION_GET_MINOR(version)         \
  (((version) >> COGL_VERSION_COMPONENT_BITS) & \
   COGL_VERSION_MAX_COMPONENT_VALUE)

/**
 * COGL_VERSION_GET_MICRO:
 * @version: An encoded version number
 *
 * Extracts the micro part of an encoded version number.
 *
 * Since: 1.12.0
 */
#define COGL_VERSION_GET_MICRO(version) \
  ((version) & COGL_VERSION_MAX_COMPONENT_VALUE)

/**
 * COGL_VERSION_CHECK:
 * @major: The major part of a version number
 * @minor: The minor part of a version number
 * @micro: The micro part of a version number
 *
 * A convenient macro to check whether the Cogl version being compiled
 * against is at least the given version number. For example if the
 * function cogl_pipeline_frobnicate was added in version 2.0.1 and
 * you want to conditionally use that function when it is available,
 * you could write the following:
 *
 * |[
 * #if COGL_VERSION_CHECK (2, 0, 1)
 * cogl_pipeline_frobnicate (pipeline);
 * #else
 * /<!-- -->* Frobnication is not supported. Use a red color instead *<!-- -->/
 * cogl_pipeline_set_color_4f (pipeline, 1.0f, 0.0f, 0.0f, 1.0f);
 * #endif
 * ]|
 *
 * Return value: %TRUE if the Cogl version being compiled against is
 *   greater than or equal to the given three part version number.
 * Since: 1.12.0
 */
#define COGL_VERSION_CHECK(major, minor, micro) \
  (COGL_VERSION >= COGL_VERSION_ENCODE (major, minor, micro))

#endif /* __COGL_VERSION_H__ */
