// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OCPickupBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;

/**
 * 所有可拾取掉落物的根基类。由 UOCLootDropComponent 在单位死亡时按概率生成。
 *
 * 本类负责与"拾取什么"无关的全部通用逻辑:
 * 触发球 + 视觉 Mesh + 漂浮表现 + 存活时限 + 拾取者资格判定 + 拾取表现 + 销毁。
 * 子类只需 override TryApplyPickup() 实现具体效果(回血/弹药/加分…)。
 *
 * 不继承 AOCPawnBase:掉落物不可被攻击、没有血量,只是个触发器 + 视觉。
 * 也不用 UBuoyancyComponent —— 掉落物不需要真实浮力,物理模拟反而会被死亡碎块撞飞。
 * 漂浮表现改为 Tick 里给 Mesh 做自转 + 正弦上下浮动(触发球本体不动,overlap 判定稳定)。
 */
UCLASS(Abstract)
class OCEANCOMBAT_API AOCPickupBase : public AActor
{
    GENERATED_BODY()

public:
    AOCPickupBase();

    // ---- 触发与存活 ----
    /** 拾取触发球半径(cm) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "0.0"))
    float PickupRadius = 250.0f;

    /** 存活时间(秒),超时自动消失。防止满地掉落物堆积;0=永不消失 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "0.0"))
    float PickupLifeSpan = 30.0f;

    // ---- 漂浮表现 ----
    /** 绕 Z 自转速度(度/秒),0=不转 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Motion")
    float SpinRateDeg = 90.0f;

    /** 上下浮动幅度(cm),0=不浮动 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Motion", meta = (ClampMin = "0.0"))
    float BobAmplitude = 30.0f;

    /** 上下浮动周期(秒) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Motion", meta = (ClampMin = "0.1"))
    float BobPeriod = 2.0f;

    // ---- 拾取表现(可选,留空跳过)----
    /** 环绕特效:血包存活期间一直播放(循环),挂在触发球上,不随 Mesh 浮动 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|FX")
    TObjectPtr<UNiagaraSystem> AmbientEffect;

    /** 环绕特效缩放。特效看起来太小就调大(1=资产原始大小) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|FX", meta = (ClampMin = "0.01"))
    float AmbientEffectScale = 1.0f;

    /** 拾取特效:被拾取瞬间挂到拾取者身上播放一次 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|FX")
    TObjectPtr<UNiagaraSystem> PickupEffect;

    /** 拾取特效缩放。特效看起来太小就调大(1=资产原始大小) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|FX", meta = (ClampMin = "0.01"))
    float PickupEffectScale = 1.0f;

    /**
     * 拾取特效播放时长(秒),到点主动停止发射(已有粒子播完淡出)。
     * 用于循环特效:否则会一直挂在拾取者身上不消失。
     * 0 = 不主动停,依赖特效自身结束(仅适合非循环的一次性特效)。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|FX", meta = (ClampMin = "0.0"))
    float PickupEffectDuration = 1.5f;

    /** 拾取音效 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|FX")
    TObjectPtr<USoundBase> PickupSound;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /**
     * 应用拾取效果。子类必须实现。
     * @param PickerPawn  拾取者(已确认是玩家控制的 Pawn,非空)
     * @return true=已消耗(基类随即播放表现并销毁自身);false=不消耗(留在原地等下次触发)
     */
    virtual bool TryApplyPickup(APawn* PickerPawn)
        PURE_VIRTUAL(AOCPickupBase::TryApplyPickup, return false;);

    /** 拾取触发球(RootComponent)。只与船体(PhysicsBody)产生 overlap */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
    TObjectPtr<USphereComponent> TriggerSphere;

    /** 视觉 Mesh(自转/浮动作用于它,触发球不动)。具体模型在蓝图里配 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
    TObjectPtr<UStaticMeshComponent> PickupMesh;

private:
    /** overlap 回调:确认是玩家控制的 Pawn 后转给 TryApplyPickup,消耗则播放表现并销毁自身 */
    UFUNCTION()
    void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    /** AmbientEffect 的运行时实例:BeginPlay 挂到触发球上,消耗时先停掉再随 Destroy 销毁 */
    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> AmbientEffectComp;

    /** Mesh 的初始相对高度,浮动以它为基准 */
    float MeshBaseRelativeZ = 0.0f;

    /** 累计存活时间,驱动浮动正弦 */
    float ElapsedTime = 0.0f;

    /** 已被拾取:防止同一帧多个 overlap 重复生效 */
    bool bConsumed = false;
};
