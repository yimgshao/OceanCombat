// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OCShipUpgradeRow.h"
#include "OCShipUpgradeComponent.generated.h"

class UDataTable;
class AOCPawnBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOCUpgradePurchased);

/**
 * 小船升级组件。持有玩家已购的各属性等级,并把加成下发到当前控制的船。
 *
 * 挂在 AOCPlayerController 上而不是船上 —— 船死亡后会被 Destroy 并重新 Spawn 一艘新的,
 * 等级数据必须活得比船久。PC 在 OnPossess 时调 ApplyToPawn,复活保留就自然成立。
 *
 * 数值全部来自 UpgradeTable(DT_ShipUpgrades):一个属性一行,行内 Levels 数组按等级排。
 * 消费走 AOCGameMode::TrySpendScore,扣分会广播 OnScoreChanged 让 HUD 自动刷新余额。
 */
UCLASS(ClassGroup=(OceanCombat), meta=(BlueprintSpawnableComponent))
class OCEANCOMBAT_API UOCShipUpgradeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOCShipUpgradeComponent();

    /** 升级配置表(行结构 FOCShipUpgradeRow),在 BP_PlayerController 上指派 DT_ShipUpgrades */
    UPROPERTY(EditDefaultsOnly, Category = "Upgrade")
    TObjectPtr<UDataTable> UpgradeTable;

    /** 购买成功广播。UI 绑定后整体刷新(买贵的会让其他条目余额不足变灰,必须全刷) */
    UPROPERTY(BlueprintAssignable, Category = "Upgrade")
    FOnOCUpgradePurchased OnUpgradePurchased;

    /** 取表内所有升级项,按 SortOrder 升序(表的行迭代顺序不保证与编辑器行序一致) */
    void GetUpgradeRows(TArray<const FOCShipUpgradeRow*>& OutRows) const;

    /** 已购等级,0 = 未升级 */
    UFUNCTION(BlueprintPure, Category = "Upgrade")
    int32 GetLevel(EOCShipUpgradeType Type) const;

    /** 等级上限(= 该行 Levels 数组长度);表里没这一行返回 0 */
    UFUNCTION(BlueprintPure, Category = "Upgrade")
    int32 GetMaxLevel(EOCShipUpgradeType Type) const;

    /** 是否已满级(含"表里没这一行"的情况) */
    UFUNCTION(BlueprintPure, Category = "Upgrade")
    bool IsMaxLevel(EOCShipUpgradeType Type) const;

    /** 下一级的消耗;已满级返回 INDEX_NONE */
    UFUNCTION(BlueprintPure, Category = "Upgrade")
    int32 GetNextCost(EOCShipUpgradeType Type) const;

    /** 下一级的属性增量;已满级返回 0 */
    UFUNCTION(BlueprintPure, Category = "Upgrade")
    float GetNextDelta(EOCShipUpgradeType Type) const;

    /** 未满级且余额够买下一级 */
    UFUNCTION(BlueprintPure, Category = "Upgrade")
    bool CanUpgrade(EOCShipUpgradeType Type) const;

    /** 购买一级:扣分成功则等级 +1、立刻应用到当前船并广播 OnUpgradePurchased。返回是否成功 */
    UFUNCTION(BlueprintCallable, Category = "Upgrade")
    bool TryUpgrade(EOCShipUpgradeType Type);

    /** 把已购等级的全部加成应用到指定船。由 PC 的 OnPossess 调用(复活后恢复升级) */
    void ApplyToPawn(AOCPawnBase* Pawn) const;

private:
    /** 按枚举查表行;表未配置或行缺失返回 nullptr */
    const FOCShipUpgradeRow* FindRow(EOCShipUpgradeType Type) const;

    /** 汇总各属性已购等级的 Delta 之和 */
    FOCShipStatBonus BuildBonus() const;

    /** 已购等级,键为属性枚举。不在表里的属性不会出现在这里 */
    TMap<EOCShipUpgradeType, int32> Levels;
};
