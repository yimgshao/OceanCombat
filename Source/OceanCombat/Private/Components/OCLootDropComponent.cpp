// OceanCombat. Copyright(c) All rights reserved.

#include "Components/OCLootDropComponent.h"

#include "Components/OCHealthComponent.h"
#include "GameFramework/Controller.h"
#include "Pickups/OCPickupBase.h"

UOCLootDropComponent::UOCLootDropComponent()
{
    // 只在死亡时被事件驱动,无需每帧 Tick
    PrimaryComponentTick.bCanEverTick = false;
}

void UOCLootDropComponent::BeginPlay()
{
    Super::BeginPlay();

    // 监听本体血量组件的 OnDeath(与 UOCDestructionComponent 各自独立监听)
    AActor* Owner = GetOwner();
    UOCHealthComponent* Health = Owner ? Owner->FindComponentByClass<UOCHealthComponent>() : nullptr;
    if (Health)
    {
        Health->OnDeath.AddDynamic(this, &UOCLootDropComponent::HandleDeath);
    }
}

void UOCLootDropComponent::HandleDeath(AActor* DeadActor, AController* KillerController)
{
    UWorld* World = GetWorld();
    if (!PickupClass || !DeadActor || !World)
    {
        return; // PickupClass 留空 = 本单位不掉落
    }

    // 玩家击杀判定
    if (bRequirePlayerKill && (!KillerController || !KillerController->IsPlayerController()))
    {
        return;
    }

    // FMath::FRand() 的值域是 [0,1] 闭区间,DropChance=0 时不早退会有极小概率掉落
    if (DropChance <= 0.0f || FMath::FRand() > DropChance)
    {
        return;
    }

    // 落点:死亡处的水平位置 + 可选随机偏移,Z 钉到海平面。
    // 不能直接用死亡位置 —— 船沉没时 Z 是负的,掉落物会生成在水下捞不到
    FVector DropLocation = DeadActor->GetActorLocation();
    if (DropOffsetRadius > 0.0f)
    {
        const FVector2D Offset = FMath::RandPointInCircle(DropOffsetRadius);
        DropLocation.X += Offset.X;
        DropLocation.Y += Offset.Y;
    }
    DropLocation.Z = SeaLevelZ;

    // AlwaysSpawn:死亡点附近有残骸/碎块,不因碰撞拒绝生成
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AOCPickupBase* Pickup = World->SpawnActor<AOCPickupBase>(
        PickupClass, FTransform(DropLocation), SpawnParams);

    if (Pickup)
    {
        UE_LOG(LogTemp, Log, TEXT("[Loot] %s 掉落 %s(掉率 %.0f%%)@ (%.0f, %.0f)"),
            *GetNameSafe(DeadActor), *GetNameSafe(Pickup), DropChance * 100.0f, DropLocation.X, DropLocation.Y);
    }
}
