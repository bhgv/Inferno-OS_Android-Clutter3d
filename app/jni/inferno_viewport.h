#ifndef IOA_INFERNO_VIEWPORT_H
#define IOA_INFERNO_VIEWPORT_H // Shared display/input geometry, independent of EGL and Android headers.
#include <stddef.h>
#include <stdint.h>

#define IOA_MAX_TOUCHES 16 // Bound simultaneously tracked sequence identities.

typedef struct IoaViewport IoaViewport;

struct IoaViewport {
	int width, height; // Immutable logical Inferno extent.
	int available_width, available_height; // Clipped Android content extent before aspect fitting.
	double x, y, scale; // Stage-space origin and uniform scale used by the canvas actor.
};

typedef struct IoaContact IoaContact;

struct IoaContact {
	uintptr_t sequence; // Stable Clutter sequence, never a mutable pointer-array index.
	int x, y; // Last valid logical coordinate for this contact.
};

typedef struct IoaPointer IoaPointer;

struct IoaPointer {
	IoaContact contacts[IOA_MAX_TOUCHES]; // Arrival order; first remaining contact owns the cursor.
	int count; // Number of successfully admitted contacts.
	int x, y; // Last published cursor position, retained for cancellation.
};

typedef struct IoaPointerEvent IoaPointerEvent;

struct IoaPointerEvent {
	int x, y, buttons; // Inferno logical position and bit mask, not a finger count.
};

enum {
	IoaTouchBegin, IoaTouchUpdate, IoaTouchEnd, IoaTouchCancel
};

int ioa_viewport_fit(IoaViewport*, int, int, int, int, int, int, int, int);
int ioa_viewport_point(const IoaViewport*, double, double, int, int*, int*);
int ioa_surface_bytes(int, int, size_t*);
int ioa_pointer_event(IoaPointer*, int, uintptr_t, double, double, int, int, IoaPointerEvent*);
#endif
