#pragma once
// Carregador de glTF 2.0 / GLB (malhas, materiais, esqueleto e animacoes). Sem dependencia de OpenGL.
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include "math3d.h"
#include "fsutil.h"

// ---------------- JSON minimo ----------------
struct Json {
    enum T { Nul, Bool, Num, Str, Arr, Obj } t = Nul;
    double n = 0;
    bool b = false;
    std::string s;
    std::vector<Json> a;
    std::vector<std::pair<std::string, Json>> o;

    static const Json& nul() { static Json j; return j; }
    const Json& at(const char* k) const { for (auto& kv : o) if (kv.first == k) return kv.second; return nul(); }
    const Json& at(size_t i) const { return i < a.size() ? a[i] : nul(); }
    bool has(const char* k) const { for (auto& kv : o) if (kv.first == k) return true; return false; }
    double num(double d = 0) const { return t == Num ? n : d; }
    int integer(int d = 0) const { return t == Num ? (int)n : d; }
    std::string str() const { return t == Str ? s : std::string(); }
    size_t size() const { return t == Arr ? a.size() : o.size(); }
};

struct JsonParser {
    const char* p;
    const char* e;
    bool ok = true;
    void ws() { while (p < e && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++; }
    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    std::string str() {
        std::string r;
        p++;  // "
        while (p < e && *p != '"') {
            if (*p == '\\' && p + 1 < e) {
                p++;
                char c = *p++;
                if (c == 'n') r += '\n';
                else if (c == 't') r += '\t';
                else if (c == 'r') r += '\r';
                else if (c == 'b') r += '\b';
                else if (c == 'f') r += '\f';
                else if (c == 'u' && p + 4 <= e) {
                    unsigned cp = (unsigned)std::strtoul(std::string(p, 4).c_str(), nullptr, 16);
                    p += 4;
                    utf8(r, cp);
                } else r += c;
            } else r += *p++;
        }
        if (p < e) p++; else ok = false;
        return r;
    }
    Json parse(int depth = 0) {
        Json j;
        ws();
        if (p >= e || depth > 64) { ok = false; return j; }
        char c = *p;
        if (c == '{') {
            p++;
            j.t = Json::Obj;
            ws();
            if (p < e && *p == '}') { p++; return j; }
            while (ok) {
                ws();
                if (p >= e || *p != '"') { ok = false; break; }
                std::string k = str();
                ws();
                if (p >= e || *p != ':') { ok = false; break; }
                p++;
                Json v = parse(depth + 1);
                j.o.push_back(std::make_pair(k, v));
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == '}') { p++; break; }
                ok = false;
            }
        } else if (c == '[') {
            p++;
            j.t = Json::Arr;
            ws();
            if (p < e && *p == ']') { p++; return j; }
            while (ok) {
                j.a.push_back(parse(depth + 1));
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == ']') { p++; break; }
                ok = false;
            }
        } else if (c == '"') {
            j.t = Json::Str;
            j.s = str();
        } else if (c == 't' && e - p >= 4 && !std::strncmp(p, "true", 4)) { j.t = Json::Bool; j.b = true; p += 4; }
        else if (c == 'f' && e - p >= 5 && !std::strncmp(p, "false", 5)) { j.t = Json::Bool; j.b = false; p += 5; }
        else if (c == 'n' && e - p >= 4 && !std::strncmp(p, "null", 4)) { p += 4; }
        else {
            char* end = nullptr;
            std::string tmp(p, std::min<size_t>((size_t)(e - p), 40));
            double v = std::strtod(tmp.c_str(), &end);
            if (end == tmp.c_str()) { ok = false; return j; }
            j.t = Json::Num;
            j.n = v;
            p += (end - tmp.c_str());
        }
        return j;
    }
};

// ---------------- util ----------------
inline std::vector<unsigned char> base64Decode(const std::string& in) {
    static int T[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; i++) T[i] = -1;
        const char* tb = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; i++) T[(unsigned char)tb[i]] = i;
        init = true;
    }
    std::vector<unsigned char> out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (T[c] < 0) continue;
        val = (val << 6) + T[c];
        bits += 6;
        if (bits >= 0) { out.push_back((unsigned char)((val >> bits) & 0xFF)); bits -= 8; }
    }
    return out;
}

