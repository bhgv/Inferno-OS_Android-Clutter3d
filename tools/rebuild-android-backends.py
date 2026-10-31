#!/usr/bin/env python3
"""Recompile modified Android backend objects against the selected installed stack."""
import argparse
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]


# Compile original source with Android feature detection, then replace only its matching archive member.
def compile_member(compiler,archive_tool,source,destination,config,includes,defines,archive):
    destination.parent.mkdir(parents=True,exist_ok=True)
    (destination.parent/'config.h').write_text(config)
    command=[compiler,'-c','-fPIC','-O2','-std=gnu99']+['-D'+name for name in defines]
    command+=['-I'+str(destination.parent)]+['-I'+str(path) for path in includes]
    subprocess.run(command+[str(source),'-o',str(destination)],check=True)
    subprocess.run([archive_tool,'rcs',str(archive),str(destination)],check=True)


# Preserve all original platform checks; do not claim BSD-only issetugid exists in Android libc.
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc',required=True)
    parser.add_argument('--ar',default='ar')
    parser.add_argument('--prefix',type=Path,default=ROOT/'install')
    parser.add_argument('--work',type=Path,default=ROOT/'build/android-backends')
    args=parser.parse_args()
    macros=subprocess.check_output([args.cc,'-dM','-E','-x','c','-'],input='',text=True)
    if '#define __arm__ 1' not in macros or '#define __SIZEOF_POINTER__ 4' not in macros:
        parser.error('The installed legacy dependency stack and this configuration require ARM32')
    prefix,work=args.prefix.resolve(),args.work.resolve()
    glib=ROOT/'sources/glib'
    clutter=ROOT/'sources/clutter-android'
    backend=work/'clutter'
    backend.mkdir(parents=True,exist_ok=True)
    declarations=['#include <glib-object.h>']
    for row in (clutter/'clutter/clutter-marshal.list').read_text().splitlines():
        row=row.strip()
        if not row or row.startswith('#'):
            continue
        result,arguments=row.split(':')
        name='_clutter_marshal_'+result+'__'+arguments.replace(',','_')
        declarations.append('extern void '+name+'(GClosure*,GValue*,guint,const GValue*,gpointer,gpointer);')
    (backend/'clutter-marshal.h').write_text('\n'.join(declarations)+'\n')
    public=[prefix/'include',prefix/'include/glib-2.0',prefix/'lib/glib-2.0/include']
    compile_member(args.cc,args.ar,clutter/'clutter/android/clutter-android-application.c',backend/'clutter-android-application.o',
                   '#define GETTEXT_PACKAGE "clutter-1.0" // Existing Clutter translation domain.\n',
                   [clutter,clutter/'clutter',clutter/'clutter/android']+public+
                   [prefix/('include/'+name) for name in ('clutter-1.0','cogl','glib-android-1.0','pango-1.0','cairo','json-glib-1.0','atk-1.0')],
                   ['CLUTTER_COMPILATION','COGL_ENABLE_EXPERIMENTAL_API'],prefix/'lib/libclutter-1.0.a')
    configuration='''#define SIZEOF_CHAR 1 // ARM32 ABI used by the existing Android stack.
#define SIZEOF_INT 4 // ARM32 ABI used by the existing Android stack.
#define SIZEOF_LONG 4 // ARM32 ABI used by the existing Android stack.
#define SIZEOF_VOID_P 4 // ARM32 ABI used by the existing Android stack.
#define HAVE_UNISTD_H 1 // Header provided by the selected Android sysroot.
#define HAVE_PWD_H 1 // Header provided by the selected Android sysroot.
#define HAVE_SYS_PARAM_H 1 // Header provided by the selected Android sysroot.
#define HAVE_MEMMOVE 1 // Function provided by Android libc.
#define HAVE_ATEXIT 1 // Function provided by Android libc.
#define HAVE_GETRESUID 1 // Function provided by Android libc.
#define HAVE_GETPWUID_R 1 // Function provided by the tested Android API 17 sysroot.
#define HAVE_GETCWD 1 // Function provided by Android libc.
#define GETTEXT_PACKAGE "glib20" // Existing GLib translation domain.
#define ENABLE_NLS 1 // Preserve localization through libintl.
'''
    compile_member(args.cc,args.ar,glib/'glib/gutils.c',work/'glib/libglib_2_0_la-gutils.o',configuration,
                   [glib,glib/'glib']+public,['GLIB_COMPILATION'],prefix/'lib/libglib-2.0.a')
    print('Updated Android Clutter backend and GLib platform-configured object; existing ARM32 stack retained')


if __name__=='__main__':
    main()
