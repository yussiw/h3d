#pragma once
// Modelo proprio do h3d (.h3dm): ossos (hierarquia), pecas (cubo/esfera/cilindro presas aos ossos)
// e animacoes por keyframes com easing. Converte para a mesma estrutura do glTF (Gltf).
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include "gltf.h"
#include "easing.h"
#include "primgeo.h"
#include "scene.h"

struct HBone { std::string name = "osso"; int parent = -1; Vec3 pos, rot, scl = Vec3(1, 1, 1); };
struct HPart {
    std::string name = "peca";
    int bone = 0, shape = 0;   // shape: 0 cubo, 1 esfera, 2 cilindro
    Vec3 pos, rot, scl = Vec3(1, 1, 1);
    float color[3] = {0.8f, 0.8f, 0.8f};
    float metal = 0.0f, rough = 0.6f;
};
struct HKey { float t = 0; Vec3 pos, rot, scl = Vec3(1, 1, 1); int ease = 0; };
struct HTrack { int bone = 0; std::vector<HKey> keys; };
struct HClip { std::string name = "clipe"; float duration = 1.0f; bool loop = true; std::vector<HTrack> tracks; };

inline GQuat quatFromEuler(const Vec3& deg) {
    auto q = [](float ang, int axis) {
        float h = ang * kDeg2Rad * 0.5f, s = std::sin(h), c = std::cos(h);
        GQuat r;
        r.w = c;
        if (axis == 0) r.x = s; else if (axis == 1) r.y = s; else r.z = s;
        return r;
    };
    auto mul = [](const GQuat& a, const GQuat& b) {
        GQuat r;
        r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
        r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
        r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
        r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
        return r;
    };
    return qNorm(mul(mul(q(deg.z, 2), q(deg.y, 1)), q(deg.x, 0)));   // Rz * Ry * Rx
}

struct H3dModel {
    std::vector<HBone> bones;
    std::vector<HPart> parts;
    std::vector<HClip> clips;

    static H3dModel makeDefault() {
        H3dModel m;
        HBone b;
        b.name = "raiz";
        m.bones.push_back(b);
        HPart p;
        p.name = "corpo";
        p.pos = Vec3(0, 0.5f, 0);
        m.parts.push_back(p);
        return m;
    }

    int findClip(const std::string& n) const {
        for (size_t i = 0; i < clips.size(); i++) if (clips[i].name == n) return (int)i;
        return -1;
    }

    HTrack* track(HClip& c, int bone, bool create) {
        for (auto& t : c.tracks) if (t.bone == bone) return &t;
        if (!create) return nullptr;
        HTrack t;
        t.bone = bone;
        c.tracks.push_back(t);
        return &c.tracks.back();
    }

    // valores do osso no tempo t (repouso se nao houver trilha)
    void sampleBone(const HClip* c, int bone, float t, Vec3& p, Vec3& r, Vec3& s) const {
        p = bones[bone].pos; r = bones[bone].rot; s = bones[bone].scl;
        if (!c) return;
        const HTrack* tr = nullptr;
        for (auto& x : c->tracks) if (x.bone == bone) { tr = &x; break; }
        if (!tr || tr->keys.empty()) return;
        const std::vector<HKey>& k = tr->keys;
        if (t <= k.front().t) { p = k.front().pos; r = k.front().rot; s = k.front().scl; return; }
        if (t >= k.back().t) { p = k.back().pos; r = k.back().rot; s = k.back().scl; return; }
        size_t i = 0;
        while (i + 1 < k.size() && k[i + 1].t <= t) i++;
        const HKey& a = k[i];
        const HKey& b = k[i + 1];
        float u = (t - a.t) / std::max(1e-6f, b.t - a.t);
        float e = easeApply(a.ease, u);
        p = a.pos + (b.pos - a.pos) * e;
        r = a.rot + (b.rot - a.rot) * e;
        s = a.scl + (b.scl - a.scl) * e;
    }

    // cria/atualiza a key do osso no tempo t
    void setKey(HClip& c, int bone, float t, const Vec3& p, const Vec3& r, const Vec3& s) {
        HTrack* tr = track(c, bone, true);
        for (auto& k : tr->keys) {
            if (std::fabs(k.t - t) < 0.002f) { k.pos = p; k.rot = r; k.scl = s; return; }
        }
        HKey nk;
        nk.t = t; nk.pos = p; nk.rot = r; nk.scl = s;
        size_t at = 0;
        while (at < tr->keys.size() && tr->keys[at].t < t) at++;
        if (at > 0) nk.ease = tr->keys[at - 1].ease;
        tr->keys.insert(tr->keys.begin() + at, nk);
    }

