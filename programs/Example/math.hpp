#pragma once
#include <temrixstd.h>

class Fixed
{
public:
    int32_t raw;

    inline Fixed() : raw(0) {}
    inline explicit Fixed(int32_t raw) : raw(raw) {}

    inline Fixed fromInt(int32_t i) const { return Fixed(i << 16); }
    inline Fixed fromFloat(float f) const { return Fixed((int32_t)(f * 65536.0f)); }
    inline int32_t toInt() const { return raw >> 16; }
    inline float toFloat() const { return (float)raw / 65536.0f; }

    inline Fixed operator+(Fixed o) const { return Fixed(raw + o.raw); }
    inline Fixed operator-(Fixed o) const { return Fixed(raw - o.raw); }
    inline Fixed operator-() const { return Fixed(-raw); }
    inline Fixed operator*(Fixed o) const { return Fixed((int32_t)(((int64_t)raw * o.raw) >> 16)); }
    inline Fixed operator/(Fixed o) const { return Fixed((int32_t)(((int64_t)raw << 16) / o.raw)); }

    inline Fixed &operator+=(Fixed o)
    {
        raw += o.raw;
        return *this;
    }
    inline Fixed &operator-=(Fixed o)
    {
        raw -= o.raw;
        return *this;
    }
    inline Fixed &operator*=(Fixed o)
    {
        *this = *this * o;
        return *this;
    }
    inline Fixed &operator/=(Fixed o)
    {
        *this = *this / o;
        return *this;
    }

    inline bool operator==(Fixed o) const { return raw == o.raw; }
    inline bool operator!=(Fixed o) const { return raw != o.raw; }
    inline bool operator<(Fixed o) const { return raw < o.raw; }
    inline bool operator<=(Fixed o) const { return raw <= o.raw; }
    inline bool operator>(Fixed o) const { return raw > o.raw; }
    inline bool operator>=(Fixed o) const { return raw >= o.raw; }

    inline Fixed abs() const { return Fixed(raw < 0 ? -raw : raw); }
    inline Fixed mulInt(int32_t i) const { return Fixed(raw * i); }
    inline Fixed divInt(int32_t i) const { return Fixed(raw / i); }
    inline Fixed half() const { return Fixed(raw >> 1); }

    inline Fixed zero() const { return Fixed(0); }
    inline Fixed one() const { return Fixed(1.0f * 65536.0f); }
    inline Fixed pi() const { return Fixed(3.14159265 * 65536.0f); }
    inline Fixed twoPi() const { return Fixed(2 * 3.14159265 * 65536.0f); }

    inline Fixed sin() const
    {
        Fixed angle(raw);
        Fixed p = pi();
        Fixed tp = twoPi();
        while (angle > p)
            angle -= tp;
        while (angle < -p)
            angle += tp;

        Fixed x = angle;
        Fixed x2 = x * x;
        Fixed x3 = x2 * x;
        Fixed x5 = x3 * x2;
        Fixed x7 = x5 * x2;

        return x - x3 / x.fromInt(6) + x5 / x.fromInt(120) - x7 / x.fromInt(5040);
    }

    inline Fixed cos() const
    {
        return Fixed(raw + (((int32_t)(3.14159265 * 65536.0f)) >> 1)).sin();
    }

    inline Fixed sqrt() const
    {
        if (raw <= 0)
            return Fixed(0);
        Fixed v(raw);
        Fixed est(raw);
        est = (est + v / est).divInt(2);
        est = (est + v / est).divInt(2);
        est = (est + v / est).divInt(2);
        est = (est + v / est).divInt(2);
        return est;
    }
};

class Vec3
{
public:
    Fixed x, y, z;

    inline Vec3() {}
    inline Vec3(Fixed x, Fixed y, Fixed z) : x(x), y(y), z(z) {}

    inline Vec3 fromInt(int32_t x, int32_t y, int32_t z) const
    {
        Fixed f;
        return Vec3(f.fromInt(x), f.fromInt(y), f.fromInt(z));
    }

