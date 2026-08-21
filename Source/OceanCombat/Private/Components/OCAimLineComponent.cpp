// OceanCombat. Copyright(c) All rights reserved.

#include "Components/OCAimLineComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Pawns/Boats/OCBoatBase.h"
#include "Weapons/OCWeaponTurret.h"

UOCAimLineComponent::UOCAimLineComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    // 只在显示时才 Tick,平时不消耗
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UOCAimLineComponent::SetShowing(bool bInShowing)
{
    bShowing = bInShowing;
    SetComponentTickEnabled(bInShowing);
}

void UOCAimLineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bShowing)
    {
        return;
    }

    UWorld* World = GetWorld();
    AOCBoatBase* Boat = Cast<AOCBoatBase>(GetOwner());
    AOCWeaponTurret* Turret = Boat ? Boat->GetTurret() : nullptr;
    if (!World || !Turret)
    {
        return;
    }

    TArray<FVector> Points;
    FVector End = FVector::ZeroVector;
    bool bHitWater = false;
    Turret->PredictTrajectory(Points, End, bHitWater);
    if (Points.Num() < 2)
    {
        return;
    }

    // DrawDebug:LifeTime=-1 且每帧重画 → 持续一帧,随瞄准实时更新
    const FColor Color = bHitWater ? WaterHitColor : HardHitColor;
    for (int32 i = 0; i + 1 < Points.Num(); ++i)
    {
        DrawDebugLine(World, Points[i], Points[i + 1], Color, false, -1.0f, 0, LineThickness);
    }
    DrawDebugSphere(World, End, EndMarkerRadius, 12, Color, false, -1.0f, 0, 2.0f);
}
