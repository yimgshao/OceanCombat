// OceanCombat. Copyright(c) All rights reserved.

#include "Pickups/OCPickupBase.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

AOCPickupBase::AOCPickupBase()
{
    // 自转 + 浮动需要每帧更新
    PrimaryActorTick.bCanEverTick = true;

    TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
    SetRootComponent(TriggerSphere);

    // 纯查询触发器:身份 WorldDynamic,先全通道 Ignore,再只对船体
    // (PhysicsActor profile → ECC_PhysicsBody)开 Overlap。
    // 全通道 Ignore 是必须的:炮弹的 sphere 对 WorldDynamic 是 Block(见
    // AOCProjectileBase),不 Ignore 的话炮弹会撞在掉落物上直接爆炸。
    // 船体侧对 WorldDynamic 是 Block,但引擎取两侧中较弱的响应 → Overlap,触发正常。
    TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
    TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerSphere->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
    TriggerSphere->SetGenerateOverlapEvents(true);

    PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
    PickupMesh->SetupAttachment(TriggerSphere);
    PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AOCPickupBase::BeginPlay()
{
    Super::BeginPlay();

    // 半径是蓝图可调参数,构造函数之后才覆盖到成员,所以在这里应用
    TriggerSphere->SetSphereRadius(PickupRadius);
    TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AOCPickupBase::HandleBeginOverlap);

    MeshBaseRelativeZ = PickupMesh->GetRelativeLocation().Z;

    // 环绕特效:挂到触发球(根,不随 Mesh 浮动),血包存活期间一直播放,随本体销毁而消失
    if (AmbientEffect)
    {
        AmbientEffectComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
            AmbientEffect, TriggerSphere, NAME_None,
            FVector::ZeroVector, FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget, /*bAutoDestroy=*/true);
        if (AmbientEffectComp)
        {
            AmbientEffectComp->SetRelativeScale3D(FVector(AmbientEffectScale));
        }
    }

    if (PickupLifeSpan > 0.0f)
    {
        SetLifeSpan(PickupLifeSpan);
    }

    // 注:生成瞬间玩家已经压在掉落物上的情况(船正好停在死亡点上方)不需要特殊处理 ——
    // AActor::DispatchBeginPlay 在 BeginPlay 返回后会调 UpdateInitialOverlaps,
    // 运行时生成的 Actor 走 bDoNotifies=true,已存在的重叠照样会触发 BeginOverlap。
    // 委托在上面就已绑定,收得到
}

void AOCPickupBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 已被拾取(Destroy 可能延迟到帧末生效),不必再动表现
    if (bConsumed)
    {
        return;
    }

    ElapsedTime += DeltaTime;

    // 自转与浮动都作用在 Mesh 上,触发球本体保持不动 —— overlap 判定不受表现影响
    if (!FMath::IsNearlyZero(SpinRateDeg))
    {
        PickupMesh->AddLocalRotation(FRotator(0.0f, SpinRateDeg * DeltaTime, 0.0f));
    }

    if (BobAmplitude > 0.0f)
    {
        // ClampMin 只约束编辑器输入,挡不住 C++/蓝图赋值,除零会把 NaN 传进 Mesh 变换
        const float SafePeriod = FMath::Max(BobPeriod, 0.01f);
        const float BobOffset = BobAmplitude * FMath::Sin(2.0f * PI * ElapsedTime / SafePeriod);
        FVector MeshLocation = PickupMesh->GetRelativeLocation();
        MeshLocation.Z = MeshBaseRelativeZ + BobOffset;
        PickupMesh->SetRelativeLocation(MeshLocation);
    }
}

void AOCPickupBase::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (bConsumed || !OtherActor)
    {
        return;
    }

    // 只有玩家控制的 Pawn 能拾取。用 Controller 判定而不是 Cast<AOCPlayerBoat>:
    // 复活后的新船、将来的多人/换船都不用改这里
    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || !Pawn->GetController() || !Pawn->GetController()->IsPlayerController())
    {
        return;
    }

    // 具体效果交给子类;子类说没消耗就留在原地等下次
    if (!TryApplyPickup(Pawn))
    {
        return;
    }

    bConsumed = true;

    // 环绕特效立刻停:Destroy 到帧末才生效,不 Deactivate 的话循环特效会多播一帧
    if (AmbientEffectComp)
    {
        AmbientEffectComp->Deactivate();
    }

    // 拾取表现(可选,留空跳过)。脱离本体播放,不受下面的 Destroy 影响
    const FVector PickupLocation = GetActorLocation();

    // 拾取特效挂到拾取者身上(回血表现,跟着船走),而不是留在血包原地。
    // 挂在 Pawn 根组件上,bAutoDestroy 让一次性特效播完自动清理
    if (PickupEffect)
    {
        UNiagaraComponent* PickupFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
            PickupEffect, Pawn->GetRootComponent(), NAME_None,
            FVector::ZeroVector, FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget, /*bAutoDestroy=*/true);
        if (PickupFX)
        {
            PickupFX->SetRelativeScale3D(FVector(PickupEffectScale));

            // 循环特效不会自己结束,到点主动停发射(已有粒子淡出后随 bAutoDestroy 清理)。
            // 本 Actor 马上要 Destroy,定时器绑到存活的 Niagara 组件上而非 this
            if (PickupEffectDuration > 0.0f)
            {
                FTimerHandle StopHandle;
                GetWorld()->GetTimerManager().SetTimer(
                    StopHandle,
                    FTimerDelegate::CreateWeakLambda(PickupFX, [PickupFX]()
                    {
                        PickupFX->Deactivate();
                    }),
                    PickupEffectDuration, /*bLoop=*/false);
            }
        }
    }
    if (PickupSound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), PickupSound, PickupLocation);
    }

    Destroy();
}
