// OceanCombat. Copyright(c) All rights reserved.

#include "Components/OCDestructionComponent.h"

#include "Components/OCHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Field/FieldSystemObjects.h"
#include "Field/FieldSystemTypes.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Pawns/OCPawnBase.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Weapons/OCWeaponTurret.h"

UOCDestructionComponent::UOCDestructionComponent()
{
    // 只在死亡时被事件驱动,无需每帧 Tick
    PrimaryComponentTick.bCanEverTick = false;
}

void UOCDestructionComponent::BeginPlay()
{
    Super::BeginPlay();

    // 监听本体血量组件的 OnDeath(血条/停火由各自监听者处理)
    AActor* Owner = GetOwner();
    UOCHealthComponent* Health = Owner ? Owner->FindComponentByClass<UOCHealthComponent>() : nullptr;
    if (Health)
    {
        Health->OnDeath.AddDynamic(this, &UOCDestructionComponent::HandleDeath);
    }
}

void UOCDestructionComponent::HandleDeath(AActor* DeadActor, AController* KillerController)
{
    AActor* Owner = GetOwner();
    UWorld* World = Owner ? Owner->GetWorld() : nullptr;
    if (!Owner || !World)
    {
        return;
    }

    // 根组件(船=BoatMesh / 建筑=BuildingMesh),提供残骸的世界变换与死亡瞬间速度
    UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Owner->GetRootComponent());

    // ---- 1. 隐藏本体所有静态 Mesh + 关碰撞,隐藏炮塔 ----
    TArray<UStaticMeshComponent*> Meshes;
    Owner->GetComponents<UStaticMeshComponent>(Meshes);
    for (UStaticMeshComponent* Mesh : Meshes)
    {
        Mesh->SetVisibility(false);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if (AOCPawnBase* Pawn = Cast<AOCPawnBase>(Owner))
    {
        TArray<AOCWeaponTurret*> Turrets;
        Pawn->GetTurrets(Turrets);
        for (AOCWeaponTurret* Turret : Turrets)
        {
            if (Turret)
            {
                Turret->SetActorHiddenInGame(true);
            }
        }
    }

    // ---- 2. 原位生成碎裂残骸并炸开(用根组件世界变换,与视觉本体严格对齐)----
    if (WreckCollection && RootPrim)
    {
        const FTransform WreckTransform = RootPrim->GetComponentTransform();

        AGeometryCollectionActor* Wreck = World->SpawnActorDeferred<AGeometryCollectionActor>(
            AGeometryCollectionActor::StaticClass(), WreckTransform);
        if (Wreck)
        {
            UGeometryCollectionComponent* WreckComp = Wreck->GetGeometryCollectionComponent();
            WreckComp->SetRestCollection(WreckCollection);
            // 关键:关掉"碰撞产生应变"。默认开启时,残骸生成瞬间与地形/地基重叠,Chaos 解穿透的碰撞冲量
            // 会产生远超阈值的应变 → 整块当场震碎(与我们施加的场无关)。关掉后只由受击点应变场控制断裂。
            WreckComp->SetEnableDamageFromCollision(false);
            UGameplayStatics::FinishSpawningActor(Wreck, WreckTransform);

            // 碎块继承死亡瞬间速度:先给整块赋速度,碎块带着本体速度散开
            if (bInheritVelocity && RootPrim)
            {
                WreckComp->SetPhysicsLinearVelocity(RootPrim->GetPhysicsLinearVelocity());
                WreckComp->SetPhysicsAngularVelocityInDegrees(RootPrim->GetPhysicsAngularVelocityInDegrees());
            }

            // 决定"爆炸"的中心与大小:有爆炸信息(炮弹/炸药桶命中)就用爆心+爆炸半径;
            // 否则(非爆炸/点伤致死、或关闭局部碎裂)以残骸自身包围球为一次"覆盖全身的爆炸"回退。
            UOCHealthComponent* Health = Owner->FindComponentByClass<UOCHealthComponent>();
            FVector BlastCenter;
            float BlastRadius;
            if (bUseImpactFracture && Health && Health->HasLastHitInfo() && Health->GetLastBlastRadius() > 0.0f)
            {
                BlastCenter = Health->GetLastHitLocation();
                BlastRadius = Health->GetLastBlastRadius();
            }
            else
            {
                BlastCenter = RootPrim->Bounds.Origin;
                BlastRadius = RootPrim->Bounds.SphereRadius;
            }

            // 半径/力度 = 爆炸值 × 组件系数(回退时"爆炸值"=残骸自身大小)。取局部值供延迟 lambda 捕获。
            const float FractureRad = BlastRadius * FractureRadiusScale;   // 断裂范围
            const float ScatterRad = BlastRadius * ScatterRadiusScale;     // 爆开衰减半径
            const float ScatterPeak = BlastRadius * ScatterImpulseScale;   // 爆心处峰值冲量(∝爆炸大小)
            TWeakObjectPtr<UGeometryCollectionComponent> WeakWreck = WreckComp;

            // 局部断裂:立即施加,只让爆心 FractureRad 内的子块断开(回退时半径覆盖全身=整体碎开)。
            WreckComp->ApplyExternalStrain(
                WreckComp->GetRootIndex(), BlastCenter, FractureRad,
                FracturePropagationDepth, FracturePropagationFactor, FractureStrainMagnitude);

            // 爆炸冲击在断裂后隔一帧施加(等碎块断开成自由刚体)。
            World->GetTimerManager().SetTimerForNextTick(
                [WeakWreck, BlastCenter, ScatterPeak, ScatterRad]()
            {
                UGeometryCollectionComponent* GC2 = WeakWreck.Get();
                if (!GC2 || ScatterPeak <= 0.0f || ScatterRad <= 0.0f)
                {
                    return;
                }

                // 径向(从爆心向外)× 线性距离衰减(爆心=峰值、半径边缘→0、半径外=0)。
                // 同一爆炸内碎片离爆心越近冲量越大(真实爆炸物理)。
                URadialVector* Dir = NewObject<URadialVector>(GC2);
                Dir->SetRadialVector(ScatterPeak, BlastCenter);
                URadialFalloff* Bound = NewObject<URadialFalloff>(GC2);
                // MinRange=0,MaxRange=1 → 爆心处系数 1、线性衰减到半径边缘 0,半径外=Default=0
                Bound->SetRadialFalloff(1.0f, 0.0f, 1.0f, 0.0f, ScatterRad, BlastCenter,
                    EFieldFalloffType::Field_Falloff_Linear);
                UOperatorField* Bounded = NewObject<UOperatorField>(GC2);
                Bounded->SetOperatorField(1.0f, Bound, Dir, EFieldOperationType::Field_Multiply);
                GC2->ApplyPhysicsField(
                    true, EGeometryCollectionPhysicsTypeEnum::Chaos_LinearImpulse, nullptr, Bounded);
            });

            // 残骸定时清理
            Wreck->SetLifeSpan(WreckLifeSpan);
        }
    }

    // ---- 3. 爆炸表现(可选,留空跳过)----
    const FVector EffectLocation = RootPrim ? RootPrim->GetComponentLocation() : Owner->GetActorLocation();
    if (ExplosionEffect)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ExplosionEffect, EffectLocation);
    }
    if (ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(World, ExplosionSound, EffectLocation);
    }

    // ---- 4. 本体延时销毁(残骸是独立 Actor,不受影响)----
    Owner->SetLifeSpan(OwnerLifeSpan);
}