    void sortKeys(HClip& c) {
        for (auto& t : c.tracks) std::sort(t.keys.begin(), t.keys.end(), [](const HKey& a, const HKey& b) { return a.t < b.t; });
    }

    void removeBone(int idx) {
        if (idx < 0 || idx >= (int)bones.size()) return;
        std::vector<char> dead(bones.size(), 0);
        dead[idx] = 1;
        for (bool changed = true; changed;) {
            changed = false;
            for (size_t i = 0; i < bones.size(); i++)
                if (!dead[i] && bones[i].parent >= 0 && dead[bones[i].parent]) { dead[i] = 1; changed = true; }
        }
        std::vector<int> remap(bones.size(), -1);
        std::vector<HBone> nb;
        for (size_t i = 0; i < bones.size(); i++) if (!dead[i]) { remap[i] = (int)nb.size(); nb.push_back(bones[i]); }
        for (auto& b : nb) b.parent = b.parent >= 0 ? remap[b.parent] : -1;
        std::vector<HPart> np;
        for (auto& p : parts) if (!dead[p.bone]) { p.bone = remap[p.bone]; np.push_back(p); }
        for (auto& c : clips) {
            std::vector<HTrack> nt;
            for (auto& t : c.tracks) if (!dead[t.bone]) { t.bone = remap[t.bone]; nt.push_back(t); }
            c.tracks = nt;
        }
        bones = nb;
        parts = np;
    }

