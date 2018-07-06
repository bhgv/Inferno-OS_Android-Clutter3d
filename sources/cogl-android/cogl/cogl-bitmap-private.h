/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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

#ifndef __COGL_BITMAP_H
#define __COGL_BITMAP_H

#include <glib.h>

#include "cogl-object-private.h"
#include "cogl-buffer.h"
#include "cogl-bitmap.h"

#ifdef COGL_HAS_ANDROID_SUPPORT
#include <android/asset_manager.h>
#endif

struct _CoglBitmap
{
  CoglObject _parent;

  /* Pointer back to the context that this bitmap was created with */
  CoglContext *context;

  CoglPixelFormat format;
  int width;
  int height;
  int rowstride;

  uint8_t *data;

  CoglBool mapped;
  CoglBool bound;

  /* If this is non-null then 'data' is ignored and instead it is
     fetched from this shared bitmap. */
  CoglBitmap *shared_bmp;

  /* If this is non-null then 'data' is treated as an offset into the
     buffer and map will divert to mapping the buffer */
  CoglBuffer *buffer;
};


/*
 * _cogl_bitmap_new_with_malloc_buffer:
 * @context: A #CoglContext
 * @width: width of the bitmap in pixels
 * @height: height of the bitmap in pixels
 * @format: the format of the pixels the array will store
 * @error: A #CoglError for catching exceptional errors or %NULL
 *
 * This is equivalent to cogl_bitmap_new_with_size() except that it
 * allocated the buffer using g_malloc() instead of creating a
 * #CoglPixelBuffer. The buffer will be automatically destroyed when
 * the bitmap is freed.
 *
 * Return value: a #CoglPixelBuffer representing the newly created array
 *
 * Since: 1.10
 * Stability: Unstable
 */
CoglBitmap *
_cogl_bitmap_new_with_malloc_buffer (CoglContext *context,
                                     unsigned int width,
                                     unsigned int height,
                                     CoglPixelFormat format,
                                     CoglError **error);

/* The idea of this function is that it will create a bitmap that
   shares the actual data with another bitmap. This is needed for the
   atlas texture backend because it needs upload a bitmap to a sub
   texture but override the format so that it ignores the premult
   flag. */
CoglBitmap *
_cogl_bitmap_new_shared (CoglBitmap      *shared_bmp,
                         CoglPixelFormat  format,
                         int              width,
                         int              height,
                         int              rowstride);

CoglBitmap *
_cogl_bitmap_convert (CoglBitmap *bmp,
		      CoglPixelFormat dst_format,
                      CoglError **error);

CoglBool
_cogl_bitmap_convert_into_bitmap (CoglBitmap *src_bmp,
                                  CoglBitmap *dst_bmp,
                                  CoglError **error);

CoglBitmap *
_cogl_bitmap_from_file (CoglContext *ctx,
                        const char *filename,
			CoglError **error);

#ifdef COGL_HAS_ANDROID_SUPPORT
CoglBitmap *
_cogl_android_bitmap_new_from_asset (CoglContext *ctx,
                                     AAssetManager *manager,
                                     const char *filename,
                                     CoglError **error);
#endif

CoglBool
_cogl_bitmap_unpremult (CoglBitmap *dst_bmp,
                        CoglError **error);

CoglBool
_cogl_bitmap_premult (CoglBitmap *dst_bmp,
                      CoglError **error);

CoglBool
_cogl_bitmap_convert_premult_status (CoglBitmap *bmp,
                                     CoglPixelFormat dst_format,
                                     CoglError **error);

CoglBool
_cogl_bitmap_copy_subregion (CoglBitmap *src,
			     CoglBitmap *dst,
			     int src_x,
			     int src_y,
			     int dst_x,
			     int dst_y,
			     int width,
			     int height,
                             CoglError **error);

/* Creates a deep copy of the source bitmap */
CoglBitmap *
_cogl_bitmap_copy (CoglBitmap *src_bmp,
                   CoglError **error);

CoglBool
_cogl_bitmap_get_size_from_file (const char *filename,
                                 int        *width,
                                 int        *height);

void
_cogl_bitmap_set_format (CoglBitmap *bitmap,
                         CoglPixelFormat format);

/* Maps the bitmap so that the pixels can be accessed directly or if
   the bitmap is just a memory bitmap then it just returns the pointer
   to memory. Note that the bitmap isn't guaranteed to allocated to
   the full size of rowstride*height so it is not safe to read up to
   the rowstride of the last row. This will be the case if the user
   uploads data using gdk_pixbuf_new_subpixbuf with a sub region
   containing the last row of the pixbuf because in that case the
   rowstride can be much larger than the width of the image */
uint8_t *
_cogl_bitmap_map (CoglBitmap *bitmap,
                  CoglBufferAccess access,
                  CoglBufferMapHint hints,
                  CoglError **error);

void
_cogl_bitmap_unmap (CoglBitmap *bitmap);

/* These two are replacements for map and unmap that should used when
 * the pointer is going to be passed to GL for pixel packing or
 * unpacking. The address might not be valid for reading if the bitmap
 * was created with new_from_buffer but it will however be good to
 * pass to glTexImage2D for example. The access should be READ for
 * unpacking and WRITE for packing. It can not be both
 *
 * TODO: split this bind/unbind functions out into a GL specific file
 */
uint8_t *
_cogl_bitmap_gl_bind (CoglBitmap *bitmap,
                      CoglBufferAccess access,
                      CoglBufferMapHint hints,
                      CoglError **error);

void
_cogl_bitmap_gl_unbind (CoglBitmap *bitmap);

CoglContext *
_cogl_bitmap_get_context (CoglBitmap *bitmap);

#endif /* __COGL_BITMAP_H */
