// OCEANCOMBAT-MOD: 解析波源求值器（自研动态水面核心）。本文件为自研新增。

#pragma once

#include "CoreMinimal.h"
#include "Waves/WaveSourceTypes.h"

/**
 * 波源求值器：静态纯函数，无 UObject 依赖，物理线程可安全调用。
 * 输入为波源数组的只读视图（来自 UOceanWaveSubsystem 的不可变快照）。
 */
class WATER_API FWaveEvaluation
{
public:
	/** 单源求值（调试 / 公式锚点验证用） */
	static float EvaluateSource(const FWaveSourceBase& Source, const FVector2D& SamplePos, double Time);

	/** 主入口：所有波源在 WorldPos 处的高度叠加（线性扫描 + 截断半径早期剔除） */
	static float GetWaveHeight(const FVector& WorldPos, double Time, TArrayView<const TSharedPtr<FWaveSourceBase>> Sources);

	/** 高度 + 单位法线（数值梯度，ε = 10cm，与高度严格自洽） */
	static float GetWaveHeightAndNormal(const FVector& WorldPos, double Time, TArrayView<const TSharedPtr<FWaveSourceBase>> Sources, FVector& OutNormal);
};
