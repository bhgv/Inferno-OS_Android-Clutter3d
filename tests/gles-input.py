#!/usr/bin/env python3
"""Connect actual GLES callbacks and native translator to bounded host platform interfaces."""
import os
from pathlib import Path
import re
import resource
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


# Preserve complete production bodies while excluding comment/literal braces from matching.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise RuntimeError(signature)


# Execute actual touch dispatch and resize decisions; the adapter does not create a GL context.
def main():
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    app = (ROOT/'app/jni/clutter_main.c').read_text()
    backend = (ROOT/'sources/clutter-android/clutter/android/clutter-android-application.c').read_text()
    parts = [function(app, signature) for signature in ('static void\ncancel_inferno_pointer(', 'static void\nsync_surface_geometry(', 'static void\non_stage_deactivate(')]
    if os.environ.get('GLES_TOUCH_SOURCE'):
        parent = Path(os.environ['GLES_TOUCH_SOURCE']).read_text()
        parts += [function(parent,'static gboolean\non_touch (').replace('on_touch (','parent_on_touch (',1)]
        parts += ['static int on_touch(ClutterActor *actor,ClutterEvent *event,void *data){(void)data;return parent_on_touch(actor,event);}']
    else:
        parts += [function(app,'static gboolean\non_touch(')]
    parts += [function(backend, signature) for signature in ('static void\nupdate_android_stage_geometry(', 'static void\nupdate_android_stage_focus(', 'static gboolean\ntranslate_motion_event_to_touch_event(')]
    source = (ROOT/'tests/gles-input.c').read_text().replace('IMPLEMENTATION','\n\n'.join(parts))
    with tempfile.TemporaryDirectory(prefix='gles-input-',dir=ROOT) as directory:
        work = Path(directory)
        (work/'input.c').write_text(source)
        compiler = shlex.split(os.environ.get('CC','cc'))
        flags = shlex.split(os.environ.get('CFLAGS','-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        binary = work/'input'
        subprocess.run(compiler+flags+['-std=c99','-I'+str(ROOT/'app/jni'),str(work/'input.c'),str(ROOT/'app/jni/inferno_viewport.c'),'-lm','-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)


if __name__ == '__main__':
    main()
