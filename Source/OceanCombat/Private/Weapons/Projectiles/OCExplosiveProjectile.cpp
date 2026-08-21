// OceanCombat. Copyright(c) All rights reserved.

#include "Weapons/Projectiles/OCExplosiveProjectile.h"

#include "GameFramework/DamageType.h"
#include "Weapons/OCCombatStatics.h"

AOCExplosiveProjectile::AOCExplosiveProjectile()
{
    // 默认用引擎基础 DamageType,蓝图可覆盖
    DamageTypeClass = UDamageType::StaticClass();

    // 不向浅水模拟注入落水波纹(如需恢复落水波纹删除此行/改 true)
    bWaterRippleEnabled = false;
}

void AOCExplosiveProjectile::ApplyDamageConfig(const FOCCombatConfigRow& Config)
{
    Explosion.BaseDamage = Config.BaseDamage;
    Explosion.MinimumDamage = Config.MinimumDamage;
    Explosion.DamageInnerRadius = Config.DamageInnerRadius;
    Explosion.DamageOuterRadius = Config.DamageOuterRadius;
    Explosion.DamageFalloff = Config.DamageFalloff;
}

void AOCExplosiveProjectile::OnImpact(const FHitResult& Hit, bool bHitWater)
{
    // 防重复:硬物命中与海面查询可能同帧触发,只结算一次伤害
    if (bHasImpacted)
    {
        return;
    }

    // 在落点施加球形 AOE 伤害(内圈满伤 → 外圈按 DamageFalloff 衰减)+ 径向冲量。
    // 忽略自己;DamageCauser=this,InstigatedBy 用 Instigator 的 Controller。
    // 伤害与冲量逻辑与炸药桶共用 UOCCombatStatics::ApplyExplosion(单一实现)。
    const TArray<AActor*> IgnoreActors = { this };
    UOCCombatStatics::ApplyExplosion(
        this,
        Hit.ImpactPoint,
        Explosion,
        DamageTypeClass,
        IgnoreActors,
        /*DamageCauser=*/this,
        GetInstigatorController());

    // 爆炸向浅水模拟注入水面冲击(炸出水坑→向外扩散涟漪);与伤害同源爆心
    UOCCombatStatics::RegisterWaterExplosion(this, Hit.ImpactPoint, WaterExplosionStrength, WaterExplosionRadius);

    // 交给基类播特效/音效并销毁(会置 bHasImpacted)
    Super::OnImpact(Hit, bHitWater);
}
