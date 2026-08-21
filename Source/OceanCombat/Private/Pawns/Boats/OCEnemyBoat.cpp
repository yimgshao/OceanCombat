// OceanCombat. Copyright(c) All rights reserved.

#include "Pawns/Boats/OCEnemyBoat.h"

#include "Components/OCDestructionComponent.h"
#include "Components/OCLootDropComponent.h"
#include "Controllers/OCAIBoatController.h"

AOCEnemyBoat::AOCEnemyBoat()
{
    // AI 自动接管(关卡摆放或运行时生成都可)
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AOCAIBoatController::StaticClass();

    // 移动参数初值(蓝图可覆盖)。转向比玩家船略强以便来回巡弋掉头,
    // 具体数值需编译后实测调整
    ForwardThrust   = 55000.0f;   // N
    ReverseThrust   = 30000.0f;   // N
    TurnTorque      = 60.0f;      // 度/秒²:稳态转向≈TurnTorque/AngularDamping。8 太肉(≈3°/s)躲不开障碍,先给 60(≈24°/s),据埋点再调
    MaxForwardSpeed = 1600.0f;    // cm/s(= 16 m/s)
    LinearDamping   = 0.5f;
    AngularDamping  = 2.5f;

    // ---- 死亡碎裂表现(碎裂资产 GC_Boat_01、爆炸特效在蓝图里配)----
    // 船在动,碎块继承死亡瞬间船速,飞散更自然
    DestructionComp = CreateDefaultSubobject<UOCDestructionComponent>(TEXT("DestructionComp"));
    DestructionComp->bInheritVelocity = true;

    // ---- 掉落(掉落物蓝图 BP_HealthPickup、掉率在蓝图里配)----
    // 落点偏移开一点:掉落物正好压在死亡碎块堆里会看不见
    LootDropComp = CreateDefaultSubobject<UOCLootDropComponent>(TEXT("LootDropComp"));
    LootDropComp->DropOffsetRadius = 300.0f;
}
