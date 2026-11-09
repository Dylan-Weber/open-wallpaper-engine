module;

#include <rstd/macro.hpp>

export module wescene.utils;
import wescene.core;
import eigen;
import rstd;
import rstd.log;
export import wescene.types;

using rstd::time::Instant;

using namespace rstd::prelude;

// ---------- Module-purview entities ----------------------------------------

export namespace owe
{

// FpsCounter — was Utils/FpsCounter.h. Implementation in Utils/FpsCounter.cpp.
class FpsCounter {
public:
    FpsCounter();
    u32  Fps() const { return m_fps; }
    u32  FrameCount() const { return m_frameCount; }
    void RegisterFrame();

private:
    u32     m_fps;
    u32     m_frameCount;
    Instant m_startTime;
};

namespace algorism
{
// Was Utils/Algorism.h. Out-of-line in Utils/Algorism.cpp module impl unit.
double CalculatePersperctiveDistance(double fov, double height) noexcept;
double CalculatePersperctiveFov(double distence, double height) noexcept;
double PerlinNoise(double x, double y, double z) noexcept;

constexpr u32 PowOfTwo(u32 x) {
    u32 pow2 { 8 };
    while (pow2 < x) pow2 *= u32(2);
    return pow2;
}
constexpr bool IsPowOfTwo(u32 x) { return (x > u32(1)) && ((x & (x - u32(1))) == u32()); }

inline Eigen::Vector3d sph2cart(const Eigen::Vector3d& sph) noexcept {
    double azimuth   = sph.x();
    double elevation = sph.y();
    double radius    = sph.z();
    return radius * Eigen::Vector3d {
        f64(azimuth).cos().to_primitive() * f64(elevation).cos().to_primitive(),
        f64(azimuth).sin().to_primitive() * f64(elevation).cos().to_primitive(),
        f64(elevation).sin().to_primitive(),
    };
}

template<typename TFUNC>
Eigen::Vector3d GenSphereSurface(TFUNC&& random) noexcept {
    double azimuth   = f64::consts::TAU.to_primitive() * random();
    double elevation = f64(2.0 * random() - 1.0).asin().to_primitive();
    return sph2cart({ azimuth, elevation, 1.0 });
}

template<typename TFUNC>
Eigen::Vector3d GenSphereSurfaceNormal(TFUNC&&                normal_random,
                                       const Eigen::Vector3d& direct) noexcept {
    double u    = direct.x() > 0.0 ? normal_random(0.0, direct.x()) : 0.0;
    double v    = direct.y() > 0.0 ? normal_random(0.0, direct.y()) : 0.0;
    double w    = direct.z() > 0.0 ? normal_random(0.0, direct.z()) : 0.0;
    double norm = f64((u * u + v * v + w * w)).sqrt().to_primitive();
    return Eigen::Vector3d(u, v, w) / norm;
}

template<typename TFUNC>
Eigen::Vector3d GenSphereIn(TFUNC&& random) noexcept {
    return f64(random()).powf(f64(1.0 / 3.0)).to_primitive() * GenSphereSurface(random);
}

constexpr double DragForce(double speed, double strength, double density) {
    return -speed * strength * density;
}
inline Eigen::Vector3d DragForce(Eigen::Vector3d v, double strength,
                                 double density = 1.0) noexcept {
    return v.normalized() * DragForce(v.norm(), strength, density);
}

constexpr double lerp(double t, double a, double b) noexcept { return a + t * (b - a); }

constexpr double PerlinEase(double t) noexcept { return t * t * t * (t * (t * 6 - 15) + 10); }

inline Eigen::Vector3d PerlinNoiseVec3(Eigen::Vector3d p) noexcept {
    return Eigen::Vector3d { PerlinNoise(p[0], p[1], p[2]),
                             PerlinNoise(p[0] + 89.2, p[1] + 33.1, p[2] + 57.3),
                             PerlinNoise(p[0] + 100.3, p[1] + 120.1, p[2] + 142.2) };
}

// Gustavson's 3D simplex noise, as Wallpaper Engine's turbulence samples it (wallpaper64.exe's noise
// object: Ken Perlin's permutation, the 12 edge gradients, falloff 0.6 - r^2, times 32; about -1..1).
inline double SimplexNoise3(double xin, double yin, double zin) noexcept {
    static constexpr unsigned char perm[256] = { 151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180 };
    static constexpr float gx[12] = { 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0 };
    static constexpr float gy[12] = { 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1 };
    static constexpr float gz[12] = { 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1 };
    auto p  = [](int i) { return static_cast<int>(perm[i & 255]); };
    const float x = static_cast<float>(xin), y = static_cast<float>(yin), z = static_cast<float>(zin);
    constexpr float F3 = 1.0f / 3.0f, G3 = 1.0f / 6.0f;
    const float s = (x + y + z) * F3;
    const int   i = static_cast<int>(f32(x + s).floor().to_primitive());
    const int   j = static_cast<int>(f32(y + s).floor().to_primitive());
    const int   k = static_cast<int>(f32(z + s).floor().to_primitive());
    const float t  = static_cast<float>(i + j + k) * G3;
    const float x0 = x - (static_cast<float>(i) - t), y0 = y - (static_cast<float>(j) - t),
                z0 = z - (static_cast<float>(k) - t);
    int i1, j1, k1, i2, j2, k2;
    if (x0 >= y0) {
        if (y0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
        else if (x0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1; }
        else { i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1; }
    } else {
        if (y0 < z0) { i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1; }
        else if (x0 < z0) { i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1; }
        else { i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
    }
    const float corner[4][3] = {
        { x0, y0, z0 },
        { x0 - i1 + G3, y0 - j1 + G3, z0 - k1 + G3 },
        { x0 - i2 + 2.0f * G3, y0 - j2 + 2.0f * G3, z0 - k2 + 2.0f * G3 },
        { x0 - 1.0f + 3.0f * G3, y0 - 1.0f + 3.0f * G3, z0 - 1.0f + 3.0f * G3 },
    };
    const int offset[4][3] = { { 0, 0, 0 }, { i1, j1, k1 }, { i2, j2, k2 }, { 1, 1, 1 } };
    const int ii = i & 255, jj = j & 255, kk = k & 255;
    float     n  = 0.0f;
    for (int c = 0; c < 4; ++c) {
        float r = 0.6f - corner[c][0] * corner[c][0] - corner[c][1] * corner[c][1] -
                  corner[c][2] * corner[c][2];
        if (r <= 0.0f) continue;
        const int g = p(ii + offset[c][0] + p(jj + offset[c][1] + p(kk + offset[c][2]))) % 12;
        r *= r;
        n += r * r * (gx[g] * corner[c][0] + gy[g] * corner[c][1] + gz[g] * corner[c][2]);
    }
    return 32.0 * static_cast<double>(n);
}

inline Eigen::Vector3d CurlNoise(Eigen::Vector3d p) noexcept {
    using namespace Eigen;
    constexpr double e = 1e-5;
    Vector3d         dx(e, 0, 0), dy(0, e, 0), dz(0, 0, e);
    Vector3d         x0 = PerlinNoiseVec3(p - dx), x1 = PerlinNoiseVec3(p + dx),
                     y0 = PerlinNoiseVec3(p - dy), y1 = PerlinNoiseVec3(p + dy),
                     z0 = PerlinNoiseVec3(p - dz), z1 = PerlinNoiseVec3(p + dz);
    double           x = y1.z() - y0.z() - z1.y() + z0.y();
    double           y = z1.x() - z0.x() - x1.z() + x0.z();
    double           z = x1.y() - x0.y() - y1.x() + y0.x();
    return Vector3d(x, y, z) / (2.0 * e);
}
} // namespace algorism

} // namespace owe

// ---------- Migrated from former Utils/{Hash,DynamicLibrary} ----------------

export namespace utils
{

String genSha1(slice<rstd::byte> input);

// DynamicLibrary lives in wescene.types now (re-exported above).

} // namespace utils

// Eigen helpers — were Utils/Eigen.h. Module-attached additions to the Eigen
// namespace; only visible to TUs that `import wescene.utils;`.
export namespace Eigen
{
constexpr double Radians(double a) noexcept {
    return (a / 180.0f) * f64::consts::PI.to_primitive();
}

inline Matrix4d LookAt(Vector3d eye, Vector3d center, Vector3d up) noexcept {
    Vector3d camDir = center - eye;
    Vector3d zAxis  = -camDir.normalized();
    Vector3d xAxis  = up.cross(zAxis).normalized();
    Vector3d yAxis  = zAxis.cross(xAxis).normalized();

    Matrix4d view          = Matrix4d::Identity();
    view.block<1, 3>(0, 0) = xAxis.transpose();
    view.block<1, 3>(1, 0) = yAxis.transpose();
    view.block<1, 3>(2, 0) = zAxis.transpose();
    view(0, 3)             = -xAxis.dot(eye);
    view(1, 3)             = -yAxis.dot(eye);
    view(2, 3)             = -zAxis.dot(eye);
    return view;
}

inline Matrix4d Ortho(double left, double right, double bottom, double top, double nearz,
                      double farz) noexcept {
    Affine3d trans = Affine3d::Identity();
    trans.pretranslate(
        Vector3d(-(left + right) / 2.0f, -(top + bottom) / 2.0f, -(nearz + farz) / 2.0f));
    trans.prescale(Vector3d(2.0f / (right - left), 2.0f / (top - bottom), 2.0f / (farz - nearz)));
    trans.scale(Vector3d(1.0f, 1.0f, -1.0f));
    trans.prescale(Vector3d(1.0f, 1.0f, 0.5f));
    trans.pretranslate(Vector3d(0.0f, 0.0f, 0.5f));
    return trans.matrix();
}

inline Matrix4d Perspective(double fov, double aspect, double nearz, double farz) noexcept {
    Projective3d trans = Projective3d::Identity();
    trans.prescale(Vector3d(nearz, nearz, (nearz + farz)));
    trans(3, 2)  = 1.0f;
    trans(3, 3)  = 0.0f;
    trans(2, 3)  = -nearz * farz;
    double top   = f64(fov / 2.0f).tan().to_primitive() * f64(nearz).abs().to_primitive();
    double right = top * aspect;
    trans.scale(Vector3d(1.0f, 1.0f, -1.0f));
    trans.prescale(Vector3d(1.0f, 1.0f, -1.0f));
    return Ortho(-right, right, -top, top, nearz, farz) * trans.matrix();
}
} // namespace Eigen