    inline Vec3 operator+(const Vec3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
    inline Vec3 operator-(const Vec3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
    inline Vec3 operator*(Fixed s) const { return {x * s, y * s, z * s}; }
    inline Vec3 operator-() const { return {-x, -y, -z}; }

    inline Vec3 &operator+=(const Vec3 &o)
    {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    inline Vec3 &operator-=(const Vec3 &o)
    {
        x -= o.x;
        y -= o.y;
        z -= o.z;
        return *this;
    }
    inline Vec3 &operator*=(Fixed s)
    {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    inline Fixed dot(const Vec3 &o) const { return x * o.x + y * o.y + z * o.z; }

    inline Vec3 cross(const Vec3 &o) const
    {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x};
    }

    inline Fixed lengthSq() const { return dot(*this); }
    inline Fixed length() const { return lengthSq().sqrt(); }

    inline Vec3 normalize() const
    {
        Fixed len = length();
        if (len.raw == 0)
            return *this;
        return {x / len, y / len, z / len};
    }
};

class Vec4
{
public:
    Fixed x, y, z, w;

    inline Vec4() {}
    inline Vec4(Fixed x, Fixed y, Fixed z, Fixed w) : x(x), y(y), z(z), w(w) {}
    inline Vec4(Vec3 v, Fixed w) : x(v.x), y(v.y), z(v.z), w(w) {}

    inline Vec3 xyz() const { return {x, y, z}; }

    inline Vec4 operator+(const Vec4 &o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    inline Vec4 operator-(const Vec4 &o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    inline Vec4 operator*(Fixed s) const { return {x * s, y * s, z * s, w * s}; }
    inline Vec4 operator-() const { return {-x, -y, -z, -w}; }

    inline Vec4 &operator+=(const Vec4 &o)
    {
        x += o.x;
        y += o.y;
        z += o.z;
        w += o.w;
        return *this;
    }
    inline Vec4 &operator-=(const Vec4 &o)
    {
        x -= o.x;
        y -= o.y;
        z -= o.z;
        w -= o.w;
        return *this;
    }

    inline Fixed dot(const Vec4 &o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
};

class Mat4
{
public:
    Fixed m[4][4];

    inline Mat4()
    {
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                m[i][j] = (i == j) ? Fixed(65536) : Fixed(0);
    }

    inline Vec4 operator*(const Vec4 &v) const
    {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w,
            m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w};
    }

    inline Mat4 operator*(const Mat4 &o) const
    {
        Mat4 r;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
            {
                r.m[i][j] = Fixed(0);
                for (int k = 0; k < 4; k++)
                    r.m[i][j] += m[i][k] * o.m[k][j];
            }
        return r;
    }

    inline Mat4 rotationX(Fixed a) const
    {
        Mat4 r;
        Fixed c = a.cos(), s = a.sin();
        r.m[1][1] = c;
        r.m[1][2] = -s;
        r.m[2][1] = s;
        r.m[2][2] = c;
        return r;
    }

    inline Mat4 rotationY(Fixed a) const
    {
        Mat4 r;
        Fixed c = a.cos(), s = a.sin();
        r.m[0][0] = c;
        r.m[0][2] = s;
        r.m[2][0] = -s;
        r.m[2][2] = c;
        return r;
    }

    inline Mat4 rotationZ(Fixed a) const
    {
        Mat4 r;
        Fixed c = a.cos(), s = a.sin();
        r.m[0][0] = c;
        r.m[0][1] = -s;
        r.m[1][0] = s;
        r.m[1][1] = c;
        return r;
    }

    inline Mat4 translation(Fixed tx, Fixed ty, Fixed tz) const
    {
        Mat4 r;
        r.m[0][3] = tx;
        r.m[1][3] = ty;
        r.m[2][3] = tz;
        return r;
    }

    inline Mat4 perspective(Fixed fovY, Fixed aspect, Fixed zNear, Fixed zFar) const
    {
        Mat4 r;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                r.m[i][j] = Fixed(0);

        Fixed tanHalf = fovY.half().sin() / fovY.half().cos();
        Fixed f = Fixed(1.0f * 65536.0f) / tanHalf;

        r.m[0][0] = f / aspect;
        r.m[1][1] = f;
        r.m[2][2] = (zFar + zNear) / (zNear - zFar);
        r.m[2][3] = (Fixed(2.0f * 65536.0f) * zFar * zNear) / (zNear - zFar);
        r.m[3][2] = -Fixed(1.0f * 65536.0f);
        return r;
    }
};