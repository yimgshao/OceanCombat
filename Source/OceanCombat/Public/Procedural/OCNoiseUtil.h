// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 精度免疫的程序化噪声。
 *
 * 为什么不用 FMath::PerlinNoise2D:
 *   1. float 输入,远离原点(>10km)后有效位不足,噪声量化成阶梯纹;
 *   2. 排列表周期 256,换算世界距离约 6~7km 重复一次,远航可察觉。
 *
 * 本实现把 double 坐标拆成"整数格点(参与哈希,永不丢精度)+ 小数部分(参与插值)",
 * 任意远的位置都能得到稳定、确定、不重复的噪声值。
 */
namespace OCNoise
{
    /**
     * 梯度噪声(Perlin 风格),返回约 [-1, 1]。
     * @param P 采样位置(double;通常 = 世界坐标 × 频率)
     * @param Seed 用途种子,不同用途传不同常量即可去相关
     */
    OCEANCOMBAT_API float GradientNoise2D(FVector2D P, int32 Seed);

    /**
     * fBm(分形叠加):振幅逐倍频减半、频率翻倍、归一化,返回约 [-1, 1]。
     * 每个倍频用 Seed + Octave 去相关。
     */
    OCEANCOMBAT_API float FBm2D(FVector2D P, int32 Octaves, int32 Seed);
}
