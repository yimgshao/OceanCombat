// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "OCAimLineComponent.generated.h"

/**
 * 瞄准线组件。挂在玩家船上,长按右键时由玩家控制器 SetShowing(true) 打开;
 * 打开后每帧向本船首个炮塔取 PredictTrajectory 得到与真实炮弹一致的弹道折线并绘制。
 *
 * 当前渲染:DrawDebug 折线 + 落点标记(用于验证"预测线落点 == 炮弹落点")。
 * 后续外观:换成 Spline + SplineMesh(细长 mesh + 半透明材质),接口保持不变。
 */
UCLASS(ClassGroup=(OceanCombat), meta=(BlueprintSpawnableComponent))
class OCEANCOMBAT_API UOCAimLineComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UOCAimLineComponent();

    /** 显示/隐藏瞄准线(玩家长按右键时由控制器调用);隐藏时停 Tick */
    void SetShowing(bool bInShowing);

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    /** 落点标记调试球半径(cm) */
    UPROPERTY(EditDefaultsOnly, Category = "AimLine")
    float EndMarkerRadius = 40.0f;

    /** 线宽(调试线厚度) */
    UPROPERTY(EditDefaultsOnly, Category = "AimLine")
    float LineThickness = 6.0f;

    /** 命中硬物时线色 */
    UPROPERTY(EditDefaultsOnly, Category = "AimLine")
    FColor HardHitColor = FColor(255, 80, 80);

    /** 落水时线色 */
    UPROPERTY(EditDefaultsOnly, Category = "AimLine")
    FColor WaterHitColor = FColor(80, 200, 255);

private:
    bool bShowing = false;
};
