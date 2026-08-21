// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "OCCombatStatics.generated.h"

class UDamageType;

/**
 * 一次爆炸的完整参数(球形 AOE 伤害 + 可选物理冲量)。
 * 炮弹伤害配置(FOCCombatConfigRow)与炸药桶各自持有一份,施放时打包传给 ApplyExplosion。
 * 默认值与 AOCExplosiveProjectile 原先硬编码的一致。
 */
USTRUCT(BlueprintType)
struct OCEANCOMBAT_API FOCExplosionParams
{
    GENERATED_BODY()

    // ---- 球形 AOE 伤害(内圈满伤 → 外圈按 Falloff 衰减)----
    /** 落点(内圈)满伤害值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion|Damage", meta = (ClampMin = "0.0"))
    float BaseDamage = 100.0f;

    /** 外圈边缘的最小伤害值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion|Damage", meta = (ClampMin = "0.0"))
    float MinimumDamage = 0.0f;

    /** 满伤内圈半径(cm) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion|Damage", meta = (ClampMin = "0.0"))
    float DamageInnerRadius = 150.0f;

    /** 伤害外圈半径(cm):此半径外不受伤 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion|Damage", meta = (ClampMin = "0.0"))
    float DamageOuterRadius = 500.0f;

    /** 内→外衰减指数(1=线性,>1 衰减更快) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion|Damage", meta = (ClampMin = "0.0"))
    float DamageFalloff = 1.0f;

    // ---- 物理冲量(把附近在模拟物理的物体掀开:船、炸药桶等)----
    /** 是否施加径向冲量。暂关闭:炸药桶质量很轻,冲量会把它直接掀飞;需要击退效果时再开 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion|Impulse")
    bool bApplyImpulse = false;

    /** 冲量大小(线性衰减到边缘为 0)。单位与 AddRadialImpulse 一致(默认按质量缩放的冲量) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion|Impulse", meta = (ClampMin = "0.0"))
    float ImpulseStrength = 60000.0f;

    /** 冲量作用半径(cm);<=0 时回退用 DamageOuterRadius */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion|Impulse", meta = (ClampMin = "0.0"))
    float ImpulseRadius = 0.0f;

    /** 取实际冲量半径(处理 <=0 回退) */
    float GetEffectiveImpulseRadius() const { return ImpulseRadius > 0.0f ? ImpulseRadius : DamageOuterRadius; }
};

/**
 * 战斗通用静态库:无状态纯逻辑,供炮弹/炸药桶等共用(C++ 和蓝图都能调)。
 */
UCLASS()
class OCEANCOMBAT_API UOCCombatStatics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 在 Origin 施放一次爆炸:球形 AOE 伤害(UGameplayStatics::ApplyRadialDamageWithFalloff)
     * + 可选径向冲量(对范围内在模拟物理的组件 AddRadialImpulse)。
     *
     * 伤害通过引擎伤害系统发出 → 命中方 TakeDamage → 各自血量组件结算,
     * 因此炸药桶的爆炸能自然引爆邻近炸药桶(连锁)。
     *
     * @param WorldContextObject 世界上下文(通常传 this)
     * @param Origin             爆心(世界坐标)
     * @param Params             伤害 + 冲量参数
     * @param DamageTypeClass    伤害类型(可空,空则用 UDamageType)
     * @param IgnoreActors       忽略的 Actor(通常含施放者自身,避免自伤/自推)
     * @param DamageCauser       直接伤害来源(如炮弹/炸药桶 Actor)
     * @param InstigatedBy       伤害来源 Controller(击杀归属)
     */
    UFUNCTION(BlueprintCallable, Category = "Combat", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "IgnoreActors"))
    static void ApplyExplosion(
        const UObject* WorldContextObject,
        const FVector& Origin,
        const FOCExplosionParams& Params,
        TSubclassOf<UDamageType> DamageTypeClass,
        const TArray<AActor*>& IgnoreActors,
        AActor* DamageCauser,
        AController* InstigatedBy);

    /**
     * 向浅水模拟(WaterAdvanced)注入一次爆炸水面冲击:在 Origin 处以向下冲量炸出水坑,
     * 由浅水方程自然回弹、向外扩散成涟漪。子系统不存在(插件禁用/Dedicated Server)时静默跳过。
     * 与 ApplyExplosion 分开调用,便于炮弹/炸药桶各自给不同大小。
     *
     * @param WorldContextObject 世界上下文(通常传 this)
     * @param Origin             爆心(世界坐标;水面 Z 由浅水子系统内部对齐)
     * @param ImpulseStrength    向下冲量大小(cm/s);越大水坑越深、涟漪越强。<=0 跳过
     * @param ImpulseRadius      作用半径(cm);越大波及范围越广。<=0 跳过
     */
    UFUNCTION(BlueprintCallable, Category = "Combat", meta = (WorldContext = "WorldContextObject"))
    static void RegisterWaterExplosion(
        const UObject* WorldContextObject,
        const FVector& Origin,
        float ImpulseStrength,
        float ImpulseRadius);
};
