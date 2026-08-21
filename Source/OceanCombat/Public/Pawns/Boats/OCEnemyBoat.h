// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Pawns/Boats/OCBoatBase.h"
#include "OCEnemyBoat.generated.h"

class UOCDestructionComponent;
class UOCLootDropComponent;

/**
 * 敌方船。由 AOCAIBoatController 拥有,AI 决策(侧舷阵线巡弋 + 避障)全在 Controller 里。
 * 本阶段与玩家船结构相同,mesh 先用同款,后续替换。
 *
 * 移动仍走基类 AOCBoatBase 的物理接口(AddThrottleInput/AddSteerInput),
 * 本类只负责:声明具体类(基类是 Abstract 不能实例化)+ 指定 AI 控制器 + 移动参数初值
 * + 死亡碎裂表现(UOCDestructionComponent)+ 掉落物(UOCLootDropComponent)。
 */
UCLASS()
class OCEANCOMBAT_API AOCEnemyBoat : public AOCBoatBase
{
    GENERATED_BODY()

public:
    AOCEnemyBoat();

protected:
    /** 死亡碎裂/爆炸表现:碎裂资产 GC_Boat_01、爆炸特效等在蓝图的该组件上配。碎块继承船速 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat")
    TObjectPtr<UOCDestructionComponent> DestructionComp;

    /** 掉落:掉落物蓝图与掉率在蓝图的该组件上配。PickupClass 留空则不掉落 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat")
    TObjectPtr<UOCLootDropComponent> LootDropComp;
};

