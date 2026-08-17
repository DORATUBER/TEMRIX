#pragma once

#include <temrixstd/stdint.h>
#include <temrixstd/stddef.h>

namespace Math
{
    namespace detail
    {
        static inline uint64_t DoubleToBits(double x)
        {
            uint64_t bits;
            __builtin_memcpy(&bits, &x, sizeof(bits));
            return bits;
        }

        static inline double BitsToDouble(uint64_t bits)
        {
            double x;
            __builtin_memcpy(&x, &bits, sizeof(x));
            return x;
        }

        static inline uint32_t FloatToBits(float x)
        {
            uint32_t bits;
            __builtin_memcpy(&bits, &x, sizeof(bits));
            return bits;
        }

        static inline float BitsToFloat(uint32_t bits)
        {
            float x;
            __builtin_memcpy(&x, &bits, sizeof(x));
            return x;
        }
    }

    

    static inline int Abs(int x) { return x < 0 ? -x : x; }
    static inline long LAbs(long x) { return x < 0 ? -x : x; }
    static inline long long LLAbs(long long x) { return x < 0 ? -x : x; }

    

    static inline double FAbs(double x)
    {
        uint64_t bits = detail::DoubleToBits(x) & 0x7FFFFFFFFFFFFFFFULL;
        return detail::BitsToDouble(bits);
    }

    static inline float FAbsF(float x)
    {
        uint32_t bits = detail::FloatToBits(x) & 0x7FFFFFFFUL;
        return detail::BitsToFloat(bits);
    }

    

    static inline double Floor(double x)
    {
        if (x != x) return x; 
        long long i = (long long)x;
        double id = (double)i;
        if (id > x) id -= 1.0;
        return id;
    }

    static inline double Ceil(double x)
    {
        if (x != x) return x; 
        long long i = (long long)x;
        double id = (double)i;
        if (id < x) id += 1.0;
        return id;
    }

    
    
    static inline double Frexp(double x, int *exp)
    {
        if (x == 0.0 || x != x) { *exp = 0; return x; }

        uint64_t bits = detail::DoubleToBits(x);
        uint64_t sign = bits & 0x8000000000000000ULL;
        int e = (int)((bits >> 52) & 0x7FF);
        uint64_t mantissa = bits & 0xFFFFFFFFFFFFFULL;

        if (e == 0)
        {
            
            double scaled = x * 18446744073709551616.0; 
            uint64_t sbits = detail::DoubleToBits(scaled);
            int se = (int)((sbits >> 52) & 0x7FF);
            uint64_t smant = sbits & 0xFFFFFFFFFFFFFULL;
            *exp = se - 1022 - 64;
            uint64_t outBits = sign | (uint64_t)1022ULL << 52 | smant;
            return detail::BitsToDouble(outBits);
        }
        if (e == 0x7FF) { *exp = 0; return x; } 

        *exp = e - 1022;
        uint64_t outBits = sign | (uint64_t)1022ULL << 52 | mantissa;
        return detail::BitsToDouble(outBits);
    }

    static inline double Ldexp(double x, int exp)
    {
        if (x == 0.0 || exp == 0 || x != x) return x;

        uint64_t bits = detail::DoubleToBits(x);
        uint64_t sign = bits & 0x8000000000000000ULL;
        int64_t e = (int64_t)((bits >> 52) & 0x7FF);
        uint64_t mantissa = bits & 0xFFFFFFFFFFFFFULL;

        if (e == 0x7FF) return x; 

        if (e == 0)
        {
            
            double result = x;
            while (exp > 0) { result *= 2.0; exp--; }
            while (exp < 0) { result *= 0.5; exp++; }
            return result;
        }

        int64_t newExp = e + exp;
        if (newExp >= 0x7FF)
        {
            uint64_t inf = sign | 0x7FF0000000000000ULL;
            return detail::BitsToDouble(inf);
        }
        if (newExp <= 0)
        {
            
            
            
            double result = x;
            while (exp > 0) { result *= 2.0; exp--; }
            while (exp < 0) { result *= 0.5; exp++; }
            return result;
        }

        uint64_t outBits = sign | ((uint64_t)newExp << 52) | mantissa;
        return detail::BitsToDouble(outBits);
    }

