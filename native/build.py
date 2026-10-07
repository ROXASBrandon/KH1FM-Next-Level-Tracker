"""Build Windows x64 helper and run portable counter tests. Requires ziglang."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CC = [sys.executable, '-m', 'ziglang', 'cc']
with tempfile.TemporaryDirectory() as temp:
    for name in ('counter', 'hud'):
        exe = Path(temp)/(name+'-test'+('.exe' if sys.platform == 'win32' else ''))
        subprocess.run(CC+['-O2', '-UNDEBUG', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT/'native'),
                          str(ROOT/f'tests/{name}_test.c'), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
if sys.platform == 'win32':
    with tempfile.TemporaryDirectory() as temp:
        exe = Path(temp)/'render-test.exe'
        subprocess.run(CC+['-O2', '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
                          str(ROOT/'tests/render_test.c'), '-o', str(exe),
                          '-luser32', '-lkernel32'], check=True)
        subprocess.run([str(exe)], check=True)
output = ROOT/'scripts/io_packages/kh1_next_level_popup.dll'
output.parent.mkdir(parents=True, exist_ok=True)
subprocess.run(CC+['-target', 'x86_64-windows-gnu', '-O2', '-shared', '-s',
                  '-Wall', '-Wextra', '-Werror', str(ROOT/'native/popup.c'),
                  '-o', str(output), '-luser32', '-lkernel32'], check=True)
print(output)
