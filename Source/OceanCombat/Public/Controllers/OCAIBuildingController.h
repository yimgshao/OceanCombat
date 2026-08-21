// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controllers/OCAIControllerBase.h"
#include "OCAIBuildingController.generated.h"

/**
 * 防御建筑/主基地 AI。行为全部继承基类战斗循环(索敌/瞄准/开火),
 * 建筑不移动,无需额外逻辑;参数(射程/散布)在 Details 里调。
 */
UCLASS()
class OCEANCOMBAT_API AOCAIBuildingController : public AOCAIControllerBase
{
    GENERATED_BODY()
};
