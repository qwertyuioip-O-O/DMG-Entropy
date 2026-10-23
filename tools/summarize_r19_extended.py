#!/usr/bin/env python3
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parent / "reporting" / "summarize_r19_extended.py"),run_name="__main__")
