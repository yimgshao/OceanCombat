#include "Components/OCBoatShallowWaterComponent.h"

#include "ShallowWaterSubsystem.h"

UOCBoatShallowWaterComponent::UOCBoatShallowWaterComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

UShallowWaterSubsystem* UOCBoatShallowWaterComponent::GetShallowWaterSubsystem()
{
    if (ShallowWaterSubsystem)
    {
        return ShallowWaterSubsystem;
    }

    TArray<UShallowWaterSubsystem*> Subsystems = GetWorld()->GetSubsystemArrayCopy<UShallowWaterSubsystem>();
    ShallowWaterSubsystem = Subsystems.Num() > 0 ? Subsystems[0] : nullptr;
    return ShallowWaterSubsystem;
}

void UOCBoatShallowWaterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UShallowWaterSubsystem* Subsystem = GetShallowWaterSubsystem();
    if (!Subsystem)
    {
        return;
    }

    TimeSinceLastImpact += DeltaTime;
    const float Interval = 1.0f / ImpactsPerSecond;
    if (TimeSinceLastImpact < Interval)
    {
        return;
    }

    const AActor* Owner = GetOwner();
    const FVector Velocity = Owner->GetVelocity();
    const float Speed = Velocity.Size2D();
    if (Speed < MinSpeed)
    {
        TimeSinceLastImpact = 0.0f;
        return;
    }
    TimeSinceLastImpact = FMath::Fmod(TimeSinceLastImpact, Interval);

    const FVector ImpactVelocity = Velocity * ImpactVelocityScale;
    const FVector Forward = Owner->GetActorForwardVector();
    const FVector Base = Owner->GetActorLocation();

    // 水面定位由 UShallowWaterSubsystem::RegisterImpact 内部的水面查询负责(OCEANCOMBAT-MOD),
    // 这里只需给出大致位置
    Subsystem->RegisterImpact(Base + Forward * BowOffset, ImpactVelocity, ImpactRadius);
    if (!FMath::IsNearlyZero(SternOffset))
    {
        Subsystem->RegisterImpact(Base + Forward * SternOffset, ImpactVelocity, ImpactRadius);
    }
}
