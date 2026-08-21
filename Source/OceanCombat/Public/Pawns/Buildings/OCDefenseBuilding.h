// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Pawns/Buildings/OCBuildingBase.h"
#include "OCDefenseBuilding.generated.h"

class UOCWeaponMountComponent;

/**
 * 敌方防御建筑。静态建筑 + 若干门炮塔(复用 AOCWeaponTurret),索敌/开火由 AOCAIBuildingController 驱动。
 * 炮塔挂载点是【蓝图组件树里的 Child Actor Component】——数量/类型/位置全在蓝图配:
 * 防御塔、城堡、任意 N 门混装建筑都是它的蓝图,不需要为炮塔数量新建 C++ 类。
 */
UCLASS()
class OCEANCOMBAT_API AOCDefenseBuilding : public AOCBuildingBase
{
    GENERATED_BODY()

public:
    AOCDefenseBuilding();

protected:
    /** 武器挂载组件:统一提供炮塔归集/配置下发(挂载点在蓝图组件树里) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<UOCWeaponMountComponent> WeaponMount;
};
