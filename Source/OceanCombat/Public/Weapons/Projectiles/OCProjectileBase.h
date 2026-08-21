// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/OCCombatConfig.h"
#include "OCProjectileBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UWaterBodyComponent;
class UShallowWaterSubsystem;
class UNiagaraSystem;
class USoundBase;

/**
 * 炮弹基类。管"飞 + 命中检测 + 命中表现 + 自毁"。
 * 由武器的 Fire() 在运行时生成,Spawn 后立即调用 Launch() 注入初速度。
 *
 * 命中检测(两路):
 * - 硬物(礁石/敌船/建筑): 靠 ProjectileMovement 的扫掠碰撞,首次阻挡即触发
 *   OnProjectileStop → HandleProjectileStop → OnImpact(bHitWater=false)。
 * - 海面: 每帧 Tick 用水体组件查询当前位置的水面高度,炮弹低于水面即
 *   OnImpact(bHitWater=true)。(用查询而非物理碰撞,未来加波浪也能命中波峰波谷)
 *
 * 命中后:播特效/音效并 Destroy。爆炸伤害等由子类 override OnImpact 追加。
 */
UCLASS()
class OCEANCOMBAT_API AOCProjectileBase : public AActor
{
    GENERATED_BODY()

public:
    AOCProjectileBase();

    /** 注入初速度(cm/s),由发射方在 Spawn 后、FinishSpawning 前调用 */
    void Launch(const FVector& Velocity);

    /** 注入伤害配置,由发射方在 FinishSpawning 前调用。基类无伤害参数为空实现,子类(爆炸炮弹)override */
    virtual void ApplyDamageConfig(const FOCCombatConfigRow& Config);

    /** 重力缩放(供弹道解算读取实际重力) */
    UFUNCTION(BlueprintPure, Category = "Projectile")
    float GetGravityScale() const { return GravityScale; }

    /** 碰撞球半径(cm),供瞄准线预测用相同半径做扫掠,保证与真实炮弹一致 */
    UFUNCTION(BlueprintPure, Category = "Projectile")
    float GetCollisionRadius() const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /**
     * 命中处理:播命中特效/音效并销毁自身。
     * 子类可 override 在销毁前追加逻辑(如范围伤害),记得调用 Super。
     * @param Hit       命中信息(硬物为真实扫掠结果,海面为构造的水面命中点)
     * @param bHitWater true=命中海面, false=命中硬物
     */
    virtual void OnImpact(const FHitResult& Hit, bool bHitWater);

    /** ProjectileMovement 命中硬物停止时的回调(bShouldBounce=false 时首次撞击即触发) */
    UFUNCTION()
    void HandleProjectileStop(const FHitResult& ImpactResult);

    /** 碰撞体(根组件)。QueryOnly,靠扫掠检测硬物;忽略 owner 船和水体 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> SphereCollision;

    /** 炮弹显示 Mesh(无碰撞,纯显示) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> BulletMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    /** 重力缩放:1 = 正常重力抛物线 */
    UPROPERTY(EditDefaultsOnly, Category = "Projectile")
    float GravityScale = 1.0f;

    // ---- 命中表现(蓝图配资产,留空则不显示/播放)----
    /** 命中硬物特效 */
    UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
    TObjectPtr<UNiagaraSystem> ImpactEffect;

    /** 命中海面特效(如水花) */
    UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
    TObjectPtr<UNiagaraSystem> WaterImpactEffect;

    /** 命中硬物音效 */
    UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
    TObjectPtr<USoundBase> ImpactSound;

    /** 命中海面音效 */
    UPROPERTY(EditDefaultsOnly, Category = "Projectile|FX")
    TObjectPtr<USoundBase> WaterImpactSound;

    // ---- 落水波纹(浅水模拟,WaterAdvanced)----
    /** 落水时是否向浅水模拟注册冲击产生波纹 */
    UPROPERTY(EditDefaultsOnly, Category = "Projectile|WaterRipple")
    bool bWaterRippleEnabled = true;

    /** 落水冲击半径(cm),爆炸弹可在默认值里调大 */
    UPROPERTY(EditDefaultsOnly, Category = "Projectile|WaterRipple", meta = (ClampMin = "0.0"))
    float WaterRippleRadius = 400.0f;

    /** 冲击速度缩放:炮弹速度极大,直接传入会过猛,RegisterImpact 的速度 = 弹速 × 该系数 */
    UPROPERTY(EditDefaultsOnly, Category = "Projectile|WaterRipple", meta = (ClampMin = "0.0"))
    float WaterRippleVelocityScale = 0.1f;

    /** 命中只处理一次(防 Tick 与 OnProjectileStop 同帧重复触发)。子类 OnImpact 起始应检查它 */
    bool bHasImpacted = false;

private:
    /** 关卡里的水体组件(缓存,可能多个);BeginPlay 收集 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UWaterBodyComponent>> WaterBodies;

    /** 收集关卡水体组件,并让扫掠碰撞忽略水体(海面交给 Tick 查询处理) */
    void CacheWaterBodies();

    /** 查询当前位置是否已没入水面;命中则填充 OutHit 并返回 true */
    bool CheckWaterImpact(FHitResult& OutHit) const;

    /** 落水时向浅水模拟注册冲击(子系统不存在时静默跳过) */
    void RegisterWaterRipple(const FVector& ImpactPoint) const;
};
