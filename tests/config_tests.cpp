#include "bev/core.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
int main(int argc, char **argv) {
    try {
        if (argc != 2)
            return 2;
        auto r = bev::load_yaml(argv[1]);
        if (r.cameras.size() != 4 || r.backend != "opengl")
            return 1;
        std::ifstream in(argv[1]);
        std::stringstream buffer;
        buffer << in.rdbuf();
        auto base = buffer.str();
        auto tmp = std::filesystem::temp_directory_path() / "9xbev-bad-config.yaml";
        auto reject = [&](const std::string &text) {
            std::ofstream(tmp) << text;
            bool failed = false;
            try {
                bev::load_yaml(tmp.string());
            } catch (const std::exception &) {
                failed = true;
            }
            std::filesystem::remove(tmp);
            if (!failed)
                throw std::runtime_error("Invalid YAML accepted");
        };
        reject(base + "\nunrecognized: true\n");
        reject(base + "\nschema_version: 1\n");
        auto invalid = base;
        auto at = invalid.find("schema_version: 1");
        invalid.replace(at, 17, "schema_version: 99");
        reject(invalid);
        std::cout << "Valid YAML, unknown keys, duplicate keys, unsupported version checked\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
