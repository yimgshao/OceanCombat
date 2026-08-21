// Fill out your copyright notice in the Description page of Project Settings.


#include "Pawns/OCPawnBase.h"

#include "Components/OCHealthComponent.h"
#include "Components/OCWeaponMountComponent.h"
#include "Components/WidgetComponent.h"
#include "Data/OCCombatConfig.h"
#include "Engine/DamageEvents.h"
#include "UI/OCHealthBarWidget.h"

// Sets default values
AOCPawnBase::AOCPawnBase()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	HealthComponent = CreateDefaultSubobject<UOCHealthComponent>(TEXT("HealthComponent"));

	// 头顶血条:屏幕空间渲染,永远正对相机。基类构造时根组件还没建(船/建筑各自设根),
	// 挂接到 PostInitializeComponents 里补
	HealthBarWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidgetComp"));
	HealthBarWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidgetComp->SetDrawSize(FVector2D(120.0f, 16.0f));
}

void AOCPawnBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 根组件由子类(船体/建筑 Mesh)提供,基类构造时还不存在,在这里补挂接
	if (HealthBarWidgetComp && !HealthBarWidgetComp->GetAttachParent() && GetRootComponent())
	{
		HealthBarWidgetComp->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}
}

AOCWeaponTurret* AOCPawnBase::GetTurret() const
{
	// 转发给武器挂载组件;没挂该组件的单位(如后续 buff 建筑)恒返回 nullptr
	const UOCWeaponMountComponent* WeaponMount = FindComponentByClass<UOCWeaponMountComponent>();
	return WeaponMount ? WeaponMount->GetTurret() : nullptr;
}

void AOCPawnBase::GetTurrets(TArray<AOCWeaponTurret*>& OutTurrets) const
{
	OutTurrets.Reset();
	// 炮塔归集逻辑已下沉到 UOCWeaponMountComponent(挂载点在蓝图组件树里,数量/类型随蓝图)
	if (const UOCWeaponMountComponent* WeaponMount = FindComponentByClass<UOCWeaponMountComponent>())
	{
		WeaponMount->GetTurrets(OutTurrets);
	}
}

float AOCPawnBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// 先走引擎默认结算(处理 RadialDamage 衰减等),拿到实际伤害值
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (HealthComponent)
	{
		// 从伤害事件解析爆炸信息(受击点 + 半径),供死亡碎裂按攻击来源(炮弹/炸药桶爆炸大小不同)调整。
		// 必须在 ApplyDamage 之前写入:OnDeath 在 ApplyDamage 内广播,碎裂组件届时读取。
		FVector HitLocation = GetActorLocation();
		float BlastRadius = 0.0f;   // 点伤/非爆炸为 0 → 碎裂组件回退到整块炸开
		bool bParsedHit = false;
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
			HitLocation = PointEvent.HitInfo.ImpactPoint;   // 点伤无半径,BlastRadius 保持 0
			bParsedHit = true;
		}
		else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
		{
			// 爆炸伤害(炮弹/炸药桶):Origin=弹着点/爆心;外半径来自各自的 FOCExplosionParams,天然不同
			const FRadialDamageEvent& RadialEvent = static_cast<const FRadialDamageEvent&>(DamageEvent);
			HitLocation = RadialEvent.Origin;
			BlastRadius = RadialEvent.Params.OuterRadius;   // 爆炸外半径(=波及范围,也代表爆炸大小)
			bParsedHit = true;
		}
		if (bParsedHit)
		{
			HealthComponent->SetLastHitInfo(HitLocation, BlastRadius);
		}

		HealthComponent->ApplyDamage(ActualDamage, EventInstigator, DamageCauser);
	}

	return ActualDamage;
}

void AOCPawnBase::ApplyCombatConfig(const FOCCombatConfigRow& Config)
{
	// 炮塔配置下发转发给武器挂载组件(血量已在 BeginPlay 直接处理);没挂该组件的单位无操作
	if (UOCWeaponMountComponent* WeaponMount = FindComponentByClass<UOCWeaponMountComponent>())
	{
		WeaponMount->ApplyConfig(Config);
	}
}

void AOCPawnBase::ApplyDifficultyScaling(float HealthMultiplier, float DamageMultiplier, float ScoreMultiplier)
{
	if (const FOCCombatConfigRow* Config = CombatConfigRow.GetRow<FOCCombatConfigRow>(TEXT("AOCPawnBase::ApplyDifficultyScaling")))
	{
		// 复制一行缩放后整体覆盖 BeginPlay 下发的默认值
		FOCCombatConfigRow Scaled = *Config;
		Scaled.MaxHealth *= HealthMultiplier;
		Scaled.BaseDamage *= DamageMultiplier;
		Scaled.MinimumDamage *= DamageMultiplier; // 两条伤害线同步缩,保持 AOE 衰减比例

		if (HealthComponent)
		{
			HealthComponent->InitMaxHealth(Scaled.MaxHealth);
		}
		ApplyCombatConfig(Scaled);

		// 缩放后的值成为新基准:后续升级加成叠加在难度缩放之上
		BaseCombatConfig = Scaled;
		bHasBaseCombatConfig = true;
	}
	else if (HealthComponent)
	{
		// 没配行的单位走默认值:血量在组件上直接缩,伤害无配置行可缩(炮塔走自身默认值)
		HealthComponent->InitMaxHealth(HealthComponent->GetMaxHealth() * HealthMultiplier);
	}

	ScoreValue = FMath::Max(0, FMath::RoundToInt(static_cast<float>(ScoreValue) * ScoreMultiplier));
}

