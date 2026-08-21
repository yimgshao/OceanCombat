// OceanCombat. Copyright(c) All rights reserved.

#include "Pawns/Buildings/OCBuildingBase.h"

#include "Components/OCDestructionComponent.h"
#include "Components/StaticMeshComponent.h"

AOCBuildingBase::AOCBuildingBase()
{
    // 建筑没有每帧逻辑(索敌/开火交给 Controller),不需要 Tick
    PrimaryActorTick.bCanEverTick = false;

    // ---- 建筑 Mesh(作为 RootComponent)----
    BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
    SetRootComponent(BuildingMesh);

    // 静态建筑:不模拟物理
    BuildingMesh->SetSimulatePhysics(false);

    // 碰撞:BlockAllDynamic(物体类型 WorldDynamic,Block 所有通道)。
    // 必须用动态物体类型:径向 AOE 伤害只 Overlap 动态对象,
    // WorldStatic 会导致炮弹落在建筑旁边时范围伤害打不到它。
    BuildingMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    // ---- 死亡碎裂/爆炸表现(碎裂资产、爆炸特效在蓝图里配)----
    // 建筑是静态的,碎块无需继承速度(bInheritVelocity 保持默认 false)
    DestructionComp = CreateDefaultSubobject<UOCDestructionComponent>(TEXT("DestructionComp"));
}
