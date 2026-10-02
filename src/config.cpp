#include "bev/core.hpp"
#include <filesystem>
#include <set>
#include <stdexcept>
#include <yaml-cpp/yaml.h>
namespace bev {
static void keys(const YAML::Node &node, std::initializer_list<const char *> allowed) {
    if (!node.IsMap())
        throw std::invalid_argument("Expected YAML map");
    std::set<std::string> seen;
    for (auto item : node) {
        auto key = item.first.as<std::string>();
        bool found = false;
        for (auto a : allowed)
            if (key == a)
                found = true;
        if (!found || !seen.insert(key).second)
            throw std::invalid_argument("Unknown or duplicate YAML key: " + key);
    }
}
template <class T, std::size_t N> static std::array<T, N> array(const YAML::Node &n) {
    if (!n.IsSequence() || n.size() != N)
        throw std::invalid_argument("Wrong YAML vector length");
    std::array<T, N> a{};
    for (std::size_t i = 0; i < N; ++i)
        a[i] = n[i].as<T>();
    return a;
}
Rig load_yaml(const std::string &path) {
    auto y = YAML::LoadFile(path);
    keys(y, {"schema_version", "renderer", "vehicle", "bowl", "view", "blend", "cameras"});
    if (y["schema_version"].as<int>() != 1)
        throw std::invalid_argument("Unsupported schema version");
    Rig r;
    auto n = y["renderer"];
    keys(n, {"backend", "output", "width", "height", "cuda_device", "cuda_block_size"});
    r.backend = n["backend"].as<std::string>();
    r.output = n["output"].as<std::string>();
    r.width = n["width"].as<int>();
    r.height = n["height"].as<int>();
    r.cuda_device = n["cuda_device"].as<int>();
    r.cuda_block_size = n["cuda_block_size"].as<int>();
    n = y["vehicle"];
    keys(n, {"length_m", "width_m", "color"});
    r.length = n["length_m"].as<double>();
    r.vehicle_width = n["width_m"].as<double>();
    r.body_color = array<float, 3>(n["color"]);
    n = y["bowl"];
    keys(n, {"flat_radius_m", "outer_radius_m", "height_m", "rings", "sectors"});
    r.flat_radius = n["flat_radius_m"].as<double>();
    r.outer_radius = n["outer_radius_m"].as<double>();
    r.bowl_height = n["height_m"].as<double>();
    r.rings = n["rings"].as<int>();
    r.sectors = n["sectors"].as<int>();
    n = y["view"];
    keys(n, {"yaw_rad", "tilt_rad", "extent_m"});
    r.yaw = n["yaw_rad"].as<double>();
    r.tilt = n["tilt_rad"].as<double>();
    r.view_extent = n["extent_m"].as<double>();
    n = y["blend"];
    keys(n, {"weight_power", "min_weight", "background"});
    r.weight_power = n["weight_power"].as<double>();
    r.min_weight = n["min_weight"].as<double>();
    r.background = array<float, 3>(n["background"]);
    auto base = std::filesystem::absolute(path).parent_path();
    auto resolve = [&](const std::string &p) {
        if (p.empty())
            throw std::invalid_argument("Empty input/output path");
        return (base / std::filesystem::path(p)).lexically_normal().string();
    };
    r.output = resolve(r.output);
    if (!y["cameras"].IsSequence())
        throw std::invalid_argument("cameras must be sequence");
    for (auto c : y["cameras"]) {
        keys(c, {"id", "input", "resolution", "position_m", "R_vehicle_camera", "intrinsics",
                 "distortion", "max_theta_rad", "gain"});
        Camera cam;
        cam.id = c["id"].as<std::string>();
        cam.input = resolve(c["input"].as<std::string>());
        auto size = array<int, 2>(c["resolution"]);
        cam.width = size[0];
        cam.height = size[1];
        auto p = array<double, 3>(c["position_m"]);
        cam.position = {p[0], p[1], p[2]};
        cam.rotation = array<double, 9>(c["R_vehicle_camera"]);
        cam.intrinsics = array<double, 4>(c["intrinsics"]);
        cam.distortion = array<double, 4>(c["distortion"]);
        cam.max_theta = c["max_theta_rad"].as<double>();
        cam.gain = c["gain"].as<double>();
        r.cameras.push_back(cam);
    }
    validate(r);
    return r;
}
} // namespace bev
