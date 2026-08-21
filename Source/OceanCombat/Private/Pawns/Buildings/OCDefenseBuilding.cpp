// OceanCombat. Copyright(c) All rights reserved.

#include "Pawns/Buildings/OCDefenseBuilding.h"

#include "Components/OCWeaponMountComponent.h"
#include "Controllers/OCAIBuildingController.h"

AOCDefenseBuilding::AOCDefenseBuilding()
{
    // 自动由 AI 拥有:摆进关卡/运行时生成都会 Possess,无需蓝图配置
    AIControllerClass = AOCAIBuildingController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // 武器挂载组件:炮塔挂载点(Child Actor Component)在蓝图组件树里配,数量/类型随蓝图
    WeaponMount = CreateDefaultSubobject<UOCWeaponMountComponent>(TEXT("WeaponMount"));
}
