#!/usr/bin/env python3
"""Current portable research build: index model + magic sliding + constant piece tables."""
from pathlib import Path
import subprocess,sys
subprocess.run([sys.executable,str(Path(__file__).with_name('build_goal10_static.py')),*sys.argv[1:]],check=True)
