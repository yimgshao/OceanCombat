// OceanCombat. Copyright(c) All rights reserved.

#include "Pickups/OCHealthPickup.h"

#include "Components/OCHealthComponent.h"
#include "GameFramework/Pawn.h"

bool AOCHealthPickup::TryApplyPickup(APawn* PickerPawn)
{
    UOCHealthComponent* Health = PickerPawn->FindComponentByClass<UOCHealthComponent>();
    if (!Health || Health->IsDead())
    {
        return false; // 没有血量组件或已死亡:不消耗,血包留在原地
    }

    // 满血也照常拾取消失(Heal 内部会夹到上限)
    const float TotalHeal = HealAmount + Health->GetMaxHealth() * HealPercentOfMax;
    Health->Heal(TotalHeal); // 血条经 OnHealthChanged 自动刷新

    UE_LOG(LogTemp, Log, TEXT("[Loot] %s 拾取血包,回血 %.0f(当前 %.0f/%.0f)"),
        *GetNameSafe(PickerPawn), TotalHeal, Health->GetCurrentHealth(), Health->GetMaxHealth());

    return true;
}
