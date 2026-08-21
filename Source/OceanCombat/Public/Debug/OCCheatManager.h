// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "OCCheatManager.generated.h"

/**
 * 本项目的控制台作弊命令集合(仅开发期使用)。
 *
 * 为什么用 UCheatManager 子类而不是在 PlayerController 上加 exec 函数:
 * 引擎的 APlayerController::AddCheats() 整个函数体包在 #if UE_WITH_CHEAT_MANAGER 里,
 * Shipping 配置下该宏为 0,CheatManager 永不实例化 —— 作弊码天然不会泄漏到正式版,
 * 也不需要自己写 #if !UE_BUILD_SHIPPING 包裹。顺带还能白拿引擎自带的 God/Slomo/DamageTarget 等命令。
 *
 * 启用方式:在 BP_PlayerController 的 Cheat Manager → Cheat Class 里指派本类。
 * PIE 下 AGameModeBase::AllowCheats 恒为 true,不需要先输 EnableCheats。
 */
UCLASS()
class OCEANCOMBAT_API UOCCheatManager : public UCheatManager
{
    GENERATED_BODY()

public:
    /**
     * 无敌模式开关(再输一次关闭)。
     *
     * 原理是切换当前 Pawn 的 bCanBeDamaged:UGameplayStatics::ApplyRadialDamageWithFalloff
     * 在收集 Overlap 时会跳过 CanBeDamaged()==false 的 Actor,而本项目的伤害只有
     * AOCExplosiveProjectile 这一条路径,所以能挡住全部伤害(连 TakeDamage 都不会进)。
     *
     * 注意:不跨复活保留。玩家船死亡后 GameMode 会 Spawn 一艘全新的船,新船的 bCanBeDamaged 回到默认 true。
     */
    UFUNCTION(exec, BlueprintCallable, Category = "Cheat|OceanCombat")
    virtual void OCGod();

    /** 直接加分(总分与余额同时增加,与击杀加分同一路径)。Amount<=0 时提示用法并忽略 */
    UFUNCTION(exec, BlueprintCallable, Category = "Cheat|OceanCombat")
    virtual void OCAddScore(int32 Amount);
};
