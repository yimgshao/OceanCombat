// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "OCShipUpgradeRow.generated.h"

/**
 * 可升级属性种类。
 * 新增一个可升级属性需要三处改动:此处加枚举值、DT_ShipUpgrades 里加一行、
 * AOCPawnBase(或子类) 的 ApplyStatBonus/GetUpgradableStatValue 里处理对应字段。
 */
UENUM(BlueprintType)
enum class EOCShipUpgradeType : uint8
{
    /** 航速上限:AOCBoatBase::MaxForwardSpeed(cm/s),推力按同比例一起放大 */
    MoveSpeed   UMETA(DisplayName = "航速"),

    /** 转向角加速度:AOCBoatBase::TurnTorque(度/秒²) */
    TurnSpeed   UMETA(DisplayName = "转向"),

    /** 炮弹伤害:FOCCombatConfigRow::BaseDamage(MinimumDamage 按比例同步) */
    Damage      UMETA(DisplayName = "伤害"),

    /** 血量上限:FOCCombatConfigRow::MaxHealth */
    MaxHealth   UMETA(DisplayName = "血量上限"),

    /** 射速:FOCCombatConfigRow::FireRate(每秒发射次数,开火 CD 间隔 = 1/FireRate) */
    FireRate    UMETA(DisplayName = "射速"),
};

/** 单个升级等级的消耗与增量 */
USTRUCT(BlueprintType)
struct OCEANCOMBAT_API FOCUpgradeLevel
{
    GENERATED_BODY()

    /** 升到本级消耗的得分(从余额扣) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0"))
    int32 Cost = 0;

    /** 本级带来的属性增量。总加成 = 已购各级 Delta 之和,叠加在基础值上 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
    float Delta = 0.0f;
};

/**
 * 小船升级表行:一个可升级属性一行,行内 Levels 数组按等级排列。
 * Levels[0] 是 1 级、Levels[1] 是 2 级……数组长度即该属性的等级上限。
 * 加等级只需在数组末尾追加一项,代码与 UI 无需改动。
 */
USTRUCT(BlueprintType)
struct OCEANCOMBAT_API FOCShipUpgradeRow : public FTableRowBase
{
    GENERATED_BODY()

    /** 本行对应的属性种类。同一种类在表里只应出现一次(重复时取第一条并打 Warning) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
    EOCShipUpgradeType UpgradeType = EOCShipUpgradeType::MoveSpeed;

    /** UI 显示名(如"航速") */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
    FText DisplayName;

    /** UI 补充说明,可留空 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
    FText Description;

    /** UI 列表排序权重(升序)。DataTable 的行迭代顺序不保证与编辑器行序一致,故显式排序 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
    int32 SortOrder = 0;

    /** 各等级的消耗与增量,索引 0 对应 1 级 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
    TArray<FOCUpgradeLevel> Levels;
};

/**
 * 升级带来的属性加成总量(各已购等级 Delta 之和)。
 * 由 UOCShipUpgradeComponent 汇总后整体下发给单位,单位按"基础值 + 加成"重算,不做累加。
 */
USTRUCT(BlueprintType)
struct OCEANCOMBAT_API FOCShipStatBonus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
    float MoveSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
    float TurnSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
    float Damage = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
    float MaxHealth = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
    float FireRate = 0.0f;

    /** 按属性种类累加增量。ApplyStatBonus 的反向操作(汇总侧) */
    void Add(EOCShipUpgradeType Type, float Delta)
    {
        switch (Type)
        {
        case EOCShipUpgradeType::MoveSpeed: MoveSpeed += Delta; break;
        case EOCShipUpgradeType::TurnSpeed: TurnSpeed += Delta; break;
        case EOCShipUpgradeType::Damage:    Damage += Delta;    break;
        case EOCShipUpgradeType::MaxHealth: MaxHealth += Delta; break;
        case EOCShipUpgradeType::FireRate:  FireRate += Delta;  break;
        }
    }
};