inline std::string uriDecode(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) { r += (char)std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
        else r += s[i];
    }
    return r;
}

struct GQuat { float x = 0, y = 0, z = 0, w = 1; };

inline GQuat qNorm(GQuat q) {
    float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (l < 1e-8f) return GQuat();
    q.x /= l; q.y /= l; q.z /= l; q.w /= l;
    return q;
}
inline GQuat qSlerp(const GQuat& a, GQuat b, float t) {
    float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (d < 0) { d = -d; b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w; }
    GQuat r;
    if (d > 0.9995f) {
        r.x = a.x + (b.x - a.x) * t; r.y = a.y + (b.y - a.y) * t; r.z = a.z + (b.z - a.z) * t; r.w = a.w + (b.w - a.w) * t;
        return qNorm(r);
    }
    float th = std::acos(std::min(1.0f, d)), s = std::sin(th);
    float wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
    r.x = a.x * wa + b.x * wb; r.y = a.y * wa + b.y * wb; r.z = a.z * wa + b.z * wb; r.w = a.w * wa + b.w * wb;
    return qNorm(r);
}
inline Mat4 quatToMat(const GQuat& q0) {
    GQuat q = qNorm(q0);
    float x = q.x, y = q.y, z = q.z, w = q.w;
    Mat4 m = Mat4::identity();
    m.m[0] = 1 - 2 * (y * y + z * z); m.m[1] = 2 * (x * y + z * w);     m.m[2] = 2 * (x * z - y * w);
    m.m[4] = 2 * (x * y - z * w);     m.m[5] = 1 - 2 * (x * x + z * z); m.m[6] = 2 * (y * z + x * w);
    m.m[8] = 2 * (x * z + y * w);     m.m[9] = 2 * (y * z - x * w);     m.m[10] = 1 - 2 * (x * x + y * y);
    return m;
}
inline Vec3 xformPoint(const Mat4& m, const Vec3& p) {
    return Vec3(m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
                m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
                m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]);
}

// ---------------- estruturas ----------------
struct GNode {
    std::string name;
    int parent = -1;
    std::vector<int> children;
    Vec3 t, s = Vec3(1, 1, 1);
    GQuat r;
    bool hasMatrix = false;
    Mat4 mat = Mat4::identity();
    int mesh = -1, skin = -1;
};
struct GPrimCpu {
    std::vector<float> v;    // 8 floats por vertice: pos, normal, uv
    std::vector<float> sk;   // 8 floats: 4 juntas + 4 pesos (vazio se nao tiver pele)
    std::vector<uint32_t> idx;
    int material = -1;
    bool skinned() const { return !sk.empty(); }
};
struct GMat {
    float base[4] = {1, 1, 1, 1};
    float metal = 0.0f, rough = 1.0f;
    float emis[3] = {0, 0, 0};
    int image = -1;
};
struct GSkin {
    std::vector<int> joints;
    std::vector<Mat4> ibm;
};
struct GSampler {
    std::vector<float> times, values;
    int interp = 0;   // 0 linear, 1 step, 2 cubic
    int comps = 3;
};
struct GChannel { int node = -1, path = 0, sampler = 0; };  // path: 0 T, 1 R, 2 S
struct GAnim {
    std::string name;
    float duration = 0;
    std::vector<GSampler> samplers;
    std::vector<GChannel> channels;
};
struct GImage { std::vector<unsigned char> bytes; };
struct Pose { std::vector<Vec3> t, s; std::vector<GQuat> r; };

struct Gltf {
    std::vector<GNode> nodes;
    std::vector<int> roots;
    std::vector<std::vector<GPrimCpu>> meshes;
    std::vector<GMat> mats;
    std::vector<GSkin> skins;
    std::vector<GAnim> anims;
    std::vector<GImage> images;
    Vec3 center;
    float scale = 1.0f;
    Vec3 half = Vec3(0.5f, 0.5f, 0.5f);

    const GAnim* find(const std::string& name) const {
        for (auto& a : anims) if (a.name == name) return &a;
        return nullptr;
    }

