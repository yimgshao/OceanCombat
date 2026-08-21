// OceanCombat. Copyright(c) All rights reserved.

#include "Hazards/OCExplosiveBarrel.h"

#include "BuoyancyComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Components/OCHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

AOCExplosiveBarrel::AOCExplosiveBarrel()
{
    // 浮力/物理由组件驱动,Actor 本身不需要 Tick
    PrimaryActorTick.bCanEverTick = false;

    // ---- 桶体 Mesh(作为 RootComponent)----
    BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
    SetRootComponent(BarrelMesh);

    // 物理:浮力组件依赖物理模拟
    BarrelMesh->SetSimulatePhysics(true);
    BarrelMesh->SetEnableGravity(true);
    BarrelMesh->SetLinearDamping(1.0f);   // 水阻,漂浮时别乱滑
    BarrelMesh->SetAngularDamping(1.0f);

    // 碰撞:与船同用 PhysicsActor —— 能被炮弹(阻挡 PhysicsBody)直接命中,也能被撞
    BarrelMesh->SetCollisionProfileName(TEXT("PhysicsActor"));

    // ---- 浮力组件(pontoon 可在蓝图配;没配则 BeginPlay 补默认 pontoon)----
    BuoyancyComp = CreateDefaultSubobject<UBuoyancyComponent>(TEXT("BuoyancyComp"));

    // ---- 血量组件:桶的耐久,默认很低(基本一击即爆);蓝图可调高做"耐揍"桶 ----
    HealthComponent = CreateDefaultSubobject<UOCHealthComponent>(TEXT("HealthComponent"));
    HealthComponent->MaxHealth = 1.0f;

    // 默认用引擎基础 DamageType,蓝图可覆盖
    DamageTypeClass = UDamageType::StaticClass();
}

void AOCExplosiveBarrel::BeginPlay()
{
    // 关键:必须在 Super::BeginPlay() 之前补 pontoon —— AActor::BeginPlay 会派发各组件的
    // BeginPlay(含浮力组件),届时 pontoon 必须已就位,否则桶不会漂。
    if (BuoyancyComp && !BuoyancyComp->HasPontoons())
    {
        BuoyancyComp->AddCustomPontoon(DefaultPontoonRadius, FVector::ZeroVector);
    }

    Super::BeginPlay();

    if (HealthComponent)
    {
        HealthComponent->OnDeath.AddDynamic(this, &AOCExplosiveBarrel::HandleDeath);
    }
}

float AOCExplosiveBarrel::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    // 先走引擎默认结算(处理 RadialDamage 衰减等),再把实际伤害转给血量组件(与 AOCPawnBase 一致)
    const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    if (HealthComponent)
    {
        HealthComponent->ApplyDamage(ActualDamage, EventInstigator, DamageCauser);
    }

    return ActualDamage;
}

void AOCExplosiveBarrel::HandleDeath(AActor* /*DeadActor*/, AController* KillerController)
{
    if (bDetonating)
    {
        return;
    }
    bDetonating = true;
    PendingInstigator = KillerController;

    // 一旦注定引爆就锁住桶体:让同一发炮弹接下来施加的爆炸冲量对它无效。
    // ApplyExplosion 里伤害先于冲量结算(见 OCCombatStatics::ApplyExplosion),此处关物理后
    // 冲量步会因 IsSimulatingPhysics()==false 跳过本桶,于是桶在原地爆炸,而不会被推开一段再炸。
    // (死亡前物理一直开着,船照常能推动漂浮的桶;级联连爆也照常触发,只是被点燃的桶原地待爆。)
    if (BarrelMesh)
    {
        BarrelMesh->SetSimulatePhysics(false);
    }

    // 短随机延时后引爆:避免连锁在同一帧递归炸穿一片,并形成级联连爆观感
    const float Delay = FMath::Max(0.0f, FMath::FRandRange(DetonateDelayRange.X, DetonateDelayRange.Y));
    if (Delay <= 0.0f)
    {
        Detonate();
    }
    else
    {
        GetWorldTimerManager().SetTimer(DetonateTimerHandle, this, &AOCExplosiveBarrel::Detonate, Delay, /*bLoop=*/false);
    }
}

void AOCExplosiveBarrel::Detonate()
{
    const FVector Origin = GetActorLocation();

    // 施放爆炸:伤害 + 冲量。忽略自己(马上销毁);InstigatedBy 用打爆本桶的 Controller(击杀归属传递)
    const TArray<AActor*> IgnoreActors = { this };
    UOCCombatStatics::ApplyExplosion(
        this,
        Origin,
        Explosion,
        DamageTypeClass,
        IgnoreActors,
        /*DamageCauser=*/this,
        PendingInstigator.Get());

    // 大爆炸向浅水模拟注入水面冲击(炸出水坑→向外扩散涟漪);强度/半径远大于炮弹
    UOCCombatStatics::RegisterWaterExplosion(this, Origin, WaterExplosionStrength, WaterExplosionRadius);

    // 表现(留空则跳过)
    if (ExplosionEffect)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this, ExplosionEffect, Origin, FRotator::ZeroRotator,
            FVector(ExplosionEffectScale));
    }
    if (ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, Origin);
    }
    if (ExplosionShake)
    {
        // 世界空间屏幕抖动:内半径内满强度,内/外半径间线性衰减,外半径外不抖(对所有本地玩家生效)
        UGameplayStatics::PlayWorldCameraShake(this, ExplosionShake, Origin, ShakeInnerRadius, ShakeOuterRadius, /*Falloff=*/1.0f, /*bOrientShakeTowardsEpicenter=*/false);
    }

    UE_LOG(LogTemp, Log, TEXT("[炸药桶] 引爆 @ (%.0f, %.0f, %.0f)"), Origin.X, Origin.Y, Origin.Z);

    Destroy();
}
