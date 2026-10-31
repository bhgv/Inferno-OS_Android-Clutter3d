#include "inferno_viewport.h"
#include <limits.h>
#include <math.h>


/* Fit an immutable logical desktop into the actual content rectangle without stretching. */
int
ioa_viewport_fit(IoaViewport *out, int width, int height, int surfacewidth, int surfaceheight,
	int left, int top, int right, int bottom)
{
	IoaViewport result;
	double sx, sy;

	if(out == NULL || width <= 0 || height <= 0 || surfacewidth <= 0 || surfaceheight <= 0)
		return 0;
	if(left == 0 && top == 0 && right == 0 && bottom == 0){
		right = surfacewidth;
		bottom = surfaceheight;
	}
	if(left < 0) left = 0;
	if(top < 0) top = 0;
	if(right > surfacewidth) right = surfacewidth;
	if(bottom > surfaceheight) bottom = surfaceheight;
	if(right <= left || bottom <= top)
		return 0;
	result.width = width;
	result.height = height;
	result.available_width = right-left;
	result.available_height = bottom-top;
	sx = (double)(right-left)/width;
	sy = (double)(bottom-top)/height;
	result.scale = sx < sy ? sx : sy;
	result.x = left+((right-left)-width*result.scale)/2;
	result.y = top+((bottom-top)-height*result.scale)/2;
	*out = result;
	return 1;
}


/* Reject non-finite coordinates before any float-to-integer conversion; clamp only active drags. */
static int
logicalpoint(int width, int height, double x, double y, int clamp, int *px, int *py)
{
	if(width <= 0 || height <= 0 || !isfinite(x) || !isfinite(y))
		return 0;
	if(!clamp && (x < 0 || y < 0 || x >= width || y >= height))
		return 0;
	if(x < 0) x = 0;
	if(y < 0) y = 0;
	if(x >= width) x = width-1;
	if(y >= height) y = height-1;
	*px = (int)x;
	*py = (int)y;
	return 1;
}


/* Invert exactly the scale/translation applied to the GLES canvas. */
int
ioa_viewport_point(const IoaViewport *viewport, double x, double y, int clamp, int *px, int *py)
{
	if(viewport == NULL || px == NULL || py == NULL ||
	   !isfinite(viewport->scale) || viewport->scale <= 0)
		return 0;
	return logicalpoint(viewport->width, viewport->height,
		(x-viewport->x)/viewport->scale, (y-viewport->y)/viewport->scale, clamp, px, py);
}


/* Bound the original int-sized RGBA consumers as well as the allocator's size_t. */
int
ioa_surface_bytes(int width, int height, size_t *bytes)
{
	if(bytes == NULL || width <= 0 || height <= 0 || width > INT_MAX/4 || height > INT_MAX/(width*4))
		return 0;
	*bytes = (size_t)width*height*4;
	return 1;
}


/* Track sequence identities and release every button on cancel, duplicate-safe and bounded. */
int
ioa_pointer_event(IoaPointer *pointer, int type, uintptr_t sequence, double x, double y,
	int width, int height, IoaPointerEvent *out)
{
	int index, px, py;

	if(pointer == NULL || out == NULL)
		return 0;
	if(type == IoaTouchCancel){
		if(pointer->count == 0)
			return 0;
		pointer->count = 0;
		out->x = pointer->x;
		out->y = pointer->y;
		out->buttons = 0;
		return 1;
	}
	for(index = 0; index < pointer->count; index++)
		if(pointer->contacts[index].sequence == sequence)
			break;
	if(type == IoaTouchBegin){
		if(sequence == 0 || index != pointer->count || pointer->count == IOA_MAX_TOUCHES ||
		   !logicalpoint(width, height, x, y, 0, &px, &py))
			return 0;
		pointer->contacts[index].sequence = sequence;
		pointer->contacts[index].x = px;
		pointer->contacts[index].y = py;
		pointer->count++;
	}else if(type == IoaTouchUpdate || type == IoaTouchEnd){
		if(index == pointer->count)
			return 0;
		if(logicalpoint(width, height, x, y, 1, &px, &py)){
			pointer->contacts[index].x = px;
			pointer->contacts[index].y = py;
		}
		if(type == IoaTouchEnd){
			pointer->x = pointer->contacts[index].x;
			pointer->y = pointer->contacts[index].y;
			for(; index+1 < pointer->count; index++)
				pointer->contacts[index] = pointer->contacts[index+1];
			pointer->count--;
		}
	}else
		return 0;
	if(pointer->count != 0){
		pointer->x = pointer->contacts[0].x;
		pointer->y = pointer->contacts[0].y;
	}
	out->x = pointer->x;
	out->y = pointer->y;
	out->buttons = pointer->count == 0 ? 0 : pointer->count == 1 ? 1 : pointer->count == 2 ? 2 : 4;
	return 1;
}
