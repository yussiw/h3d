#pragma once
#include <cmath>
#include <algorithm>

static const float kDeg2Rad = 3.14159265358979f / 180.0f;

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
};

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
inline float length(const Vec3& a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(const Vec3& a) {
    float l = length(a);
    return l > 1e-8f ? a * (1.0f / l) : Vec3(0, 0, 0);
}

// Matriz 4x4 coluna-major (formato OpenGL)
struct Mat4 {
    float m[16];
    static Mat4 identity() {
        Mat4 r;
        for (int i = 0; i < 16; i++) r.m[i] = 0;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
        return r;
    }
    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int c = 0; c < 4; c++)
            for (int row = 0; row < 4; row++) {
                float s = 0;
                for (int k = 0; k < 4; k++) s += m[k * 4 + row] * b.m[c * 4 + k];
                r.m[c * 4 + row] = s;
            }
        return r;
    }
};

inline Mat4 translateM(const Vec3& t) {
    Mat4 r = Mat4::identity();
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
    return r;
}
inline Mat4 scaleM(const Vec3& s) {
    Mat4 r = Mat4::identity();
    r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
    return r;
}
inline Mat4 rotX(float deg) {
    float c = std::cos(deg * kDeg2Rad), s = std::sin(deg * kDeg2Rad);
    Mat4 r = Mat4::identity();
    r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
    return r;
}
inline Mat4 rotY(float deg) {
    float c = std::cos(deg * kDeg2Rad), s = std::sin(deg * kDeg2Rad);
    Mat4 r = Mat4::identity();
    r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
    return r;
}
inline Mat4 rotZ(float deg) {
    float c = std::cos(deg * kDeg2Rad), s = std::sin(deg * kDeg2Rad);
    Mat4 r = Mat4::identity();
    r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c;
    return r;
}
inline Mat4 rotationM(const Vec3& eulerDeg) {
    return rotZ(eulerDeg.z) * rotY(eulerDeg.y) * rotX(eulerDeg.x);
}
inline Mat4 perspectiveM(float fovDeg, float aspect, float n, float f) {
    float t = 1.0f / std::tan(fovDeg * kDeg2Rad * 0.5f);
    Mat4 r;
    for (int i = 0; i < 16; i++) r.m[i] = 0;
    r.m[0] = t / aspect;
    r.m[5] = t;
    r.m[10] = (f + n) / (n - f);
    r.m[11] = -1;
    r.m[14] = 2 * f * n / (n - f);
    return r;
}
inline Mat4 lookAtM(const Vec3& e, const Vec3& c, const Vec3& up) {
    Vec3 f = normalize(c - e);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);
    Mat4 r = Mat4::identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, e); r.m[13] = -dot(u, e); r.m[14] = dot(f, e);
    return r;
}
// Multiplica vetor pela transposta da parte 3x3 (inversa de uma rotação)
inline Vec3 mulTransposed3(const Mat4& R, const Vec3& v) {
    return Vec3(R.m[0] * v.x + R.m[1] * v.y + R.m[2] * v.z,
                R.m[4] * v.x + R.m[5] * v.y + R.m[6] * v.z,
                R.m[8] * v.x + R.m[9] * v.y + R.m[10] * v.z);
}

inline Mat4 orthoM(float l, float r, float b, float t, float n, float f) {
    Mat4 m = Mat4::identity();
    m.m[0] = 2 / (r - l);
    m.m[5] = 2 / (t - b);
    m.m[10] = -2 / (f - n);
    m.m[12] = -(r + l) / (r - l);
    m.m[13] = -(t + b) / (t - b);
    m.m[14] = -(f + n) / (f - n);
    return m;
}
// Projeta ponto do mundo para pixels da tela. false se estiver atras da camera.
inline bool projectPoint(const Mat4& vp, const Vec3& p, float W, float H, float& sx, float& sy) {
    float x = vp.m[0] * p.x + vp.m[4] * p.y + vp.m[8] * p.z + vp.m[12];
    float y = vp.m[1] * p.x + vp.m[5] * p.y + vp.m[9] * p.z + vp.m[13];
    float w = vp.m[3] * p.x + vp.m[7] * p.y + vp.m[11] * p.z + vp.m[15];
    if (w <= 0.0001f) return false;
    sx = (x / w * 0.5f + 0.5f) * W;
    sy = (1.0f - (y / w * 0.5f + 0.5f)) * H;
    return true;
}
