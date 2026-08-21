// OCEANCOMBAT-MOD: 解析波源体系（自研动态水面核心）。本文件为自研新增。
// 设计文档：docs/步骤1-波源数据层详细设计.md

#pragma once

#include "CoreMinimal.h"

/** 波源类型标识，GPU 图章分派用（步骤 3 使用） */
enum class EWaveSourceType : uint8
{
	ShipWake,	// 船只尾迹
	Splash,		// 落水冲击
	Generic,	// 预留
};

/**
 * 波源基类（抽象）。
 * 纯 C++ struct（UHT 不支持 USTRUCT 继承，故不走反射）；
 * 创建后不可变、无 UObject 引用，可在物理线程上求值。
 * 派生类只需实现 Evaluate 与 GetSourceType，共享波形工具保证所有波型传播物理一致。
 */
struct WATER_API FWaveSourceBase
{
	int32 SourceID = 0;							// 稳定身份编号，由 UOceanWaveSubsystem 注册时分配（MID 缓存键）
	FVector2D Position = FVector2D::ZeroVector;	// 世界 XY（cm）
	double StartTime = 0.0;						// 出生时间（WaterTime 基准，秒）
	float Amplitude = 0.0f;						// 初始振幅（cm）
	float WaveLength = 300.0f;					// 波长（cm）
	float WaveSpeed = 600.0f;					// 波前传播速度（cm/s，独立于色散，手感参数）
	float DecayRate = 0.5f;						// 时间衰减率（1/秒）
	float CutoffRadius = 2000.0f;				// 影响截断半径（cm）

	virtual ~FWaveSourceBase() = default;

	/** 采样点高度贡献（纯函数，物理线程可调用） */
	virtual float Evaluate(const FVector2D& SamplePos, double Time) const = 0;

	/** 当前有效振幅（淘汰判断用），默认 Amplitude·exp(−DecayRate·age) */
	virtual float GetEffectiveAmplitude(double Time) const;

	/** 早期剔除包围半径 */
	virtual float GetCutoffRadius() const { return CutoffRadius; }

	/** GPU 图章分派用类型标识 */
	virtual EWaveSourceType GetSourceType() const { return EWaveSourceType::Generic; }

	// ---- 共享波形工具（派生类 Evaluate 复用，子系统调试也可用） ----

	static double ComputeAge(double Time, double StartTime);

	/** 深水色散：k = 2π/λ，ω = √(g·k)，cg = ω/(2k)（g = 980 cm/s²） */
	static void GetDispersionParams(float InWaveLength, float& OutK, float& OutOmega, float& OutGroupSpeed);

	/** 时间衰减 D(age) = exp(−DecayRate·age) */
	static float ComputeTimeDecay(float InDecayRate, double Age);

	/** 径向包络：1/√r 扩散衰减 × 截断处（0.8~1.0·Cutoff）平滑收尾 */
	static float ComputeRadialEnvelope(float R, float InWaveLength, float InCutoffRadius);

	/** 波前因果：波前（cg·age）之外贡献为 0，波前后沿 2 个波长处平滑过渡 */
	static float ComputeWavefrontFactor(float R, float InWaveLength, float InGroupSpeed, double Age);
};

/** 船只尾迹波源：Kelvin 楔形方向权重 + 全向近场分量 */
struct WATER_API FShipWakeSource : public FWaveSourceBase
{
	float Heading = 0.0f;				// 发射时刻航向（弧度，0 = +X）
	float WedgeHalfAngleDeg = 19.47f;	// Kelvin 楔形半角（度）
	float OmniWeight = 0.2f;			// 全向分量权重 [0,1]

	virtual float Evaluate(const FVector2D& SamplePos, double Time) const override;
	virtual EWaveSourceType GetSourceType() const override { return EWaveSourceType::ShipWake; }
};

/** 落水冲击波源：全向环形波（炮弹、建筑碎块等，靠参数区分规模） */
struct WATER_API FSplashWaveSource : public FWaveSourceBase
{
	virtual float Evaluate(const FVector2D& SamplePos, double Time) const override;
	virtual EWaveSourceType GetSourceType() const override { return EWaveSourceType::Splash; }
};
