// OCEANCOMBAT-MOD: 波源注册表子系统（自研动态水面核心）。
// 设计文档：docs/步骤1-波源数据层详细设计.md

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Waves/WaveSourceTypes.h"
#include "OceanWaveSubsystem.generated.h"

/**
 * 波源注册表（游戏线程写入，物理线程经快照只读）。
 * 管理所有解析波源（船尾迹、落水冲击等）的注册、淘汰与不可变快照发布，
 * Tick 末尾驱动波纹 RT 渲染（UOceanWaveRTManager）。
 */
UCLASS()
class WATER_API UOceanWaveSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UOceanWaveSubsystem, STATGROUP_Tickables); }

	/** 通用注册（多态入口），返回索引句柄。仅游戏线程调用。 */
	int32 AddWaveSource(const TSharedPtr<FWaveSourceBase>& Source);

	/** 便捷构造：船只尾迹。Heading 为弧度（0 = +X） */
	UFUNCTION(BlueprintCallable, Category="Waves")
	int32 AddShipWakeSource(FVector2D Pos, float Heading, float InAmplitude,
		float InWaveLength = 300.0f, float InWaveSpeed = 600.0f, float InDecayRate = 0.08f, float InCutoffRadius = 2000.0f);

	/** 便捷构造：落水冲击 */
	UFUNCTION(BlueprintCallable, Category="Waves")
	int32 AddSplashSource(FVector2D Pos, float InAmplitude,
		float InWaveLength = 150.0f, float InWaveSpeed = 300.0f, float InDecayRate = 1.0f, float InCutoffRadius = 1000.0f);

	/** 当前不可变快照（原子获取，游戏/物理线程均可调用） */
	TSharedPtr<const TArray<TSharedPtr<FWaveSourceBase>>, ESPMode::ThreadSafe> GetSnapshot() const;

	/** 当前 WaterTime（秒） */
	double GetWaveTime() const;

protected:
	UPROPERTY(EditAnywhere, Category = "Waves")
	int32 MaxSources = 256;

	/** 有效振幅低于该值（cm）即淘汰 */
	UPROPERTY(EditAnywhere, Category = "Waves")
	float MinAmplitude = 1.0f;

	/** 存活时间硬上限（秒） */
	UPROPERTY(EditAnywhere, Category = "Waves")
	float MaxLifetime = 30.0f;

private:
	/** 仅游戏线程读写 */
	TArray<TSharedPtr<FWaveSourceBase>> Sources;

	/** 发布给物理线程的不可变快照，增删时重建 */
	TSharedPtr<TArray<TSharedPtr<FWaveSourceBase>>, ESPMode::ThreadSafe> Snapshot;

	bool bSnapshotDirty = false;

	/** 波源稳定 ID 分配器（MID 缓存键） */
	int32 NextSourceID = 1;

	/** 波纹 RT 渲染管理器（步骤 3） */
	UPROPERTY()
	TObjectPtr<class UOceanWaveRTManager> RTManager;

	void DrawDebugSources(double Now) const;
};
