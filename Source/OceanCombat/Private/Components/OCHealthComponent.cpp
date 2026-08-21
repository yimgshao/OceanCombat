// OceanCombat. Copyright(c) All rights reserved.

#include "Components/OCHealthComponent.h"

UOCHealthComponent::UOCHealthComponent()
{
    // 血量全靠事件驱动,不需要 Tick
    PrimaryComponentTick.bCanEverTick = false;
}

void UOCHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;
    OnHealthChanged.Broadcast(CurrentHealth, CurrentHealth);
}

void UOCHealthComponent::InitMaxHealth(float NewMaxHealth)
{
    MaxHealth = FMath::Max(1.0f, NewMaxHealth);
    CurrentHealth = MaxHealth;
    OnHealthChanged.Broadcast(CurrentHealth, CurrentHealth);
}

void UOCHealthComponent::SetMaxHealth(float NewMaxHealth, bool bAdjustCurrent)
{
    if (bIsDead)
    {
        return;
    }

    const float ClampedMax = FMath::Max(1.0f, NewMaxHealth);
    const float MaxDelta = ClampedMax - MaxHealth;
    if (FMath::IsNearlyZero(MaxDelta))
    {
        return;
    }

    const float OldHealth = CurrentHealth;
    MaxHealth = ClampedMax;

    // 上限提升的部分直接补进当前血量(升级即时收益);上限下降时靠 Min 夹住
    CurrentHealth = bAdjustCurrent
        ? FMath::Clamp(CurrentHealth + MaxDelta, 1.0f, MaxHealth)
        : FMath::Min(CurrentHealth, MaxHealth);

    // 上限变了血条百分比就变了(即使当前血量没动),无条件广播让 UI 重刷
    OnHealthChanged.Broadcast(OldHealth, CurrentHealth);
}

void UOCHealthComponent::ApplyDamage(float Amount, AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsDead || Amount <= 0.0f)
    {
        return;
    }

    const float OldHealth = CurrentHealth;
    CurrentHealth = FMath::Max(0.0f, CurrentHealth - Amount);
    LastDamageInstigator = EventInstigator; // 击杀归属:死亡时随 OnDeath 广播

    OnHealthChanged.Broadcast(OldHealth, CurrentHealth);

    if (CurrentHealth <= 0.0f)
    {
        bIsDead = true;
        UE_LOG(LogTemp, Log, TEXT("[Health] %s 死亡"), *GetNameSafe(GetOwner()));
        OnDeath.Broadcast(GetOwner(), LastDamageInstigator.Get());
    }
}

void UOCHealthComponent::Heal(float Amount)
{
    if (bIsDead || Amount <= 0.0f)
    {
        return;
    }

    const float OldHealth = CurrentHealth;
    CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);

    if (!FMath::IsNearlyEqual(OldHealth, CurrentHealth))
    {
        OnHealthChanged.Broadcast(OldHealth, CurrentHealth);
    }
}

float UOCHealthComponent::GetHealthPercent() const
{
    return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

void UOCHealthComponent::SetLastHitInfo(const FVector& WorldLocation, float BlastRadius)
{
    LastHitLocation = WorldLocation;
    LastBlastRadius = BlastRadius;
    bHasLastHitInfo = true;
}