    void restPose(Pose& p) const {
        size_t n = nodes.size();
        p.t.resize(n); p.s.resize(n); p.r.resize(n);
        for (size_t i = 0; i < n; i++) { p.t[i] = nodes[i].t; p.s[i] = nodes[i].s; p.r[i] = nodes[i].r; }
    }

    void sample(const GAnim& an, float time, Pose& p) const {
        for (const GChannel& ch : an.channels) {
            if (ch.node < 0 || ch.node >= (int)p.t.size() || ch.sampler < 0 || ch.sampler >= (int)an.samplers.size()) continue;
            const GSampler& s = an.samplers[ch.sampler];
            size_t n = s.times.size();
            if (n == 0) continue;
            float t = std::max(s.times.front(), std::min(s.times.back(), time));
            size_t i = 0;
            while (i + 1 < n && s.times[i + 1] <= t) i++;
            size_t i2 = std::min(i + 1, n - 1);
            float f = (i2 == i) ? 0.0f : (t - s.times[i]) / (s.times[i2] - s.times[i]);
            if (s.interp == 1) f = 0;
            int c = s.comps;
            auto at = [&](size_t k) -> size_t { return s.interp == 2 ? (k * 3 + 1) * (size_t)c : k * (size_t)c; };
            size_t a0 = at(i), a1 = at(i2);
            if (a1 + c > s.values.size() || a0 + c > s.values.size()) continue;
            const float* v0 = &s.values[a0];
            const float* v1 = &s.values[a1];
            if (ch.path == 1 && c == 4) {
                GQuat q0{v0[0], v0[1], v0[2], v0[3]}, q1{v1[0], v1[1], v1[2], v1[3]};
                p.r[ch.node] = qSlerp(qNorm(q0), qNorm(q1), f);
            } else if (c == 3) {
                Vec3 r(v0[0] + (v1[0] - v0[0]) * f, v0[1] + (v1[1] - v0[1]) * f, v0[2] + (v1[2] - v0[2]) * f);
                if (ch.path == 0) p.t[ch.node] = r; else if (ch.path == 2) p.s[ch.node] = r;
            }
        }
    }

    // out = mistura de a (w=0) para b (w=1)
    static void blend(Pose& out, const Pose& a, const Pose& b, float w) {
        size_t n = std::min(a.t.size(), b.t.size());
        out.t.resize(n); out.s.resize(n); out.r.resize(n);
        for (size_t i = 0; i < n; i++) {
            out.t[i] = a.t[i] + (b.t[i] - a.t[i]) * w;
            out.s[i] = a.s[i] + (b.s[i] - a.s[i]) * w;
            out.r[i] = qSlerp(a.r[i], b.r[i], w);
        }
    }

    void globals(const Pose& p, std::vector<Mat4>& g) const {
        g.assign(nodes.size(), Mat4::identity());
        for (int r : roots) rec(r, Mat4::identity(), p, g, 0);
    }

    void jointMatrices(int skin, const std::vector<Mat4>& g, std::vector<float>& out) const {
        out.clear();
        if (skin < 0 || skin >= (int)skins.size()) return;
        const GSkin& sk = skins[skin];
        out.resize(sk.joints.size() * 16);
        for (size_t j = 0; j < sk.joints.size(); j++) {
            Mat4 m = Mat4::identity();
            int jn = sk.joints[j];
            if (jn >= 0 && jn < (int)g.size()) m = g[jn];
            if (j < sk.ibm.size()) m = m * sk.ibm[j];
            std::memcpy(&out[j * 16], m.m, sizeof(float) * 16);
        }
    }

