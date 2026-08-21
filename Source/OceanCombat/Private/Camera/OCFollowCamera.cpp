// OceanCombat. Copyright(c) All rights reserved.

#include "Camera/OCFollowCamera.h"

#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"

AOCFollowCamera::AOCFollowCamera()
{
    PrimaryActorTick.bCanEverTick = true;
    // 让相机在 Pawn 之后 Tick,保证拿到最新的目标位置
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;

    // ---- 默认参数:第三人称斜俯视 ----
    // 假设船头朝 +X,那么:
    //   CameraOffset  = (-400, 0, 600)  → 相机在船后 4m,上方 6m
    //   LookAtOffset  = (300, 0, 0)     → 看船头前方 3m
    // 俯角约 atan(600/700) ≈ 40°
    CameraOffset = FVector(-2000.0f, 0.0f, 2000.0f);
    LookAtOffset = FVector(1600.0f, 0.0f, 0.0f);

    LocationLagSpeed = 6.0f;
    RotationLagSpeed = 10.0f;
    HeightLagSpeed = 2.0f;

    // FOV 调宽一点,俯视海战视野更广
    if (GetCameraComponent())
    {
        GetCameraComponent()->SetFieldOfView(90.0f);
        // 不约束纵横比:约束会产生黑边,且 UI(AddToViewport)会画到黑边上
        GetCameraComponent()->bConstrainAspectRatio = false;
    }
}

void AOCFollowCamera::SetFollowTarget(AActor* InTarget)
{
    TargetActor = InTarget;
    bSnapOnNextTick = true; // 切换目标时立即 snap 到新位置
}

void AOCFollowCamera::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!TargetActor)
    {
        return;
    }

    // 稳定化的目标参考系:只取位置和偏航角,浪造成的俯仰/横滚不传给相机
    const FVector TargetLoc = TargetActor->GetActorLocation();
    const FRotator YawOnly(0.0f, TargetActor->GetActorRotation().Yaw, 0.0f);

    // 计算理想位置(CameraOffset 在"仅偏航"的目标空间解释,再转世界空间)
    const FVector IdealLocation = TargetLoc + YawOnly.RotateVector(CameraOffset);

    // 计算理想朝向(从相机位置看向 LookAtPoint)
    const FVector LookAtPoint = TargetLoc + YawOnly.RotateVector(LookAtOffset);
    const FRotator IdealRotation = UKismetMathLibrary::FindLookAtRotation(IdealLocation, LookAtPoint);

    // 首次或切换目标时直接 snap,避免"飞过来"过程
    if (bSnapOnNextTick)
    {
        SetActorLocationAndRotation(IdealLocation, IdealRotation);
        bSnapOnNextTick = false;
        return;
    }

    // 平滑插值:XY 用 LocationLagSpeed,Z 单独用更慢的 HeightLagSpeed 滤掉浪的颠簸
    const FVector CurrentLocation = GetActorLocation();
    FVector NewLocation = FMath::VInterpTo(CurrentLocation, IdealLocation, DeltaTime, LocationLagSpeed);
    NewLocation.Z = FMath::FInterpTo(CurrentLocation.Z, IdealLocation.Z, DeltaTime, HeightLagSpeed);
    const FRotator NewRotation = FMath::RInterpTo(
        GetActorRotation(), IdealRotation, DeltaTime, RotationLagSpeed);

    SetActorLocationAndRotation(NewLocation, NewRotation);
}
