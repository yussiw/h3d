#pragma once
// Fisica simples: corpos estaticos e dinamicos, colisores caixa (alinhada aos eixos) e esfera.
#include <vector>
#include <set>
#include <utility>
#include <functional>
#include <cmath>
#include <algorithm>
#include "math3d.h"
#include "scene.h"

struct Collider {
    bool sphere = false;
    Vec3 c, half;
    float r = 0.5f;
};

struct PhysicsWorld {
    float gravity = 9.8f;
    std::function<Vec3(const Object&)> halfOf;   // meia-extensao (mundo) de caixa do objeto
    std::vector<std::pair<int, int>> enter;      // contatos novos neste passo (uids)

    void step(std::vector<Object>& objs, float dt) {
        enter.clear();
        if (dt <= 0) return;
        const int sub = 2;
        float h = dt / sub;
        std::set<std::pair<int, int>> cur;
        for (auto& o : objs) o.grounded = false;
        for (int s = 0; s < sub; s++) {
            for (auto& o : objs) {
                if (o.physics != PHYS_DYNAMIC || o.shape == SHAPE_LIGHT) continue;
                if (o.useGravity) o.vel.y -= gravity * h;
                o.pos += o.vel * h;
            }
            for (size_t i = 0; i < objs.size(); i++) {
                Object& a = objs[i];
                if (!active(a)) continue;
                for (size_t j = i + 1; j < objs.size(); j++) {
                    Object& b = objs[j];
                    if (!active(b)) continue;
                    if (a.physics != PHYS_DYNAMIC && b.physics != PHYS_DYNAMIC) continue;
                    if (resolve(a, b, h)) cur.insert(std::make_pair(std::min(a.uid, b.uid), std::max(a.uid, b.uid)));
                }
            }
        }
        for (auto& c : cur) if (!prev.count(c)) enter.push_back(c);
        prev = cur;
    }

    void reset() { prev.clear(); enter.clear(); }

private:
    std::set<std::pair<int, int>> prev;

    static bool active(const Object& o) { return o.physics != PHYS_NONE && o.shape != SHAPE_LIGHT && o.visible; }

    Collider make(const Object& o) const {
        Collider c;
        c.c = o.pos;
        if (o.shape == SHAPE_SPHERE) {
            c.sphere = true;
            c.r = 0.5f * std::max(std::fabs(o.scl.x), std::max(std::fabs(o.scl.y), std::fabs(o.scl.z)));
        } else {
            c.half = halfOf ? halfOf(o) : Vec3(0.5f * std::fabs(o.scl.x), 0.5f * std::fabs(o.scl.y), 0.5f * std::fabs(o.scl.z));
        }
        return c;
    }

    // normal aponta de b para a
    static bool contact(const Collider& a, const Collider& b, Vec3& n, float& pen) {
        if (a.sphere && b.sphere) {
            Vec3 d = a.c - b.c;
            float dist = length(d), rr = a.r + b.r;
            if (dist >= rr) return false;
            n = dist > 1e-6f ? d * (1.0f / dist) : Vec3(0, 1, 0);
            pen = rr - dist;
            return true;
        }
        if (!a.sphere && !b.sphere) {
            Vec3 d = a.c - b.c;
            float ox = a.half.x + b.half.x - std::fabs(d.x);
            float oy = a.half.y + b.half.y - std::fabs(d.y);
            float oz = a.half.z + b.half.z - std::fabs(d.z);
            if (ox <= 0 || oy <= 0 || oz <= 0) return false;
            if (oy <= ox && oy <= oz) { n = Vec3(0, d.y >= 0 ? 1.0f : -1.0f, 0); pen = oy; }
            else if (ox <= oz) { n = Vec3(d.x >= 0 ? 1.0f : -1.0f, 0, 0); pen = ox; }
            else { n = Vec3(0, 0, d.z >= 0 ? 1.0f : -1.0f); pen = oz; }
            return true;
        }
        if (!a.sphere && b.sphere) {
            bool ok = contact(b, a, n, pen);
            n = n * -1.0f;
            return ok;
        }
        // a esfera, b caixa
        Vec3 lo = b.c - b.half, hi = b.c + b.half;
        Vec3 p(std::max(lo.x, std::min(hi.x, a.c.x)), std::max(lo.y, std::min(hi.y, a.c.y)), std::max(lo.z, std::min(hi.z, a.c.z)));
        Vec3 d = a.c - p;
        float dist = length(d);
        if (dist > 1e-6f) {
            if (dist >= a.r) return false;
            n = d * (1.0f / dist);
            pen = a.r - dist;
            return true;
        }
        Vec3 dc = a.c - b.c;
        float ox = b.half.x + a.r - std::fabs(dc.x), oy = b.half.y + a.r - std::fabs(dc.y), oz = b.half.z + a.r - std::fabs(dc.z);
        if (oy <= ox && oy <= oz) { n = Vec3(0, dc.y >= 0 ? 1.0f : -1.0f, 0); pen = oy; }
        else if (ox <= oz) { n = Vec3(dc.x >= 0 ? 1.0f : -1.0f, 0, 0); pen = ox; }
        else { n = Vec3(0, 0, dc.z >= 0 ? 1.0f : -1.0f); pen = oz; }
        return true;
    }

    bool resolve(Object& a, Object& b, float h) {
        Collider ca = make(a), cb = make(b);
        Vec3 n;
        float pen;
        if (!contact(ca, cb, n, pen)) return false;
        float ia = a.physics == PHYS_DYNAMIC ? 1.0f / std::max(a.mass, 0.001f) : 0.0f;
        float ib = b.physics == PHYS_DYNAMIC ? 1.0f / std::max(b.mass, 0.001f) : 0.0f;
        float tot = ia + ib;
        if (tot <= 0) return true;
        a.pos += n * (pen * ia / tot);
        b.pos -= n * (pen * ib / tot);
        Vec3 rel = a.vel - b.vel;
        float vn = dot(rel, n);
        if (vn < 0) {
            float e = 0.5f * (a.bounce + b.bounce);
            if (-vn < 1.0f) e = 0;
            float j = -(1.0f + e) * vn / tot;
            a.vel += n * (j * ia);
            b.vel -= n * (j * ib);
            Vec3 vt = rel - n * vn;
            float k = std::min(1.0f, 0.5f * (a.friction + b.friction) * 10.0f * h);
            a.vel -= vt * (k * ia / tot);
            b.vel += vt * (k * ib / tot);
        }
        if (n.y > 0.6f) a.grounded = true;
        if (n.y < -0.6f) b.grounded = true;
        return true;
    }
};
