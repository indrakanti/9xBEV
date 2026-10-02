#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace bev {
struct Vec3 {
    double x = 0, y = 0, z = 0;
};
struct Camera {
    std::string id, input;
    int width = 0, height = 0;
    Vec3 position;
    std::array<double, 9> rotation{}; // row-major R_vehicle_camera
    std::array<double, 4> intrinsics{}, distortion{};
    double max_theta = 0, gain = 1;
};
struct Rig {
    int width = 640, height = 480, rings = 40, sectors = 96;
    double length = 4.6, vehicle_width = 1.85, flat_radius = 4, outer_radius = 9, bowl_height = 3;
    double yaw = 0, tilt = 0.7853981633974483, view_extent = 11, weight_power = 2,
           min_weight = 1e-6;
    std::array<float, 3> background{0.025f, 0.035f, 0.05f}, body_color{0.12f, 0.17f, 0.24f};
    std::string backend = "cpu", output = "surround.ppm";
    int cuda_device = 0, cuda_block_size = 128;
    std::vector<Camera> cameras;
};
struct Projection {
    float u = 0, v = 0, weight = 0, gain = 1;
};
struct Image {
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgb;
};
struct Mapping {
    int width = 0, height = 0, cameras = 0;
    std::vector<Projection> samples; // camera-major, top-left rows
    std::vector<std::uint8_t> mask;  // 0 unseen, 1 bowl, 2 vehicle
};
void validate(const Rig &);
Rig load_yaml(const std::string &path);
Projection project(Vec3 point, const Camera &, double weight_power);
Mapping build_mapping(const Rig &);
Image read_ppm(const std::string &);
void write_ppm(const std::string &, const Image &);
std::vector<Image> load_images(const Rig &);
Image render_cpu(const Rig &, const Mapping &, const std::vector<Image> &);
Image render_opengl(const Rig &, const Mapping &, const std::vector<Image> &);
Image render_cuda(const Rig &, const Mapping &, const std::vector<Image> &);
void validate_inputs(const Rig &, const Mapping &, const std::vector<Image> &);
} // namespace bev
