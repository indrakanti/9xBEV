#include "bev/core.hpp"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace bev {
namespace {
void check_gl(const char *where) {
    GLenum e = glGetError();
    if (e != GL_NO_ERROR)
        throw std::runtime_error(std::string(where) + ": GL error " + std::to_string(e));
}
struct Context {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    GLuint program = 0, vao = 0, fbo = 0, output = 0, textures[3] = {};
    ~Context() {
        if (display != EGL_NO_DISPLAY) {
            if (context != EGL_NO_CONTEXT) {
                glDeleteProgram(program);
                glDeleteVertexArrays(1, &vao);
                glDeleteFramebuffers(1, &fbo);
                glDeleteTextures(1, &output);
                glDeleteTextures(3, textures);
                eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                eglDestroyContext(display, context);
            }
            if (surface != EGL_NO_SURFACE)
                eglDestroySurface(display, surface);
            eglTerminate(display);
        }
    }
    void init() {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint major, minor;
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor))
            throw std::runtime_error("EGL initialization failed: configure board display or "
                                     "EGL_PLATFORM=surfaceless on Mesa");
        if (!eglBindAPI(EGL_OPENGL_ES_API))
            throw std::runtime_error("Cannot bind OpenGL ES");
        const EGLint attrs[] = {EGL_SURFACE_TYPE,
                                EGL_PBUFFER_BIT,
                                EGL_RENDERABLE_TYPE,
                                0x0040 /* EGL_OPENGL_ES3_BIT */,
                                EGL_RED_SIZE,
                                8,
                                EGL_GREEN_SIZE,
                                8,
                                EGL_BLUE_SIZE,
                                8,
                                EGL_NONE};
        EGLConfig config;
        EGLint count;
        if (!eglChooseConfig(display, attrs, &config, 1, &count) || count != 1)
            throw std::runtime_error("No GLES3 pbuffer EGL config");
        const EGLint pbuf[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
        surface = eglCreatePbufferSurface(display, config, pbuf);
        const EGLint ctx[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx);
        if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT ||
            !eglMakeCurrent(display, surface, surface, context))
            throw std::runtime_error("Cannot create GLES3 context");
        std::cerr << "GLES renderer: " << glGetString(GL_RENDERER) << "\n";
    }
};
GLuint compile(GLenum kind, const char *source) {
    GLuint s = glCreateShader(kind);
    glShaderSource(s, 1, &source, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        glDeleteShader(s);
        throw std::runtime_error(log);
    }
    return s;
}
const char *vertex = R"GLSL(#version 300 es
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0.0,1.0);}
)GLSL";
const char *fragment = R"GLSL(#version 300 es
precision highp float;
precision highp sampler2DArray;
precision highp usampler2D;
uniform sampler2DArray images;
uniform sampler2DArray mapping;
uniform usampler2D masks;
uniform int count;
uniform int output_height;
uniform vec2 atlas_size;
uniform vec3 background;
uniform vec3 body;
uniform float min_weight;
out vec4 color;
void main(){
 ivec2 pixel=ivec2(int(gl_FragCoord.x),output_height-1-int(gl_FragCoord.y));
 uint mask=texelFetch(masks,pixel,0).r;
 if(mask==2u){color=vec4(body,1);return;}
 vec3 sum=vec3(0);float weight=0.0;
 if(mask==1u)for(int c=0;c<count;++c){vec4 s=texelFetch(mapping,ivec3(pixel,c),0);if(s.z>0.0){sum+=texture(images,vec3((s.xy+vec2(0.5))/atlas_size,float(c))).rgb*s.z*s.w;weight+=s.z;}}
 color=vec4(weight>=min_weight?clamp(sum/weight,0.0,1.0):background,1);
}
)GLSL";
void params(GLenum target, GLint filter) {
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
} // namespace
Image render_opengl(const Rig &r, const Mapping &m, const std::vector<Image> &images) {
    validate_inputs(r, m, images);
    Context ctx;
    ctx.init();
    GLint size, layers;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &layers);
    int w = 0, h = 0;
    for (const auto &im : images) {
        w = std::max(w, im.width);
        h = std::max(h, im.height);
    }
    if (std::max({r.width, r.height, w, h}) > size || m.cameras > layers)
        throw std::runtime_error("GPU texture limits exceeded");
    std::size_t bytes = std::size_t(w) * h * m.cameras * 3;
    if (bytes > 512ULL * 1024 * 1024)
        throw std::runtime_error("Camera atlas exceeds 512 MiB ceiling");
    std::vector<std::uint8_t> atlas(bytes);
    for (int c = 0; c < m.cameras; ++c)
        for (int y = 0; y < images[c].height; ++y)
            std::memcpy(atlas.data() + (std::size_t(c) * w * h + std::size_t(y) * w) * 3,
                        images[c].rgb.data() + std::size_t(y) * images[c].width * 3,
                        std::size_t(images[c].width) * 3);
    GLuint vs = compile(GL_VERTEX_SHADER, vertex), fs = 0;
    try {
        fs = compile(GL_FRAGMENT_SHADER, fragment);
    } catch (...) {
        glDeleteShader(vs);
        throw;
    }
    ctx.program = glCreateProgram();
    glAttachShader(ctx.program, vs);
    glAttachShader(ctx.program, fs);
    glLinkProgram(ctx.program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok;
    glGetProgramiv(ctx.program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(ctx.program, sizeof(log), nullptr, log);
        throw std::runtime_error(log);
    }
    glUseProgram(ctx.program);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(3, ctx.textures);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, ctx.textures[0]);
    params(GL_TEXTURE_2D_ARRAY, GL_LINEAR);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, w, h, m.cameras, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 atlas.data());
    static_assert(sizeof(Projection) == 4 * sizeof(float), "Projection must be packed RGBA floats");
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, ctx.textures[1]);
    params(GL_TEXTURE_2D_ARRAY, GL_NEAREST);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA32F, r.width, r.height, m.cameras, 0, GL_RGBA,
                 GL_FLOAT, m.samples.data());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, ctx.textures[2]);
    params(GL_TEXTURE_2D, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, r.width, r.height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
                 m.mask.data());
    glGenTextures(1, &ctx.output);
    glBindTexture(GL_TEXTURE_2D, ctx.output);
    params(GL_TEXTURE_2D, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, r.width, r.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    // Output allocation changed unit 2 binding; restore the mask texture.
    glBindTexture(GL_TEXTURE_2D, ctx.textures[2]);
    glGenFramebuffers(1, &ctx.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx.output, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Incomplete rendering framebuffer");
    auto loc = [&](const char *n) { return glGetUniformLocation(ctx.program, n); };
    glUniform1i(loc("images"), 0);
    glUniform1i(loc("mapping"), 1);
    glUniform1i(loc("masks"), 2);
    glUniform1i(loc("count"), m.cameras);
    glUniform1i(loc("output_height"), r.height);
    glUniform2f(loc("atlas_size"), float(w), float(h));
    glUniform3fv(loc("background"), 1, r.background.data());
    glUniform3fv(loc("body"), 1, r.body_color.data());
    glUniform1f(loc("min_weight"), float(r.min_weight));
    check_gl("texture upload");
    glGenVertexArrays(1, &ctx.vao);
    glBindVertexArray(ctx.vao);
    glViewport(0, 0, r.width, r.height);
    glDisable(GL_DITHER);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    check_gl("draw");
    std::vector<std::uint8_t> pixels(std::size_t(r.width) * r.height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, r.width, r.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    check_gl("readback");
    Image out{r.width, r.height, {}};
    out.rgb.resize(std::size_t(r.width) * r.height * 3);
    for (int y = 0; y < r.height; ++y)
        for (int x = 0; x < r.width; ++x)
            for (int k = 0; k < 3; ++k)
                out.rgb[(std::size_t(y) * r.width + x) * 3 + k] =
                    pixels[(std::size_t(r.height - 1 - y) * r.width + x) * 4 + k];
    return out;
}
} // namespace bev
