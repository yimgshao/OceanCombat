// OceanCombat. Copyright(c) All rights reserved.

#include "Procedural/OCInfiniteOceanManager.h"

#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "WaterBodyComponent.h"
#include "WaterBodyCustomActor.h"

AOCInfiniteOceanManager::AOCInfiniteOceanManager()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AOCInfiniteOceanManager::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Log, TEXT("[OCInfiniteOcean] 无限海面已启动(Tag=%s,吸附=%.0fcm,自动跟随=%s)"),
        *OceanActorTag.ToString(), FollowSnapSize, bAutoFollow ? TEXT("开") : TEXT("关"));
}

void AOCInfiniteOceanManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    TimeSinceLastCheck += DeltaSeconds;
    if (TimeSinceLastCheck < CheckInterval)
    {
        return;
    }
    TimeSinceLastCheck = 0.0f;

    if (!bAutoFollow)
    {
        return;
    }

    const UWorld* World = GetWorld();
    const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    AWaterBodyCustom* Body = Pawn ? FindOceanBody() : nullptr;
    if (!Body)
    {
        return;
    }

    // 任一轴偏离超过一个完整步长才平移(四舍五入吸附使触发距离其实是半步长,平移后玩家会落在
    // 对面边界上,小幅来回就反复触发 → 闪烁)。移动量按步长四舍五入,平移后玩家落在中心半步长内,
    // 到下一个触发边界至少还有半步长缓冲
    const FVector2D Offset = FVector2D(Pawn->GetActorLocation()) - FVector2D(Body->GetActorLocation());
    if (FMath::Abs(Offset.X) >= FollowSnapSize || FMath::Abs(Offset.Y) >= FollowSnapSize)
    {
        MoveOceanByDelta(FVector2D(
            FMath::GridSnap(Offset.X, FollowSnapSize),
            FMath::GridSnap(Offset.Y, FollowSnapSize)), /*bSnap=*/false);
    }
}

AWaterBodyCustom* AOCInfiniteOceanManager::FindOceanBody()
{
    if (AWaterBodyCustom* Body = CachedBody.Get())
    {
        return Body;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    AWaterBodyCustom* Fallback = nullptr;
    for (TActorIterator<AWaterBodyCustom> It(World); It; ++It)
    {
        if (It->ActorHasTag(OceanActorTag))
        {
            CachedBody = *It;
            return *It;
        }
        if (!Fallback)
        {
            Fallback = *It;
        }
    }

    if (Fallback)
    {
        UE_LOG(LogTemp, Warning, TEXT("[OCInfiniteOcean] 找不到 Tag=%s 的 AWaterBodyCustom,回退用关卡里第一个: %s"),
            *OceanActorTag.ToString(), *Fallback->GetName());
        CachedBody = Fallback;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[OCInfiniteOcean] 关卡里没有 AWaterBodyCustom"));
    }

    if (AWaterBodyCustom* Body = CachedBody.Get())
    {
        EnsureBodyMovable(Body);
    }
    return CachedBody.Get();
}

void AOCInfiniteOceanManager::EnsureBodyMovable(AWaterBodyCustom* Body) const
{
    if (!Body)
    {
        return;
    }
    if (UWaterBodyComponent* WaterBody = Body->GetWaterBodyComponent())
    {
        if (WaterBody->Mobility != EComponentMobility::Movable)
        {
            WaterBody->SetMobility(EComponentMobility::Movable);
            UE_LOG(LogTemp, Log, TEXT("[OCInfiniteOcean] 已将 %s 的水体组件 mobility 改为 Movable(运行时移动的前提)"), *Body->GetName());
        }
        // 网格组件(渲染+碰撞)附着在 WaterBodyComponent 下,Static 不能挂在 Movable 父组件下,一并改
        for (UPrimitiveComponent* Comp : WaterBody->GetCollisionComponents())
        {
            if (Comp && Comp->Mobility != EComponentMobility::Movable)
            {
                Comp->SetMobility(EComponentMobility::Movable);
            }
        }
    }
}

void AOCInfiniteOceanManager::MoveOceanByDelta(FVector2D DeltaXY, bool bSnap)
{
    AWaterBodyCustom* Body = FindOceanBody();
    if (!Body)
    {
        UE_LOG(LogTemp, Warning, TEXT("[OCInfiniteOcean] 平移失败:找不到 AWaterBodyCustom"));
        return;
    }

    FVector2D Delta = DeltaXY;
    if (bSnap)
    {
        Delta = FVector2D(
            FMath::GridSnap(DeltaXY.X, FollowSnapSize),
            FMath::GridSnap(DeltaXY.Y, FollowSnapSize));
    }
    if (Delta.IsNearlyZero())
    {
        return;
    }

    // 网格/碰撞/材质都挂在这个 Actor 上,整体 teleport 即可,无任何重建
    Body->SetActorLocation(Body->GetActorLocation() + FVector(Delta, 0.0),
        /*bSweep=*/false, /*OutHit=*/nullptr, ETeleportType::TeleportPhysics);
}
