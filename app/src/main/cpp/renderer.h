#pragma once
#include <GLES3/gl3.h>
#include <android/log.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <array>
#include <sstream>
#include "math3d.h"
#include "scene.h"
#include "fsutil.h"
#include "gltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "h3d", __VA_ARGS__)

struct Mesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei count = 0;
    GLenum mode = GL_TRIANGLES;
};

struct ModelData {
    Mesh mesh;
    Vec3 half = Vec3(0.5f, 0.5f, 0.5f);
    bool ok = false;
};

struct View {
    Mat4 vp;
    Vec3 eye, fwd, right, up;
    float tanY = 0.577f, aspect = 1.0f;
    int W = 1, H = 1;
    Vec3 focus;   // centro da area de sombra
};

static inline float lin(float c) { return std::pow(c < 0 ? 0 : c, 2.2f); }

static const char* GLSL_COMMON = R"GLSL(
uniform vec3 uSkyTop;
uniform vec3 uSkyHor;
uniform vec3 uGround;
uniform float uExposure;
uniform int uTonemap;
vec3 skyColor(vec3 d) {
  float t = clamp(d.y, 0.0, 1.0);
  vec3 sky = mix(uSkyHor, uSkyTop, pow(t, 0.55));
  vec3 gnd = mix(uSkyHor, uGround, clamp(-d.y * 4.0, 0.0, 1.0));
  return d.y >= 0.0 ? sky : gnd;
}
vec3 finishColor(vec3 c) {
  c *= uExposure;
  if (uTonemap == 1) c = (c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14);
  c = clamp(c, 0.0, 1.0);
  return pow(c, vec3(1.0 / 2.2));
}
)GLSL";

static const char* SKIN_GLSL = R"GLSL(
uniform sampler2D uBones;
uniform int uSkinned;
mat4 boneMat(int j) {
  return mat4(texelFetch(uBones, ivec2(j * 4, 0), 0), texelFetch(uBones, ivec2(j * 4 + 1, 0), 0),
              texelFetch(uBones, ivec2(j * 4 + 2, 0), 0), texelFetch(uBones, ivec2(j * 4 + 3, 0), 0));
}
mat4 skinMat(vec4 jn, vec4 w) {
  return w.x * boneMat(int(jn.x + 0.5)) + w.y * boneMat(int(jn.y + 0.5)) +
         w.z * boneMat(int(jn.z + 0.5)) + w.w * boneMat(int(jn.w + 0.5));
}
)GLSL";

static const char* MAIN_VS_BODY = R"GLSL(
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aJoints;
layout(location=4) in vec4 aWeights;
uniform mat4 uVP;
uniform mat4 uModel;
uniform mat4 uLightVP;
uniform float uNormalOffset;
out vec3 vWorld;
out vec3 vN;
out vec2 vUV;
out vec4 vLS;
void main() {
  vec4 lp = vec4(aPos, 1.0);
  vec3 ln = aNormal;
  if (uSkinned == 1) {
    mat4 sk = skinMat(aJoints, aWeights);
    lp = sk * lp;
    ln = mat3(sk) * ln;
  }
  vec4 w = uModel * lp;
  vWorld = w.xyz;
  vN = normalize(transpose(inverse(mat3(uModel))) * ln);
  vUV = aUV;
  vLS = uLightVP * vec4(w.xyz + vN * uNormalOffset, 1.0);
  gl_Position = uVP * w;
}
)GLSL";

static const char* MAIN_FS_BODY = R"GLSL(
precision highp sampler2DShadow;
in vec3 vWorld;
in vec3 vN;
in vec2 vUV;
in vec4 vLS;
out vec4 oColor;
uniform vec3 uCam;
uniform vec4 uAlbedo;
uniform float uMetal;
uniform float uRough;
uniform vec3 uEmis;
uniform sampler2D uTex;
uniform int uHasTex;
uniform vec2 uTile;
uniform sampler2DShadow uShadowMap;
uniform int uShadowOn;
uniform float uShadowSoft;
uniform float uTexel;
uniform vec3 uSunDir;
uniform vec3 uSunCol;
uniform float uAmbient;
uniform int uNL;
uniform vec3 uLPos[8];
uniform vec3 uLCol[8];
uniform float uLRange[8];
uniform float uFog;
uniform int uUnlit;
const float PI = 3.14159265;

