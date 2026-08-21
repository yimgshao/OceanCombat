// OceanCombat. Copyright(c) All rights reserved.

#include "Weapons/Projectiles/OCProjectileBase.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "ShallowWaterSubsystem.h"
#include "WaterBodyComponent.h"
#include "WaterBodyManager.h"

AOCProjectileBase::AOCProjectileBase()
{
    // 每帧查询海面高度,需要 Tick
    PrimaryActorTick.bCanEverTick = true;

    SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
    SphereCollision->InitSphereRadius(20.0f);
    SetRootComponent(SphereCollision);

    // 命中硬物检测:QueryOnly 即可(ProjectileMovement 靠扫掠查询检测阻挡,不需物理模拟)。
    // 身份设为 WorldDynamic,阻挡场景/船/角色;忽略其余通道。
    // 注:炮弹与炮弹同为 WorldDynamic → 互相阻挡、双双爆炸(设计如此)。
    SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SphereCollision->SetCollisionObjectType(ECC_WorldDynamic);
    SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    SphereCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);   // 礁石/建筑/地形
    SphereCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);  // 可动物件/其他炮弹
    SphereCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);          // 角色类敌方单位
    SphereCollision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);   // 船(PhysicsActor)

    BulletMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BulletMesh"));
    BulletMesh->SetupAttachment(SphereCollision);
    BulletMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = SphereCollision;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;  // 首次阻挡即停 → 触发 OnProjectileStop
    ProjectileMovement->InitialSpeed = 0.0f;    // 初速由 Launch() 注入
    ProjectileMovement->MaxSpeed = 0.0f;        // 0 = 不限速

    // 关键:bInitialVelocityInLocalSpace 引擎默认 true,InitializeComponent 时会把
    // Launch() 设好的世界空间速度误当局部空间速度、再按组件旋转多转一次(弹道歪)。
    // 我们的初速度是世界空间的,必须关掉。
    ProjectileMovement->bInitialVelocityInLocalSpace = false;

    // 寿命兜底:万一没命中任何东西(理论上总会落海),防止泄漏
    SetLifeSpan(10.0f);
}

void AOCProjectileBase::BeginPlay()
{
    Super::BeginPlay();

    // 蓝图里调的重力缩放在构造函数执行后才覆盖到成员,所以在 BeginPlay 应用
    ProjectileMovement->ProjectileGravityScale = GravityScale;

    // 硬物命中回调
    ProjectileMovement->OnProjectileStop.AddDynamic(this, &AOCProjectileBase::HandleProjectileStop);

    // 忽略发射者(炮口紧贴船,避免出膛瞬间撞自己船自爆)
    if (AActor* const MyOwner = GetOwner())
    {
        SphereCollision->IgnoreActorWhenMoving(MyOwner, true);
    }

    // 收集水体并让扫掠忽略水体(海面命中交给 Tick 查询,避免撞到平面水体碰撞盒的错误高度)
    CacheWaterBodies();
}

void AOCProjectileBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bHasImpacted)
    {
        return;
    }

    FHitResult WaterHit;
    if (CheckWaterImpact(WaterHit))
    {
        OnImpact(WaterHit, /*bHitWater=*/true);
    }
}

void AOCProjectileBase::Launch(const FVector& Velocity)
{
    ProjectileMovement->Velocity = Velocity;
}

float AOCProjectileBase::GetCollisionRadius() const
{
    return SphereCollision ? SphereCollision->GetUnscaledSphereRadius() : 20.0f;
}

void AOCProjectileBase::ApplyDamageConfig(const FOCCombatConfigRow& Config)
{
    // 基类无伤害参数,子类(爆炸炮弹)override
}

void AOCProjectileBase::HandleProjectileStop(const FHitResult& ImpactResult)
{
    if (bHasImpacted)
    {
        return;
    }
    OnImpact(ImpactResult, /*bHitWater=*/false);
}

