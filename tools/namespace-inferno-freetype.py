#!/usr/bin/env python3
"""Isolate Inferno's private FreeType implementation from Clutter's FreeType ABI."""
import argparse
from pathlib import Path
import subprocess


# Rename definitions and references together in copied core archives, leaving source archives intact.
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--nm',required=True)
    parser.add_argument('--objcopy',required=True)
    args=parser.parse_args()
    source,output=args.source.resolve(),args.output.resolve()
    if source==output:
        raise RuntimeError('Private copies must not replace the original Inferno libraries')
    listing=subprocess.check_output([args.nm,'-P','-g','--defined-only',str(source/'libfreetype.a')],text=True)
    symbols=set()
    for line in listing.splitlines():
        fields=line.split()
        if len(fields)>=3 and len(fields[1])==1 and fields[1].isalpha():
            symbols.add(fields[0])
    if 'FT_Init_FreeType' not in symbols:
        raise RuntimeError('No complete Inferno FreeType symbol table')
    output.mkdir(parents=True,exist_ok=True)
    mapping=output/'freetype-symbols.txt'
    mapping.write_text(''.join(name+' inferno_private_'+name+'\n' for name in sorted(symbols)))
    for archive in sorted(source.glob('*.a')):
        destination=output/archive.name
        temporary=output/(archive.name+'.tmp')
        subprocess.run([args.objcopy,'--redefine-syms='+str(mapping),str(archive),str(temporary)],check=True)
        temporary.replace(destination)
    (output/'ready.stamp').write_text('Inferno FreeType symbols isolated from Clutter\n')


if __name__=='__main__':
    main()
