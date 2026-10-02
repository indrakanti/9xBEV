#include "bev/core.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
namespace bev {
static void require(bool b, const char *text) {
    if (!b)
        throw std::invalid_argument(text);
}
void validate(const Rig &r) {
    require(r.width > 0 && r.height > 0 && r.width <= 4096 && r.height <= 4096,
            "Output dimensions outside 1..4096");
    require(!r.cameras.empty() && r.cameras.size() <= 16, "Camera count outside 1..16");
    require(r.rings >= 2 && r.rings <= 256 && r.sectors >= 8 && r.sectors <= 512,
            "Invalid mesh resolution");
    for (double x : {r.length, r.vehicle_width, r.flat_radius, r.outer_radius, r.bowl_height, r.yaw,
                     r.tilt, r.view_extent, r.weight_power, r.min_weight})
        require(std::isfinite(x), "Nonfinite rig parameter");
    require(r.length > 0 && r.vehicle_width > 0 &&
                r.flat_radius > std::hypot(r.length / 2, r.vehicle_width / 2) &&
                r.outer_radius > r.flat_radius && r.bowl_height >= 0,
            "Invalid vehicle or bowl geometry");
    require(r.outer_radius <= 1000 && r.bowl_height <= 1000 && r.view_extent >= 0.001 &&
                r.view_extent <= 1000 && r.weight_power > 0 && r.weight_power <= 100 &&
                r.min_weight > 0 && r.min_weight <= 1 && r.tilt >= 0 && r.tilt < 1.5707963267948966,
            "Invalid view or blend parameters");
    require(r.backend == "cpu" || r.backend == "opengl" || r.backend == "cuda", "Unknown backend");
    require(r.cuda_device >= 0 && r.cuda_block_size >= 32 && r.cuda_block_size <= 1024 &&
                r.cuda_block_size % 32 == 0,
            "Invalid CUDA settings");
    for (auto color : {r.background, r.body_color})
        for (float x : color)
            require(std::isfinite(x) && x >= 0 && x <= 1, "Colors must be in [0,1]");
    // Bound mapping allocation to 512 MiB; limit applies before allocation.
    require(std::size_t(r.width) * r.height * r.cameras.size() * sizeof(Projection) <=
                512ULL * 1024 * 1024,
            "Mapping exceeds 512 MiB resource ceiling");
    std::set<std::string> ids;
    for (const auto &c : r.cameras) {
        require(!c.id.empty() && ids.insert(c.id).second, "Empty or duplicate camera id");
        require(c.width > 0 && c.height > 0 && c.width <= 4096 && c.height <= 4096,
                "Invalid camera dimensions");
        for (double x : {c.position.x, c.position.y, c.position.z, c.max_theta, c.gain})
            require(std::isfinite(x), "Nonfinite camera parameter");
        require(c.max_theta > 0 && c.max_theta < 3.141592653589793 && c.gain > 0 &&
                    c.gain <= 1000 &&
                    std::max({std::abs(c.position.x), std::abs(c.position.y),
                              std::abs(c.position.z)}) <= 1000,
                "Invalid lens angle, gain or position");
        for (double x : c.rotation)
            require(std::isfinite(x), "Nonfinite rotation");
        for (double x : c.intrinsics)
            require(std::isfinite(x), "Nonfinite intrinsics");
        for (double x : c.distortion)
            require(std::isfinite(x), "Nonfinite distortion");
        require(c.intrinsics[0] > 0 && c.intrinsics[1] > 0, "Focal lengths must be positive");
        auto &m = c.rotation;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                double dot = 0;
                for (int k = 0; k < 3; ++k)
                    dot += m[k * 3 + i] * m[k * 3 + j];
                require(std::abs(dot - (i == j ? 1 : 0)) < 1e-6, "Rotation not orthonormal");
            }
        double det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) +
                     m[2] * (m[3] * m[7] - m[4] * m[6]);
        require(std::abs(det - 1) < 1e-6, "Rotation must be right-handed");
        // Sample derivative over supported domain: rejects common nonmonotonic fits.
        for (int i = 0; i <= 1024; ++i) {
            double t = c.max_theta * i / 1024, q = t * t;
            auto k = c.distortion;
            double derivative = 1 + q * (3 * k[0] + q * (5 * k[1] + q * (7 * k[2] + q * 9 * k[3])));
            require(std::isfinite(derivative) && derivative > 1e-6,
                    "Distortion not monotonic on sampled domain");
        }
    }
}
Projection project(Vec3 p, const Camera &c, double exponent) {
    double a[3] = {p.x - c.position.x, p.y - c.position.y, p.z - c.position.z}, v[3] = {};
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i)
            v[j] += c.rotation[i * 3 + j] * a[i];
    double rho = std::hypot(v[0], v[1]), theta = std::atan2(rho, v[2]);
    Projection out;
    out.gain = float(c.gain);
    if (!std::isfinite(theta) || std::hypot(rho, v[2]) < 1e-9 || theta > c.max_theta)
        return out;
    double q = theta * theta;
    auto k = c.distortion;
    double td = theta * (1 + q * (k[0] + q * (k[1] + q * (k[2] + q * k[3]))));
    double scale = rho > 1e-12 ? td / rho : 0;
    double u = c.intrinsics[0] * v[0] * scale + c.intrinsics[2],
           w = c.intrinsics[1] * v[1] * scale + c.intrinsics[3];
    if (!std::isfinite(u) || !std::isfinite(w) || u < 0 || w < 0 || u > c.width - 1 ||
        w > c.height - 1)
        return out;
    out.u = float(u);
    out.v = float(w);
    out.weight = float(std::pow(std::max(0.0, 1 - theta / c.max_theta), exponent));
    return out;
}
struct Vertex {
    Vec3 p;
    double x, y, depth;
};
Mapping build_mapping(const Rig &r) {
    validate(r);
    Mapping out;
    out.width = r.width;
    out.height = r.height;
    out.cameras = int(r.cameras.size());
    std::size_t n = std::size_t(r.width) * r.height;
    out.mask.resize(n);
    out.samples.resize(n * r.cameras.size());
    std::vector<double> depths(n, std::numeric_limits<double>::infinity());
    std::vector<Vec3> points(n);
    std::vector<Vertex> vertices;
    double cy = std::cos(r.yaw), sy = std::sin(r.yaw), ct = std::cos(r.tilt), st = std::sin(r.tilt),
           aspect = double(r.width) / r.height;
    for (int i = 0; i <= r.rings; ++i)
        for (int j = 0; j <= r.sectors; ++j) {
            double radius = r.outer_radius * i / r.rings, a = 6.283185307179586 * j / r.sectors,
                   q = std::max(0.0, (radius - r.flat_radius) / (r.outer_radius - r.flat_radius));
            Vec3 p{radius * std::cos(a), radius * std::sin(a), r.bowl_height * q * q};
            double xx = (-sy * p.x + cy * p.y) / (r.view_extent * aspect),
                   yy = (-cy * ct * p.x - sy * ct * p.y + st * p.z) / r.view_extent;
            vertices.push_back({p, (xx + 1) * r.width / 2, (1 - yy) * r.height / 2,
                                -cy * st * p.x - sy * st * p.y - ct * p.z});
        }
    auto raster = [&](const Vertex &a, const Vertex &b, const Vertex &c) {
        double den = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
        if (std::abs(den) < 1e-10)
            return;
        int x0 = std::max(0, int(std::floor(std::min({a.x, b.x, c.x})))),
            x1 = std::min(r.width - 1, int(std::ceil(std::max({a.x, b.x, c.x}))));
        int y0 = std::max(0, int(std::floor(std::min({a.y, b.y, c.y})))),
            y1 = std::min(r.height - 1, int(std::ceil(std::max({a.y, b.y, c.y}))));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                double px = x + .5, py = y + .5;
                double u = ((b.y - c.y) * (px - c.x) + (c.x - b.x) * (py - c.y)) / den,
                       v = ((c.y - a.y) * (px - c.x) + (a.x - c.x) * (py - c.y)) / den,
                       w = 1 - u - v;
                if (std::min({u, v, w}) < -1e-9)
                    continue;
                std::size_t idx = std::size_t(y) * r.width + x;
                double d = u * a.depth + v * b.depth + w * c.depth;
                if (d >= depths[idx])
                    continue;
                depths[idx] = d;
                points[idx] = {u * a.p.x + v * b.p.x + w * c.p.x, u * a.p.y + v * b.p.y + w * c.p.y,
                               u * a.p.z + v * b.p.z + w * c.p.z};
                out.mask[idx] = 1;
            }
    };
    for (int i = 0; i < r.rings; ++i)
        for (int j = 0; j < r.sectors; ++j) {
            int k = i * (r.sectors + 1) + j;
            raster(vertices[k], vertices[k + 1], vertices[k + r.sectors + 1]);
            raster(vertices[k + 1], vertices[k + r.sectors + 2], vertices[k + r.sectors + 1]);
        }
    for (std::size_t i = 0; i < n; ++i)
        if (out.mask[i]) {
            if (std::abs(points[i].x) <= r.length / 2 &&
                std::abs(points[i].y) <= r.vehicle_width / 2) {
                out.mask[i] = 2;
                continue;
            }
            for (std::size_t c = 0; c < r.cameras.size(); ++c)
                out.samples[c * n + i] = project(points[i], r.cameras[c], r.weight_power);
        }
    return out;
}
void validate_inputs(const Rig &r, const Mapping &m, const std::vector<Image> &images) {
    validate(r);
    std::size_t n = std::size_t(r.width) * r.height;
    require(m.width == r.width && m.height == r.height && m.cameras == int(r.cameras.size()) &&
                m.mask.size() == n && m.samples.size() == n * r.cameras.size(),
            "Mapping dimensions mismatch");
    require(images.size() == r.cameras.size(), "Camera image count mismatch");
    for (std::size_t c = 0; c < images.size(); ++c)
        require(images[c].width == r.cameras[c].width && images[c].height == r.cameras[c].height &&
                    images[c].rgb.size() == std::size_t(images[c].width) * images[c].height * 3,
                "Camera image dimensions mismatch");
}
Image render_cpu(const Rig &r, const Mapping &m, const std::vector<Image> &images) {
    validate_inputs(r, m, images);
    Image out{r.width, r.height, {}};
    std::size_t n = m.mask.size();
    out.rgb.resize(n * 3);
    for (std::size_t i = 0; i < n; ++i) {
        float color[3] = {}, sum = 0;
        if (m.mask[i] == 1)
            for (std::size_t c = 0; c < images.size(); ++c) {
                const auto &s = m.samples[c * n + i];
                if (s.weight <= 0)
                    continue;
                auto &im = images[c];
                int x = int(s.u), y = int(s.v), x1 = std::min(x + 1, im.width - 1),
                    y1 = std::min(y + 1, im.height - 1);
                float u = s.u - x, v = s.v - y;
                for (int k = 0; k < 3; ++k) {
                    auto at = [&](int xx, int yy) {
                        return float(im.rgb[(std::size_t(yy) * im.width + xx) * 3 + k]) / 255;
                    };
                    float value = (1 - v) * ((1 - u) * at(x, y) + u * at(x1, y)) +
                                  v * ((1 - u) * at(x, y1) + u * at(x1, y1));
                    color[k] += s.weight * s.gain * value;
                }
                sum += s.weight;
            }
        for (int k = 0; k < 3; ++k) {
            float v = m.mask[i] == 2 ? r.body_color[k]
                                     : (sum >= r.min_weight ? color[k] / sum : r.background[k]);
            out.rgb[i * 3 + k] = std::uint8_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255));
        }
    }
    return out;
}
} // namespace bev
