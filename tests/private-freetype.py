#!/usr/bin/env python3
"""Link conflicting FreeType implementations through the actual archive preparation tool."""
from pathlib import Path
import subprocess
import tempfile
import hashlib

ROOT=Path(__file__).resolve().parents[1]


# Model incompatible implementations with different results; require their callers to stay isolated.
def main():
    with tempfile.TemporaryDirectory(prefix='private-freetype-',dir=ROOT) as directory:
        work=Path(directory)
        source=work/'source';source.mkdir()
        units={
            'ft':'/* Private implementation. */\nint FT_Init_FreeType(void){return 11;}\n/* Private wrapper. */\nint ftnewface(void){return FT_Init_FreeType();}\n',
            'interp':'extern int ftnewface(void);\n/* VM caller must bind to its private wrapper. */\nint vm_font(void){return ftnewface();}\n',
            'external':'/* Simulate the separate Clutter font ABI. */\nint FT_Init_FreeType(void){return 29;}\n',
            'main':'extern int FT_Init_FreeType(void),vm_font(void);\n/* Both distinct implementations must remain reachable. */\nint main(void){return vm_font()!=11 || FT_Init_FreeType()!=29;}\n'}
        for name,body in units.items():
            (work/(name+'.c')).write_text(body)
            subprocess.run(['cc','-c',str(work/(name+'.c')),'-o',str(work/(name+'.o'))],check=True)
        for archive,obj in [('libfreetype','ft'),('libinterp','interp')]:
            subprocess.run(['ar','rcs',str(source/(archive+'.a')),str(work/(obj+'.o'))],check=True)
        before={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in source.glob('*.a')}
        subprocess.run(['python3',str(ROOT/'tools/namespace-inferno-freetype.py'),'--source',str(source),'--output',str(work/'private'),'--nm','nm','--objcopy','objcopy'],check=True)
        subprocess.run(['cc',str(work/'main.o'),str(work/'private/libinterp.a'),str(work/'private/libfreetype.a'),str(work/'external.o'),'-o',str(work/'run')],check=True)
        subprocess.run([str(work/'run')],check=True)
        if before!={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in source.glob('*.a')}:
            raise RuntimeError('Original libraries were modified')
    print('PASS actual private archive link: distinct font implementations, matching references, original archives preserved')


if __name__=='__main__':
    main()
