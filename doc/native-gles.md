# Native storage and GLES geometry

The NativeActivity entry remains C and the renderer remains Clutter/Cogl GLES.
This series adds no Java source or JNI calls. Existing audio code is unchanged.

## Runtime files

Runtime files are used exclusively from `/sdcard/Inferno`. By default the APK
contains no runtime assets; populate this existing tree yourself, including
dis/emuinit.dis. No internal-directory fallback exists. Private trees created
by the withdrawn installer are neither migrated nor deleted automatically.

To opt into bundling and extraction of missing runtime files directly into the
shared directory, build with:

```
make inferno INFERNO_BUNDLE_RUNTIME=1 INFERNO_ROOT=/path/to/Inferno
```

The default is INFERNO_BUNDLE_RUNTIME=0. Both make inferno and inferno-full stage
assets only when enabled, and remove the generated asset tree when disabled.
Existing destination files are preserved; failed extraction is retried on the
next launch. The CMake alternative uses -DINFERNO_BUNDLE_RUNTIME=ON and requires
running tools/stage-inferno.py separately to prepare app/assets/inferno.
Direct ndk-build likewise needs staged assets and INFERNO_BUNDLE_RUNTIME=1.
Execution always uses /sdcard/Inferno, with no asset-backed runtime or private copy.

Native startup validates shared-directory access and sets EXTERNAL_STORAGE to
/sdcard only for compatibility with the older bundled amain. It neither uses
internalDataPath nor supplies INFERNO_FILES_DIR. If access fails, startup reports
an explicit error. Android's shared-storage permissions still apply; access on
every Android version has not been established. No private fallback is used.

## One transform for rendering and input

All Inferno texture actors share one canvas with a logical desktop size. Native
window dimensions and the clipped Android content rectangle determine a uniform,
centered scale. Rotation, keyboard insets and resizing refit that canvas without
resizing buffers already owned by Inferno. Unused space is black.

The stage captures touch events once. Clutter's actual inverse canvas transform
maps window coordinates into logical coordinates. A new touch outside the canvas
is ignored; an existing drag is clamped at the desktop edge. Stable sequence IDs
handle pointer index reordering, duplicate begin/end and primary contact changes.
One, two and three contacts produce mouse masks 1, 2 and 4. Extra contacts retain
mask 4. Cancellation, focus loss and transform changes release held buttons.
The first active contact owns the cursor until it ends.

The native backend updates both Cogl surface dimensions and Clutter stage size,
including CONTENT_RECT_CHANGED. CANCEL is emitted for every active Android
pointer. Native nanosecond timestamps are converted to Clutter milliseconds.

`--wrap=libinit` in both native build paths delays screen allocation until amain
has parsed startup options, including `-g`. The wrapper aligns width to the same
four-pixel boundary as attachscreen, rejects invalid/overflowing RGBA dimensions,
sets the canvas extent and only then starts the actual core. Keep this linker
option when creating another GLES build entry. The framebuffer alternative does
not use this wrapper.

## Building

The repository's primary APK path is Makefile, ndk-build and Ant. Set INFERNO_ROOT
to a populated Inferno checkout. Native ABI mapping is:

| Android ABI | Inferno archive directory |
| --- | --- |
| armeabi-v7a | Android/arm/lib |
| arm64-v8a | Android/arm64/lib |
| x86_64 | Android/amd64/lib |

CMake offers the same mapping and accepts INFERNO_ROOT and CLUTTER_PREFIX.
The old hardcoded host gcc selection is removed so the Android toolchain can
select its compiler. These selections do not supply missing 64-bit platform ports.
Every dependency and archive must be rebuilt for the selected ABI. In particular,
rebuild and reinstall the bundled `sources/clutter-android` backend: an existing
prebuilt Clutter library does not contain these resize/focus/touch fixes.
The legacy SDK/NDK/Ant dependency stack has not been modernized in this series.
The existing inferno-os gitlink is unchanged; select and build the desired core
checkout explicitly rather than assuming the link contains current archives.

## Validation and limits

Run `python3 tests/gles-input.py`, `python3 tests/gles-startup.py`,
`python3 tests/shared-storage.py`, and `python3 tests/cmake-abis.py`.
The input test executes extracted production callbacks and backend translator
against bounded host Android/Clutter interfaces: 3312 active predicates, including
rotation, insets, inverse mapping, multi-touch, cancellation and timestamp wrap.
It passes strict UBSan at O1/O3 and combined ASan/UBSan. Leak scanning is disabled
in that host fixture because the sandbox hides /proc; ASan memory checks remain.
The parent touch handler compiles in the same fixture and fails the coordinate
predicate after two checks.

The startup test links a separate emu fixture archive through the actual GNU
linker wrapper, checks parsed -g, alignment, allocation bounds and repeat guards.
The shared-storage test checks exact path requests, required permissions,
compatibility environment and missing/non-directory/denied paths using bounded
host filesystem interfaces.
CMake configuration checks three archive selections and unsupported ABI rejection.
These are host checks, not an NDK compilation, APK installation, physical touch
calibration or GPU rendering test. No Android device/SDK or rebuilt Clutter stack
was available for end-to-end verification.

`python3 tests/native-storage.py` also exercises enabled provisioning against a
host asset adapter and emulated shared mount: 14 packaging/extraction/retry and
asset-disable predicates. Existing files are preserved. Both provisioning modes
are checked in actual CMake configuration; Android device testing remains open.

## Verified ARM32 test build follow-up

The 2026-10-31 test APK was actually compiled and linked with NDK r16b/API 17.
Core libraries used classic mk and EXTWINFLAGS=-DEXT_WIN; the framebuffer APK
used a separate clean core with that flag empty. Never mix those archives.
Both native entries allocate their logical buffer after startup -g parsing.

Clutter carries a different FreeType implementation from Inferno. CMake now
runs tools/namespace-inferno-freetype.py to copy core archives and consistently
rename the private FreeType definitions and references. It keeps the original
core archives unchanged. The normal make/ndk-build entry prepares equivalent
copies with inferno-libs. Set INFERNO_NM and INFERNO_OBJCOPY to suitable tools.
The actual host link test tests/private-freetype.py keeps both implementations
reachable and checks preservation of input archives. The unused mx-2.0 link
requirement was removed; libintl was added for existing translation calls.
Legacy fat LTO archives are linked through their native code when using GCC,
without loading a different GCC version's LTO plugin.

After extracting install.tgz with tar --no-same-owner, update the changed native
backend against that ARM32 stack with:

```
python3 tools/rebuild-android-backends.py --cc /path/to/standalone/bin/arm-linux-androideabi-gcc
```

It recompiles the actual Clutter Android source and the original GLib gutils
source with Android feature detection (issetugid is not supplied by this libc).
No GLib function or privilege-check implementation is replaced. The helper
rejects a non-ARM32 compiler; this legacy dependency archive is not an ARM64 port.
The GLES CMake build used CMAKE_NM=/usr/bin/nm in this sandbox because the old
NDK nm plugin could not run with the sandbox's restricted /proc.

The runtime packager now also includes available acme, locale, man, module and
services trees. This makes compiled Acme files and Limbo module interfaces
available with the normal runtime directories.
