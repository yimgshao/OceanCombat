// OceanCombat. Copyright(c) All rights reserved.

#include "Weapons/OCWeaponTurret.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "WaterBodyComponent.h"
#include "WaterBodyManager.h"
#include "Weapons/OCAimStatics.h"
#include "Weapons/Projectiles/OCProjectileBase.h"

AOCWeaponTurret::AOCWeaponTurret()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    TurretBaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretBaseMesh"));
    TurretBaseMesh->SetupAttachment(SceneRoot);

    BarrelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BarrelPivot"));
    BarrelPivot->SetupAttachment(TurretBaseMesh);

    TurretBarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretBarrelMesh"));
    TurretBarrelMesh->SetupAttachment(BarrelPivot);

    MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
    MuzzlePoint->SetupAttachment(TurretBarrelMesh);

    // 关键:炮塔不模拟物理、不产生碰撞。
    // 船体是模拟物理的根组件,炮塔带碰撞会干扰船的浮力和航行。
    TurretBaseMesh->SetSimulatePhysics(false);
    TurretBaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TurretBarrelMesh->SetSimulatePhysics(false);
    TurretBarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AOCWeaponTurret::SetBarrelPitch(float Pitch)
{
    const float ClampedPitch = FMath::Clamp(Pitch, PitchLimitMin, PitchLimitMax);
    BarrelPivot->SetRelativeRotation(FRotator(ClampedPitch, 0.0f, 0.0f));
}

void AOCWeaponTurret::AddBarrelPitch(float DeltaPitch)
{
    SetBarrelPitch(GetBarrelPitch() + DeltaPitch);
}

float AOCWeaponTurret::GetBarrelPitch() const
{
    return BarrelPivot->GetRelativeRotation().Pitch;
}

void AOCWeaponTurret::SetTurretYaw(float Yaw)
{
    TurretBaseMesh->SetRelativeRotation(FRotator(0.0f, Yaw, 0.0f));
}

FVector AOCWeaponTurret::GetMuzzleLocation() const
{
    return MuzzlePoint->GetComponentLocation();
}

void AOCWeaponTurret::PredictTrajectory(TArray<FVector>& OutPoints, FVector& OutEnd, bool& bOutHitWater) const
{
    OutPoints.Reset();
    bOutHitWater = false;

    UWorld* World = GetWorld();
    if (!World || !MuzzlePoint)
    {
        return;
    }

    // 起点/初速与 Fire() 完全同源:炮口世界变换 + MuzzleVelocity
    const FVector Start = MuzzlePoint->GetComponentLocation();
    const FVector LaunchVel = MuzzlePoint->GetForwardVector() * MuzzleVelocity;
    OutPoints.Add(Start);
    OutEnd = Start;

    // 重力缩放与碰撞半径取自炮弹 CDO(与真实炮弹一致);没配炮弹则直线、半径默认
    float GravityScale = 0.0f;
    float Radius = 20.0f;
    if (ProjectileClass)
    {
        const AOCProjectileBase* CDO = ProjectileClass->GetDefaultObject<AOCProjectileBase>();
        GravityScale = CDO->GetGravityScale();
        Radius = CDO->GetCollisionRadius();
    }
    const float GravityZ = World->GetGravityZ() * GravityScale;

    // 收集水体(海面终止,与炮弹 Tick 查询一致)
    TArray<UWaterBodyComponent*> Waters;
    FWaterBodyManager::ForEachWaterBodyComponent(World, [&Waters](UWaterBodyComponent* WB)
    {
        if (WB)
        {
            Waters.Add(WB);
        }
        return true;
    });

    // 硬物扫掠:对象类型 = 炮弹阻挡的四类;忽略自己的船与炮塔(与炮弹忽略 Owner 一致)
    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjParams.AddObjectTypesToQuery(ECC_Pawn);
    ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);

    FCollisionQueryParams QParams(SCENE_QUERY_STAT(OCAimTrajectory), /*bTraceComplex=*/false);
    QParams.AddIgnoredActor(this);
    if (AActor* MountOwner = GetOwner() ? GetOwner() : GetAttachParentActor())
    {
        QParams.AddIgnoredActor(MountOwner);
    }

    const FCollisionShape Probe = FCollisionShape::MakeSphere(Radius);
    const float Dt = 1.0f / 30.0f;   // 采样步长
    const int32 MaxSteps = 200;      // 步数封顶(≈6.6s 飞行),防极端不落地

    FVector Prev = Start;
    for (int32 i = 1; i <= MaxSteps; ++i)
    {
        const float T = i * Dt;
        const FVector Cur = UOCAimStatics::CalcParabolaPoint(Start, LaunchVel, GravityZ, T);

        // ① 硬物命中(礁石/建筑/敌船)
        FHitResult Hit;
        if (World->SweepSingleByObjectType(Hit, Prev, Cur, FQuat::Identity, ObjParams, Probe, QParams))
        {
            OutPoints.Add(Hit.ImpactPoint);
            OutEnd = Hit.ImpactPoint;
            return;
        }

        // ② 落水:低于任一水体水面即止(与炮弹一致)
        for (UWaterBodyComponent* WB : Waters)
        {
            FVector SurfLoc, SurfNormal, WaterVel;
            float Depth = 0.0f;
            if (WB->GetWaterSurfaceInfoAtLocation(Cur, SurfLoc, SurfNormal, WaterVel, Depth, /*bIncludeDepth=*/false)
                && Cur.Z <= SurfLoc.Z)
            {
                const FVector WaterPoint(Cur.X, Cur.Y, SurfLoc.Z);
                OutPoints.Add(WaterPoint);
                OutEnd = WaterPoint;
                bOutHitWater = true;
                return;
            }
        }

        OutPoints.Add(Cur);
        OutEnd = Cur;
        Prev = Cur;
    }
}

