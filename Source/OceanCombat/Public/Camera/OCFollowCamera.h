// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "OCFollowCamera.generated.h"

/**
 * 第三人称跟随相机。独立 Actor,跟随目标 Actor(通常是玩家船)。
 *
 * 特性:
 * - 相机始终在目标的后上方(斜俯视)
 * - 跟随目标的朝向旋转(船转向时相机一起转)
 * - 用 VInterpTo/RInterpTo 实现平滑跟随,不僵硬
 *
 * 偏移在目标的 local 空间解释:
 * - CameraOffset: 相机相对目标的位置(如 -400, 0, 600 = 后 4m + 上 6m)
 * - LookAtOffset: 相机看向目标的某个点(如 300, 0, 0 = 看船头前方 3m)
 */
UCLASS()
class OCEANCOMBAT_API AOCFollowCamera : public ACameraActor
{
    GENERATED_BODY()

public:
    AOCFollowCamera();

    virtual void Tick(float DeltaTime) override;

    /** 设置跟随目标(通常是玩家船) */
    UFUNCTION(BlueprintCallable, Category = "Follow Camera")
    void SetFollowTarget(AActor* InTarget);

protected:
    /** 跟随目标 */
    UPROPERTY(EditAnywhere, Category = "Follow Camera")
    TObjectPtr<AActor> TargetActor;

    /** 相机相对目标的位置偏移(目标 local 空间,单位 cm) */
    UPROPERTY(EditAnywhere, Category = "Follow Camera", meta = (ClampMin = "-5000.0", ClampMax = "5000.0"))
    FVector CameraOffset;

    /** 相机看向的目标 local 空间偏移(单位 cm) */
    UPROPERTY(EditAnywhere, Category = "Follow Camera", meta = (ClampMin = "-5000.0", ClampMax = "5000.0"))
    FVector LookAtOffset;

    /** 位置跟随速度(越大越跟得紧) */
    UPROPERTY(EditAnywhere, Category = "Follow Camera|Smoothing", meta = (ClampMin = "0.1"))
    float LocationLagSpeed;

    /** 旋转跟随速度(越大越跟得紧) */
    UPROPERTY(EditAnywhere, Category = "Follow Camera|Smoothing", meta = (ClampMin = "0.1"))
    float RotationLagSpeed;

    /** 高度(Z)跟随速度:故意比 LocationLagSpeed 慢,把浪造成的上下颠簸滤掉。
     *  船的俯仰/横滚永远不传给相机(参考系只取偏航角) */
    UPROPERTY(EditAnywhere, Category = "Follow Camera|Smoothing", meta = (ClampMin = "0.1"))
    float HeightLagSpeed;

private:
    /** 是否已经做过首次定位(避免 Spawn 后相机"飞过来"的过程) */
    bool bSnapOnNextTick = true;
};
