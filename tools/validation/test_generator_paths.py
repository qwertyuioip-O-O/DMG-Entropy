#!/usr/bin/env python3
"""Exercise code generators in isolated copies, including generated includes."""
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    generators = sorted((ROOT / 'tools/generation').glob('*.py'))
    for generator in generators:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in ('src', 'include', 'tools/native', 'tools/generation'):
                shutil.copytree(ROOT / relative, root / relative)
            script = root / generator.relative_to(ROOT)
            result = subprocess.run(
                [sys.executable, str(script)], cwd=root,
                capture_output=True, text=True, timeout=60,
            )
            if result.returncode:
                raise RuntimeError(f'{generator.name}: {result.stdout}\n{result.stderr}')
            assert not list((root / 'tools').glob('*.*')), generator.name
            sources = [*(root / 'tools/native').iterdir(),
                       *(root / 'src').rglob('*.cpp'), *(root / 'src').rglob('*.hpp')]
            for source in sources:
                for include in re.findall(r'#include "([^"]+)"', source.read_text()):
                    assert ((source.parent / include).is_file()
                            or (root / 'include' / include).is_file()
                            or (root / 'src' / include).is_file()), (
                                generator.name, source.name, include)
        print(f'PASS {generator.name}', flush=True)
    print(f'PASS {len(generators)} isolated code generators')


if __name__ == '__main__':
    main()
