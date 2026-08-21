// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "OCCombatConfig.generated.h"

/**
 * 战斗配置表行:一个单位一行的完整战斗数值(血量 + 武器 + 炮弹伤害)。
 * 单位(AOCPawnBase)用 FDataTableRowHandle 指向本结构的一行,
 * BeginPlay 时统一下发:血量 → HealthComponent,武器/伤害 → 炮塔 → 炮弹。
 * 默认值与代码内原默认值一致;单位不配行时各类走自己的默认值(向后兼容)。
 */
USTRUCT(BlueprintType)
struct OCEANCOMBAT_API FOCCombatConfigRow : public FTableRowBase
{
    GENERATED_BODY()

    // ---- 血量 ----
    /** 最大血量 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
    float MaxHealth = 100.0f;

    // ---- 武器(炮塔)----
    /** 每秒最大发射次数 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
    float FireRate = 1.0f;

    /** 炮口初速(cm/s) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
    float MuzzleVelocity = 3000.0f;

    // ---- 炮弹伤害(爆炸 AOE)----
    /** 落点(内圈)满伤害值 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
    float BaseDamage = 100.0f;

    /** 外圈边缘的最小伤害值 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
    float MinimumDamage = 0.0f;

    /** 满伤内圈半径(cm):此半径内都吃满伤 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
    float DamageInnerRadius = 150.0f;

    /** 伤害外圈半径(cm):此半径外不受伤 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
    float DamageOuterRadius = 500.0f;

    /** 内→外衰减指数(1=线性,>1 衰减更快) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
    float DamageFalloff = 1.0f;

    // ---- 得分 ----
    /** 击败该单位的基础得分(难度分档系数在生成时另行乘算) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0"))
    int32 Score = 0;
};