    static inline float LdexpF(float x, int exp)
    {
        return (float)Ldexp((double)x, exp);
    }

    static inline double Sqrt(double x)
    {
        if (x <= 0.0) return x == 0.0 ? 0.0 : (0.0 / 0.0); 
        if (x != x) return x;

        
        
        uint64_t i = detail::DoubleToBits(x);
        i = 0x5FE6EB50C7B537A9ULL - (i >> 1);
        double y = detail::BitsToDouble(i);

        y = y * (1.5 - 0.5 * x * y * y);
        y = y * (1.5 - 0.5 * x * y * y);
        y = y * (1.5 - 0.5 * x * y * y);
        y = y * (1.5 - 0.5 * x * y * y);

        double result = x * y;
        
        result = 0.5 * (result + x / result);
        return result;
    }

    static inline float SqrtF(float x)
    {
        return (float)Sqrt((double)x);
    }

    static const double kLn2 = 0.6931471805599453;
    static const double kLog2E = 1.4426950408889634;

    static inline double Exp(double x)
    {
        if (x != x) return x;
        if (x > 709.0) return detail::BitsToDouble(0x7FF0000000000000ULL);  
        if (x < -745.0) return 0.0;

        
        double kf = Floor(x * kLog2E + 0.5);
        int k = (int)kf;
        double r = x - kf * kLn2;

        
        double term = 1.0;
        double sum = 1.0;
        for (int n = 1; n <= 15; n++)
        {
            term *= r / (double)n;
            sum += term;
        }

        return Ldexp(sum, k);
    }

    static inline double Log(double x)
    {
        if (x < 0.0 || x != x) return 0.0 / 0.0; 
        if (x == 0.0) return detail::BitsToDouble(0xFFF0000000000000ULL); 

        int e;
        double m = Frexp(x, &e); 
        
        m *= 2.0;
        e -= 1;

        
        double f = (m - 1.0) / (m + 1.0);
        double f2 = f * f;
        double term = f;
        double sum = f;
        for (int n = 3; n <= 15; n += 2)
        {
            term *= f2;
            sum += term / (double)n;
        }

        return 2.0 * sum + (double)e * kLn2;
    }

    static inline double Pow(double base, double exponent)
    {
        if (exponent == 0.0) return 1.0;
        if (base == 0.0) return exponent > 0.0 ? 0.0 : (1.0 / 0.0);

        
        
        double intPart = Floor(exponent);
        if (intPart == exponent && FAbs(exponent) <= 64.0)
        {
            bool neg = exponent < 0.0;
            unsigned n = (unsigned)(neg ? -exponent : exponent);
            double result = 1.0;
            double b = base;
            while (n)
            {
                if (n & 1) result *= b;
                b *= b;
                n >>= 1;
            }
            return neg ? 1.0 / result : result;
        }

        if (base < 0.0) return 0.0 / 0.0; 
        return Exp(exponent * Log(base));
    }

    static inline float PowF(float base, float exponent)
    {
        return (float)Pow((double)base, (double)exponent);
    }
}

static inline int abs(int x) { return Math::Abs(x); }
static inline long labs(long x) { return Math::LAbs(x); }
static inline long long llabs(long long x) { return Math::LLAbs(x); }

static inline double fabs(double x) { return Math::FAbs(x); }
static inline float fabsf(float x) { return Math::FAbsF(x); }

static inline double floor(double x) { return Math::Floor(x); }
static inline double ceil(double x) { return Math::Ceil(x); }

static inline double frexp(double x, int *exp) { return Math::Frexp(x, exp); }
static inline double ldexp(double x, int exp) { return Math::Ldexp(x, exp); }
static inline float ldexpf(float x, int exp) { return Math::LdexpF(x, exp); }

static inline double sqrt(double x) { return Math::Sqrt(x); }
static inline float sqrtf(float x) { return Math::SqrtF(x); }

static inline double exp(double x) { return Math::Exp(x); }
static inline double log(double x) { return Math::Log(x); }

static inline double pow(double base, double exponent) { return Math::Pow(base, exponent); }
static inline float powf(float base, float exponent) { return Math::PowF(base, exponent); }