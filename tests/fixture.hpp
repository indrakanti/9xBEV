#pragma once
#include "bev/core.hpp"
#include <cmath>
#include <stdexcept>
inline void expect(bool condition, const char *text) {
    if (!condition)
        throw std::runtime_error(text);
}
inline bev::Rig fixture() {
    bev::Rig r;
    r.width = 80;
    r.height = 60;
    r.rings = 12;
    r.sectors = 32;
    for (int i = 0; i < 4; ++i) {
        double yaw = i * 1.5707963267948966, down = .35, cy = std::cos(yaw), sy = std::sin(yaw),
               cd = std::cos(down), sd = std::sin(down);
        bev::Camera c;
        c.id = std::to_string(i);
        c.width = 32 + i * 2;
        c.height = 24 + i * 2;
        c.position = {2 * cy, sy, .8};
        c.rotation = {sy, -cy * sd, cy * cd, -cy, -sy * sd, sy * cd, 0, -cd, -sd};
        c.intrinsics = {8, 8, (c.width - 1) / 2.0, (c.height - 1) / 2.0};
        c.max_theta = 1.65;
        r.cameras.push_back(c);
    }
    return r;
}
inline std::vector<bev::Image> images(const bev::Rig &r) {
    std::vector<bev::Image> out;
    for (const auto &c : r.cameras) {
        bev::Image im{c.width, c.height, {}};
        for (int y = 0; y < c.height; ++y)
            for (int x = 0; x < c.width; ++x) {
                im.rgb.push_back((x * 7 + y * 3) % 256);
                im.rgb.push_back((x * 2 + y * 9) % 256);
                im.rgb.push_back((x * 11 + y * 5) % 256);
            }
        out.push_back(im);
    }
    return out;
}
