#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OCBoatShallowWaterComponent.generated.h"

class UShallowWaterSubsystem;

/**
 * 小船与浅水模拟(WaterAdvanced 插件)的交互组件。
 * 挂在船上,按固定频率调用 UShallowWaterSubsystem::RegisterImpact
 * 把船的运动注入浅水模拟,使船航行时在水面产生波纹。
 *
 * 注意:仅视觉效果——浅水模拟没有 CPU 回读通道,波纹不影响浮力。
 * 模拟域跟随玩家 Pawn,域外船只的注入会被模拟忽略。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OCEANCOMBAT_API UOCBoatShallowWaterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOCBoatShallowWaterComponent();

    /** 冲击注入频率(次/秒) */
    UPROPERTY(EditDefaultsOnly, Category = "ShallowWater", meta = (ClampMin = "1.0"))
    float ImpactsPerSecond = 10.0f;

    /** 船速低于该值(cm/s)时不注入,避免静水起波 */
    UPROPERTY(EditDefaultsOnly, Category = "ShallowWater", meta = (ClampMin = "0.0"))
    float MinSpeed = 50.0f;

    /** 船首冲击点:沿船头方向相对船锚点的偏移(cm) */
    UPROPERTY(EditDefaultsOnly, Category = "ShallowWater")
    float BowOffset = 300.0f;

    /** 船尾冲击点:沿船头方向的偏移(负值,cm);填 0 则不启用船尾点 */
    UPROPERTY(EditDefaultsOnly, Category = "ShallowWater")
    float SternOffset = -300.0f;

    /** 冲击半径(cm) */
    UPROPERTY(EditDefaultsOnly, Category = "ShallowWater", meta = (ClampMin = "0.0"))
    float ImpactRadius = 300.0f;

    /** 冲击速度缩放:RegisterImpact 的 Velocity 参数 = 船速向量 × 该系数 */
    UPROPERTY(EditDefaultsOnly, Category = "ShallowWater", meta = (ClampMin = "0.0"))
    float ImpactVelocityScale = 1.0f;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    /** 懒查找并缓存浅水子系统;插件未启用或子系统未初始化时返回 nullptr */
    UShallowWaterSubsystem* GetShallowWaterSubsystem();

    UPROPERTY(Transient)
    TObjectPtr<UShallowWaterSubsystem> ShallowWaterSubsystem;

    float TimeSinceLastImpact = 0.0f;
};
