// OceanCombat. Copyright(c) All rights reserved.

#include "Weapons/OCCombatStatics.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/DamageType.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "ShallowWaterSubsystem.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"

// 控制台开关:OC.DrawExplosionRadius 1 —— 每次爆炸画出内圈满伤区(绿)与外圈判定范围(红)
static TAutoConsoleVariable<int32> CVarDrawExplosionRadius(
    TEXT("OC.DrawExplosionRadius"),
    0,
    TEXT("画出爆炸判定范围调试球:0=关, 1=开(绿=满伤内圈, 红=判定外圈)"),
    ECVF_Cheat);
#endif

void UOCCombatStatics::ApplyExplosion(
    const UObject* WorldContextObject,
    const FVector& Origin,
    const FOCExplosionParams& Params,
    TSubclassOf<UDamageType> DamageTypeClass,
    const TArray<AActor*>& IgnoreActors,
    AActor* DamageCauser,
    AController* InstigatedBy)
{
    UWorld* const World = GEngine
        ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
        : nullptr;
    if (!World)
    {
        return;
    }

    const TSubclassOf<UDamageType> EffectiveDamageType =
        DamageTypeClass ? DamageTypeClass : TSubclassOf<UDamageType>(UDamageType::StaticClass());

    // ---- 1) 球形 AOE 伤害(与原爆炸炮弹一致的调用)----
    UGameplayStatics::ApplyRadialDamageWithFalloff(
        WorldContextObject,
        Params.BaseDamage,
        Params.MinimumDamage,
        Origin,
        Params.DamageInnerRadius,
        Params.DamageOuterRadius,
        Params.DamageFalloff,
        EffectiveDamageType,
        IgnoreActors,
        DamageCauser,
        InstigatedBy);

#if ENABLE_DRAW_DEBUG
    // 调试可视化:画出判定球(OC.DrawExplosionRadius 1 开启),停留几秒便于观察
    if (CVarDrawExplosionRadius.GetValueOnGameThread() > 0)
    {
        constexpr float DebugLifeTime = 4.0f;
        DrawDebugSphere(World, Origin, Params.DamageInnerRadius, 16, FColor::Green, false, DebugLifeTime, 0, 3.0f);
        DrawDebugSphere(World, Origin, Params.DamageOuterRadius, 24, FColor::Red, false, DebugLifeTime, 0, 3.0f);
    }
#endif

    // ---- 2) 径向物理冲量:把范围内在模拟物理的组件掀开 ----
    if (!Params.bApplyImpulse || Params.ImpulseStrength <= 0.0f)
    {
        return;
    }

    const float ImpulseRadius = Params.GetEffectiveImpulseRadius();
    if (ImpulseRadius <= 0.0f)
    {
        return;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OCExplosionImpulse), /*bTraceComplex=*/false);
    QueryParams.AddIgnoredActors(IgnoreActors);

    // 只关心会被物理推动的物体:船(PhysicsBody)与可动物件(WorldDynamic,如炸药桶)
    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

    TArray<FOverlapResult> Overlaps;
    World->OverlapMultiByObjectType(
        Overlaps,
        Origin,
        FQuat::Identity,
        ObjectParams,
        FCollisionShape::MakeSphere(ImpulseRadius),
        QueryParams);

    // 同一组件可能因多形状被多次命中,去重后每个只施一次冲量
    TSet<UPrimitiveComponent*> Processed;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        UPrimitiveComponent* const Comp = Overlap.GetComponent();
        if (Comp && Comp->IsSimulatingPhysics() && !Processed.Contains(Comp))
        {
            Processed.Add(Comp);
            Comp->AddRadialImpulse(Origin, ImpulseRadius, Params.ImpulseStrength, ERadialImpulseFalloff::RIF_Linear, /*bVelChange=*/false);
        }
    }
}

void UOCCombatStatics::RegisterWaterExplosion(
    const UObject* WorldContextObject,
    const FVector& Origin,
    float ImpulseStrength,
    float ImpulseRadius)
{
    if (ImpulseStrength <= 0.0f || ImpulseRadius <= 0.0f)
    {
        return;
    }

    UWorld* const World = GEngine
        ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
        : nullptr;
    if (!World)
    {
        return;
    }

    // 实际实例是 UBasicShallowWaterSubsystem,GetSubsystem<基类> 查不到,按基类取派生实例;
    // 插件禁用 / Dedicated Server 上子系统不存在,静默跳过
    const TArray<UShallowWaterSubsystem*> Subsystems = World->GetSubsystemArrayCopy<UShallowWaterSubsystem>();
    if (UShallowWaterSubsystem* const Subsystem = Subsystems.Num() > 0 ? Subsystems[0] : nullptr)
    {
        // 向下冲量 → 炸出水坑,浅水方程回弹后自然向外扩成环。水面 Z 由 RegisterImpact 内部对齐。
        Subsystem->RegisterImpact(Origin, FVector(0.0, 0.0, -static_cast<double>(ImpulseStrength)), ImpulseRadius);
    }
}
