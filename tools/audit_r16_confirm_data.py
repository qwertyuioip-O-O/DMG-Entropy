#!/usr/bin/env python3
"""Backward-compatible entry point; implementation lives in tools/analysis/."""
from pathlib import Path
import runpy

runpy.run_path(str(Path(__file__).resolve().parent / "analysis" / "audit_r16_confirm_data.py"), run_name="__main__")
