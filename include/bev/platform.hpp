#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace bev {
// Integration contracts; no vendor capture or display implementation in PR1.
enum class PixelFormat { NV12, RGB888 };
struct DmaPlane {
    int fd = -1;
    std::uint32_t offset = 0, stride = 0;
    std::uint64_t modifier = 0;
};
struct Frame {
    std::string camera_id, clock_domain;
    std::uint64_t exposure_timestamp_ns = 0, sequence = 0;
    std::uint32_t width = 0, height = 0;
    PixelFormat format = PixelFormat::NV12;
    std::vector<DmaPlane> planes;
    int acquire_fence_fd = -1;
    // Retains producer allocation until GPU release fence completes.
    std::shared_ptr<void> allocation_owner;
};
struct FrameSet {
    std::vector<Frame> frames;
    std::uint64_t calibration_version = 0;
};
class Capture {
  public:
    virtual ~Capture() = default;
    virtual FrameSet acquire() = 0;
    virtual void release(FrameSet, int release_fence_fd) = 0;
};
class Display {
  public:
    virtual ~Display() = default;
    virtual void present(const Frame &output, int acquire_fence_fd) = 0;
};
} // namespace bev
