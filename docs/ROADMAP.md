# Delivery roadmap

1. **PR1 — native configurable renderer:** C++ geometry, GLES/EGL and CUDA implementations, YAML rig, CPU baseline and parity tests. CUDA build and device validation remains outstanding.
2. **Recorded replay:** synchronized frame timestamps, NV12 conversion, bounded buffer pools and a persistent renderer lifecycle to avoid per-frame allocation. Benchmark initialization separately from frame rendering.
3. **Qualcomm Linux integration:** selected SoC/BSP toolchain, capture adapter, buffer import/fences, actual display presentation, latency/bandwidth profiling and sustained camera-loss/recovery tests.
4. **Image quality:** valid-region masks, color/exposure normalization, stable overlap seams, vehicle CAD mask and steering overlays.
5. **Online calibration:** feature tracking and static-scene selection, motion/ground constraints, uncertainty/observability, independent validation, atomic version activation and rollback.
6. **TDA4 integration:** vendor buffer/capture/display adapters and physical-board validation.
7. **Vehicle qualification:** target-specific functional/timing/resource requirements, environmental recordings, fault injection and intended-use safety evidence.

Before PR3: exact Qualcomm SoC/board, BSP/SDK, camera sensor/serializer/deserializer, camera count/resolution/FPS, timestamp source, available wheel/IMU signals, display composition path, target output size and latency.
