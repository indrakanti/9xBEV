# Embedded surround-view architecture

First integration target: Qualcomm + Linux. Secondary target: TI TDA4 + Linux. CUDA is supported as separate NVIDIA-only source; it cannot execute on Qualcomm Adreno or TI PowerVR GPUs.

## Current implementation

C++17 geometry generates a tessellated bowl, rasterizes the selected orthographic virtual viewpoint once on CPU, then projects visible surface samples into the calibrated fisheye cameras. That produces a camera-major projection lookup table. GLES3 fragment rendering and CUDA compute rendering consume the same mapping and perform image sampling and weighted stitching on the GPU. CPU rendering provides an independent implementation of the pixel sampling/blending path. Viewpoint or calibration changes require rebuilding the mapping. Mapping construction is a prototype/reference approach, not yet optimized for rapid interactive viewpoints on an embedded CPU.

GLES3/EGL is the embedded OpenGL path; desktop OpenGL, CUDA/OpenGL interop and GPU mesh-based projection are not part of PR1. Current input is P6 RGB images, uploaded to a texture array, with a GPU framebuffer readback to PPM. This intentionally permits reproducible offscreen validation. No actual board performance claim is made. The sample includes four cameras; code supports 1–16 and different resolutions with a padded atlas. The dense LUT and texture atlas each have a fixed 512 MiB allocation ceiling; final in-vehicle profiles must use substantially tighter measured budgets.

## Vehicle runtime to implement

Capture/ISP → bounded synchronized FrameSet → camera-buffer import → GPU stitch/render → output surface → display. A separate lower-priority calibration worker consumes selected frames and ego motion. It returns candidates, not mutable renderer state.

`include/bev/platform.hpp` defines DMA-BUF plane metadata, clock domain, exposure timestamp, sequence, acquire fence and producer allocation lifetime. It is a contract only. Imported camera buffers must stay alive until the GPU release fence completes. Producer ownership, FD ownership and display ownership will be finalized against the actual Qualcomm camera API. NV12 handling must specify color matrix/range and chroma siting; neither is implemented yet.

Linux adapters are expected to use the selected Qualcomm camera SDK or supported V4L2 capture, EGLImage DMA-BUF import where supported, native fences, and DRM/KMS or the vehicle compositor. All these capabilities need verification against the exact SoC, BSP/SDK version, display stack and camera driver. Do not assume the camera allocations are directly GPU-importable.

TI adapter must use the selected PSDK capture/ISP/buffer path (possibly TIOVX/OpenVX) and supported EGLImage sharing. The same capture interface may need a vendor buffer descriptor on systems that do not expose DMA-BUF. QNX is a separate future port.

## Calibration

Intrinsics start from measured module calibration. Nominal mounts come from CAD or installation measurement. Online extrinsics require static feature tracks, ego-motion alignment, ground constraints, motion excitation and independent holdout validation. Metric scale must be anchored by measured geometry/calibrated odometry. `calibration.hpp` is only a gate policy; there is no online estimator, calibration activation, persistence or validated numerical thresholds yet.

States: UNINITIALIZED, COLLECTING, CANDIDATE, VALIDATING, ACCEPTED, DEGRADED. Poor observability does not permit automatic parameter updates. Keep last accepted calibration, activate at frame boundaries, and retain rollback version. Estimate body/ground motion separately from mounting drift.

## Fault policy to implement

Detect camera loss, stale or mismatched timestamps, sequence gaps, blocked images, GPU timeout, display deadline miss and calibration drift. Mark unavailable regions explicitly; never present a stale frozen image as live. Set target FPS, latency, maximum timestamp skew and recovery limits only after target hardware measurement. Functional safety classification and evidence depend on intended vehicle function.

## Vendor references

- Qualcomm Linux Graphics Guide: https://docs.qualcomm.com/bundle/publicresource/topics/80-80020-19 (graphics API support; exact automotive BSP must be checked).
- TI TDA4VM Linux graphics documentation: https://software-dl.ti.com/jacinto7/esd/processor-sdk-linux-sk-tda4vm/09_01_00/exports/docs/linux/Foundational_Components/Graphics/Graphics_and_Display.html (EGL/OpenGL ES libraries in that SDK).
- Khronos OpenGL ES specifications: https://registry.khronos.org/OpenGL/index_es.php
- NVIDIA CUDA Programming Guide: https://docs.nvidia.com/cuda/cuda-programming-guide/