float shadowFactor() {
  if (uShadowOn == 0) return 1.0;
  vec3 p = vLS.xyz / vLS.w * 0.5 + 0.5;
  if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
  float s = 0.0;
  for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
      s += texture(uShadowMap, vec3(p.xy + vec2(float(x), float(y)) * uTexel * uShadowSoft, p.z - 0.0006));
  return s / 9.0;
}
float distGGX(float NdH, float a) {
  float a2 = a * a;
  float d = NdH * NdH * (a2 - 1.0) + 1.0;
  return a2 / (PI * d * d);
}
float geoSchlick(float NdX, float k) { return NdX / (NdX * (1.0 - k) + k); }
vec3 fresnel(float c, vec3 f0) { return f0 + (1.0 - f0) * pow(1.0 - c, 5.0); }
vec3 brdf(vec3 N, vec3 V, vec3 L, vec3 albedo, float metal, float rough, vec3 f0, vec3 radiance) {
  vec3 H = normalize(V + L);
  float NdL = max(dot(N, L), 0.0);
  float NdV = max(dot(N, V), 0.001);
  float NdH = max(dot(N, H), 0.0);
  float VdH = max(dot(V, H), 0.0);
  float a = max(rough * rough, 0.002);
  float k = (rough + 1.0) * (rough + 1.0) / 8.0;
  vec3 F = fresnel(VdH, f0);
  float D = distGGX(NdH, a);
  float G = geoSchlick(NdL, k) * geoSchlick(NdV, k);
  vec3 spec = D * G * F / max(4.0 * NdV * NdL, 0.001);
  vec3 kd = (1.0 - F) * (1.0 - metal);
  return (kd * albedo / PI + spec) * radiance * NdL;
}
void main() {
  vec4 base = uAlbedo;
  if (uHasTex == 1) base.rgb *= texture(uTex, vUV * uTile).rgb;
  if (uUnlit == 1) { oColor = vec4(pow(base.rgb, vec3(1.0 / 2.2)), 1.0); return; }
  vec3 N = normalize(vN);
  vec3 V = normalize(uCam - vWorld);
  if (dot(N, V) < 0.0) N = -N;
  vec3 albedo = base.rgb;
  float metal = clamp(uMetal, 0.0, 1.0);
  float rough = clamp(uRough, 0.04, 1.0);
  vec3 f0 = mix(vec3(0.04), albedo, metal);
  vec3 col = vec3(0.0);

  vec3 L = normalize(-uSunDir);
  col += brdf(N, V, L, albedo, metal, rough, f0, uSunCol) * shadowFactor();

  for (int i = 0; i < 8; i++) {
    if (i >= uNL) break;
    vec3 d = uLPos[i] - vWorld;
    float dist = length(d);
    vec3 Lp = d / max(dist, 0.0001);
    float att = 1.0 / (1.0 + dist * dist);
    float w = clamp(1.0 - pow(dist / uLRange[i], 4.0), 0.0, 1.0);
    att *= w * w;
    col += brdf(N, V, Lp, albedo, metal, rough, f0, uLCol[i] * att);
  }

  // ambiente: luz difusa do ceu + reflexo do ceu
  float NdV = max(dot(N, V), 0.0);
  vec3 irr = mix(uGround * 0.6, mix(uSkyHor, uSkyTop, 0.55), N.y * 0.5 + 0.5);
  vec3 Fe = f0 + (max(vec3(1.0 - rough), f0) - f0) * pow(1.0 - NdV, 5.0);
  vec3 kdA = (1.0 - Fe) * (1.0 - metal);
  vec3 R = reflect(-V, N);
  vec3 envSpec = mix(skyColor(R), mix(uSkyHor, uSkyTop, 0.5), rough * rough);
  col += (kdA * albedo * irr + envSpec * Fe) * uAmbient;

  col += uEmis;

  float dd = length(uCam - vWorld);
  float ff = 1.0 - exp(-uFog * dd);
  col = mix(col, uSkyHor, clamp(ff, 0.0, 1.0));
  oColor = vec4(finishColor(col), 1.0);
}
)GLSL";

static const char* SHADOW_VS_BODY = R"GLSL(
layout(location=0) in vec3 aPos;
layout(location=3) in vec4 aJoints;
layout(location=4) in vec4 aWeights;
uniform mat4 uLightVP;
uniform mat4 uModel;
void main() {
  vec4 lp = vec4(aPos, 1.0);
  if (uSkinned == 1) lp = skinMat(aJoints, aWeights) * lp;
  gl_Position = uLightVP * uModel * lp;
}
)GLSL";
static const char* SHADOW_FS = R"GLSL(#version 300 es
precision mediump float;
void main() {}
)GLSL";

static const char* SKY_VS = R"GLSL(#version 300 es
out vec2 vNdc;
void main() {
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  vNdc = p * 2.0 - 1.0;
  gl_Position = vec4(vNdc, 1.0, 1.0);
}
)GLSL";
static const char* SKY_FS_BODY = R"GLSL(
in vec2 vNdc;
out vec4 oColor;
uniform vec3 uFwd;
uniform vec3 uRight;
uniform vec3 uUp;
uniform float uTanY;
uniform float uAspect;
uniform vec3 uSunDir;
uniform vec3 uSunCol;
void main() {
  vec3 d = normalize(uFwd + uRight * (vNdc.x * uTanY * uAspect) + uUp * (vNdc.y * uTanY));
  vec3 c = skyColor(d);
  float s = max(dot(d, -uSunDir), 0.0);
  c += uSunCol * (pow(s, 1500.0) * 6.0 + pow(s, 24.0) * 0.08);
  oColor = vec4(finishColor(c), 1.0);
}
)GLSL";

class Renderer {
public:
    Mesh meshes[SHAPE_COUNT];
    Mesh grid, axisX, axisY, axisZ, wireCube;
    std::string projectDir;

