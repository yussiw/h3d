#pragma once
// Geometria das primitivas (cubo, esfera, cilindro) no CPU, no mesmo formato do glTF carregado.
#include <vector>
#include <cmath>
#include "gltf.h"

inline void pgPush(std::vector<float>& v, const Vec3& p, const Vec3& n, float u, float w) {
    v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
    v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
    v.push_back(u); v.push_back(w);
}

inline GPrimCpu primGeometry(int shape) {   // 0 cubo, 1 esfera, 2 cilindro
    GPrimCpu g;
    const float PI = 3.14159265f;
    if (shape == 1) {
        const int rings = 12, sectors = 20;
        for (int r = 0; r <= rings; r++) {
            float th = PI * r / rings;
            for (int s = 0; s <= sectors; s++) {
                float ph = 2 * PI * s / sectors;
                Vec3 n(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
                pgPush(g.v, n * 0.5f, n, (float)s / sectors, (float)r / rings);
            }
        }
        for (int r = 0; r < rings; r++)
            for (int s = 0; s < sectors; s++) {
                uint32_t a = (uint32_t)(r * (sectors + 1) + s), b = a + sectors + 1;
                g.idx.insert(g.idx.end(), {a, b, a + 1, a + 1, b, b + 1});
            }
    } else if (shape == 2) {
        const int S = 24;
        for (int i = 0; i <= S; i++) {
            float a = 2 * PI * i / S;
            Vec3 n(std::cos(a), 0, std::sin(a));
            pgPush(g.v, Vec3(n.x * 0.5f, 0.5f, n.z * 0.5f), n, (float)i / S, 0);
            pgPush(g.v, Vec3(n.x * 0.5f, -0.5f, n.z * 0.5f), n, (float)i / S, 1);
        }
        for (int i = 0; i < S; i++) {
            uint32_t t = (uint32_t)(2 * i);
            g.idx.insert(g.idx.end(), {t, t + 1, t + 2, t + 1, t + 3, t + 2});
        }
        for (int sgn = 1; sgn >= -1; sgn -= 2) {
            Vec3 n(0, (float)sgn, 0);
            uint32_t c = (uint32_t)(g.v.size() / 8);
            pgPush(g.v, Vec3(0, 0.5f * sgn, 0), n, 0.5f, 0.5f);
            uint32_t r0 = (uint32_t)(g.v.size() / 8);
            for (int i = 0; i <= S; i++) {
                float a = 2 * PI * i / S;
                pgPush(g.v, Vec3(std::cos(a) * 0.5f, 0.5f * sgn, std::sin(a) * 0.5f), n, std::cos(a) * 0.5f + 0.5f, std::sin(a) * 0.5f + 0.5f);
            }
            for (int i = 0; i < S; i++) g.idx.insert(g.idx.end(), {c, r0 + i, r0 + i + 1});
        }
    } else {
        const Vec3 N[6] = {Vec3(1,0,0), Vec3(-1,0,0), Vec3(0,1,0), Vec3(0,-1,0), Vec3(0,0,1), Vec3(0,0,-1)};
        const Vec3 U[6] = {Vec3(0,1,0), Vec3(0,1,0), Vec3(0,0,1), Vec3(0,0,1), Vec3(0,1,0), Vec3(0,1,0)};
        const float sa[4] = {-1, 1, 1, -1}, sb[4] = {-1, -1, 1, 1};
        for (int f = 0; f < 6; f++) {
            Vec3 n = N[f], u = U[f], w = cross(n, u);
            uint32_t base = (uint32_t)(g.v.size() / 8);
            for (int i = 0; i < 4; i++)
                pgPush(g.v, n * 0.5f + u * (sa[i] * 0.5f) + w * (sb[i] * 0.5f), n, sa[i] * 0.5f + 0.5f, sb[i] * 0.5f + 0.5f);
            g.idx.insert(g.idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
    }
    return g;
}