void AOCWeaponTurret::AimAt(const FVector& TargetLocation)
{
    const FRotator AimRotation = UOCAimStatics::CalcAimRotation(GetActorLocation(), TargetLocation);

    // 底座是局部旋转:世界方位角减去自身(随挂载单位)的世界朝向
    const float LocalYaw = FRotator::NormalizeAxis(AimRotation.Yaw - GetActorRotation().Yaw);
    SetTurretYaw(LocalYaw);

    // SetBarrelPitch 内部自动 Clamp 到俯仰限制
    SetBarrelPitch(AimRotation.Pitch);
}

bool AOCWeaponTurret::AimAtBallistic(const FVector& TargetLocation)
{
    const FVector MuzzleLocation = GetMuzzleLocation();

    // 实际重力 = 世界重力 × 炮弹的 GravityScale;没配炮弹或零重力炮弹 → 直线即弹道,必然有解
    const float WorldGravity = GetWorld() ? -GetWorld()->GetGravityZ() : 980.0f;
    const float Gravity = ProjectileClass
        ? WorldGravity * ProjectileClass->GetDefaultObject<AOCProjectileBase>()->GetGravityScale()
        : 0.0f;

    // 默认按直线瞄准;弹道有解时会被覆盖为弹道解
    FRotator AimRotation = UOCAimStatics::CalcAimRotation(MuzzleLocation, TargetLocation);
    bool bSolvable = true;
    if (Gravity > KINDA_SMALL_NUMBER)
    {
        bSolvable = UOCAimStatics::CalcBallisticAimRotation(
            MuzzleLocation, TargetLocation, MuzzleVelocity, Gravity, AimRotation);
        // 打不到时 AimRotation 保持直线瞄准,把炮口摆向目标(但调用方不应开火)
    }

    // 底座是局部旋转:世界方位角减去自身(随挂载单位)的世界朝向
    const float LocalYaw = FRotator::NormalizeAxis(AimRotation.Yaw - GetActorRotation().Yaw);
    SetTurretYaw(LocalYaw);

    // SetBarrelPitch 内部自动 Clamp 到俯仰限制
    SetBarrelPitch(AimRotation.Pitch);

    return bSolvable;
}

float AOCWeaponTurret::GetFireCooldownPercent() const
{
    const UWorld* World = GetWorld();
    if (!World || FireRate <= 0.0f)
    {
        return 0.0f;
    }

    const float Interval = 1.0f / FireRate;
    const float Remaining = Interval - (World->GetTimeSeconds() - LastFireTime);
    return FMath::Clamp(Remaining / Interval, 0.0f, 1.0f);
}

void AOCWeaponTurret::ApplyConfig(const FOCCombatConfigRow& Config)
{
    FireRate = Config.FireRate;
    MuzzleVelocity = Config.MuzzleVelocity;

    CombatConfig = Config;
    bHasCombatConfig = true;
}

bool AOCWeaponTurret::Fire()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    // CD 检查:距上次开火不足 1/FireRate 秒则拒发
    const float Now = World->GetTimeSeconds();
    const float FireInterval = (FireRate > 0.0f) ? (1.0f / FireRate) : 0.0f;
    if (Now - LastFireTime < FireInterval)
    {
        return false;
    }

    if (!ProjectileClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[OCWeaponTurret] %s 未配置 ProjectileClass,无法开火"), *GetName());
        return false;
    }

    // 挂载者:优先 Owner;Child Actor 的 Owner 可能为空,退而用 Attach 父级(就是船)
    AActor* const MountOwner = GetOwner() ? GetOwner() : GetAttachParentActor();

    // Deferred Spawn:先 Launch 注入初速度,再 FinishSpawning 触发 BeginPlay,
    // 否则炮弹会以零速度先飞一帧
    const FTransform MuzzleTransform(MuzzlePoint->GetComponentRotation(), MuzzlePoint->GetComponentLocation());
    AOCProjectileBase* Projectile = World->SpawnActorDeferred<AOCProjectileBase>(
        ProjectileClass, MuzzleTransform, MountOwner, Cast<APawn>(MountOwner),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Projectile)
    {
        UE_LOG(LogTemp, Warning, TEXT("[OCWeaponTurret] %s 生成炮弹失败"), *GetName());
        return false;
    }

    // 有战斗配置则注入伤害参数(必须在 FinishSpawning 前)
    if (bHasCombatConfig)
    {
        Projectile->ApplyDamageConfig(CombatConfig);
    }

    const FVector Velocity = MuzzlePoint->GetForwardVector() * MuzzleVelocity;
    Projectile->Launch(Velocity);
    UGameplayStatics::FinishSpawningActor(Projectile, MuzzleTransform);

    LastFireTime = Now;
    return true;
}