    bool init() {
        std::string fsMain = std::string("#version 300 es\nprecision highp float;\n") + GLSL_COMMON + MAIN_FS_BODY;
        std::string fsSky = std::string("#version 300 es\nprecision highp float;\n") + GLSL_COMMON + SKY_FS_BODY;
        std::string vsMain = std::string("#version 300 es\nprecision highp float;\nprecision highp sampler2D;\n") + SKIN_GLSL + MAIN_VS_BODY;
        std::string vsShadow = std::string("#version 300 es\nprecision highp float;\nprecision highp sampler2D;\n") + SKIN_GLSL + SHADOW_VS_BODY;
        progMain = link(vsMain.c_str(), fsMain.c_str());
        progShadow = link(vsShadow.c_str(), SHADOW_FS);
        progSky = link(SKY_VS, fsSky.c_str());
        if (!progMain || !progShadow || !progSky) return false;

        auto U = [](GLuint p, const char* n) { return glGetUniformLocation(p, n); };
        m.vp = U(progMain, "uVP"); m.model = U(progMain, "uModel"); m.lightVP = U(progMain, "uLightVP");
        m.normalOffset = U(progMain, "uNormalOffset"); m.cam = U(progMain, "uCam");
        m.albedo = U(progMain, "uAlbedo"); m.metal = U(progMain, "uMetal"); m.rough = U(progMain, "uRough");
        m.emis = U(progMain, "uEmis"); m.tex = U(progMain, "uTex"); m.hasTex = U(progMain, "uHasTex");
        m.tile = U(progMain, "uTile"); m.shadowMap = U(progMain, "uShadowMap"); m.shadowOn = U(progMain, "uShadowOn");
        m.shadowSoft = U(progMain, "uShadowSoft"); m.texel = U(progMain, "uTexel");
        m.sunDir = U(progMain, "uSunDir"); m.sunCol = U(progMain, "uSunCol"); m.ambient = U(progMain, "uAmbient");
        m.skyTop = U(progMain, "uSkyTop"); m.skyHor = U(progMain, "uSkyHor"); m.ground = U(progMain, "uGround");
        m.nl = U(progMain, "uNL"); m.lpos = U(progMain, "uLPos[0]"); m.lcol = U(progMain, "uLCol[0]");
        m.lrange = U(progMain, "uLRange[0]"); m.exposure = U(progMain, "uExposure"); m.tonemap = U(progMain, "uTonemap");
        m.fog = U(progMain, "uFog"); m.unlit = U(progMain, "uUnlit");
        m.bones = U(progMain, "uBones"); m.skinned = U(progMain, "uSkinned");
        sh.lightVP = U(progShadow, "uLightVP"); sh.model = U(progShadow, "uModel");
        sh.bones = U(progShadow, "uBones"); sh.skinned = U(progShadow, "uSkinned");
        sk.skyTop = U(progSky, "uSkyTop"); sk.skyHor = U(progSky, "uSkyHor"); sk.ground = U(progSky, "uGround");
        sk.exposure = U(progSky, "uExposure"); sk.tonemap = U(progSky, "uTonemap");
        sk.fwd = U(progSky, "uFwd"); sk.right = U(progSky, "uRight"); sk.up = U(progSky, "uUp");
        sk.tanY = U(progSky, "uTanY"); sk.aspect = U(progSky, "uAspect");
        sk.sunDir = U(progSky, "uSunDir"); sk.sunCol = U(progSky, "uSunCol");

        meshes[SHAPE_CUBE] = makeCube();
        meshes[SHAPE_SPHERE] = makeSphere();
        meshes[SHAPE_CYLINDER] = makeCylinder();
        meshes[SHAPE_PLANE] = makePlane();
        meshes[SHAPE_LIGHT] = meshes[SHAPE_SPHERE];  // compartilha (nao destruir 2x)

        std::vector<Vec3> g;
        for (int i = -10; i <= 10; i++) {
            if (i == 0) continue;
            g.push_back(Vec3((float)i, 0, -10)); g.push_back(Vec3((float)i, 0, 10));
            g.push_back(Vec3(-10, 0, (float)i)); g.push_back(Vec3(10, 0, (float)i));
        }
        grid = makeLines(g);
        axisX = makeLines({Vec3(-10, 0, 0), Vec3(10, 0, 0)});
        axisY = makeLines({Vec3(0, 0, 0), Vec3(0, 5, 0)});
        axisZ = makeLines({Vec3(0, 0, -10), Vec3(0, 0, 10)});

        std::vector<Vec3> w;
        Vec3 c[8];
        for (int i = 0; i < 8; i++)
            c[i] = Vec3((i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f);
        const int e[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
        for (auto& ed : e) { w.push_back(c[ed[0]]); w.push_back(c[ed[1]]); }
        wireCube = makeLines(w);

        glGenVertexArrays(1, &emptyVao);
        unsigned char white[4] = {255, 255, 255, 255};
        glGenTextures(1, &whiteTex);
        glBindTexture(GL_TEXTURE_2D, whiteTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // textura de matrizes dos ossos (4 texels por junta, ate 256 juntas)
        glGenTextures(1, &boneTex);
        glBindTexture(GL_TEXTURE_2D, boneTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1024, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        shadowSize = 0;
        return true;
    }

    void shutdown() {
        clearAssets();
        for (int i = 0; i < SHAPE_COUNT; i++) if (i != SHAPE_LIGHT && i != SHAPE_MODEL) destroy(meshes[i]);
        meshes[SHAPE_LIGHT] = Mesh();
        destroy(grid); destroy(axisX); destroy(axisY); destroy(axisZ); destroy(wireCube);
        if (progMain) glDeleteProgram(progMain);
        if (progShadow) glDeleteProgram(progShadow);
        if (progSky) glDeleteProgram(progSky);
        progMain = progShadow = progSky = 0;
        if (emptyVao) glDeleteVertexArrays(1, &emptyVao);
        if (whiteTex) glDeleteTextures(1, &whiteTex);
        if (boneTex) glDeleteTextures(1, &boneTex);
        emptyVao = whiteTex = boneTex = 0;
        destroyShadow();
    }

    void setProjectDir(const std::string& d) {
        if (d != projectDir) clearAssets();
        projectDir = d;
    }

    void clearAssets() {
        for (auto& kv : texCache) if (kv.second) glDeleteTextures(1, &kv.second);
        texCache.clear();
        for (auto& kv : modelCache) destroy(kv.second.mesh);
        modelCache.clear();
        for (auto& kv : gltfCache) {
            for (auto& pr : kv.second.prims) destroy(pr.mesh);
            for (GLuint t : kv.second.imgTex) if (t) glDeleteTextures(1, &t);
        }
        gltfCache.clear();
    }

    // forca recarregar um modelo/textura (apos editar arquivo)
    void reloadAssets() { clearAssets(); }

    ModelData& getModel(const std::string& name) {
        auto it = modelCache.find(name);
        if (it != modelCache.end()) return it->second;
        ModelData md;
        std::vector<float> verts;
        std::vector<uint32_t> idx;
        Vec3 half;
        if (!name.empty() && loadObj(projectDir + "/models/" + name, verts, idx, half)) {
            md.mesh = upload(verts, idx, GL_TRIANGLES);
            md.half = half;
            md.ok = true;
        } else {
            LOGI("falha ao carregar modelo %s", name.c_str());
        }
        return modelCache[name] = md;
    }

    static bool isGltfName(const std::string& n) {
        std::string e = fsu::ext(n);
        return e == "glb" || e == "gltf";
    }

    struct GpuPrim { Mesh mesh; int node = -1, skin = -1, mat = -1; bool skinned = false; };
    struct GpuGltf {
        Gltf g;
        std::vector<GpuPrim> prims;
        std::vector<GLuint> imgTex;
        bool ok = false;
    };
    struct ObjPose {
        std::vector<Mat4> glob;
        std::vector<std::vector<float>> joints;
    };

    GpuGltf* getGltf(const std::string& name) {
        if (!isGltfName(name)) return nullptr;
        auto it = gltfCache.find(name);
        if (it != gltfCache.end()) return it->second.ok ? &it->second : nullptr;
        GpuGltf& gm = gltfCache[name];
        std::string err;
        if (!gm.g.load(projectDir + "/models/" + name, err)) {
            LOGI("falha ao carregar %s: %s", name.c_str(), err.c_str());
            loadErrors[name] = err;
            return nullptr;
        }
        for (size_t i = 0; i < gm.g.nodes.size(); i++) {
            int mi = gm.g.nodes[i].mesh;
            if (mi < 0 || mi >= (int)gm.g.meshes.size()) continue;
            for (auto& pr : gm.g.meshes[mi]) {
                GpuPrim gp;
                gp.node = (int)i;
                gp.skin = gm.g.nodes[i].skin;
                gp.mat = pr.material;
                gp.skinned = pr.skinned() && gp.skin >= 0;
                gp.mesh = gp.skinned ? uploadSkinned(pr.v, pr.sk, pr.idx) : upload(pr.v, pr.idx, GL_TRIANGLES);
                gm.prims.push_back(gp);
            }
        }
        for (auto& im : gm.g.images) {
            GLuint tex = 0;
            int w = 0, h = 0, c = 0;
            if (!im.bytes.empty()) {
                unsigned char* data = stbi_load_from_memory(im.bytes.data(), (int)im.bytes.size(), &w, &h, &c, 4);
                if (data) {
                    glGenTextures(1, &tex);
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                    glGenerateMipmap(GL_TEXTURE_2D);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    stbi_image_free(data);
                }
            }
            gm.imgTex.push_back(tex);
        }
        gm.ok = !gm.prims.empty();
        return gm.ok ? &gm : nullptr;
    }

    const Gltf* gltfInfo(const std::string& name) {
        GpuGltf* g = getGltf(name);
        return g ? &g->g : nullptr;
    }

    std::string modelError(const std::string& name) {
        auto it = loadErrors.find(name);
        return it == loadErrors.end() ? std::string() : it->second;
    }

    Vec3 modelHalf(const std::string& name) {
        if (isGltfName(name)) {
            GpuGltf* g = getGltf(name);
            return g ? g->g.half : Vec3(0.5f, 0.5f, 0.5f);
        }
        return getModel(name).half;
    }

    GLuint getTexture(const std::string& name) {
        auto it = texCache.find(name);
        if (it != texCache.end()) return it->second;
        GLuint tex = 0;
        int w = 0, h = 0, comp = 0;
        unsigned char* data = stbi_load((projectDir + "/textures/" + name).c_str(), &w, &h, &comp, 4);
        if (data) {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            stbi_image_free(data);
        } else {
            LOGI("falha ao carregar textura %s", name.c_str());
        }
        texCache[name] = tex;
        return tex;
    }

    static Vec3 sunDirection(const Env& e) {
        float y = e.sunYaw * kDeg2Rad, p = e.sunPitch * kDeg2Rad;
        return normalize(Vec3(std::cos(p) * std::cos(y), std::sin(p), std::cos(p) * std::sin(y)));
    }

    void renderScene(const Scene& s, const Env& e, const View& v, int selected, bool editor, bool showGrid) {
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);

        // poses dos modelos animados (uma vez por quadro)
        std::vector<ObjPose> poses(s.objects.size());
        for (size_t i = 0; i < s.objects.size(); i++) {
            const Object& o = s.objects[i];
            if (o.shape != SHAPE_MODEL || !isGltfName(o.model)) continue;
            if (!editor && !o.visible) continue;
            if (GpuGltf* gm = getGltf(o.model)) evalPose(o, *gm, poses[i]);
        }

        Vec3 sunDir = sunDirection(e);
        Vec3 sunCol(lin(e.sunColor[0]) * e.sunIntensity, lin(e.sunColor[1]) * e.sunIntensity, lin(e.sunColor[2]) * e.sunIntensity);
        Mat4 lightVP = Mat4::identity();
        bool useShadow = e.shadows != 0;
        ensureShadow(e.shadowSize);

        // ---------- passe de sombra ----------
        if (useShadow) {
            float r = std::max(e.shadowRange, 3.0f);
            Vec3 up = std::fabs(sunDir.y) > 0.95f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
            Mat4 lv = lookAtM(v.focus - sunDir * (r * 2.0f), v.focus, up);
            lightVP = orthoM(-r, r, -r, r, 0.5f, r * 4.0f) * lv;
            glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
            glViewport(0, 0, shadowSize, shadowSize);
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(2.0f, 4.0f);
            glUseProgram(progShadow);
            glUniformMatrix4fv(sh.lightVP, 1, GL_FALSE, lightVP.m);
            glUniform1i(sh.bones, 2);
            glUniform1i(sh.skinned, 0);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, boneTex);
            glActiveTexture(GL_TEXTURE0);
            for (size_t oi = 0; oi < s.objects.size(); oi++) {
                const Object& o = s.objects[oi];
                if (o.shape == SHAPE_LIGHT || !o.castShadow) continue;
                if (!editor && !o.visible) continue;
                if (o.shape == SHAPE_MODEL && isGltfName(o.model)) {
                    GpuGltf* gm = getGltf(o.model);
                    if (gm && !poses[oi].glob.empty()) drawGltf(o, *gm, poses[oi], true);
                    glUniform1i(sh.skinned, 0);
                    continue;
                }
                const Mesh* me = meshFor(o);
                Mat4 md = modelMatrix(o);
                glUniformMatrix4fv(sh.model, 1, GL_FALSE, md.m);
                drawMesh(*me);
            }
            glDisable(GL_POLYGON_OFFSET_FILL);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // ---------- passe principal ----------
        glViewport(0, 0, v.W, v.H);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float top[3] = {lin(e.skyTop[0]), lin(e.skyTop[1]), lin(e.skyTop[2])};
        float hor[3] = {lin(e.skyHorizon[0]), lin(e.skyHorizon[1]), lin(e.skyHorizon[2])};
        float gnd[3] = {lin(e.ground[0]), lin(e.ground[1]), lin(e.ground[2])};

        // ceu
        glDisable(GL_DEPTH_TEST);
        glUseProgram(progSky);
        glUniform3fv(sk.skyTop, 1, top); glUniform3fv(sk.skyHor, 1, hor); glUniform3fv(sk.ground, 1, gnd);
        glUniform1f(sk.exposure, e.exposure); glUniform1i(sk.tonemap, e.tonemap);
        glUniform3f(sk.fwd, v.fwd.x, v.fwd.y, v.fwd.z);
        glUniform3f(sk.right, v.right.x, v.right.y, v.right.z);
        glUniform3f(sk.up, v.up.x, v.up.y, v.up.z);
        glUniform1f(sk.tanY, v.tanY); glUniform1f(sk.aspect, v.aspect);
        glUniform3f(sk.sunDir, sunDir.x, sunDir.y, sunDir.z);
        glUniform3f(sk.sunCol, sunCol.x, sunCol.y, sunCol.z);
        glBindVertexArray(emptyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
        glUseProgram(progMain);
        glUniformMatrix4fv(m.vp, 1, GL_FALSE, v.vp.m);
        glUniformMatrix4fv(m.lightVP, 1, GL_FALSE, lightVP.m);
        glUniform3f(m.cam, v.eye.x, v.eye.y, v.eye.z);
        glUniform3f(m.sunDir, sunDir.x, sunDir.y, sunDir.z);
        glUniform3f(m.sunCol, sunCol.x, sunCol.y, sunCol.z);
        glUniform1f(m.ambient, e.ambient);
        glUniform3fv(m.skyTop, 1, top); glUniform3fv(m.skyHor, 1, hor); glUniform3fv(m.ground, 1, gnd);
        glUniform1f(m.exposure, e.exposure); glUniform1i(m.tonemap, e.tonemap);
        glUniform1f(m.fog, e.fog);
        glUniform1i(m.shadowOn, useShadow ? 1 : 0);
        glUniform1f(m.shadowSoft, e.shadowSoft);
        glUniform1f(m.texel, 1.0f / (float)shadowSize);
        glUniform1f(m.normalOffset, (2.0f * e.shadowRange / (float)shadowSize) * 1.5f);
        glUniform1i(m.unlit, 0);
        glUniform1i(m.skinned, 0);
        glUniform1i(m.bones, 2);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, boneTex);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowTex);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(m.tex, 0);
        glUniform1i(m.shadowMap, 1);

        // luzes de ponto (ate 8)
        float lp[24], lc[24], lr[8];
        int nl = 0;
        for (const Object& o : s.objects) {
            if (o.shape != SHAPE_LIGHT || !o.visible || nl >= 8) continue;
            lp[nl * 3] = o.pos.x; lp[nl * 3 + 1] = o.pos.y; lp[nl * 3 + 2] = o.pos.z;
            lc[nl * 3] = lin(o.color[0]) * o.intensity;
            lc[nl * 3 + 1] = lin(o.color[1]) * o.intensity;
            lc[nl * 3 + 2] = lin(o.color[2]) * o.intensity;
            lr[nl] = std::max(o.range, 0.1f);
            nl++;
        }
        glUniform1i(m.nl, nl);
        if (nl > 0) {
            glUniform3fv(m.lpos, nl, lp);
            glUniform3fv(m.lcol, nl, lc);
            glUniform1fv(m.lrange, nl, lr);
        }

        for (size_t oi = 0; oi < s.objects.size(); oi++) {
            const Object& o = s.objects[oi];
            if (o.shape == SHAPE_LIGHT) continue;
            if (!editor && !o.visible) continue;
            if (o.shape == SHAPE_MODEL && isGltfName(o.model)) {
                GpuGltf* gm = getGltf(o.model);
                if (gm && !poses[oi].glob.empty()) {
                    drawGltf(o, *gm, poses[oi], false);
                    glUniform1i(m.skinned, 0);
                    glUniform1i(m.hasTex, 0);
                    continue;
                }
            }
            Mat4 md = modelMatrix(o);
            glUniformMatrix4fv(m.model, 1, GL_FALSE, md.m);
            glUniform4f(m.albedo, lin(o.color[0]), lin(o.color[1]), lin(o.color[2]), 1.0f);
            glUniform1f(m.metal, o.metallic);
            glUniform1f(m.rough, o.roughness);
            glUniform3f(m.emis, lin(o.emissive[0]) * o.emissiveStrength, lin(o.emissive[1]) * o.emissiveStrength,
                        lin(o.emissive[2]) * o.emissiveStrength);
            GLuint tx = o.texture.empty() ? 0 : getTexture(o.texture);
            glBindTexture(GL_TEXTURE_2D, tx ? tx : whiteTex);
            glUniform1i(m.hasTex, tx ? 1 : 0);
            glUniform2f(m.tile, o.tile[0], o.tile[1]);
            drawMesh(*meshFor(o));
        }

        // ---------- sobreposicoes do editor ----------
        glUniform1i(m.hasTex, 0);
        glBindTexture(GL_TEXTURE_2D, whiteTex);
        if (showGrid) {
            Mat4 id = Mat4::identity();
            drawUnlit(grid, id, 0.30f, 0.33f, 0.38f);
            drawUnlit(axisX, id, 0.90f, 0.25f, 0.25f);
            drawUnlit(axisY, id, 0.25f, 0.90f, 0.25f);
            drawUnlit(axisZ, id, 0.25f, 0.45f, 0.95f);
        }
        if (editor) {
            for (const Object& o : s.objects) {
                if (o.shape != SHAPE_LIGHT) continue;
                drawUnlit(meshes[SHAPE_SPHERE], modelMatrix(o), o.color[0], o.color[1], o.color[2]);
            }
            if (selected >= 0 && selected < (int)s.objects.size()) {
                const Object& o = s.objects[selected];
                Mat4 md = modelMatrix(o);
                if (o.shape == SHAPE_MODEL) {
                    Vec3 h = modelHalf(o.model);
                    md = md * scaleM(Vec3(h.x * 2, h.y * 2, h.z * 2));
                } else if (o.shape == SHAPE_PLANE) {
                    md = md * scaleM(Vec3(1, 0.05f, 1));
                }
                glDisable(GL_DEPTH_TEST);
                drawUnlit(wireCube, md, 1.0f, 0.85f, 0.1f);
                glEnable(GL_DEPTH_TEST);
            }
        }
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // matriz do objeto (luzes usam tamanho fixo)
    static Mat4 modelMatrix(const Object& o) {
        if (o.shape == SHAPE_LIGHT) return translateM(o.pos) * scaleM(Vec3(0.35f, 0.35f, 0.35f));
        return o.matrix();
    }

private:
    GLuint progMain = 0, progShadow = 0, progSky = 0, emptyVao = 0, whiteTex = 0;
    GLuint shadowFBO = 0, shadowTex = 0;
    int shadowSize = 0;
    std::map<std::string, GLuint> texCache;
    std::map<std::string, ModelData> modelCache;
    std::map<std::string, GpuGltf> gltfCache;
    std::map<std::string, std::string> loadErrors;
    GLuint boneTex = 0;

    struct MainLocs { GLint vp, model, lightVP, normalOffset, cam, albedo, metal, rough, emis, tex, hasTex, tile,
        shadowMap, shadowOn, shadowSoft, texel, sunDir, sunCol, ambient, skyTop, skyHor, ground, nl, lpos, lcol,
        lrange, exposure, tonemap, fog, unlit, bones, skinned; } m;
    struct ShadowLocs { GLint lightVP, model, bones, skinned; } sh;
    struct SkyLocs { GLint skyTop, skyHor, ground, exposure, tonemap, fwd, right, up, tanY, aspect, sunDir, sunCol; } sk;

    void evalPose(const Object& o, GpuGltf& gm, ObjPose& op) {
        const Gltf& g = gm.g;
        Pose cur;
        g.restPose(cur);
        const GAnim* a = o.animCur.empty() ? nullptr : g.find(o.animCur);
        if (a) g.sample(*a, o.animT, cur);
        if (a && o.animBlend > 0 && o.animBlendT < o.animBlend && !o.animPrev.empty()) {
            const GAnim* pa = g.find(o.animPrev);
            if (pa) {
                Pose pv, out;
                g.restPose(pv);
                g.sample(*pa, o.animPrevT, pv);
                Gltf::blend(out, pv, cur, o.animBlendT / o.animBlend);
                cur = out;
            }
        }
        g.globals(cur, op.glob);
        op.joints.resize(g.skins.size());
        for (size_t i = 0; i < g.skins.size(); i++) g.jointMatrices((int)i, op.glob, op.joints[i]);
    }

    void drawGltf(const Object& o, GpuGltf& gm, ObjPose& op, bool shadow) {
        const Gltf& g = gm.g;
        Mat4 base = o.matrix() * scaleM(Vec3(g.scale, g.scale, g.scale)) * translateM(Vec3(-g.center.x, -g.center.y, -g.center.z));
        for (GpuPrim& pr : gm.prims) {
            Mat4 md = pr.skinned ? base : base * op.glob[pr.node];
            if (pr.skinned && pr.skin >= 0 && pr.skin < (int)op.joints.size() && !op.joints[pr.skin].empty()) {
                std::vector<float>& jm = op.joints[pr.skin];
                int count = std::min((int)(jm.size() / 16), 256);
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, boneTex);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, count * 4, 1, GL_RGBA, GL_FLOAT, jm.data());
                glActiveTexture(GL_TEXTURE0);
                glUniform1i(shadow ? sh.skinned : m.skinned, 1);
            } else {
                glUniform1i(shadow ? sh.skinned : m.skinned, 0);
            }
            if (shadow) {
                glUniformMatrix4fv(sh.model, 1, GL_FALSE, md.m);
            } else {
                glUniformMatrix4fv(m.model, 1, GL_FALSE, md.m);
                GMat gmt;
                bool useFile = o.fileMaterial && pr.mat >= 0 && pr.mat < (int)g.mats.size();
                if (useFile) gmt = g.mats[pr.mat];
                GLuint tx = 0;
                if (useFile) {
                    glUniform4f(m.albedo, gmt.base[0] * lin(o.color[0]), gmt.base[1] * lin(o.color[1]), gmt.base[2] * lin(o.color[2]), 1.0f);
                    glUniform1f(m.metal, gmt.metal);
                    glUniform1f(m.rough, gmt.rough);
                    glUniform3f(m.emis, gmt.emis[0] + lin(o.emissive[0]) * o.emissiveStrength,
                                gmt.emis[1] + lin(o.emissive[1]) * o.emissiveStrength,
                                gmt.emis[2] + lin(o.emissive[2]) * o.emissiveStrength);
                    if (gmt.image >= 0 && gmt.image < (int)gm.imgTex.size()) tx = gm.imgTex[gmt.image];
                } else {
                    glUniform4f(m.albedo, lin(o.color[0]), lin(o.color[1]), lin(o.color[2]), 1.0f);
                    glUniform1f(m.metal, o.metallic);
                    glUniform1f(m.rough, o.roughness);
                    glUniform3f(m.emis, lin(o.emissive[0]) * o.emissiveStrength, lin(o.emissive[1]) * o.emissiveStrength,
                                lin(o.emissive[2]) * o.emissiveStrength);
                }
                if (!tx && !o.texture.empty()) tx = getTexture(o.texture);
                glBindTexture(GL_TEXTURE_2D, tx ? tx : whiteTex);
                glUniform1i(m.hasTex, tx ? 1 : 0);
                glUniform2f(m.tile, tx && useFile ? 1.0f : o.tile[0], tx && useFile ? 1.0f : o.tile[1]);
            }
            drawMesh(pr.mesh);
        }
    }

    const Mesh* meshFor(const Object& o) {
        if (o.shape == SHAPE_MODEL) {
            ModelData& md = getModel(o.model);
            return md.ok ? &md.mesh : &meshes[SHAPE_CUBE];
        }
        return &meshes[o.shape];
    }

    void drawMesh(const Mesh& me) {
        glBindVertexArray(me.vao);
        glDrawElements(me.mode, me.count, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    void drawUnlit(const Mesh& me, const Mat4& model, float r, float g, float b) {
        glUniformMatrix4fv(m.model, 1, GL_FALSE, model.m);
        glUniform4f(m.albedo, lin(r), lin(g), lin(b), 1.0f);
        glUniform1i(m.unlit, 1);
        drawMesh(me);
        glUniform1i(m.unlit, 0);
    }

    void destroyShadow() {
        if (shadowFBO) glDeleteFramebuffers(1, &shadowFBO);
        if (shadowTex) glDeleteTextures(1, &shadowTex);
        shadowFBO = shadowTex = 0;
        shadowSize = 0;
    }

    void ensureShadow(int size) {
        if (size < 256) size = 256;
        if (size > 4096) size = 4096;
        if (shadowTex && shadowSize == size) return;
        destroyShadow();
        shadowSize = size;
        glGenTextures(1, &shadowTex);
        glBindTexture(GL_TEXTURE_2D, shadowTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glGenFramebuffers(1, &shadowFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
        GLenum none = GL_NONE;
        glDrawBuffers(1, &none);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) LOGI("FBO de sombra incompleto");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    static GLuint compile(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(s, 1024, nullptr, log);
            LOGI("shader error: %s", log);
            return 0;
        }
        return s;
    }

    static GLuint link(const char* vs, const char* fs) {
        GLuint v = compile(GL_VERTEX_SHADER, vs);
        GLuint f = compile(GL_FRAGMENT_SHADER, fs);
        if (!v || !f) return 0;
        GLuint p = glCreateProgram();
        glAttachShader(p, v);
        glAttachShader(p, f);
        glLinkProgram(p);
        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetProgramInfoLog(p, 1024, nullptr, log);
            LOGI("link error: %s", log);
            return 0;
        }
        glDeleteShader(v);
        glDeleteShader(f);
        return p;
    }

    static void destroy(Mesh& me) {
        if (me.vao) glDeleteVertexArrays(1, &me.vao);
        if (me.vbo) glDeleteBuffers(1, &me.vbo);
        if (me.ebo) glDeleteBuffers(1, &me.ebo);
        me = Mesh();
    }

    static void pushV(std::vector<float>& v, const Vec3& p, const Vec3& n, float u = 0, float w = 0) {
        v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
        v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
        v.push_back(u); v.push_back(w);
    }

    static Mesh upload(const std::vector<float>& v, const std::vector<uint32_t>& idx, GLenum mode) {
        Mesh me;
        glGenVertexArrays(1, &me.vao);
        glBindVertexArray(me.vao);
        glGenBuffers(1, &me.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, me.vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * sizeof(float)), v.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &me.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, me.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size() * sizeof(uint32_t)), idx.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void*)12);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void*)24);
        glBindVertexArray(0);
        me.count = (GLsizei)idx.size();
        me.mode = mode;
        return me;
    }

    static Mesh uploadSkinned(const std::vector<float>& v, const std::vector<float>& sk, const std::vector<uint32_t>& idx) {
        size_t n = v.size() / 8;
        std::vector<float> all(n * 16);
        for (size_t i = 0; i < n; i++) {
            std::memcpy(&all[i * 16], &v[i * 8], 8 * sizeof(float));
            std::memcpy(&all[i * 16 + 8], &sk[i * 8], 8 * sizeof(float));
        }
        Mesh me;
        glGenVertexArrays(1, &me.vao);
        glBindVertexArray(me.vao);
        glGenBuffers(1, &me.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, me.vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(all.size() * sizeof(float)), all.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &me.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, me.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size() * sizeof(uint32_t)), idx.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 64, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 64, (void*)12);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 64, (void*)24);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 64, (void*)32);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 64, (void*)48);
        glBindVertexArray(0);
        me.count = (GLsizei)idx.size();
        me.mode = GL_TRIANGLES;
        return me;
    }

    static Mesh makeLines(const std::vector<Vec3>& pts) {
        std::vector<float> v;
        std::vector<uint32_t> idx;
        for (size_t i = 0; i < pts.size(); i++) {
            pushV(v, pts[i], Vec3(0, 1, 0));
            idx.push_back((uint32_t)i);
        }
        return upload(v, idx, GL_LINES);
    }

    static Mesh makeCube() {
        std::vector<float> v;
        std::vector<uint32_t> idx;
        const Vec3 N[6] = {Vec3(1,0,0), Vec3(-1,0,0), Vec3(0,1,0), Vec3(0,-1,0), Vec3(0,0,1), Vec3(0,0,-1)};
        const Vec3 U[6] = {Vec3(0,1,0), Vec3(0,1,0), Vec3(0,0,1), Vec3(0,0,1), Vec3(0,1,0), Vec3(0,1,0)};
        const float sa[4] = {-1, 1, 1, -1};
        const float sb[4] = {-1, -1, 1, 1};
        for (int f = 0; f < 6; f++) {
            Vec3 n = N[f], u = U[f], w = cross(n, u);
            uint32_t base = (uint32_t)(v.size() / 8);
            for (int i = 0; i < 4; i++)
                pushV(v, n * 0.5f + u * (sa[i] * 0.5f) + w * (sb[i] * 0.5f), n, sa[i] * 0.5f + 0.5f, sb[i] * 0.5f + 0.5f);
            idx.push_back(base); idx.push_back(base + 1); idx.push_back(base + 2);
            idx.push_back(base); idx.push_back(base + 2); idx.push_back(base + 3);
        }
        return upload(v, idx, GL_TRIANGLES);
    }

    static Mesh makePlane() {
        std::vector<float> v;
        std::vector<uint32_t> idx = {0, 1, 2, 0, 2, 3};
        Vec3 n(0, 1, 0);
        pushV(v, Vec3(-0.5f, 0, -0.5f), n, 0, 0);
        pushV(v, Vec3(-0.5f, 0, 0.5f), n, 0, 1);
        pushV(v, Vec3(0.5f, 0, 0.5f), n, 1, 1);
        pushV(v, Vec3(0.5f, 0, -0.5f), n, 1, 0);
        return upload(v, idx, GL_TRIANGLES);
    }

    static Mesh makeSphere() {
        std::vector<float> v;
        std::vector<uint32_t> idx;
        const int rings = 24, sectors = 36;
        const float PI = 3.14159265f;
        for (int r = 0; r <= rings; r++) {
            float th = PI * r / rings;
            for (int s = 0; s <= sectors; s++) {
                float ph = 2 * PI * s / sectors;
                Vec3 n(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
                pushV(v, n * 0.5f, n, (float)s / sectors, (float)r / rings);
            }
        }
        for (int r = 0; r < rings; r++)
            for (int s = 0; s < sectors; s++) {
                uint32_t a = (uint32_t)(r * (sectors + 1) + s);
                uint32_t b = a + sectors + 1;
                idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
                idx.push_back(a + 1); idx.push_back(b); idx.push_back(b + 1);
            }
        return upload(v, idx, GL_TRIANGLES);
    }

    static Mesh makeCylinder() {
        std::vector<float> v;
        std::vector<uint32_t> idx;
        const int S = 32;
        const float PI = 3.14159265f;
        for (int i = 0; i <= S; i++) {
            float a = 2 * PI * i / S;
            Vec3 n(std::cos(a), 0, std::sin(a));
            pushV(v, Vec3(n.x * 0.5f, 0.5f, n.z * 0.5f), n, (float)i / S, 0);
            pushV(v, Vec3(n.x * 0.5f, -0.5f, n.z * 0.5f), n, (float)i / S, 1);
        }
        for (int i = 0; i < S; i++) {
            uint32_t t = (uint32_t)(2 * i);
            idx.push_back(t); idx.push_back(t + 1); idx.push_back(t + 2);
            idx.push_back(t + 1); idx.push_back(t + 3); idx.push_back(t + 2);
        }
        for (int sgn = 1; sgn >= -1; sgn -= 2) {
            Vec3 n(0, (float)sgn, 0);
            uint32_t c = (uint32_t)(v.size() / 8);
            pushV(v, Vec3(0, 0.5f * sgn, 0), n, 0.5f, 0.5f);
            uint32_t r0 = (uint32_t)(v.size() / 8);
            for (int i = 0; i <= S; i++) {
                float a = 2 * PI * i / S;
                pushV(v, Vec3(std::cos(a) * 0.5f, 0.5f * sgn, std::sin(a) * 0.5f), n,
                      std::cos(a) * 0.5f + 0.5f, std::sin(a) * 0.5f + 0.5f);
            }
            for (int i = 0; i < S; i++) {
                idx.push_back(c); idx.push_back(r0 + i); idx.push_back(r0 + i + 1);
            }
        }
        return upload(v, idx, GL_TRIANGLES);
    }

    // Carrega .obj (v, vt, vn, f). Normaliza para caber em 1 unidade, centralizado.
    static bool loadObj(const std::string& path, std::vector<float>& out, std::vector<uint32_t>& idx, Vec3& half) {
        std::string text;
        if (!fsu::readFile(path, text)) return false;
        std::vector<Vec3> P, N;
        std::vector<float> T;
        std::vector<Vec3> opos, onor;
        std::vector<float> ouv;
        std::vector<char> hasN;
        std::map<std::array<int, 3>, uint32_t> cache;
        std::istringstream ss(text);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.size() < 2) continue;
            std::istringstream ls(line);
            std::string tag;
            ls >> tag;
            if (tag == "v") { float x = 0, y = 0, z = 0; ls >> x >> y >> z; P.push_back(Vec3(x, y, z)); }
            else if (tag == "vn") { float x = 0, y = 0, z = 0; ls >> x >> y >> z; N.push_back(Vec3(x, y, z)); }
            else if (tag == "vt") { float u = 0, w = 0; ls >> u >> w; T.push_back(u); T.push_back(1.0f - w); }
            else if (tag == "f") {
                std::vector<uint32_t> fi;
                std::string tok;
                while (ls >> tok) {
                    int vi = 0, ti = 0, ni = 0;
                    size_t s1 = tok.find('/');
                    vi = std::atoi(tok.substr(0, s1).c_str());
                    if (s1 != std::string::npos) {
                        size_t s2 = tok.find('/', s1 + 1);
                        std::string ts = tok.substr(s1 + 1, s2 == std::string::npos ? std::string::npos : s2 - s1 - 1);
                        if (!ts.empty()) ti = std::atoi(ts.c_str());
                        if (s2 != std::string::npos) {
                            std::string ns = tok.substr(s2 + 1);
                            if (!ns.empty()) ni = std::atoi(ns.c_str());
                        }
                    }
                    int pv = vi > 0 ? vi - 1 : (int)P.size() + vi;
                    int pt = ti == 0 ? -1 : (ti > 0 ? ti - 1 : (int)(T.size() / 2) + ti);
                    int pn = ni == 0 ? -1 : (ni > 0 ? ni - 1 : (int)N.size() + ni);
                    if (pv < 0 || pv >= (int)P.size()) continue;
                    if (pt >= (int)(T.size() / 2) || pt < -1) pt = -1;
                    if (pn >= (int)N.size() || pn < -1) pn = -1;
                    std::array<int, 3> key = {pv, pt, pn};
                    auto it = cache.find(key);
                    if (it == cache.end()) {
                        uint32_t id = (uint32_t)opos.size();
                        opos.push_back(P[pv]);
                        onor.push_back(pn >= 0 ? N[pn] : Vec3());
                        hasN.push_back(pn >= 0 ? 1 : 0);
                        ouv.push_back(pt >= 0 ? T[pt * 2] : 0.0f);
                        ouv.push_back(pt >= 0 ? T[pt * 2 + 1] : 0.0f);
                        cache[key] = id;
                        fi.push_back(id);
                    } else fi.push_back(it->second);
                }
                for (size_t i = 1; i + 1 < fi.size(); i++) {
                    idx.push_back(fi[0]); idx.push_back(fi[i]); idx.push_back(fi[i + 1]);
                }
            }
        }
        if (opos.empty() || idx.empty()) return false;
        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            Vec3 a = opos[idx[i]], b = opos[idx[i + 1]], c = opos[idx[i + 2]];
            Vec3 fn = cross(b - a, c - a);
            for (int k = 0; k < 3; k++) if (!hasN[idx[i + k]]) onor[idx[i + k]] += fn;
        }
        for (auto& n : onor) {
            float l = length(n);
            n = l > 1e-8f ? n * (1.0f / l) : Vec3(0, 1, 0);
        }
        Vec3 mn = opos[0], mx = opos[0];
        for (auto& p : opos) {
            mn = Vec3(std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z));
            mx = Vec3(std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z));
        }
        Vec3 ctr = (mn + mx) * 0.5f, ext = mx - mn;
        float big = std::max(ext.x, std::max(ext.y, ext.z));
        if (big < 1e-8f) big = 1.0f;
        float sc = 1.0f / big;
        out.reserve(opos.size() * 8);
        for (size_t i = 0; i < opos.size(); i++) pushV(out, (opos[i] - ctr) * sc, onor[i], ouv[i * 2], ouv[i * 2 + 1]);
        half = Vec3(ext.x * sc * 0.5f, ext.y * sc * 0.5f, ext.z * sc * 0.5f);
        return true;
    }
};
