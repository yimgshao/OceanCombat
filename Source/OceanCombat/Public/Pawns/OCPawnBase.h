// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Engine/DataTable.h"
#include "Data/OCCombatConfig.h"
#include "Data/OCShipUpgradeRow.h"
#include "OCPawnBase.generated.h"

class UOCHealthComponent;
class UWidgetComponent;
class AOCWeaponTurret;
class UOCWeaponMountComponent;

/**
 * 所有可被攻击单位的根基类(玩家船/敌船/建筑)。
 * 持有血量组件,并通过 override TakeDamage 接住引擎推送来的伤害,
 * 转发给血量组件结算。攻击方(炮弹)无需知道本类的存在。
 */
UCLASS(Abstract)
class OCEANCOMBAT_API AOCPawnBase : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AOCPawnBase();

	/** 血量组件,所有单位共用 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UOCHealthComponent> HealthComponent;

	UFUNCTION(BlueprintPure, Category = "Health")
	UOCHealthComponent* GetHealthComponent() const { return HealthComponent; }

	/** 获取第一门挂载的炮塔(无炮塔返回 nullptr)。转发给 UOCWeaponMountComponent;没挂该组件的单位恒返回 nullptr */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	AOCWeaponTurret* GetTurret() const;

	/** 获取所有挂载的炮塔(AI/配置下发/死亡隐藏统一走这里)。转发给 UOCWeaponMountComponent;没挂该组件的单位返回空数组 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void GetTurrets(TArray<AOCWeaponTurret*>& OutTurrets) const;

	/** 战斗配置行(血量/武器/伤害),指向 DT_CombatConfig 中的一行。留空则走 C++/蓝图默认值 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	FDataTableRowHandle CombatConfigRow;

	/** 头顶血条组件。WidgetClass 在蓝图里配 WBP_HealthBar,头顶偏移(RelativeLocation)也在蓝图里调 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> HealthBarWidgetComp;

	/** 是否显示头顶血条。默认 true;玩家船在蓝图里关掉(玩家血条走屏幕 HUD) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	bool bShowOverheadHealthBar = true;

	/** AI 攻击射程(cm),目标进入范围才开火。仅 AI 单位使用;关卡里每个实例可单独覆盖 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 5000.0f;

	/** AI 攻击最近距离(cm),目标比这个还近也不开火。0=不限制。仅 AI 单位使用 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0.0"))
	float MinAttackRange = 0.0f;

	/** AI 瞄准随机偏移半径(cm):每次开火后重摇偏移点,0=指哪打哪。仅 AI 单位使用 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0.0"))
	float AimRandomRadius = 300.0f;

	/** 击败该单位获得的分数。BeginPlay 取自战斗配置行的 Score;生成器可按难度分档系数缩放 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
	int32 ScoreValue = 0;

	/**
	 * 按难度分档系数缩放血量/伤害/得分。仅供地图生成器在 spawn 后调用一次:
	 * BeginPlay 已把配置行默认值下发完毕,这里用缩放后的副本整体覆盖。
	 */
	void ApplyDifficultyScaling(float HealthMultiplier, float DamageMultiplier, float ScoreMultiplier);

	/**
	 * 应用升级加成。基类处理血量上限与炮弹伤害,移动类属性由 AOCBoatBase override 补。
	 *
	 * 幂等:每次都从 BaseCombatConfig(基础值)重算,不做累加。所以"每买一级重新下发"
	 * 与"复活后把已购等级整体重新下发"走的是同一条路径,不会重复叠加。
	 * 未配战斗配置行的单位(bHasBaseCombatConfig=false)无操作。
	 */
	virtual void ApplyStatBonus(const FOCShipStatBonus& Bonus);

	/** 取某个可升级属性当前的绝对值,供 UI 显示"当前 → 下一级"。无法识别的属性返回 0 */
	virtual float GetUpgradableStatValue(EOCShipUpgradeType Type) const;

	/** 引擎伤害入口:攻击方调 ApplyDamage 系列函数时由引擎触发,转发给血量组件 */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/** 应用战斗配置:转发给 UOCWeaponMountComponent 下发到各炮塔(血量由 BeginPlay 直接处理) */
	virtual void ApplyCombatConfig(const FOCCombatConfigRow& Config);

	/**
	 * 基础战斗配置快照(BeginPlay 从配置行缓存;ApplyDifficultyScaling 会更新成缩放后的值)。
	 * 升级加成的算式基准 —— 保证难度系数与升级加成能正确复合,且重复应用不叠加。
	 */
	FOCCombatConfigRow BaseCombatConfig;

	/** 是否配了战斗配置行。false 时不参与升级(没有基准值可算) */
	bool bHasBaseCombatConfig = false;

	/** 最近一次应用的升级加成。仅用于 GetUpgradableStatValue 回报"当前生效值"(炮塔侧没有读取接口) */
	FOCShipStatBonus AppliedStatBonus;

	// 根组件由子类(船体/建筑 Mesh)提供,基类构造时还不存在,血条组件的挂接在这里补齐
	virtual void PostInitializeComponents() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
