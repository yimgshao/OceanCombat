// OCEANCOMBAT-MOD: 船只尾迹发射器组件（自研动态水面核心）。
// 设计文档：docs/步骤2-尾迹发射器详细设计.md

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OceanWakeEmitterComponent.generated.h"

/**
 * 船只尾迹发射器：挂在船 Actor 下，按位移间隔在船尾发射解析波源。
 * 与移动方式完全解耦（物理/导航/玩家驾驶/AI 均可），只读 Owner 位移。
 */
UCLASS(ClassGroup=(OceanWaves), meta=(BlueprintSpawnableComponent))
class WATER_API UOceanWakeEmitterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOceanWakeEmitterComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- 发射规则 ----

	UPROPERTY(EditAnywhere, Category="Wake")
	bool bEmitEnabled = true;

	/** 位移发射间隔（cm） */
	UPROPERTY(EditAnywhere, Category="Wake")
	float EmissionDistanceInterval = 150.0f;

	/** 低于该速度（cm/s）不发射 */
	UPROPERTY(EditAnywhere, Category="Wake")
	float MinSpeedThreshold = 50.0f;

	// ---- 发射点（Owner 局部坐标，-X 为船尾） ----

	UPROPERTY(EditAnywhere, Category="Wake")
	float SternOffsetX = -200.0f;

	/** 左右舷偏移（cm） */
	UPROPERTY(EditAnywhere, Category="Wake")
	float BeamOffsetY = 80.0f;

	/** true=左右舷双源，false=船尾中线单源 */
	UPROPERTY(EditAnywhere, Category="Wake")
	bool bDualSources = true;

	// ---- 波形参数 ----

	/** 参考速度下的振幅（cm），实际振幅 = AmplitudeScale × (Speed/SpeedReference)² */
	UPROPERTY(EditAnywhere, Category="Wake")
	float AmplitudeScale = 30.0f;

	UPROPERTY(EditAnywhere, Category="Wake")
	float SpeedReference = 1000.0f;

	UPROPERTY(EditAnywhere, Category="Wake")
	float WaveLength = 300.0f;

	/** 波前传播速度（cm/s），越大尾迹越"跟船" */
	UPROPERTY(EditAnywhere, Category="Wake")
	float WaveSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category="Wake")
	float DecayRate = 0.08f;

	UPROPERTY(EditAnywhere, Category="Wake")
	float CutoffRadius = 2000.0f;

	// ---- 调试自动航行（验收用，正式船接管移动后关闭） ----

	UPROPERTY(EditAnywhere, Category="Wake|Debug")
	bool bAutoDrive = false;

	UPROPERTY(EditAnywhere, Category="Wake|Debug")
	float AutoDriveSpeed = 600.0f;

	/** 转弯角速度（度/秒），0=直线 */
	UPROPERTY(EditAnywhere, Category="Wake|Debug")
	float AutoDriveTurnRate = 0.0f;

private:
	FVector LastPosition = FVector::ZeroVector;
	float DistanceAccumulator = 0.0f;
	bool bFirstTick = true;
};
