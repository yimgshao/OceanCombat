// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Pawns/OCPawnBase.h"
#include "OCBuildingBase.generated.h"

class UStaticMeshComponent;
class UOCDestructionComponent;

/**
 * 建筑基类。所有静态建筑的通用部分:建筑 Mesh(根组件,不动)+ 死亡碎裂/爆炸表现。
 * 不关心是否有炮塔——防御建筑(AOCDefenseBuilding)自带武器挂载组件,功能性建筑(如 buff 建筑)
 * 可直接继承本类而不挂武器。死亡碎裂/隐藏炮塔统一由 UOCDestructionComponent 处理。
 *
 * 碰撞要点:物体类型必须是动态类型(WorldDynamic),不能用普通静态建筑的
 * WorldStatic——径向 AOE 伤害(ApplyRadialDamageWithFalloff)只 Overlap 动态对象,
 * WorldStatic 会导致炮弹落旁边时 AOE 打不到建筑。
 */
UCLASS(Abstract)
class OCEANCOMBAT_API AOCBuildingBase : public AOCPawnBase
{
    GENERATED_BODY()

public:
    AOCBuildingBase();

protected:
    /** 建筑 Mesh(也是 RootComponent)。具体模型在蓝图里配,不模拟物理 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
    TObjectPtr<UStaticMeshComponent> BuildingMesh;

    /** 死亡碎裂/爆炸表现:碎裂资产、爆炸特效等在蓝图的该组件上配 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
    TObjectPtr<UOCDestructionComponent> DestructionComp;
};
