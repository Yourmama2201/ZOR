#pragma once
#include <cmath>
#include <algorithm>
#include <numbers>

struct Vec2 {
    float x, y;

    constexpr Vec2(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}

    constexpr Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }
    constexpr Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
    constexpr Vec2 operator*(float scalar) const { return {x * scalar, y * scalar}; }
    constexpr Vec2 operator/(float scalar) const { return {x / scalar, y / scalar}; }

    constexpr Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    constexpr Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    constexpr Vec2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
    constexpr Vec2& operator/=(float scalar) { x /= scalar; y /= scalar; return *this; }

    [[nodiscard]] float Length() const { return std::sqrt(x * x + y * y); }
    [[nodiscard]] constexpr float LengthSq() const { return x * x + y * y; }

    [[nodiscard]] float Distance(const Vec2& other) const { return (*this - other).Length(); }
    [[nodiscard]] constexpr float Dot(const Vec2& other) const { return x * other.x + y * other.y; }

    [[nodiscard]] Vec2 Normalize() const {
        float len = Length();
        return len > 0.0f ? *this * (1.0f / len) : Vec2{};
    }
};

struct Vec3 {
    float x, y, z;

    constexpr Vec3(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}

    constexpr Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    constexpr Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    constexpr Vec3 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    constexpr Vec3 operator/(float scalar) const { return {x / scalar, y / scalar, z / scalar}; }

    constexpr Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    constexpr Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
    constexpr Vec3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
    constexpr Vec3& operator/=(float scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }

    [[nodiscard]] float Length() const { return std::sqrt(x * x + y * y + z * z); }
    [[nodiscard]] constexpr float LengthSq() const { return x * x + y * y + z * z; }

    [[nodiscard]] float Distance(const Vec3& other) const { return (*this - other).Length(); }
    [[nodiscard]] constexpr float Dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }

    [[nodiscard]] Vec3 Cross(const Vec3& other) const {
        return Vec3{
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        };
    }

    [[nodiscard]] constexpr Vec3 Lerp(const Vec3& other, float t) const {
        return *this + (other - *this) * std::clamp(t, 0.0f, 1.0f);
    }

    [[nodiscard]] Vec3 Normalize() const {
        float len = Length();
        return len > 0.0f ? *this * (1.0f / len) : Vec3{};
    }
};

struct Matrix4x4 {
    float m[4][4] = {};

    [[nodiscard]] bool IsIdentity() const {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                float expected = (i == j) ? 1.0f : 0.0f;
                if (m[i][j] != expected) return false;
            }
        }
        return true;
    }

    [[nodiscard]] Vec3 Multiply(const Vec3& v) const {
        return Vec3{
            v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + m[3][0],
            v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + m[3][1],
            v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + m[3][2]
        };
    }

    [[nodiscard]] Vec3 Transform(const Vec3& v) const {
        float w = v.x * m[0][3] + v.y * m[1][3] + v.z * m[2][3] + m[3][3];
        if (w < 0.001f) return Vec3{};
        return Vec3{
            (v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + m[3][0]) / w,
            (v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + m[3][1]) / w,
            (v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + m[3][2]) / w
        };
    }
};

class Math {
public:
    static constexpr float PI = 3.14159265358979f;
    static constexpr float RAD2DEG = 180.0f / PI;

    static constexpr float Clamp(float value, float min, float max) {
        return std::clamp(value, min, max);
    }

    static constexpr float Lerp(float a, float b, float t) {
        return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
    }

    static float WrapYaw(float yaw) {
        yaw = std::fmod(yaw, 360.0f);
        if (yaw > 180.0f) yaw -= 360.0f;
        if (yaw < -180.0f) yaw += 360.0f;
        return yaw;
    }

    static Vec3 NormalizeAngles(const Vec3& angles) {
        return Vec3{
            std::clamp(angles.x, -89.0f, 89.0f),
            WrapYaw(angles.y),
            0.0f
        };
    }

    static float AngleDelta(float from, float to) {
        return WrapYaw(to - from);
    }

    static Vec3 CalculateAngle(const Vec3& src, const Vec3& dst) {
        Vec3 delta = dst - src;
        float dist = delta.Length();
        if (dist < 0.001f) return Vec3{};

        float pitch = -std::asin(delta.z / dist);
        float yaw = std::atan2(-delta.x, delta.y);

        return Vec3{pitch * RAD2DEG, yaw * RAD2DEG, 0.0f};
    }

    static Vec3 AngleToDirection(const Vec3& angles) {
        float pitch = angles.x * (PI / 180.0f);
        float yaw = angles.y * (PI / 180.0f);

        return Vec3{
            -std::cos(pitch) * std::sin(yaw),
             std::cos(pitch) * std::cos(yaw),
            -std::sin(pitch)
        }.Normalize();
    }

    static float GetFOV(const Vec3& viewAngles, const Vec3& targetAngles) {
        Vec3 delta = targetAngles - viewAngles;
        delta.y = WrapYaw(delta.y);
        delta.x = std::clamp(delta.x, -180.0f, 180.0f);
        return std::sqrt(delta.x * delta.x + delta.y * delta.y);
    }

    static Vec3 SmoothAngle(const Vec3& current, const Vec3& target, float smoothness) {
        smoothness = std::clamp(smoothness, 0.0f, 1.0f);
        return Vec3{
            current.x + AngleDelta(current.x, target.x) * smoothness,
            current.y + AngleDelta(current.y, target.y) * smoothness,
            0.0f
        };
    }

    static Vec3 HumanSmooth(const Vec3& current, const Vec3& target, float base, float remainingFOV) {
        float speed = base + (1.0f - base) * std::clamp(remainingFOV / 30.0f, 0.0f, 1.0f);
        return SmoothAngle(current, target, std::clamp(speed, 0.0f, 1.0f));
    }

    static Vec2 WorldToScreen(const Vec3& worldPos, const Matrix4x4& viewMatrix, int width, int height) {
        Vec3 screenPos = viewMatrix.Transform(worldPos);
        if (screenPos.z < 0.001f) return Vec2{-9999.0f, -9999.0f};
        return Vec2{
            (width / 2.0f) + (screenPos.x * width / 2.0f),
            (height / 2.0f) - (screenPos.y * height / 2.0f)
        };
    }
};