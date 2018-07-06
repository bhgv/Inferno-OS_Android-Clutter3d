/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

/* See cogl-winsys-glx-feature-functions.h for a description of how
   this file is used */

COGL_WINSYS_FEATURE_BEGIN (swap_control,
                           "EXT\0",
                           "swap_control\0",
                           0,
                           0,
                           COGL_WINSYS_FEATURE_SWAP_THROTTLE)
COGL_WINSYS_FEATURE_FUNCTION (int, wglSwapInterval,
                              (int interval))
COGL_WINSYS_FEATURE_END ()
