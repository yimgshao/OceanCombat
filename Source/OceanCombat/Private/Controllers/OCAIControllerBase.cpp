// OceanCombat. Copyright(c) All rights reserved.

#include "Controllers/OCAIControllerBase.h"

#include "Components/OCHealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Pawns/OCPawnBase.h"
#include "Weapons/OCWeaponTurret.h"

AOCAIControllerBase::AOCAIControllerBase()
{
    PrimaryActorTick.bCanEverTick = true;
}

AActor* AOCAIControllerBase::AcquireTarget() const
{
    // 默认索敌:玩家 Pawn。阵营系统落地后改成按阵营过滤
    return UGameplayStatics::GetPlayerPawn(this, 0);
}

FVector AOCAIControllerBase::RollAimPoint(const FVector& TargetLocation, float RandomRadius) const
{
    // 水平面内随机偏移,Z 保持目标高度
    const FVector2D Offset = FMath::RandPointInCircle(RandomRadius);
    return TargetLocation + FVector(Offset.X, Offset.Y, 0.0f);
}

void AOCAIControllerBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    AOCPawnBase* ControlledPawn = GetPawn<AOCPawnBase>();
    if (!ControlledPawn)
    {
        return;
    }

    // 自己死了就停火
    const UOCHealthComponent* Health = ControlledPawn->GetHealthComponent();
    if (Health && Health->IsDead())
    {
        return;
    }

    TArray<AOCWeaponTurret*> Turrets;
    ControlledPawn->GetTurrets(Turrets);
    if (Turrets.Num() == 0)
    {
        return;
    }

    AActor* Target = AcquireTarget();
    if (!Target)
    {
        return;
    }

    // 射程检查(水平距离,最近~最远之间才开火),参数在 Pawn 上(可按实例覆盖)
    const FVector ToTarget = Target->GetActorLocation() - ControlledPawn->GetActorLocation();
    const float DistToTarget = FVector2D(ToTarget.X, ToTarget.Y).Size();
    if (DistToTarget > ControlledPawn->AttackRange || DistToTarget < ControlledPawn->MinAttackRange)
    {
        bHasAimPoint = false;  // 出射程后,下次进射程重新摇瞄准点
        return;
    }

    if (!bHasAimPoint)
    {
        CurrentAimPoint = RollAimPoint(Target->GetActorLocation(), ControlledPawn->AimRandomRadius);
        bHasAimPoint = true;
    }

    // 每门炮塔各自瞄准当前瞄准点并开火(CD 在炮塔内部各自节流,多炮塔单位自然分时开火)
    bool bAnyFired = false;
    for (AOCWeaponTurret* Turret : Turrets)
    {
        // 弹道瞄准;超出弹道射程时只摆炮口、不开火
        const bool bReachable = Turret->AimAtBallistic(CurrentAimPoint);
        if (bReachable && Turret->Fire())
        {
            bAnyFired = true;
        }
    }

    // 真打出去了才为下一发摇新瞄准点(目标当前位置 + 新随机偏移)
    if (bAnyFired)
    {
        CurrentAimPoint = RollAimPoint(Target->GetActorLocation(), ControlledPawn->AimRandomRadius);
    }
}