    // ---------------- carregar ----------------
    bool load(const std::string& path, std::string& err) {
        std::string data;
        if (!fsu::readFile(path, data)) { err = "nao foi possivel ler " + path; return false; }
        std::string dir = ".";
        size_t sl = path.rfind('/');
        if (sl != std::string::npos) dir = path.substr(0, sl);

        std::string jsonText;
        std::vector<unsigned char> glbBin;
        if (data.size() >= 12 && std::memcmp(data.data(), "glTF", 4) == 0) {
            size_t off = 12;
            while (off + 8 <= data.size()) {
                uint32_t len, type;
                std::memcpy(&len, data.data() + off, 4);
                std::memcpy(&type, data.data() + off + 4, 4);
                off += 8;
                if (off + len > data.size()) break;
                if (type == 0x4E4F534A) jsonText.assign(data.data() + off, len);
                else if (type == 0x004E4942 && glbBin.empty()) glbBin.assign(data.begin() + off, data.begin() + off + len);
                off += (len + 3) & ~3u;
            }
        } else jsonText = data;
        if (jsonText.empty()) { err = "arquivo glTF/GLB invalido"; return false; }

        JsonParser jp;
        jp.p = jsonText.data();
        jp.e = jsonText.data() + jsonText.size();
        Json root = jp.parse();
        if (!jp.ok || root.t != Json::Obj) { err = "JSON do glTF invalido"; return false; }

        if (root.at("extensionsRequired").size() > 0) {
            for (auto& x : root.at("extensionsRequired").a) {
                std::string e = x.str();
                if (e.find("draco") != std::string::npos || e.find("meshopt") != std::string::npos || e.find("quantiz") != std::string::npos) {
                    err = "extensao nao suportada: " + e + " (exporte sem compressao)";
                    return false;
                }
            }
        }

        // buffers
        std::vector<std::vector<unsigned char>> bufs;
        for (auto& b : root.at("buffers").a) {
            std::string uri = b.at("uri").str();
            std::vector<unsigned char> d;
            if (uri.empty()) d = glbBin;
            else if (uri.compare(0, 5, "data:") == 0) {
                size_t c = uri.find(',');
                if (c != std::string::npos) d = base64Decode(uri.substr(c + 1));
            } else {
                std::string raw;
                if (fsu::readFile(dir + "/" + uriDecode(uri), raw)) d.assign(raw.begin(), raw.end());
            }
            bufs.push_back(d);
        }
        const Json& views = root.at("bufferViews");
        const Json& accs = root.at("accessors");

        auto viewBytes = [&](int vi, std::vector<unsigned char>& out) {
            const Json& v = views.at((size_t)vi);
            int bi = v.at("buffer").integer(-1);
            if (bi < 0 || bi >= (int)bufs.size()) return false;
            size_t off = (size_t)v.at("byteOffset").num(0), len = (size_t)v.at("byteLength").num(0);
            if (off + len > bufs[bi].size()) return false;
            out.assign(bufs[bi].begin() + off, bufs[bi].begin() + off + len);
            return true;
        };

        auto readAcc = [&](int idx, std::vector<float>& out, int& comps) -> bool {
            if (idx < 0 || idx >= (int)accs.size()) return false;
            const Json& a = accs.at((size_t)idx);
            int count = a.at("count").integer();
            int ct = a.at("componentType").integer();
            std::string ty = a.at("type").str();
            comps = ty == "SCALAR" ? 1 : ty == "VEC2" ? 2 : ty == "VEC3" ? 3 : ty == "VEC4" ? 4 : ty == "MAT2" ? 4 : ty == "MAT3" ? 9 : ty == "MAT4" ? 16 : 0;
            if (comps == 0 || count < 0) return false;
            bool norm = a.at("normalized").b;
            out.assign((size_t)count * comps, 0.0f);
            if (!a.has("bufferView")) return true;
            int bv = a.at("bufferView").integer(-1);
            const Json& v = views.at((size_t)bv);
            int bi = v.at("buffer").integer(-1);
            if (bi < 0 || bi >= (int)bufs.size()) return false;
            size_t off = (size_t)v.at("byteOffset").num(0) + (size_t)a.at("byteOffset").num(0);
            size_t stride = (size_t)v.at("byteStride").num(0);
            size_t cs = (ct == 5126 || ct == 5125) ? 4 : (ct == 5123 || ct == 5122) ? 2 : 1;
            if (stride == 0) stride = cs * comps;
            const std::vector<unsigned char>& B = bufs[bi];
            for (int i = 0; i < count; i++) {
                for (int c = 0; c < comps; c++) {
                    size_t pos = off + (size_t)i * stride + (size_t)c * cs;
                    if (pos + cs > B.size()) return false;
                    const unsigned char* q = &B[pos];
                    float f = 0;
                    if (ct == 5126) std::memcpy(&f, q, 4);
                    else if (ct == 5121) f = norm ? q[0] / 255.0f : (float)q[0];
                    else if (ct == 5120) { int8_t x = (int8_t)q[0]; f = norm ? std::max(x / 127.0f, -1.0f) : (float)x; }
                    else if (ct == 5123) { uint16_t x; std::memcpy(&x, q, 2); f = norm ? x / 65535.0f : (float)x; }
                    else if (ct == 5122) { int16_t x; std::memcpy(&x, q, 2); f = norm ? std::max(x / 32767.0f, -1.0f) : (float)x; }
                    else if (ct == 5125) { uint32_t x; std::memcpy(&x, q, 4); f = (float)x; }
                    out[(size_t)i * comps + c] = f;
                }
            }
            return true;
        };

        // imagens
        std::vector<int> texToImage;
        for (auto& t : root.at("textures").a) texToImage.push_back(t.at("source").integer(-1));
        for (auto& im : root.at("images").a) {
            GImage gi;
            std::string uri = im.at("uri").str();
            if (!uri.empty()) {
                if (uri.compare(0, 5, "data:") == 0) {
                    size_t c = uri.find(',');
                    if (c != std::string::npos) gi.bytes = base64Decode(uri.substr(c + 1));
                } else {
                    std::string raw;
                    if (fsu::readFile(dir + "/" + uriDecode(uri), raw)) gi.bytes.assign(raw.begin(), raw.end());
                }
            } else if (im.has("bufferView")) viewBytes(im.at("bufferView").integer(), gi.bytes);
            images.push_back(gi);
        }

        // materiais
        for (auto& m : root.at("materials").a) {
            GMat gm;
            const Json& pbr = m.at("pbrMetallicRoughness");
            const Json& bc = pbr.at("baseColorFactor");
            for (int i = 0; i < 4 && (size_t)i < bc.size(); i++) gm.base[i] = (float)bc.at((size_t)i).num(1);
            gm.metal = (float)pbr.at("metallicFactor").num(1.0);
            gm.rough = (float)pbr.at("roughnessFactor").num(1.0);
            int ti = pbr.at("baseColorTexture").at("index").integer(-1);
            if (ti >= 0 && ti < (int)texToImage.size()) gm.image = texToImage[ti];
            const Json& em = m.at("emissiveFactor");
            for (int i = 0; i < 3 && (size_t)i < em.size(); i++) gm.emis[i] = (float)em.at((size_t)i).num(0);
            mats.push_back(gm);
        }

        // malhas
        for (auto& m : root.at("meshes").a) {
            std::vector<GPrimCpu> prims;
            for (auto& pr : m.at("primitives").a) {
                if (pr.at("mode").integer(4) != 4) continue;
                const Json& at = pr.at("attributes");
                std::vector<float> pos, nor, uv, jn, wt;
                int c;
                if (!readAcc(at.at("POSITION").integer(-1), pos, c) || c != 3) continue;
                size_t nv = pos.size() / 3;
                bool hasN = readAcc(at.at("NORMAL").integer(-1), nor, c) && c == 3 && nor.size() == nv * 3;
                bool hasUV = readAcc(at.at("TEXCOORD_0").integer(-1), uv, c) && c == 2 && uv.size() == nv * 2;
                bool hasSk = readAcc(at.at("JOINTS_0").integer(-1), jn, c) && c == 4 && jn.size() == nv * 4 &&
                             readAcc(at.at("WEIGHTS_0").integer(-1), wt, c) && c == 4 && wt.size() == nv * 4;
                GPrimCpu g;
                g.material = pr.at("material").integer(-1);
                if (pr.has("indices")) {
                    std::vector<float> ix;
                    if (!readAcc(pr.at("indices").integer(-1), ix, c)) continue;
                    for (float f : ix) g.idx.push_back((uint32_t)f);
                } else for (size_t i = 0; i < nv; i++) g.idx.push_back((uint32_t)i);
                for (uint32_t i : g.idx) if (i >= nv) { g.idx.clear(); break; }
                if (g.idx.empty()) continue;
                std::vector<Vec3> N(nv);
                if (hasN) for (size_t i = 0; i < nv; i++) N[i] = Vec3(nor[i * 3], nor[i * 3 + 1], nor[i * 3 + 2]);
                else {
                    for (size_t i = 0; i + 2 < g.idx.size(); i += 3) {
                        Vec3 a(pos[g.idx[i] * 3], pos[g.idx[i] * 3 + 1], pos[g.idx[i] * 3 + 2]);
                        Vec3 b(pos[g.idx[i + 1] * 3], pos[g.idx[i + 1] * 3 + 1], pos[g.idx[i + 1] * 3 + 2]);
                        Vec3 d(pos[g.idx[i + 2] * 3], pos[g.idx[i + 2] * 3 + 1], pos[g.idx[i + 2] * 3 + 2]);
                        Vec3 fn = cross(b - a, d - a);
                        for (int k = 0; k < 3; k++) N[g.idx[i + k]] += fn;
                    }
                    for (auto& n : N) n = length(n) > 1e-8f ? normalize(n) : Vec3(0, 1, 0);
                }
                g.v.resize(nv * 8);
                for (size_t i = 0; i < nv; i++) {
                    float* o = &g.v[i * 8];
                    o[0] = pos[i * 3]; o[1] = pos[i * 3 + 1]; o[2] = pos[i * 3 + 2];
                    o[3] = N[i].x; o[4] = N[i].y; o[5] = N[i].z;
                    o[6] = hasUV ? uv[i * 2] : 0.0f; o[7] = hasUV ? uv[i * 2 + 1] : 0.0f;
                }
                if (hasSk) {
                    g.sk.resize(nv * 8);
                    for (size_t i = 0; i < nv; i++) {
                        float sum = wt[i * 4] + wt[i * 4 + 1] + wt[i * 4 + 2] + wt[i * 4 + 3];
                        for (int k = 0; k < 4; k++) {
                            g.sk[i * 8 + k] = jn[i * 4 + k];
                            g.sk[i * 8 + 4 + k] = sum > 1e-6f ? wt[i * 4 + k] / sum : (k == 0 ? 1.0f : 0.0f);
                        }
                    }
                }
                prims.push_back(g);
            }
            meshes.push_back(prims);
        }

        // nos
        for (auto& n : root.at("nodes").a) {
            GNode gn;
            gn.name = n.at("name").str();
            gn.mesh = n.at("mesh").integer(-1);
            gn.skin = n.at("skin").integer(-1);
            for (auto& c : n.at("children").a) gn.children.push_back(c.integer());
            const Json& tr = n.at("translation");
            if (tr.size() >= 3) gn.t = Vec3((float)tr.at((size_t)0).num(), (float)tr.at((size_t)1).num(), (float)tr.at((size_t)2).num());
            const Json& sc = n.at("scale");
            if (sc.size() >= 3) gn.s = Vec3((float)sc.at((size_t)0).num(1), (float)sc.at((size_t)1).num(1), (float)sc.at((size_t)2).num(1));
            const Json& ro = n.at("rotation");
            if (ro.size() >= 4) gn.r = qNorm(GQuat{(float)ro.at((size_t)0).num(), (float)ro.at((size_t)1).num(), (float)ro.at((size_t)2).num(), (float)ro.at((size_t)3).num(1)});
            const Json& mt = n.at("matrix");
            if (mt.size() >= 16) {
                gn.hasMatrix = true;
                for (int i = 0; i < 16; i++) gn.mat.m[i] = (float)mt.at((size_t)i).num();
            }
            nodes.push_back(gn);
        }
        for (size_t i = 0; i < nodes.size(); i++)
            for (int c : nodes[i].children) if (c >= 0 && c < (int)nodes.size()) nodes[c].parent = (int)i;
        const Json& scenes = root.at("scenes");
        int si = root.at("scene").integer(0);
        if (scenes.size() > 0) for (auto& x : scenes.at((size_t)si).at("nodes").a) roots.push_back(x.integer());
        if (roots.empty()) for (size_t i = 0; i < nodes.size(); i++) if (nodes[i].parent < 0) roots.push_back((int)i);

        // peles
        for (auto& s : root.at("skins").a) {
            GSkin sk;
            for (auto& j : s.at("joints").a) sk.joints.push_back(j.integer());
            std::vector<float> ib;
            int c;
            if (s.has("inverseBindMatrices") && readAcc(s.at("inverseBindMatrices").integer(-1), ib, c) && c == 16) {
                for (size_t j = 0; j * 16 + 16 <= ib.size(); j++) {
                    Mat4 m;
                    std::memcpy(m.m, &ib[j * 16], sizeof(float) * 16);
                    sk.ibm.push_back(m);
                }
            }
            skins.push_back(sk);
        }

        // animacoes
        int animIdx = 0;
        for (auto& a : root.at("animations").a) {
            GAnim an;
            an.name = a.at("name").str();
            if (an.name.empty()) an.name = "anim" + std::to_string(animIdx);
            animIdx++;
            for (auto& s : a.at("samplers").a) {
                GSampler gs;
                int c;
                std::vector<float> times;
                if (!readAcc(s.at("input").integer(-1), times, c) || c != 1) { an.samplers.push_back(gs); continue; }
                gs.times = times;
                std::vector<float> vals;
                if (readAcc(s.at("output").integer(-1), vals, c)) { gs.values = vals; gs.comps = c; }
                std::string in = s.at("interpolation").str();
                gs.interp = in == "STEP" ? 1 : in == "CUBICSPLINE" ? 2 : 0;
                if (!times.empty()) an.duration = std::max(an.duration, times.back());
                an.samplers.push_back(gs);
            }
            for (auto& ch : a.at("channels").a) {
                GChannel gc;
                gc.sampler = ch.at("sampler").integer(-1);
                gc.node = ch.at("target").at("node").integer(-1);
                std::string path = ch.at("target").at("path").str();
                if (path == "translation") gc.path = 0;
                else if (path == "rotation") gc.path = 1;
                else if (path == "scale") gc.path = 2;
                else continue;
                an.channels.push_back(gc);
            }
            anims.push_back(an);
        }

        if (nodes.empty() && !meshes.empty()) {   // arquivo sem nos: cria um no por malha
            for (size_t i = 0; i < meshes.size(); i++) { GNode gn; gn.mesh = (int)i; nodes.push_back(gn); roots.push_back((int)i); }
        }

        // caixa envolvente (pose de repouso) para normalizar o tamanho
        Pose rest;
        restPose(rest);
        std::vector<Mat4> g;
        globals(rest, g);
        bool any = false;
        Vec3 mn, mx;
        for (size_t i = 0; i < nodes.size(); i++) {
            if (nodes[i].mesh < 0 || nodes[i].mesh >= (int)meshes.size()) continue;
            bool skinned = nodes[i].skin >= 0;
            for (auto& pr : meshes[nodes[i].mesh]) {
                for (size_t k = 0; k + 7 < pr.v.size(); k += 8) {
                    Vec3 p(pr.v[k], pr.v[k + 1], pr.v[k + 2]);
                    if (!skinned) p = xformPoint(g[i], p);
                    if (!any) { mn = mx = p; any = true; }
                    mn = Vec3(std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z));
                    mx = Vec3(std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z));
                }
            }
        }
        if (!any) { err = "o arquivo nao tem malhas"; return false; }
        center = (mn + mx) * 0.5f;
        Vec3 ext = mx - mn;
        float big = std::max(ext.x, std::max(ext.y, ext.z));
        if (big < 1e-8f) big = 1.0f;
        scale = 1.0f / big;
        half = Vec3(ext.x * scale * 0.5f, ext.y * scale * 0.5f, ext.z * scale * 0.5f);
        return true;
    }

private:
    void rec(int i, const Mat4& parent, const Pose& p, std::vector<Mat4>& g, int depth) const {
        if (i < 0 || i >= (int)nodes.size() || depth > 128) return;
        const GNode& n = nodes[i];
        Mat4 local = n.hasMatrix ? n.mat : translateM(p.t[i]) * quatToMat(p.r[i]) * scaleM(p.s[i]);
        g[i] = parent * local;
        for (int c : n.children) rec(c, g[i], p, g, depth + 1);
    }
};
