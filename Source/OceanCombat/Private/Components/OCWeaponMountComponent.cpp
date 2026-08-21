// OceanCombat. Copyright(c) All rights reserved.

#include "Components/OCWeaponMountComponent.h"

#include "Components/ChildActorComponent.h"
#include "Weapons/OCWeaponTurret.h"

UOCWeaponMountComponent::UOCWeaponMountComponent()
{
    // 纯聚合/转发组件,没有每帧逻辑
    PrimaryComponentTick.bCanEverTick = false;
}

void UOCWeaponMountComponent::GetTurrets(TArray<AOCWeaponTurret*>& OutTurrets) const
{
    OutTurrets.Reset();

    const AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // 挂载点在蓝图组件树里,数量/类型随蓝图。这里按"child 是炮塔"过滤,无需额外打 Tag
    TArray<UChildActorComponent*> Mounts;
    Owner->GetComponents<UChildActorComponent>(Mounts);
    for (const UChildActorComponent* Mount : Mounts)
    {
        if (AOCWeaponTurret* Turret = Mount ? Cast<AOCWeaponTurret>(Mount->GetChildActor()) : nullptr)
        {
            OutTurrets.Add(Turret);
        }
    }
}

AOCWeaponTurret* UOCWeaponMountComponent::GetTurret() const
{
    TArray<AOCWeaponTurret*> Turrets;
    GetTurrets(Turrets);
    return Turrets.Num() > 0 ? Turrets[0] : nullptr;
}

void UOCWeaponMountComponent::ApplyConfig(const FOCCombatConfigRow& Config)
{
    TArray<AOCWeaponTurret*> Turrets;
    GetTurrets(Turrets);
    for (AOCWeaponTurret* Turret : Turrets)
    {
        Turret->ApplyConfig(Config);
    }
}
