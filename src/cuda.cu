#include "bev/core.hpp"
#include <algorithm>
#include <cstring>
#include <cuda_runtime.h>
#include <stdexcept>
namespace bev {
namespace {
void check(cudaError_t e, const char *op) {
    if (e != cudaSuccess)
        throw std::runtime_error(std::string(op) + ": " + cudaGetErrorString(e));
}
template <class T> struct Device {
    T *p = nullptr;
    explicit Device(std::size_t n) {
        check(cudaMalloc(reinterpret_cast<void **>(&p), n * sizeof(T)), "cudaMalloc");
    }
    ~Device() {
        if (p)
            cudaFree(p);
    }
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
};
__device__ float sample(const unsigned char *a, int c, int w, int h, int x, int y, int k) {
    return a[(std::size_t(c) * w * h + std::size_t(y) * w + x) * 3 + k] / 255.0f;
}
__global__ void render(const Projection *map, const unsigned char *mask, const unsigned char *atlas,
                       unsigned char *output, int n, int cameras, int w, int h, float3 bg,
                       float3 body, float min_weight) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n)
        return;
    float sum[3] = {}, total = 0;
    if (mask[i] == 1)
        for (int c = 0; c < cameras; ++c) {
            Projection s = map[std::size_t(c) * n + i];
            if (s.weight <= 0)
                continue;
            int x = int(s.u), y = int(s.v), x1 = min(x + 1, w - 1), y1 = min(y + 1, h - 1);
            float u = s.u - x, v = s.v - y;
            for (int k = 0; k < 3; ++k) {
                float value = (1 - v) * ((1 - u) * sample(atlas, c, w, h, x, y, k) +
                                         u * sample(atlas, c, w, h, x1, y, k)) +
                              v * ((1 - u) * sample(atlas, c, w, h, x, y1, k) +
                                   u * sample(atlas, c, w, h, x1, y1, k));
                sum[k] += value * s.weight * s.gain;
            }
            total += s.weight;
        }
    float b[3] = {bg.x, bg.y, bg.z}, bc[3] = {body.x, body.y, body.z};
    for (int k = 0; k < 3; ++k) {
        float v = mask[i] == 2 ? bc[k] : (total >= min_weight ? sum[k] / total : b[k]);
        output[std::size_t(i) * 3 + k] =
            static_cast<unsigned char>(floorf(fminf(1, fmaxf(0, v)) * 255 + .5f));
    }
}
} // namespace
Image render_cuda(const Rig &r, const Mapping &m, const std::vector<Image> &images) {
    validate_inputs(r, m, images);
    check(cudaSetDevice(r.cuda_device), "cudaSetDevice");
    cudaDeviceProp prop;
    check(cudaGetDeviceProperties(&prop, r.cuda_device), "cudaGetDeviceProperties");
    if (r.cuda_block_size > prop.maxThreadsPerBlock)
        throw std::runtime_error("CUDA block exceeds device limit");
    int w = 0, h = 0;
    for (const auto &im : images) {
        w = std::max(w, im.width);
        h = std::max(h, im.height);
    }
    std::size_t bytes = std::size_t(w) * h * m.cameras * 3;
    if (bytes > 512ULL * 1024 * 1024)
        throw std::runtime_error("Camera atlas exceeds 512 MiB ceiling");
    std::vector<unsigned char> atlas(bytes);
    for (int c = 0; c < m.cameras; ++c)
        for (int y = 0; y < images[c].height; ++y)
            std::memcpy(atlas.data() + (std::size_t(c) * w * h + std::size_t(y) * w) * 3,
                        images[c].rgb.data() + std::size_t(y) * images[c].width * 3,
                        std::size_t(images[c].width) * 3);
    Device<Projection> map(m.samples.size());
    Device<unsigned char> mask(m.mask.size()), input(atlas.size()), output(m.mask.size() * 3);
    check(cudaMemcpy(map.p, m.samples.data(), m.samples.size() * sizeof(Projection),
                     cudaMemcpyHostToDevice),
          "copy mapping");
    check(cudaMemcpy(mask.p, m.mask.data(), m.mask.size(), cudaMemcpyHostToDevice), "copy mask");
    check(cudaMemcpy(input.p, atlas.data(), atlas.size(), cudaMemcpyHostToDevice), "copy images");
    int n = int(m.mask.size()), block = r.cuda_block_size;
    render<<<(n + block - 1) / block, block>>>(
        map.p, mask.p, input.p, output.p, n, m.cameras, w, h,
        make_float3(r.background[0], r.background[1], r.background[2]),
        make_float3(r.body_color[0], r.body_color[1], r.body_color[2]), float(r.min_weight));
    check(cudaGetLastError(), "render launch");
    check(cudaDeviceSynchronize(), "render execution");
    Image out{r.width, r.height, {}};
    out.rgb.resize(std::size_t(n) * 3);
    check(cudaMemcpy(out.rgb.data(), output.p, out.rgb.size(), cudaMemcpyDeviceToHost),
          "copy output");
    return out;
}
} // namespace bev
