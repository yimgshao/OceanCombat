// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OCLootDropComponent.generated.h"

class AOCPickupBase;

/**
 * 掉落组件。挂在任意 AOCPawnBase(敌船/建筑)上,监听其血量组件的 OnDeath,
 * 死亡时按 DropChance 摇概率生成掉落物。
 *
 * 与 UOCDestructionComponent 同构:都是"挂在单位上、监听 OnDeath、自治处理一件事"。
 * 两者互不认识,同一次死亡各做各的(碎裂表现 / 掉落)。
 *
 * 掉落物类型是 AOCPickupBase, PickupClass 留空则不掉落。
 */
UCLASS(ClassGroup=(OceanCombat), meta=(BlueprintSpawnableComponent))
class OCEANCOMBAT_API UOCLootDropComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOCLootDropComponent();

    /** 掉落物蓝图(如 BP_HealthPickup)。留空则本单位不掉落任何东西 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
    TSubclassOf<AOCPickupBase> PickupClass;

    /** 掉落概率(0=从不掉落,1=必掉) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DropChance = 0.3f;

    /**
     * 是否只有玩家击杀才掉落。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
    bool bRequirePlayerKill = true;

    /**
     * 掉落点的水面高度(cm,世界 Z),一般等于海平面。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
    float SeaLevelZ = 0.0f;

    /** 落点水平随机偏移半径(cm),避免血包与死亡碎块视觉重叠。0=正好在死亡点 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0"))
    float DropOffsetRadius = 0.0f;

protected:
    virtual void BeginPlay() override;

private:
    /** 死亡回调:校验击杀者 → 摇概率 → 在水面生成掉落物 */
    UFUNCTION()
    void HandleDeath(AActor* DeadActor, AController* KillerController);
};
