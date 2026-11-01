#!/usr/bin/env python3
"""Exercise actual GLES clipping without allowing a borrowed image to change storage geometry."""
from pathlib import Path
import os
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HELPERS = runpy.run_path(str(ROOT/'tests/gles-input.py'))
BODY = HELPERS['function']((ROOT/'app/jni/clutter_main.c').read_text(), 'static void\nioa_render_clutter_clipr_actor(')
SOURCE = r'''
#include <stdio.h>
#include <stdlib.h>
typedef void ClutterActor;
typedef struct ActorData {int w,h; void *buffer;} ActorData;
static ActorData data = {127,400,(void*)1};
static int left,top,width,height;


/* Resolve only the live fixture actor; stale handles must be ignored. */
static ActorData *find_ActorData(void *actor) {return actor==(void*)1?&data:NULL;}


/* Observe the real requested clip independently from the unchanged backing store. */
static void clutter_actor_set_clip(void *actor,int x,int y,int w,int h)
{
    if(actor!=(void*)1)exit(1);
    left=x;top=y;width=w;height=h;
}
BODY


/* Large and partial clip rectangles must never resize the texture or its borrowed pixel source. */
int main(void)
{
    ioa_render_clutter_clipr_actor((void*)1,-4,-8,2147483647,2147483647);
    if(left || top || width!=127 || height!=400)return 2;
    ioa_render_clutter_clipr_actor((void*)1,10,20,90,100);
    if(left!=10 || top!=20 || width!=80 || height!=80)return 3;
    ioa_render_clutter_clipr_actor((void*)1,150,20,200,100);
    if(width || height)return 4;
    ioa_render_clutter_clipr_actor(NULL,0,0,127,400);
    if(data.w!=127 || data.h!=400 || data.buffer!=(void*)1)return 5;
    puts("PASS GLES clip: full, partial, empty, stale actor; immutable borrowed-buffer geometry");
    return 0;
}
'''.replace('BODY',BODY)


# Compile the production callback without allocation/resize stubs: such a regression must fail to link.
def main():
    with tempfile.TemporaryDirectory(prefix='gles-clip-') as directory:
        work=Path(directory);(work/'test.c').write_text(SOURCE)
        command=shlex.split(os.environ.get('CC','cc'))
        command+=shlex.split(os.environ.get('CFLAGS','-O2 -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+[str(work/'test.c'),'-o',str(work/'test')],check=True)
        subprocess.run([str(work/'test')],check=True)


if __name__=='__main__':
    main()