    // ---------------- arquivo ----------------
    bool save(const std::string& path) const {
        std::ofstream f(path);
        if (!f) return false;
        f.precision(7);
        f << "H3DM 1\n";
        for (auto& b : bones)
            f << "bone " << encTok(b.name) << ' ' << b.parent << ' ' << b.pos.x << ' ' << b.pos.y << ' ' << b.pos.z << ' '
              << b.rot.x << ' ' << b.rot.y << ' ' << b.rot.z << ' ' << b.scl.x << ' ' << b.scl.y << ' ' << b.scl.z << '\n';
        for (auto& p : parts)
            f << "part " << encTok(p.name) << ' ' << p.bone << ' ' << p.shape << ' ' << p.pos.x << ' ' << p.pos.y << ' ' << p.pos.z << ' '
              << p.rot.x << ' ' << p.rot.y << ' ' << p.rot.z << ' ' << p.scl.x << ' ' << p.scl.y << ' ' << p.scl.z << ' '
              << p.color[0] << ' ' << p.color[1] << ' ' << p.color[2] << ' ' << p.metal << ' ' << p.rough << '\n';
        for (auto& c : clips) {
            f << "clip " << encTok(c.name) << ' ' << c.duration << ' ' << (c.loop ? 1 : 0) << ' ' << c.tracks.size() << '\n';
            for (auto& t : c.tracks) {
                f << "track " << t.bone << ' ' << t.keys.size() << '\n';
                for (auto& k : t.keys)
                    f << "key " << k.t << ' ' << k.pos.x << ' ' << k.pos.y << ' ' << k.pos.z << ' ' << k.rot.x << ' ' << k.rot.y << ' '
                      << k.rot.z << ' ' << k.scl.x << ' ' << k.scl.y << ' ' << k.scl.z << ' ' << k.ease << '\n';
            }
        }
        return (bool)f;
    }

    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        std::string magic;
        int ver = 0;
        f >> magic >> ver;
        if (magic != "H3DM") return false;
        H3dModel m;
        std::string tag;
        while (f >> tag) {
            if (tag == "bone") {
                HBone b; std::string n;
                f >> n >> b.parent >> b.pos.x >> b.pos.y >> b.pos.z >> b.rot.x >> b.rot.y >> b.rot.z >> b.scl.x >> b.scl.y >> b.scl.z;
                b.name = decTok(n);
                m.bones.push_back(b);
            } else if (tag == "part") {
                HPart p; std::string n;
                f >> n >> p.bone >> p.shape >> p.pos.x >> p.pos.y >> p.pos.z >> p.rot.x >> p.rot.y >> p.rot.z >> p.scl.x >> p.scl.y >> p.scl.z
                  >> p.color[0] >> p.color[1] >> p.color[2] >> p.metal >> p.rough;
                p.name = decTok(n);
                m.parts.push_back(p);
            } else if (tag == "clip") {
                HClip c; std::string n; int lp = 1; size_t nt = 0;
                f >> n >> c.duration >> lp >> nt;
                c.name = decTok(n);
                c.loop = lp != 0;
                for (size_t i = 0; i < nt && f; i++) {
                    std::string t2; HTrack tr; size_t nk = 0;
                    f >> t2 >> tr.bone >> nk;
                    for (size_t k = 0; k < nk && k < 100000 && f; k++) {
                        std::string t3; HKey key;
                        f >> t3 >> key.t >> key.pos.x >> key.pos.y >> key.pos.z >> key.rot.x >> key.rot.y >> key.rot.z
                          >> key.scl.x >> key.scl.y >> key.scl.z >> key.ease;
                        tr.keys.push_back(key);
                    }
                    c.tracks.push_back(tr);
                }
                m.clips.push_back(c);
            } else { std::string skip; std::getline(f, skip); }
        }
        if (m.bones.empty()) return false;
        for (auto& b : m.bones) if (b.parent >= (int)m.bones.size()) b.parent = -1;
        for (auto& p : m.parts) if (p.bone < 0 || p.bone >= (int)m.bones.size()) p.bone = 0;
        for (auto& c : m.clips) {
            c.tracks.erase(std::remove_if(c.tracks.begin(), c.tracks.end(), [&](const HTrack& t) { return t.bone < 0 || t.bone >= (int)m.bones.size(); }), c.tracks.end());
            m.sortKeys(c);
        }
        *this = m;
        return true;
    }

    // ---------------- conversao para glTF interno ----------------
    bool toGltf(Gltf& g, std::string& err) const {
        g = Gltf();
        size_t nb = bones.size();
        g.nodes.resize(nb + parts.size());
        for (size_t i = 0; i < nb; i++) {
            GNode& n = g.nodes[i];
            n.name = bones[i].name;
            n.parent = bones[i].parent;
            n.t = bones[i].pos; n.r = quatFromEuler(bones[i].rot); n.s = bones[i].scl;
            if (n.parent >= 0) g.nodes[n.parent].children.push_back((int)i);
            else g.roots.push_back((int)i);
        }
        for (size_t k = 0; k < parts.size(); k++) {
            const HPart& p = parts[k];
            GNode& n = g.nodes[nb + k];
            n.name = p.name;
            n.parent = p.bone;
            n.t = p.pos; n.r = quatFromEuler(p.rot); n.s = p.scl;
            n.mesh = (int)k;
            g.nodes[p.bone].children.push_back((int)(nb + k));
            GPrimCpu geo = primGeometry(p.shape);
            geo.material = (int)k;
            g.meshes.push_back(std::vector<GPrimCpu>{geo});
            GMat m;
            m.base[0] = std::pow(p.color[0], 2.2f); m.base[1] = std::pow(p.color[1], 2.2f); m.base[2] = std::pow(p.color[2], 2.2f);
            m.metal = p.metal; m.rough = p.rough;
            g.mats.push_back(m);
        }
        const float fps = 60.0f;
        for (const HClip& c : clips) {
            GAnim an;
            an.name = c.name;
            an.duration = std::max(0.01f, c.duration);
            int n = (int)std::ceil(an.duration * fps) + 1;
            std::vector<float> times((size_t)n);
            for (int i = 0; i < n; i++) times[(size_t)i] = std::min(an.duration, i / fps);
            for (const HTrack& tr : c.tracks) {
                if (tr.keys.empty()) continue;
                GSampler st, sr, ss;
                st.times = sr.times = ss.times = times;
                st.comps = 3; sr.comps = 4; ss.comps = 3;
                for (int i = 0; i < n; i++) {
                    Vec3 p, r, s;
                    sampleBone(&c, tr.bone, times[(size_t)i], p, r, s);
                    GQuat q = quatFromEuler(r);
                    st.values.insert(st.values.end(), {p.x, p.y, p.z});
                    sr.values.insert(sr.values.end(), {q.x, q.y, q.z, q.w});
                    ss.values.insert(ss.values.end(), {s.x, s.y, s.z});
                }
                int base = (int)an.samplers.size();
                an.samplers.push_back(st); an.samplers.push_back(sr); an.samplers.push_back(ss);
                GChannel ct, cr, cs;
                ct.node = cr.node = cs.node = tr.bone;
                ct.path = 0; cr.path = 1; cs.path = 2;
                ct.sampler = base; cr.sampler = base + 1; cs.sampler = base + 2;
                an.channels.push_back(ct); an.channels.push_back(cr); an.channels.push_back(cs);
            }
            g.anims.push_back(an);
        }
        if (parts.empty()) { err = "o modelo nao tem pecas"; return false; }
        g.center = Vec3(0, 0, 0);
        g.scale = 1.0f;
        g.half = Vec3(0.5f, 0.5f, 0.5f);
        err.clear();
        return true;
    }
};
