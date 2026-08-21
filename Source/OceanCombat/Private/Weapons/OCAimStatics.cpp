// OceanCombat. Copyright(c) All rights reserved.

#include "Weapons/OCAimStatics.h"

FRotator UOCAimStatics::CalcAimRotation(const FVector& FromLocation, const FVector& TargetLocation)
{
    const FVector ToTarget = TargetLocation - FromLocation;

    // 水平方位角
    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));

    // 仰角:高度差 / 水平距离
    const float HorizontalDist = FVector2D(ToTarget.X, ToTarget.Y).Size();
    const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Z, HorizontalDist));

    return FRotator(Pitch, Yaw, 0.0f);
}

bool UOCAimStatics::CalcBallisticAimRotation(const FVector& FromLocation, const FVector& TargetLocation,
    float LaunchSpeed, float Gravity, FRotator& OutRotation)
{
    if (LaunchSpeed <= 0.0f || Gravity <= 0.0f)
    {
        return false;
    }

    const FVector ToTarget = TargetLocation - FromLocation;
    const float HorizontalDist = FVector2D(ToTarget.X, ToTarget.Y).Size();

    // 水平方位角与直线瞄准一致
    const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));

    // 目标几乎在正上/正下方:没有水平距离,退化为直线瞄准
    if (HorizontalDist < 1.0f)
    {
        OutRotation = CalcAimRotation(FromLocation, TargetLocation);
        return true;
    }

    // 抛体低弹道解:
    //   tan(θ) = (v² - √(v⁴ - g·(g·d² + 2·h·v²))) / (g·d)
    // d = 水平距离,h = 目标高出炮口的高度(向上为正)
    const float V2 = LaunchSpeed * LaunchSpeed;
    const float H = ToTarget.Z;
    const float Discriminant = V2 * V2 - Gravity * (Gravity * HorizontalDist * HorizontalDist + 2.0f * H * V2);
    if (Discriminant < 0.0f)
    {
        return false;  // 超出弹道最大射程(v²/g)
    }

    const float TanTheta = (V2 - FMath::Sqrt(Discriminant)) / (Gravity * HorizontalDist);
    const float Pitch = FMath::RadiansToDegrees(FMath::Atan(TanTheta));

    OutRotation = FRotator(Pitch, Yaw, 0.0f);
    return true;
}

FVector UOCAimStatics::CalcParabolaPoint(const FVector& Start, const FVector& LaunchVelocity, float GravityZ, float Time)
{
    // P(t) = P0 + V0*t + 0.5*a*t²,a = (0, 0, GravityZ)
    return Start + LaunchVelocity * Time + FVector(0.0f, 0.0f, 0.5f * GravityZ * Time * Time);
}
