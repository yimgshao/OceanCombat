// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OCWeaponMountComponent.generated.h"

class AOCWeaponTurret;
struct FOCCombatConfigRow;

/**
 * 武器挂载组件。给"能开火"的单位(玩家/敌船、防御建筑)组合上,turretless 单位(如后续的
 * buff 功能性建筑)不挂它即可——炮塔能力从此是"组合"而非"继承",不再压在 AOCPawnBase 上。
 *
 * 挂载点不由本组件持有,而是由拥有者【蓝图组件树】里的 Child Actor Component 提供:
 * 数量/类型/位置全在蓝图配(1 门塔、4 门城堡、N 门混装都是纯内容,不需要新的 C++ 类)。
 * 本组件只在运行时扫描拥有者身上所有 ChildActorComponent,取其中 child 是 AOCWeaponTurret 的,
 * 对外提供统一的 GetTurrets/GetTurret/ApplyConfig。
 */
UCLASS(ClassGroup=(OceanCombat), meta=(BlueprintSpawnableComponent))
class OCEANCOMBAT_API UOCWeaponMountComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOCWeaponMountComponent();

    /** 收集拥有者身上所有挂载的炮塔(扫描 ChildActorComponent,Cast 成功即为炮塔;无则返回空数组) */
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void GetTurrets(TArray<AOCWeaponTurret*>& OutTurrets) const;

    /** 获取第一门炮塔(单炮塔单位用它即可;无炮塔返回 nullptr) */
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    AOCWeaponTurret* GetTurret() const;

    /** 把战斗配置下发给所有炮塔(由拥有者 BeginPlay 读表后调用) */
    void ApplyConfig(const FOCCombatConfigRow& Config);
};
