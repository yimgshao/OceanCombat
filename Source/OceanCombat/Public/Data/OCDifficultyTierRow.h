// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "OCDifficultyTierRow.generated.h"

/**
 * 难度分档表行:按离出生点的距离分档,每档一组乘算系数。
 * 查档规则:取所有满足 MinDistance <= 实际距离 的行中 MinDistance 最大的那行
 * (表按 MinDistance 升序填写,首行必须为 0 兜底)。
 * 由地图生成器在 spawn 敌人时查表,系数一次性烘焙到实例上
 * (血量/伤害缩放走 AOCPawnBase::ApplyDifficultyScaling,得分缩放 ScoreValue)。
 */
USTRUCT(BlueprintType)
struct OCEANCOMBAT_API FOCDifficultyTierRow : public FTableRowBase
{
    GENERATED_BODY()

    /** 距离出生点 >= 此值(cm)时进入该档 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0.0"))
    float MinDistance = 0.0f;

    /** 血量乘算系数 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0.0"))
    float HealthMultiplier = 1.0f;

    /** 伤害乘算系数(BaseDamage 与 MinimumDamage 同步缩放,保持 AOE 衰减比例) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0.0"))
    float DamageMultiplier = 1.0f;

    /** 得分乘算系数 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0.0"))
    float ScoreMultiplier = 1.0f;
};
