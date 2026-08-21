// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons/OCCombatStatics.h"
#include "OCExplosiveBarrel.generated.h"

class UStaticMeshComponent;
class UBuoyancyComponent;
class UOCHealthComponent;
class UDamageType;
class UNiagaraSystem;
class USoundBase;
class UCameraShakeBase;

/**
 * 漂浮炸药桶:海面上的可引爆物理障碍。
 *
 * 物理:BarrelMesh 为 RootComponent 且 SimulatePhysics=true,配官方 UBuoyancyComponent
 * 漂在水面、随浪起伏,也能被撞击/爆炸冲量掀开。
 *
 * 引爆:持有 UOCHealthComponent,override TakeDamage 把伤害转给它;血量归零(OnDeath)后
 * 起一个很短的随机延时再 Detonate() —— 既避免同帧递归炸穿一片,又形成"噼里啪啦"的级联连爆。
 * Detonate 复用 UOCCombatStatics::ApplyExplosion(与爆炸炮弹同一实现):
 * 球形 AOE 伤害会命中邻近炸药桶 → 触发它们的 TakeDamage → 连锁引爆。
 *
 * 击杀归属:引爆的 InstigatedBy 用"最后打爆本桶的 Controller"(OnDeath 带的 KillerController),
 * 于是玩家打爆桶炸死的敌人算玩家的,连锁下去归属也顺着传递。
 */
UCLASS()
class OCEANCOMBAT_API AOCExplosiveBarrel : public AActor
{
    GENERATED_BODY()

public:
    AOCExplosiveBarrel();

    /** 接住引擎推来的伤害(含爆炸 AOE),转发给血量组件 */
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

protected:
    virtual void BeginPlay() override;

    // ---- 组件 ----
    /** 桶体物理 Mesh(也是 RootComponent) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrel")
    TObjectPtr<UStaticMeshComponent> BarrelMesh;

    /** 官方浮力组件;未配 pontoon 时 BeginPlay 会补一个默认 pontoon 保证能漂 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrel|Buoyancy")
    TObjectPtr<UBuoyancyComponent> BuoyancyComp;

    /** 血量组件:桶的耐久,归零即引爆(默认很低,基本一击即爆) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrel")
    TObjectPtr<UOCHealthComponent> HealthComponent;

    // ---- 爆炸参数 ----
    /** 爆炸伤害 + 冲量参数(与爆炸炮弹共用的结构) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrel|Explosion")
    FOCExplosionParams Explosion;

    /** 伤害类型(可在蓝图配自定义 DamageType) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrel|Explosion")
    TSubclassOf<UDamageType> DamageTypeClass;

    /** 引爆延时区间(秒):OnDeath 后在此区间随机取一个延时再爆,形成级联连爆并避免同帧递归 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrel|Explosion", meta = (ClampMin = "0.0"))
    FVector2D DetonateDelayRange = FVector2D(0.05f, 0.2f);

    /** 爆炸向浅水模拟注入的向下冲量(cm/s);0=不注入。桶为大爆炸,默认远大于炮弹。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrel|Explosion", meta = (ClampMin = "0.0"))
    float WaterExplosionStrength = 5000.0f;

    /** 爆炸水面冲击半径(cm) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrel|Explosion", meta = (ClampMin = "0.0"))
    float WaterExplosionRadius = 1500.0f;

    /** 未在蓝图配 pontoon 时使用的默认 pontoon 半径(cm) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrel|Buoyancy", meta = (ClampMin = "1.0"))
    float DefaultPontoonRadius = 90.0f;

    // ---- 表现 ----
    /** 爆炸特效(留空则跳过) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrel|FX")
    TObjectPtr<UNiagaraSystem> ExplosionEffect;

    /** 爆炸特效缩放(1=资产原始大小) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrel|FX", meta = (ClampMin = "0.01"))
    float ExplosionEffectScale = 1.0f;

    /** 爆炸音效(留空则跳过) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrel|FX")
    TObjectPtr<USoundBase> ExplosionSound;

    /** 爆炸屏幕抖动(CameraShakeBase 蓝图;留空则跳过) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrel|FX")
    TSubclassOf<UCameraShakeBase> ExplosionShake;

    /** 抖动满强度内半径(cm):爆心此距离内不衰减 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrel|FX", meta = (ClampMin = "0.0"))
    float ShakeInnerRadius = 1000.0f;

    /** 抖动外半径(cm):超过此距离完全不抖;内/外半径之间线性衰减 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrel|FX", meta = (ClampMin = "0.0"))
    float ShakeOuterRadius = 8000.0f;

private:
    /** 血量归零回调:记录击杀者并启动引爆延时 */
    UFUNCTION()
    void HandleDeath(AActor* DeadActor, AController* KillerController);

    /** 真正引爆:施放爆炸(伤害+冲量)、播表现、销毁自身 */
    void Detonate();

    /** 已进入引爆流程,防止延时期间被再次触发 */
    bool bDetonating = false;

    /** 引爆归属:最后打爆本桶的 Controller,随爆炸伤害传出去 */
    TWeakObjectPtr<AController> PendingInstigator;

    /** 引爆延时计时器 */
    FTimerHandle DetonateTimerHandle;
};
