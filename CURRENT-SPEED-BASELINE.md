# Current portable research speed build

Builder: `tools/build_r19_current.py` (routes to `build_goal10_static.py`). Output executable: `g10static`. Fallback builder: `tools/build_goal10_index.py`.

Fixed original anchor SHA256: `7c466d55510493aa2366aa310f50583083a161f88304f64e2941089ca9333918`.
Selected executable SHA256: `872c463c64bf2b9a9bdcd794eece9d6bbba0246a253ff2985f48c3f468afa0df`.

Fresh CPU1 confirmation: encode **+10.938466%**, decode **+10.146912%** against that fixed anchor. Equal-dataset geometric means, phase wall-time, two consecutive native CLI calls averaged per measurement unit; 96 native calls, 48 units, none removed. The unit quality gate passes. Individual native-call variability remains and its separate audit fails. The observed point target is reached; a confidence lower bound >=10% is not established. The decode margin is narrow.

The CPU0 single-call screen passes its original 1.25 range gate and is +2.674355% encode / +2.904227% decode versus the previous g10index. Additional immutable attack payload: 841 KiB. No native CPU target or optional ISA is required; other physical CPUs were not tested.

Full results, validation and prior rejected attempts: [README-Goal10-Magic-Static-20261023.md](README-Goal10-Magic-Static-20261023.md).
