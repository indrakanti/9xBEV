# 9xBEV

Configurable native surround-view stack for embedded vehicles. First hardware target: **Qualcomm + Linux**, with portable **OpenGL ES 3 / EGL** rendering. TI TDA4 is a secondary integration target. An optional **CUDA** backend targets NVIDIA platforms only.

This first milestone renders calibrated recorded RGB image sets onto a virtual bowl. It includes a shared C++17 geometry core, CPU reference, actual GLES shader and CUDA kernel, strict YAML configuration, and tests. Live vehicle capture, zero-copy buffers, display presentation and targetless online calibration are subsequent milestones.

## Build and run on Linux

Install CMake, a C++17 compiler, yaml-cpp, EGL and GLES3 development libraries from your BSP/sysroot. On Ubuntu:

```sh
sudo apt-get install cmake libyaml-cpp-dev libegl1-mesa-dev libgles2-mesa-dev
cmake -S . -B build -DBEV_OPENGL=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
python3 tools/generate_demo.py
build/bev_render config/qualcomm_linux.yaml
ctest --test-dir build --output-on-failure
```

The YAML selects the backend and produces `output.ppm`. On headless Mesa use `EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1`; these settings are for workstation testing, not vehicle GPU deployment. The EGL default-display/pbuffer context path may need replacement by the board's compositor/display adapter.

For CPU-only rendering set `renderer.backend: cpu` and omit `BEV_OPENGL`. For dependency-free geometry tests configure `-DBEV_APP=OFF`.

For NVIDIA install CUDA Toolkit and configure `-DBEV_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=<target SM number>`; set YAML backend to `cuda`. CUDA tests require an NVIDIA GPU. CUDA does not run on Qualcomm or TI GPUs.

For Qualcomm AArch64 cross-compilation supply the BSP CMake toolchain and target sysroot with vendor EGL/GLES3 and yaml-cpp. No vendor SDK or generic cross-toolchain is bundled. Do not link host Mesa into a board build.

## Configuration and design

- [YAML fields and coordinate conventions](docs/design/configuration.md)
- [Embedded design and platform contracts](docs/design/embedded.md)
- [Requirements and traceability](docs/requirements/rendering.md)
- [Complete synthetic rig](config/qualcomm_linux.yaml)

The synthetic demo textures are test patterns, not a scene. Real input requires measured lens calibration and mounting geometry. Source images may have different configured resolutions. Viewpoint is orthographic; mapping regenerates on calibration/view changes.

## Validation and limits

CPU core and configuration tests plus GLES/CPU parity run in CI. CUDA has the same parity test target but needs NVIDIA hardware; no CUDA CI runner is configured. Physical Qualcomm/TDA4 deployment, latency and bandwidth are unverified. Current GPU paths copy RGB inputs and read output back for testability; they are not the final zero-copy live vehicle path. Source has no assigned distribution license; choose one before external redistribution.
