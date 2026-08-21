// OCEANCOMBAT-MOD: 波纹 RT 渲染管理器（自研动态水面核心）。
// 手册：docs/步骤3-操作手册.md

#pragma once

#include "CoreMinimal.h"
#include "Waves/WaveSourceTypes.h"
#include "OceanWaveRTManager.generated.h"

class UTextureRenderTarget2D;
class UMaterialParameterCollection;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 波纹 RT 渲染管理器：把波源注册表每帧绘制进镜头跟随的 RT。
 * 资产（RT/MPC/图章材质）按固定路径从插件 Content 加载，缺失时告警并跳过。
 */
UCLASS()
class WATER_API UOceanWaveRTManager : public UObject
{
	GENERATED_BODY()

public:
	/** 每帧由 UOceanWaveSubsystem::Tick 调用（游戏线程） */
	void RenderWaves(UWorld* World, const TArray<TSharedPtr<FWaveSourceBase>>& Sources, double WaveTime);

private:
	/** 懒加载内容资产，任一缺失则告警并返回 false */
	bool EnsureAssets();

	/** 取/建波源对应的图章 MID（仅创建时设置参数） */
	UMaterialInstanceDynamic* GetOrCreateMID(const FWaveSourceBase& Source);

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> WakeRT;

	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> MPC;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> StampMaterial;

	/** 键 = SourceID */
	UPROPERTY()
	TMap<int32, TObjectPtr<UMaterialInstanceDynamic>> MIDCache;

	bool bWarnedMissingAsset = false;

	/** RT 覆盖半边长（cm），400m×400m */
	static constexpr float Extent = 20000.0f;
	static constexpr int32 RTSize = 1024;
};