void AOCProjectileBase::OnImpact(const FHitResult& Hit, bool bHitWater)
{
    if (bHasImpacted)
    {
        return;
    }
    bHasImpacted = true;

    // 落水波纹:向浅水模拟注册冲击
    if (bHitWater && bWaterRippleEnabled)
    {
        RegisterWaterRipple(Hit.ImpactPoint);
    }

    const FVector ImpactLocation = Hit.ImpactPoint;
    const FRotator ImpactRotation = Hit.ImpactNormal.IsNearlyZero()
        ? GetActorRotation()
        : Hit.ImpactNormal.Rotation();

    // ---- 命中日志(只进 Output Log,不上屏) ----
    const FString HitTargetName = bHitWater
        ? TEXT("海面")
        : (Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT("未知物体"));
    UE_LOG(LogTemp, Log, TEXT("[炮弹命中] %s @ (%.0f, %.0f, %.0f)"),
        *HitTargetName, ImpactLocation.X, ImpactLocation.Y, ImpactLocation.Z);

    // 特效(留空则跳过)
    UNiagaraSystem* const Effect = bHitWater ? WaterImpactEffect : ImpactEffect;
    if (Effect)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, ImpactLocation, ImpactRotation);
    }

    // 音效(留空则跳过)
    USoundBase* const Sound = bHitWater ? WaterImpactSound : ImpactSound;
    if (Sound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, Sound, ImpactLocation);
    }

    Destroy();
}

void AOCProjectileBase::CacheWaterBodies()
{
    UWorld* const World = GetWorld();
    if (!World)
    {
        return;
    }

    FWaterBodyManager::ForEachWaterBodyComponent(World, [this](UWaterBodyComponent* WaterBody)
    {
        if (WaterBody)
        {
            WaterBodies.Add(WaterBody);
            if (AActor* const WaterActor = WaterBody->GetOwner())
            {
                SphereCollision->IgnoreActorWhenMoving(WaterActor, true);
            }
        }
        return true;  // 继续遍历
    });
}

bool AOCProjectileBase::CheckWaterImpact(FHitResult& OutHit) const
{
    const FVector Location = GetActorLocation();

    for (const TObjectPtr<UWaterBodyComponent>& WaterBody : WaterBodies)
    {
        if (!WaterBody)
        {
            continue;
        }

        FVector SurfaceLocation;
        FVector SurfaceNormal;
        FVector WaterVelocity;
        float WaterDepth = 0.0f;
        if (!WaterBody->GetWaterSurfaceInfoAtLocation(Location, SurfaceLocation, SurfaceNormal, WaterVelocity, WaterDepth, /*bIncludeDepth=*/false))
        {
            continue;
        }

        // 炮弹已到/没入水面
        if (Location.Z <= SurfaceLocation.Z)
        {
            OutHit = FHitResult();
            OutHit.ImpactPoint = FVector(Location.X, Location.Y, SurfaceLocation.Z);
            OutHit.Location = OutHit.ImpactPoint;
            OutHit.ImpactNormal = SurfaceNormal;
            OutHit.Normal = SurfaceNormal;
            return true;
        }
    }

    return false;
}

void AOCProjectileBase::RegisterWaterRipple(const FVector& ImpactPoint) const
{
    UWorld* const World = GetWorld();
    if (!World)
    {
        return;
    }

    // 实际实例类型是 UBasicShallowWaterSubsystem,按基类取派生实例;
    // 插件未启用/Dedicated Server 上子系统不存在,静默跳过
    const TArray<UShallowWaterSubsystem*> Subsystems = World->GetSubsystemArrayCopy<UShallowWaterSubsystem>();
    if (UShallowWaterSubsystem* const Subsystem = Subsystems.Num() > 0 ? Subsystems[0] : nullptr)
    {
        Subsystem->RegisterImpact(ImpactPoint, GetVelocity() * WaterRippleVelocityScale, WaterRippleRadius);
    }
}
