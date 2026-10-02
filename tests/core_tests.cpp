#include "bev/calibration.hpp"
#include "fixture.hpp"
#include <filesystem>
#include <iostream>
#include <limits>
int main() {
    try {
        bev::CalibrationLimits limits{2, .05, 5, 3};
        expect(bev::accept_calibration({true, true, 1, .02, 2, 3}, limits),
               "Calibration acceptance");
        expect(!bev::accept_calibration({false, true, 1, .02, 2, 3}, limits),
               "Unobservable calibration rejected");
        expect(!bev::accept_calibration({true, false, 1, .02, 2, 3}, limits),
               "Unvalidated calibration rejected");
        auto r = fixture();
        bev::validate(r);
        const auto &c = r.cameras[0];
        auto R = c.rotation;
        auto p = bev::project(
            {c.position.x + R[2] * 3, c.position.y + R[5] * 3, c.position.z + R[8] * 3}, c, 2);
        expect(std::abs(p.u - c.intrinsics[2]) < 1e-5 && std::abs(p.v - c.intrinsics[3]) < 1e-5 &&
                   p.weight > .99,
               "Optical axis projection");
        p = bev::project({c.position.x + R[0] + R[2] * 3, c.position.y + R[3] + R[5] * 3,
                          c.position.z + R[6] + R[8] * 3},
                         c, 2);
        expect(p.u > c.intrinsics[2] && p.weight > 0, "Optical right projection");
        expect(bev::project(c.position, c, 2).weight == 0, "Origin rejection");
        expect(bev::project({c.position.x - R[2], c.position.y - R[5], c.position.z - R[8]}, c, 2)
                       .weight == 0,
               "Behind rejection");
        auto reject = [](bev::Rig bad) {
            bool threw = false;
            try {
                bev::validate(bad);
            } catch (const std::invalid_argument &) {
                threw = true;
            }
            expect(threw, "Invalid rig accepted");
        };
        auto bad = r;
        bad.cameras[0].rotation[0] = 2;
        reject(bad);
        bad = r;
        bad.cameras[0].distortion[0] = -10;
        reject(bad);
        bad = r;
        bad.cameras[1].id = bad.cameras[0].id;
        reject(bad);
        bad = r;
        bad.tilt = std::numeric_limits<double>::quiet_NaN();
        reject(bad);
        bad = r;
        bad.width = 4096;
        bad.height = 4096;
        reject(bad);
        auto m = bev::build_mapping(r);
        auto im = images(r);
        auto a = bev::render_cpu(r, m, im);
        expect(a.rgb.size() == 80 * 60 * 3, "Output dimensions");
        int body = 0, visible = 0;
        for (auto mask : m.mask) {
            body += mask == 2;
            visible += mask == 1;
        }
        expect(body > 0 && visible > 0, "Bowl and body coverage");
        auto temp = std::filesystem::temp_directory_path() / "9xbev-core-roundtrip.ppm";
        bev::write_ppm(temp.string(), a);
        auto b = bev::read_ppm(temp.string());
        std::filesystem::remove(temp);
        expect(a.rgb == b.rgb, "PPM roundtrip");
        auto incorrect = im;
        incorrect[0].rgb.pop_back();
        bool threw = false;
        try {
            bev::render_cpu(r, m, incorrect);
        } catch (const std::invalid_argument &) {
            threw = true;
        }
        expect(threw, "Wrong input rejected");
        std::cout << "Core geometry, configuration bounds, raster coverage, input validation, "
                     "image roundtrip passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
