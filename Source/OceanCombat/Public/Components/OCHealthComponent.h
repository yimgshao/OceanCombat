// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OCHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, OldHealth, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeath, AActor*, DeadActor, AController*, KillerController);

/**
 * 血量组件。所有可攻击单位共用,挂在 AOCPawnBase 上。
 * 职责:血量数据 + 扣血/回血计算 + 事件广播,不碰任何表现层。
 *
 * 数据流:攻击方(炮弹)调引擎 ApplyDamage 系列函数 → 引擎触发单位的
 * TakeDamage → AOCPawnBase 转发给本组件 → 扣血并广播事件。
 * 关心血量的外部观察者(UI、GameMode 等)自行绑定 OnHealthChanged / OnDeath,
 * 本组件不认识任何监听者。
 */
UCLASS(ClassGroup=(OceanCombat), meta=(BlueprintSpawnableComponent))
class OCEANCOMBAT_API UOCHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOCHealthComponent();

    /** 最大血量 */
    UPROPERTY(EditDefaultsOnly, Category = "Health", meta = (ClampMin = "1.0"))
    float MaxHealth = 100.0f;

    /** 当前血量,运行时可见便于调试 */
    UPROPERTY(VisibleInstanceOnly, Category = "Health")
    float CurrentHealth;

    /** 血量变化广播(受伤/回血都会触发) */
    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnHealthChanged OnHealthChanged;

    /** 死亡广播,只触发一次。KillerController 为最后一次伤害的来源 Controller(可能为 null) */
    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnDeath OnDeath;

    /** 用配置表覆盖最大血量并同步当前血量。仅供拥有者在 BeginPlay 阶段调用 */
    void InitMaxHealth(float NewMaxHealth);

    /**
     * 运行时修改最大血量(升级用)。与 InitMaxHealth 的区别:不把当前血量拉满。
     * 已死亡时忽略。血量有变化时广播 OnHealthChanged(血条自动刷新)。
     * @param bAdjustCurrent true=上限的增量同步加到当前血量(升级立刻多出这部分血,已受的伤不白挨);
     *                       false=只改上限,当前血量仅在超过新上限时被夹下来
     */
    UFUNCTION(BlueprintCallable, Category = "Health")
    void SetMaxHealth(float NewMaxHealth, bool bAdjustCurrent);

    /**
     * 扣血。Amount<=0 或已死亡时忽略。
     * @param EventInstigator  伤害来源的 Controller(击杀归属用:记录后随 OnDeath 广播)
     * @param DamageCauser     直接伤害来源(如炮弹 Actor)
     */
    UFUNCTION(BlueprintCallable, Category = "Health")
    void ApplyDamage(float Amount, AController* EventInstigator, AActor* DamageCauser);

    /** 回血,夹到 MaxHealth,血量变化时同样广播 OnHealthChanged */
    UFUNCTION(BlueprintCallable, Category = "Health")
    void Heal(float Amount);

    /** 血量百分比(0~1),血条直接用 */
    UFUNCTION(BlueprintPure, Category = "Health")
    float GetHealthPercent() const;

    UFUNCTION(BlueprintPure, Category = "Health")
    bool IsDead() const { return bIsDead; }

    UFUNCTION(BlueprintPure, Category = "Health")
    float GetCurrentHealth() const { return CurrentHealth; }

    UFUNCTION(BlueprintPure, Category = "Health")
    float GetMaxHealth() const { return MaxHealth; }

    /**
     * 记录最后一击的爆炸信息(爆心 + 半径),供死亡表现层(碎裂组件)读取,
     * 让残骸碎裂/爆开的中心、范围、力度随攻击来源(炮弹/炸药桶爆炸大小不同)自动变化。
     * 由 AOCPawnBase::TakeDamage 在结算(ApplyDamage)前写入 —— OnDeath 在 ApplyDamage 内广播,
     * 因此死亡时读到的就是致命一击的信息。本组件不解释这些数据,只存取。
     * @param WorldLocation 爆心/受击点世界坐标(炮弹=弹着点;炸药桶=桶的位置,可能离本体有距离)
     * @param BlastRadius   爆炸外半径(cm);非爆炸/点伤致死时为 0。既是波及范围,也代表爆炸大小(→冲量峰值)
     */
    void SetLastHitInfo(const FVector& WorldLocation, float BlastRadius);

    bool HasLastHitInfo() const { return bHasLastHitInfo; }
    FVector GetLastHitLocation() const { return LastHitLocation; }
    float GetLastBlastRadius() const { return LastBlastRadius; }

protected:
    virtual void BeginPlay() override;

private:
    /** 已死亡:防止死后继续扣血 / 重复广播 OnDeath */
    bool bIsDead = false;

    /** 最后一次伤害的来源 Controller,死亡时随 OnDeath 广播(击杀归属判定) */
    TWeakObjectPtr<AController> LastDamageInstigator;

    /** 最后一击的爆心/受击点世界坐标(死亡碎裂定位用) */
    FVector LastHitLocation = FVector::ZeroVector;

    /** 最后一击的爆炸外半径(cm),0=非爆炸/点伤(碎裂组件据此回退到整块炸开);也代表爆炸大小 */
    float LastBlastRadius = 0.0f;

    /** 是否已记录过受击信息(未记录时碎裂组件回退到对称行为) */
    bool bHasLastHitInfo = false;
};
