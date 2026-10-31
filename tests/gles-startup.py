#!/usr/bin/env python3
"""Exercise the production libinit wrapper through a separately archived startup caller."""
from pathlib import Path
import importlib.util
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('gles_input', ROOT/'tests/gles-input.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
body = module.function((ROOT/'app/jni/clutter_main.c').read_text(), 'void\n__wrap_libinit(')
source = r'''
#include <stdio.h>
#include <stdlib.h>
#include "inferno_viewport.h"
#define LOGE(...) ((void)0) // Android logging is outside this bounded startup check.
static int canvas;
static void *inferno_canvas = &canvas;
static char *xscreendata;
int Xsize, Ysize;
static int width, height, clipwidth, clipheight, synced, started;
extern void __real_libinit(void *);
extern void amain_fixture(int, int);


/* Record final actor extent independently from the buffer allocation. */
static void clutter_actor_set_size(void *actor,int w,int h) { width=w; height=h; }


/* Record the logical clipping extent before any interpreter worker starts. */
static void clutter_actor_set_clip(void *actor,int x,int y,int w,int h) { clipwidth=w; clipheight=h; }


/* Require publication of geometry before entering the real core. */
static void sync_surface_geometry(void) { synced++; }


/* Stand in for the real core entry reached only after validated allocation. */
void libinit(void *arg)
{
    if(!xscreendata || width != Xsize || height != Ysize ||
       clipwidth != Xsize || clipheight != Ysize || synced != 1)
        exit(2);
    xscreendata[(size_t)Xsize*Ysize*4-1] = 1;
    started++;
}

WRAPPER


/* Check parsed -g, word alignment, invalid dimensions, and repeat initialization. */
int main(void)
{
    amain_fixture(643, 479);
    if(started != 1 || Xsize != 640 || Ysize != 479) return 3;
    amain_fixture(640,479);
    if(started != 1) return 4;
    free(xscreendata);
    xscreendata = NULL;
    amain_fixture(3,479);
    amain_fixture(2147483647,2147483647);
    if(started != 1 || xscreendata != NULL) return 5;
    puts("PASS GLES startup: archived libinit wrapping, parsed geometry, alignment, bounds, repeat guard");
    return 0;
}
'''.replace('WRAPPER',body)
caller = '''extern int Xsize,Ysize;
extern void libinit(void *);
/* Model the separate emu archive caller after startup option parsing. */
void amain_fixture(int x,int y) { Xsize=x; Ysize=y; libinit(0); }
'''
with tempfile.TemporaryDirectory(prefix='gles-startup-',dir=ROOT) as directory:
    work=Path(directory)
    (work/'main.c').write_text(source)
    (work/'caller.c').write_text(caller)
    cc=shlex.split(os.environ.get('CC','cc'))
    flags=shlex.split(os.environ.get('CFLAGS','-O2 -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(cc+flags+['-c',str(work/'caller.c'),'-o',str(work/'caller.o')],check=True)
    subprocess.run(['ar','rcs',str(work/'emu.a'),str(work/'caller.o')],check=True)
    subprocess.run(cc+flags+['-I'+str(ROOT/'app/jni'),str(work/'main.c'),str(ROOT/'app/jni/inferno_viewport.c'),str(work/'emu.a'),'-Wl,--wrap=libinit','-lm','-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test')],check=True)
