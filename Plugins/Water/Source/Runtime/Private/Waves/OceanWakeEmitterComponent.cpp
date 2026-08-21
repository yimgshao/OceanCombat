// OCEANCOMBAT-MOD: 船只尾迹发射器组件实现（自研动态水面核心）。

#include "Waves/OceanWakeEmitterComponent.h"

#include "Waves/OceanWaveSubsystem.h"

UOceanWakeEmitterComponent::UOceanWakeEmitterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UOceanWakeEmitterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner || DeltaTime <= 0.0f)
	{
		return;
	}

	// ---- 调试自动航行（SetActorLocation 驱动，无需输入配置） ----
	if (bAutoDrive)
	{
		FRotator Rotation = Owner->GetActorRotation();
		if (!FMath::IsNearlyZero(AutoDriveTurnRate))
		{
			Rotation.Yaw += AutoDriveTurnRate * DeltaTime;
			Owner->SetActorRotation(Rotation);
		}
		Owner->SetActorLocation(Owner->GetActorLocation() + Rotation.Vector() * AutoDriveSpeed * DeltaTime, false);
	}

	// ---- 位移采样（差分，对物理/非物理移动都成立） ----
	const FVector CurrentPosition = Owner->GetActorLocation();
	if (bFirstTick)
	{
		LastPosition = CurrentPosition;
		bFirstTick = false;
		return;
	}
	const FVector FrameDelta = CurrentPosition - LastPosition;
	LastPosition = CurrentPosition;

	if (!bEmitEnabled)
	{
		DistanceAccumulator = 0.0f;
		return;
	}

	const float FrameDistance = FrameDelta.Size2D();
	DistanceAccumulator += FrameDistance;

	const float Speed = FrameDistance / DeltaTime;
	if (Speed < MinSpeedThreshold)
	{
		return;
	}

	UOceanWaveSubsystem* Subsystem = GetWorld()->GetSubsystem<UOceanWaveSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	// 振幅 ∝ 速度²，上限防瞬移暴冲
	const float SpeedRatio = Speed / SpeedReference;
	const float Amplitude = FMath::Min(AmplitudeScale * SpeedRatio * SpeedRatio, AmplitudeScale * 4.0f);
	const float HeadingRad = FMath::DegreesToRadians(Owner->GetActorRotation().Yaw);
	const FTransform OwnerTransform = Owner->GetActorTransform();

	// 防爆发：单帧最多补发 4 个间隔（瞬移/卡顿场景），溢出清零
	int32 BurstCount = 0;
	while (DistanceAccumulator >= EmissionDistanceInterval && BurstCount < 4)
	{
		DistanceAccumulator -= EmissionDistanceInterval;
		++BurstCount;

		if (bDualSources)
		{
			for (const float OffsetY : { BeamOffsetY, -BeamOffsetY })
			{
				const FVector EmitPos = OwnerTransform.TransformPosition(FVector(SternOffsetX, OffsetY, 0.0f));
				Subsystem->AddShipWakeSource(FVector2D(EmitPos.X, EmitPos.Y), HeadingRad, Amplitude,
					WaveLength, WaveSpeed, DecayRate, CutoffRadius);
			}
		}
		else
		{
			const FVector EmitPos = OwnerTransform.TransformPosition(FVector(SternOffsetX, 0.0f, 0.0f));
			Subsystem->AddShipWakeSource(FVector2D(EmitPos.X, EmitPos.Y), HeadingRad, Amplitude,
				WaveLength, WaveSpeed, DecayRate, CutoffRadius);
		}
	}
	if (DistanceAccumulator >= EmissionDistanceInterval)
	{
		DistanceAccumulator = 0.0f;
	}
}
