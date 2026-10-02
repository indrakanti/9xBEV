#include "bev/core.hpp"
#include <exception>
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: bev_render CONFIG.yaml\n";
        return 2;
    }
    try {
        auto rig = bev::load_yaml(argv[1]);
        auto images = bev::load_images(rig);
        auto mapping = bev::build_mapping(rig);
        bev::Image image;
        if (rig.backend == "cpu")
            image = bev::render_cpu(rig, mapping, images);
        else if (rig.backend == "opengl") {
#ifdef BEV_HAS_OPENGL
            image = bev::render_opengl(rig, mapping, images);
#else
            throw std::runtime_error("OpenGL backend not compiled; enable BEV_OPENGL");
#endif
        } else {
#ifdef BEV_HAS_CUDA
            image = bev::render_cuda(rig, mapping, images);
#else
            throw std::runtime_error("CUDA backend not compiled; enable BEV_CUDA on NVIDIA target");
#endif
        }
        bev::write_ppm(rig.output, image);
        std::cout << "Rendered " << rig.width << "x" << rig.height << " using " << rig.backend
                  << " to " << rig.output << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "9xBEV: " << e.what() << "\n";
        return 1;
    }
}
