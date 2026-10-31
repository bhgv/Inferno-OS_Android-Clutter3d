# Native storage and GLES geometry

The NativeActivity entry remains C and the renderer remains Clutter/Cogl GLES.
This series adds no Java source or JNI calls. Existing audio code is unchanged.

## Runtime files

`make inferno-assets INFERNO_ROOT=/path/to/Inferno` stages runtime files under
`app/assets/inferno`. Both `make inferno` and `make inferno-full` run staging
before ndk-build and Ant. Compile the Limbo runtime tree first: staging requires
`dis/emuinit.dis`. It packages dis, lib, fonts, icons, optional usr/inferno,
empty mount directories and a bounded manifest, rather than source or host tools.

At native startup the installer uses `ANativeActivity.internalDataPath/Inferno`.
It creates missing directories and files using AAssetManager, writes through a
temporary file, and preserves existing user files. A failed extraction can be
retried. If assets are absent, a manually provisioned private root containing
`dis/emuinit.dis` is accepted. Uninstalling the app removes this private storage.
It needs no shared-storage permission and avoids the version-dependent
`/sdcard/Inferno` access rules. Private storage is not a public file-manager
folder; public document import/export is not implemented by this change.

The installer supplies INFERNO_FILES_DIR and a process-local EXTERNAL_STORAGE
compatibility value for the older bundled amain. The core resolver in the current
Inferno experiment branch also understands the native private directory.

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
`python3 tests/native-storage.py`, and `python3 tests/cmake-abis.py`.
The input test executes extracted production callbacks and backend translator
against bounded host Android/Clutter interfaces: 3312 active predicates, including
rotation, insets, inverse mapping, multi-touch, cancellation and timestamp wrap.
It passes strict UBSan at O1/O3 and combined ASan/UBSan. Leak scanning is disabled
in that host fixture because the sandbox hides /proc; ASan memory checks remain.
The parent touch handler compiles in the same fixture and fails the coordinate
predicate after two checks.

The startup test links a separate emu fixture archive through the actual GNU
linker wrapper, checks parsed -g, alignment, allocation bounds and repeat guards.
The installer test exercises 14 actual packaging/filesystem/retry predicates.
CMake configuration checks three archive selections and unsupported ABI rejection.
These are host checks, not an NDK compilation, APK installation, physical touch
calibration or GPU rendering test. No Android device/SDK or rebuilt Clutter stack
was available for end-to-end verification.
