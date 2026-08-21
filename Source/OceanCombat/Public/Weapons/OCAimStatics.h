// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OCAimStatics.generated.h"

/**
 * 瞄准数学库:无状态纯计算,供炮塔/AI/玩家轨迹线等共用(C++ 和蓝图都能调)。
 * 弹道抛物线相关(重力补偿、轨迹采样)后续也加在这个类里。
 */
UCLASS()
class OCEANCOMBAT_API UOCAimStatics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 直线瞄准:由起点和目标点算世界空间瞄准朝向(Yaw=水平方位角,Pitch=仰角,Roll=0)。
     * 不含弹道抛物线补偿。
     */
    UFUNCTION(BlueprintPure, Category = "Aim")
    static FRotator CalcAimRotation(const FVector& FromLocation, const FVector& TargetLocation);

    /**
     * 弹道瞄准:给定初速度和重力,解算命中目标所需的世界空间炮口朝向(低弹道解)。
     * @param LaunchSpeed  初速度(cm/s)
     * @param Gravity      重力大小(cm/s²,正值,如 980 * GravityScale)
     * @param OutRotation  解出的世界朝向(Yaw=方位角,Pitch=仰角)
     * @return false=目标超出弹道最大射程(v²/g),无解
     */
    UFUNCTION(BlueprintCallable, Category = "Aim")
    static bool CalcBallisticAimRotation(const FVector& FromLocation, const FVector& TargetLocation,
        float LaunchSpeed, float Gravity, FRotator& OutRotation);

    /**
     * 抛物线取点:给定起点、初速度(cm/s)、重力Z(cm/s²,通常为负),返回 Time 秒后的位置。
     * P(t) = Start + LaunchVelocity*t + 0.5*(0,0,GravityZ)*t²。与炮弹无阻力抛物线严格一致,
     * 供瞄准线逐步采样弹道。
     */
    UFUNCTION(BlueprintPure, Category = "Aim")
    static FVector CalcParabolaPoint(const FVector& Start, const FVector& LaunchVelocity, float GravityZ, float Time);
};
