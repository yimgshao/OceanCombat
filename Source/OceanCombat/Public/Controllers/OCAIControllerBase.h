// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "OCAIControllerBase.generated.h"

/**
 * 战斗单位 AI 基类:通用战斗循环——索敌 → 瞄准(带随机偏移) → 开火。
 * 只管"用炮塔打目标",不管移动;移动类行为(追击/巡逻)由子类(敌船 AI)追加。
 *
 * 每发子弹的瞄准点 = 目标当前位置 + 水平随机偏移(半径取 Pawn 的 AimRandomRadius),
 * 开火成功后重摇下一发的瞄准点,形成散布。
 * 射程/散布等参数在被 Possess 的 Pawn 上(AOCPawnBase::AttackRange / AimRandomRadius),
 * 支持按类配置和按关卡实例覆盖。
 */
UCLASS()
class OCEANCOMBAT_API AOCAIControllerBase : public AAIController
{
    GENERATED_BODY()

public:
    AOCAIControllerBase();

    virtual void Tick(float DeltaTime) override;

protected:
    /**
     * 索敌:返回当前攻击目标。
     * 默认实现 = 玩家 Pawn;子类可 override(如敌船按阵营找最近敌人)。
     */
    virtual AActor* AcquireTarget() const;

    /** 以目标位置为圆心、RandomRadius 为半径摇一个新的随机瞄准点 */
    FVector RollAimPoint(const FVector& TargetLocation, float RandomRadius) const;

private:
    /** 当前这一发的瞄准点;bHasAimPoint=false 时下一帧摇新的 */
    FVector CurrentAimPoint = FVector::ZeroVector;
    bool bHasAimPoint = false;
};
