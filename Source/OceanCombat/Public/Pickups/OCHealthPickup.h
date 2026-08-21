// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Pickups/OCPickupBase.h"
#include "OCHealthPickup.generated.h"

/**
 * 血包。玩家船碰到即回血并消失。
 *
 * 触发/漂浮/存活/销毁等通用逻辑全在基类 AOCPickupBase,本类只实现回血效果。
 * 回血走 UOCHealthComponent::Heal(),血条经其 OnHealthChanged 自动刷新,UI 无需改动。
 */
UCLASS()
class OCEANCOMBAT_API AOCHealthPickup : public AOCPickupBase
{
    GENERATED_BODY()

public:
    // ---- 回血量(两项相加,任一填 0 即退化为单一模式)----
    /** 固定回血量 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Heal", meta = (ClampMin = "0.0"))
    float HealAmount = 30.0f;

    /**
     * 按拾取者最大血量的百分比额外回血(0~1)。
     * 与固定值并存的原因:玩家血量上限随升级成长,纯绝对值到后期越来越不值钱,
     * 纯百分比又不好在前期给出手感。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Heal", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HealPercentOfMax = 0.0f;

protected:
    /** 回血。满血也照常消耗(Heal 内部会夹到上限);拾取者没有血量组件/已死亡则不消耗 */
    virtual bool TryApplyPickup(APawn* PickerPawn) override;
};