void AOCPawnBase::ApplyStatBonus(const FOCShipStatBonus& Bonus)
{
	AppliedStatBonus = Bonus;

	if (!bHasBaseCombatConfig)
	{
		// 没配战斗配置行就没有基准值,算不出"基础 + 加成"。
		// 只在确实买了走配置表的升级时才警告(纯移动升级不经过这条路径的数值)
		if (Bonus.MaxHealth > 0.0f || Bonus.Damage > 0.0f || Bonus.FireRate > 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Upgrade] %s 未配置 CombatConfigRow,血量/伤害/射速升级不生效"), *GetName());
		}
		return;
	}

	// 从基础值起算(而非在当前值上累加),保证重复调用幂等
	FOCCombatConfigRow Modified = BaseCombatConfig;
	Modified.MaxHealth += Bonus.MaxHealth;
	Modified.BaseDamage += Bonus.Damage;
	Modified.FireRate += Bonus.FireRate; // 每秒发射次数,开火 CD 间隔 = 1/FireRate

	// 外圈最小伤害按内圈同比例抬,保持 AOE 内外衰减关系(与 ApplyDifficultyScaling 的处理一致)
	if (BaseCombatConfig.BaseDamage > 0.0f)
	{
		Modified.MinimumDamage = BaseCombatConfig.MinimumDamage * (Modified.BaseDamage / BaseCombatConfig.BaseDamage);
	}

	if (HealthComponent)
	{
		// 用 SetMaxHealth 而非 InitMaxHealth:升级不该顺手把血回满,只补上上限提升的那部分
		HealthComponent->SetMaxHealth(Modified.MaxHealth, /*bAdjustCurrent=*/true);
	}

	// 伤害沿用现有下发链:配置 → 炮塔 → 开火时注入炮弹
	ApplyCombatConfig(Modified);
}

float AOCPawnBase::GetUpgradableStatValue(EOCShipUpgradeType Type) const
{
	switch (Type)
	{
	case EOCShipUpgradeType::MaxHealth:
		return HealthComponent ? HealthComponent->GetMaxHealth() : 0.0f;

	case EOCShipUpgradeType::Damage:
		// 炮塔侧没有伤害读取接口,按"基准 + 已应用加成"回推当前生效值
		return bHasBaseCombatConfig ? BaseCombatConfig.BaseDamage + AppliedStatBonus.Damage : 0.0f;

	case EOCShipUpgradeType::FireRate:
		// 同上:炮塔的 FireRate 没有读取接口,按基准回推
		return bHasBaseCombatConfig ? BaseCombatConfig.FireRate + AppliedStatBonus.FireRate : 0.0f;

	default:
		// 移动类属性(航速/转向)在 AOCBoatBase 里取
		return 0.0f;
	}
}

// Called when the game starts or when spawned
void AOCPawnBase::BeginPlay()
{
	Super::BeginPlay();

	// 战斗配置表:配了行才生效,留空则走 C++/蓝图默认值(GetRow 失败会自己打 Warning 日志)。
	// 注意:组件的 BeginPlay 先于本函数执行,所以血量要用 InitMaxHealth 同步刷新。
	if (const FOCCombatConfigRow* Config = CombatConfigRow.GetRow<FOCCombatConfigRow>(TEXT("AOCPawnBase::BeginPlay")))
	{
		if (HealthComponent)
		{
			HealthComponent->InitMaxHealth(Config->MaxHealth);
		}
		ApplyCombatConfig(*Config);
		ScoreValue = Config->Score;

		// 缓存为升级加成的基准值(难度缩放会在 spawn 后把它更新成缩放后的值)
		BaseCombatConfig = *Config;
		bHasBaseCombatConfig = true;
	}

	// 头顶血条:关掉或未配 WidgetClass 就隐藏;配了则绑定血量数据源
	if (HealthBarWidgetComp)
	{
		if (bShowOverheadHealthBar && HealthBarWidgetComp->GetWidgetClass())
		{
			if (UOCHealthBarWidget* HealthBarWidget = Cast<UOCHealthBarWidget>(HealthBarWidgetComp->GetWidget()))
			{
				HealthBarWidget->InitWithHealthComponent(HealthComponent);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[HealthBar] %s 的 WidgetClass 不是 OCHealthBarWidget 子类,无法绑定血量"), *GetName());
			}
		}
		else
		{
			HealthBarWidgetComp->SetVisibility(false);
		}
	}
}

// Called every frame
void AOCPawnBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AOCPawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

