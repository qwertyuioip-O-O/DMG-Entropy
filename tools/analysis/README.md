# Analysis and input audits

This directory houses development loss analyses and dataset/measurement audits.

Legacy command paths such as `python tools/audit_r16_data.py` are retained as compatibility launchers; new code should use `python tools/analysis/audit_r16_data.py`.

Scripts that locate repository-root datasets through `__file__` have been updated for the additional directory level. This reorganization does not change codec implementations, benchmark input data, or CMake source locations.

Do not remove the compatibility launchers until references in historical reports and automation have been migrated.
