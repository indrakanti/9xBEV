#include "bev/core.hpp"
#include <cctype>
#include <fstream>
#include <stdexcept>
namespace bev {
static std::string token(std::istream &in) {
    std::string s;
    char c;
    while (in.get(c)) {
        if (c == '#') {
            std::string line;
            std::getline(in, line);
            continue;
        }
        if (!std::isspace(static_cast<unsigned char>(c))) {
            s += c;
            break;
        }
    }
    while (in.get(c)) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (c == '\r' && in.peek() == '\n')
                in.get();
            break;
        }
        s += c;
    }
    if (s.empty())
        throw std::runtime_error("Truncated PPM header");
    return s;
}
Image read_ppm(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot open image: " + path);
    if (token(in) != "P6")
        throw std::runtime_error("Only binary RGB P6 supported");
    Image im;
    im.width = std::stoi(token(in));
    im.height = std::stoi(token(in));
    if (im.width <= 0 || im.height <= 0 || im.width > 4096 || im.height > 4096 ||
        token(in) != "255")
        throw std::runtime_error("Invalid PPM dimensions or maxval");
    im.rgb.resize(std::size_t(im.width) * im.height * 3);
    in.read(reinterpret_cast<char *>(im.rgb.data()), std::streamsize(im.rgb.size()));
    if (std::size_t(in.gcount()) != im.rgb.size())
        throw std::runtime_error("Truncated PPM pixels");
    return im;
}
void write_ppm(const std::string &path, const Image &im) {
    if (im.width <= 0 || im.height <= 0 || im.rgb.size() != std::size_t(im.width) * im.height * 3)
        throw std::invalid_argument("Invalid output image");
    std::ofstream out(path, std::ios::binary);
    if (!out)
        throw std::runtime_error("Cannot create output: " + path);
    out << "P6\n" << im.width << " " << im.height << "\n255\n";
    out.write(reinterpret_cast<const char *>(im.rgb.data()), std::streamsize(im.rgb.size()));
    if (!out)
        throw std::runtime_error("Output write failed");
}
std::vector<Image> load_images(const Rig &rig) {
    std::vector<Image> images;
    for (const auto &c : rig.cameras)
        images.push_back(read_ppm(c.input));
    return images;
}
} // namespace bev
