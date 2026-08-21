// OCEANCOMBAT-MOD: 解析波源求值器实现（自研动态水面核心）。本文件为自研新增。

#include "Waves/WaveEvaluation.h"

namespace WaveEvaluationConstants
{
	constexpr float Gravity = 980.0f;			// cm/s²
	constexpr float MinWaveLength = 1.0f;		// 防除零（cm）
	constexpr float RadialCutoffStart = 0.8f;	// 截断平滑起点（占 CutoffRadius 比例）
	constexpr float WavefrontFadeWaveLengths = 2.0f;	// 波前后沿过渡宽度（波长倍数）
}

/** 本地 smoothstep(edge0, edge1, x)，返回 [0,1]，避免引擎 API 语义歧义 */
static float SmoothStep01(float Edge0, float Edge1, float X)
{
	const float T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
	return T * T * (3.0f - 2.0f * T);
}

// ---------------------------------------------------------------- FWaveSourceBase

float FWaveSourceBase::GetEffectiveAmplitude(double Time) const
{
	const double Age = ComputeAge(Time, StartTime);
	return Amplitude * ComputeTimeDecay(DecayRate, Age);
}

double FWaveSourceBase::ComputeAge(double Time, double InStartTime)
{
	return FMath::Max(0.0, Time - InStartTime);
}

void FWaveSourceBase::GetDispersionParams(float InWaveLength, float& OutK, float& OutOmega, float& OutGroupSpeed)
{
	const float SafeWaveLength = FMath::Max(InWaveLength, WaveEvaluationConstants::MinWaveLength);
	OutK = 2.0f * PI / SafeWaveLength;
	OutOmega = FMath::Sqrt(WaveEvaluationConstants::Gravity * OutK);
	OutGroupSpeed = OutOmega / (2.0f * OutK);
}

float FWaveSourceBase::ComputeTimeDecay(float InDecayRate, double Age)
{
	return FMath::Exp(-InDecayRate * static_cast<float>(Age));
}

float FWaveSourceBase::ComputeRadialEnvelope(float R, float InWaveLength, float InCutoffRadius)
{
	const float SafeWaveLength = FMath::Max(InWaveLength, WaveEvaluationConstants::MinWaveLength);
	const float Spread = FMath::Sqrt(SafeWaveLength / FMath::Max(R, SafeWaveLength));
	const float Cutoff = SmoothStep01(
		InCutoffRadius * WaveEvaluationConstants::RadialCutoffStart, InCutoffRadius, R);
	return Spread * (1.0f - Cutoff);
}

float FWaveSourceBase::ComputeWavefrontFactor(float R, float InWaveLength, float InGroupSpeed, double Age)
{
	const float Front = InGroupSpeed * static_cast<float>(Age);
	const float FadeWidth = InWaveLength * WaveEvaluationConstants::WavefrontFadeWaveLengths;
	// R 远小于波前 → 1；R 超过波前 → 0
	return 1.0f - SmoothStep01(Front - FadeWidth, Front, R);
}

// ---------------------------------------------------------------- FShipWakeSource

float FShipWakeSource::Evaluate(const FVector2D& SamplePos, double Time) const
{
	const double Age = ComputeAge(Time, StartTime);
	if (Age <= 0.0)
	{
		return 0.0f;
	}

	const FVector2D Delta = SamplePos - Position;
	const float R = Delta.Size();
	if (R > CutoffRadius)
	{
		return 0.0f;
	}

	const float K = 2.0f * PI / FMath::Max(WaveLength, 1.0f);
	const float Omega = K * WaveSpeed;	// 波速解耦：相位与波前同速

	// 方位权重：采样方向相对"船尾方向（航向反向）"的夹角决定 Kelvin 楔形权重
	const float SampleAngle = FMath::Atan2(Delta.Y, Delta.X);
	const float BehindAngle = Heading + PI;
	const float AbsTheta = FMath::Abs(FMath::UnwindRadians(SampleAngle - BehindAngle));
	const float HalfAngleRad = FMath::DegreesToRadians(WedgeHalfAngleDeg);
	const float Wedge = 1.0f - SmoothStep01(HalfAngleRad * 0.5f, HalfAngleRad, AbsTheta);
	const float DirectionWeight = OmniWeight + (1.0f - OmniWeight) * Wedge;

	const float Phase = K * R - Omega * static_cast<float>(Age);
	return Amplitude
		* ComputeTimeDecay(DecayRate, Age)
		* ComputeRadialEnvelope(R, WaveLength, CutoffRadius)
		* ComputeWavefrontFactor(R, WaveLength, WaveSpeed, Age)
		* FMath::Cos(Phase)
		* DirectionWeight;
}

// ---------------------------------------------------------------- FSplashWaveSource

float FSplashWaveSource::Evaluate(const FVector2D& SamplePos, double Time) const
{
	const double Age = ComputeAge(Time, StartTime);
	if (Age <= 0.0)
	{
		return 0.0f;
	}

	const FVector2D Delta = SamplePos - Position;
	const float R = Delta.Size();
	if (R > CutoffRadius)
	{
		return 0.0f;
	}

	const float K = 2.0f * PI / FMath::Max(WaveLength, 1.0f);
	const float Omega = K * WaveSpeed;	// 波速解耦：相位与波前同速

	const float Phase = K * R - Omega * static_cast<float>(Age);
	return Amplitude
		* ComputeTimeDecay(DecayRate, Age)
		* ComputeRadialEnvelope(R, WaveLength, CutoffRadius)
		* ComputeWavefrontFactor(R, WaveLength, WaveSpeed, Age)
		* FMath::Cos(Phase);
}

// ---------------------------------------------------------------- FWaveEvaluation

float FWaveEvaluation::EvaluateSource(const FWaveSourceBase& Source, const FVector2D& SamplePos, double Time)
{
	return Source.Evaluate(SamplePos, Time);
}

float FWaveEvaluation::GetWaveHeight(const FVector& WorldPos, double Time, TArrayView<const TSharedPtr<FWaveSourceBase>> Sources)
{
	const FVector2D SamplePos(WorldPos.X, WorldPos.Y);

	float Sum = 0.0f;
	for (const TSharedPtr<FWaveSourceBase>& Source : Sources)
	{
		if (!Source.IsValid())
		{
			continue;
		}

		// 截断半径早期剔除：先比分量再开方
		const float Cutoff = Source->GetCutoffRadius();
		const float DX = FMath::Abs(SamplePos.X - (float)Source->Position.X);
		const float DY = FMath::Abs(SamplePos.Y - (float)Source->Position.Y);
		if (DX > Cutoff || DY > Cutoff)
		{
			continue;
		}

		Sum += Source->Evaluate(SamplePos, Time);
	}
	return Sum;
}

float FWaveEvaluation::GetWaveHeightAndNormal(const FVector& WorldPos, double Time, TArrayView<const TSharedPtr<FWaveSourceBase>> Sources, FVector& OutNormal)
{
	constexpr float Epsilon = 10.0f; // cm

	const float H  = GetWaveHeight(WorldPos, Time, Sources);
	const float HX = GetWaveHeight(WorldPos + FVector(Epsilon, 0.0, 0.0), Time, Sources);
	const float HY = GetWaveHeight(WorldPos + FVector(0.0, Epsilon, 0.0), Time, Sources);

	const float DHDX = (HX - H) / Epsilon;
	const float DHDY = (HY - H) / Epsilon;
	OutNormal = FVector(-DHDX, -DHDY, 1.0f).GetSafeNormal();
	return H;
}
