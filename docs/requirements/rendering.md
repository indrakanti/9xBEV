# Rendering requirements and traceability

| ID | Requirement | Code | Validation/status |
|---|---|---|---|
| CFG-001 | YAML controls all exposed runtime rendering parameters | config.cpp, qualcomm_linux.yaml | config_tests |
| CFG-002 | Reject unknown/duplicate keys, nonfinite values, invalid rotations and oversized mapping | config.cpp, core.cpp | config_tests, core_tests |
| GEO-001 | Explicit optical/vehicle frames, metric dimensions, fisheye model | core.hpp, core.cpp | optical-axis/right/origin/backward checks |
| REN-001 | Configurable flat floor and curved bowl with selected virtual view | build_mapping | mesh visibility/body mask checks |
| REN-002 | GLES3/EGL sampling and blending backend | opengl.cpp | gpu_tests, Mesa CI; physical board pending |
| REN-003 | CUDA sampling and blending backend | cuda.cu | parity test supplied; NVIDIA build/device pending |
| REN-004 | CPU baseline and same image orientation across backends | render_cpu, GPU backends | parity threshold two 8-bit levels |
| IO-001 | Input dimensions agree with configured lens model | validate_inputs | core_tests |
| EMB-001 | Qualcomm Linux capture and direct display | platform.hpp contract | not implemented |
| EMB-002 | Bounded live synchronization and buffer lifetimes | platform.hpp contract | not implemented |
| CAL-001 | Targetless driving calibration with confidence and rollback | calibration.hpp gate, embedded.md | estimator not implemented |

Config/runtime tunables do not include compile-time backend dependencies or fixed defensive allocation ceilings. Runtime device capability checks remain mandatory. Requirements for live timing, diagnostics and calibration acceptance need target-specific evidence before assigning numerical limits.
