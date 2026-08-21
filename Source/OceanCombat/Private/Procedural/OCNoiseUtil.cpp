// OceanCombat. Copyright(c) All rights reserved.

#include "Procedural/OCNoiseUtil.h"

namespace
{
    /** 整数格点哈希:xxhash 风格混合,同 (X, Y, Seed) 恒定,跨格/跨种子良好雪崩 */
    FORCEINLINE uint32 HashInt2(int32 X, int32 Y, int32 Seed)
    {
        uint32 H = static_cast<uint32>(X) * 0x8DA6B343u;
        H ^= static_cast<uint32>(Y) * 0xD8163841u;
        H ^= static_cast<uint32>(Seed) * 0xCB1AB31Fu;
        H ^= H >> 15;
        H *= 0x85EBCA6Bu;
        H ^= H >> 13;
        H *= 0xC2B2AE35u;
        H ^= H >> 16;
        return H;
    }

    /** 8 方向单位梯度表(轴向 + 对角) */
    const FVector2f GGradients[8] = {
        FVector2f(1.0f, 0.0f), FVector2f(-1.0f, 0.0f),
        FVector2f(0.0f, 1.0f), FVector2f(0.0f, -1.0f),
        FVector2f(0.70710678f, 0.70710678f), FVector2f(-0.70710678f, 0.70710678f),
        FVector2f(0.70710678f, -0.70710678f), FVector2f(-0.70710678f, -0.70710678f),
    };

    /** 五次 fade 曲线(6t^5-15t^4+10t^3),C^2 连续,格点处一阶/二阶导均为 0 */
    FORCEINLINE float Fade(float T)
    {
        return T * T * T * (T * (T * 6.0f - 15.0f) + 10.0f);
    }
}

float OCNoise::GradientNoise2D(FVector2D P, int32 Seed)
{
    // 拆成整数格点 + 小数部分:整数参与哈希(任意远不丢精度),小数参与插值(∈[0,1),精度充足)
    const double X0d = FMath::Floor(P.X);
    const double Y0d = FMath::Floor(P.Y);
    const int32 X0 = static_cast<int32>(X0d);
    const int32 Y0 = static_cast<int32>(Y0d);
    const float Fx = static_cast<float>(P.X - X0d);
    const float Fy = static_cast<float>(P.Y - Y0d);

    const float U = Fade(Fx);
    const float V = Fade(Fy);

    auto GradientDot = [Seed](int32 CellX, int32 CellY, float Dx, float Dy)
    {
        const FVector2f& G = GGradients[HashInt2(CellX, CellY, Seed) & 7];
        return G.X * Dx + G.Y * Dy;
    };

    const float Bottom = FMath::Lerp(GradientDot(X0, Y0, Fx, Fy), GradientDot(X0 + 1, Y0, Fx - 1.0f, Fy), U);
    const float Top = FMath::Lerp(GradientDot(X0, Y0 + 1, Fx, Fy - 1.0f), GradientDot(X0 + 1, Y0 + 1, Fx - 1.0f, Fy - 1.0f), U);
    // 梯度点积理论极值约 ±0.707,放大到约 ±1
    return FMath::Lerp(Bottom, Top, V) * 1.41421356f;
}

float OCNoise::FBm2D(FVector2D P, int32 Octaves, int32 Seed)
{
    float Sum = 0.0f;
    float Amplitude = 1.0f;
    float Norm = 0.0f;
    for (int32 Octave = 0; Octave < Octaves; ++Octave)
    {
        Sum += Amplitude * GradientNoise2D(P, Seed + Octave);
        Norm += Amplitude;
        Amplitude *= 0.5f; // persistence:每倍频振幅减半
        P *= 2.0;          // lacunarity:每倍频频率翻倍
    }
    return Norm > 0.0f ? Sum / Norm : 0.0f;
}
