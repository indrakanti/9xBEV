#include "fixture.hpp"
#include <iostream>
int main() {
    try {
        for (int iteration = 0; iteration < 3; ++iteration) {
            auto r = fixture();
            if (iteration == 1) {
                r.tilt = 0;
                r.cameras.resize(1);
            }
            if (iteration == 2) {
                r.yaw = .7;
                r.tilt = 1.2;
                r.width = 101;
                r.height = 73;
                for (auto &c : r.cameras)
                    c.gain = 1.4;
            }
            auto m = bev::build_mapping(r);
            auto im = images(r);
            auto reference = bev::render_cpu(r, m, im);
#ifdef TEST_CUDA
            auto gpu = bev::render_cuda(r, m, im);
#else
            auto gpu = bev::render_opengl(r, m, im);
#endif
            expect(reference.rgb.size() == gpu.rgb.size(), "Output length mismatch");
            int maximum = 0;
            double sum = 0;
            for (std::size_t i = 0; i < gpu.rgb.size(); ++i) {
                int d = std::abs(int(gpu.rgb[i]) - int(reference.rgb[i]));
                maximum = std::max(maximum, d);
                sum += d;
            }
            std::cout << "GPU/CPU parity: max channel difference=" << maximum
                      << ", mean=" << sum / gpu.rgb.size() << '\n';
            expect(maximum <= 2, "GPU parity exceeds two 8-bit levels");
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
