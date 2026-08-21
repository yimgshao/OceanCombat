// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/Projectiles/OCProjectileBase.h"
#include "Weapons/OCCombatStatics.h"
#include "OCExplosiveProjectile.generated.h"

class UDamageType;

/**
 * 爆炸炮弹。命中后在落点施加球形 AOE 伤害(带内圈满伤 → 外圈衰减)。
 * 伤害通过 UGameplayStatics::ApplyRadialDamageWithFalloff 发出,
 * 受击方在自身 TakeDamage 里处理扣血(血量系统后续阶段实现)。
 */
UCLASS()
class OCEANCOMBAT_API AOCExplosiveProjectile : public AOCProjectileBase
{
    GENERATED_BODY()

public:
    AOCExplosiveProjectile();

    /** 注入伤害配置(来自发射单位的配置表,Fire 时由炮塔在 FinishSpawning 前调用) */
    virtual void ApplyDamageConfig(const FOCCombatConfigRow& Config) override;

protected:
    virtual void OnImpact(const FHitResult& Hit, bool bHitWater) override;

    /** 爆炸伤害 + 冲量参数(与炸药桶共用的结构,施放时直接传给 ApplyExplosion) */
    UPROPERTY(EditDefaultsOnly, Category = "Explosion")
    FOCExplosionParams Explosion;

    /** 伤害类型(可在蓝图配自定义 DamageType) */
    UPROPERTY(EditDefaultsOnly, Category = "Explosion")
    TSubclassOf<UDamageType> DamageTypeClass;

    /** 爆炸向浅水模拟注入的向下冲量(cm/s);0=不注入。炮弹为小爆炸,默认较小。 */
    UPROPERTY(EditDefaultsOnly, Category = "Explosion|Water", meta = (ClampMin = "0.0"))
    float WaterExplosionStrength = 2000.0f;

    /** 爆炸水面冲击半径(cm) */
    UPROPERTY(EditDefaultsOnly, Category = "Explosion|Water", meta = (ClampMin = "0.0"))
    float WaterExplosionRadius = 700.0f;
};
