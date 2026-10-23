#!/usr/bin/env python3
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parent / "reproduction" / "reproduce_r19.py"),run_name="__main__")
