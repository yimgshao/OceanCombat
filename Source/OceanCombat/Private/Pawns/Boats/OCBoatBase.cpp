// OceanCombat. Copyright(c) All rights reserved.

#include "Pawns/Boats/OCBoatBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/OCWeaponMountComponent.h"
#include "Components/OCHealthComponent.h"
#include "WaterBodyComponent.h"
#include "BuoyancyComponent.h"

AOCBoatBase::AOCBoatBase()
{
    PrimaryActorTick.bCanEverTick = true;

    // ---- 船体 Mesh(作为 RootComponent)----
    BoatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoatMesh"));
    SetRootComponent(BoatMesh);

    // 物理设置:浮力组件必须依赖物理模拟
    BoatMesh->SetSimulatePhysics(true);
    BoatMesh->SetEnableGravity(true);
    BoatMesh->SetLinearDamping(0.5f);
    BoatMesh->SetAngularDamping(2.0f);

    // 碰撞:船体需要能被炮弹/礁石撞到
    BoatMesh->SetCollisionProfileName(TEXT("PhysicsActor"));

    // 开启 CCD(连续碰撞检测):船被炮弹高速砸下时用扫掠检测,避免隧穿薄海底平面/地形
    // 被卡到海底之下浮不上来。
    BoatMesh->BodyInstance.bUseCCD = true;

    // ---- 浮力组件 ----
    // 只创建空组件,pontoon 在蓝图里配
    BuoyancyComp = CreateDefaultSubobject<UBuoyancyComponent>(TEXT("BuoyancyComp"));

    // ---- 武器挂载组件(炮塔挂载点在蓝图组件树里配,位置/类型随蓝图)----
    WeaponMount = CreateDefaultSubobject<UOCWeaponMountComponent>(TEXT("WeaponMount"));

    // ---- 默认移动参数(子类/蓝图可覆盖)----
    ForwardThrust = 50000.0f;     // N
    ReverseThrust = 30000.0f;     // N
    TurnTorque = 5.0f;            // 度/秒²(AddTorqueInDegrees 语义),后续调试
    MaxForwardSpeed = 1500.0f;    // cm/s(= 15 m/s)
    LinearDamping = 0.5f;
    AngularDamping = 2.0f;

    // 关键:船的朝向由物理决定,不要让 Controller 直接控制旋转
    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
}

void AOCBoatBase::BeginPlay()
{
    Super::BeginPlay();

    // 应用阻尼(蓝图可能修改了参数)
    if (BoatMesh)
    {
        BoatMesh->SetLinearDamping(LinearDamping);
        BoatMesh->SetAngularDamping(AngularDamping);

        // 下压质心:让"倒扣"变成不稳定平衡,浮力会自己把船推离肚皮朝天状态。
        // 翻船能否自动回正的物理根因(纯靠回正力矩打不赢浮力反扭矩)。
        if (bEnableSelfRighting && !CenterOfMassOffset.IsNearlyZero())
        {
            BoatMesh->SetCenterOfMass(CenterOfMassOffset);
        }
    }

    // 快照蓝图配好的移动参数,作为升级加成的基准(必须在任何 ApplyStatBonus 之前)
    BaseForwardThrust = ForwardThrust;
    BaseReverseThrust = ReverseThrust;
    BaseTurnTorque = TurnTorque;
    BaseMaxForwardSpeed = MaxForwardSpeed;
}

void AOCBoatBase::ApplyStatBonus(const FOCShipStatBonus& Bonus)
{
    // 血量与炮弹伤害交给基类
    Super::ApplyStatBonus(Bonus);

    MaxForwardSpeed = FMath::Max(0.0f, BaseMaxForwardSpeed + Bonus.MoveSpeed);
    TurnTorque = FMath::Max(0.0f, BaseTurnTorque + Bonus.TurnSpeed);

    // MaxForwardSpeed 在 ApplyMovementPhysics 里只是个限速判据,实际加速度来自推力与阻尼的平衡。
    // 只抬上限不抬推力,船到不了新的速度上限,所以推力按限速的比例同步放大。
    const float SpeedRatio = BaseMaxForwardSpeed > 0.0f ? MaxForwardSpeed / BaseMaxForwardSpeed : 1.0f;
    ForwardThrust = BaseForwardThrust * SpeedRatio;
    ReverseThrust = BaseReverseThrust * SpeedRatio;
}

float AOCBoatBase::GetUpgradableStatValue(EOCShipUpgradeType Type) const
{
    switch (Type)
    {
    case EOCShipUpgradeType::MoveSpeed: return MaxForwardSpeed;
    case EOCShipUpgradeType::TurnSpeed: return TurnTorque;
    default:                            return Super::GetUpgradableStatValue(Type);
    }
}

float AOCBoatBase::EstimateTurnRadius(float SpeedCmS) const
{
    // 稳态角速度:角加速度(TurnTorque) 与 角阻尼 平衡后的结果
    const float OmegaDegPerSec = (AngularDamping > KINDA_SMALL_NUMBER)
        ? TurnTorque / AngularDamping
        : 0.0f;
    if (OmegaDegPerSec <= KINDA_SMALL_NUMBER)
    {
        return UE_BIG_NUMBER; // 转不过来,调用方会被 MaxLookAhead 夹住
    }

    const float OmegaRadPerSec = FMath::DegreesToRadians(OmegaDegPerSec);
    return FMath::Abs(SpeedCmS) / OmegaRadPerSec;
}

void AOCBoatBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 死亡后不再施加移动力:死亡表现(UOCDestructionComponent)会隐藏船体并关碰撞,
    // 此时对无碰撞的船体调 AddForce/AddTorque 会触发引擎告警
    const UOCHealthComponent* Health = GetHealthComponent();
    if (Health && Health->IsDead())
    {
        ThrottleInput = 0.0f;
        SteerInput = 0.0f;
        return;
    }

    ApplyMovementPhysics();

    // 侧倾/回正独立于推进转向,须在清零 SteerInput 之前调用(侧倾要读它)
    ApplyRollAndRighting();

    // 应急上浮:船沉出水体碰撞体范围后官方浮力失效(active=0),没有上浮力会永久沉底。
    // 一旦"明显在水面下 + 浮力失效",立刻把下沉速度抬成上浮速度,顶回水体让官方浮力重新接管。
    // (死船在 Tick 顶部已提前 return,不会走到这里,照常下沉沉底。)
    if (bEnableEmergencyResurface && BuoyancyComp && !BuoyancyComp->IsActive())
    {
        if (GetActorLocation().Z < WaterSurfaceZ - ResurfaceTriggerDepth)
        {
            FVector Vel = BoatMesh->GetPhysicsLinearVelocity();
            Vel.Z = FMath::Max(Vel.Z, ResurfaceRiseSpeed);
            BoatMesh->SetPhysicsLinearVelocity(Vel);
        }
    }

    // 输入消费后清零(避免下次没有输入时还在推进)
    ThrottleInput = 0.0f;
    SteerInput = 0.0f;
}

void AOCBoatBase::AddThrottleInput(float ThrottleValue)
{
    ThrottleInput = FMath::Clamp(ThrottleValue, -1.0f, 1.0f);
}

void AOCBoatBase::AddSteerInput(float SteerValue)
{
    SteerInput = FMath::Clamp(SteerValue, -1.0f, 1.0f);
}

void AOCBoatBase::ApplyMovementPhysics()
{
    if (!BoatMesh)
    {
        return;
    }

    // 没有输入就不施力(让阻尼自然把船停下来)
    if (FMath::IsNearlyZero(ThrottleInput) && FMath::IsNearlyZero(SteerInput))
    {
        return;
    }

    // 当前物理速度,投影到船头方向,得到"前进方向的速度分量"(可正可负)
    // GetPhysicsLinearVelocity 返回 cm/s
    const FVector VelocityCmS = BoatMesh->GetPhysicsLinearVelocity();
    const float ForwardSpeedCmS = FVector::DotProduct(VelocityCmS, GetActorForwardVector());
    const float ForwardSpeedMs = ForwardSpeedCmS / 100.0f;
    const float MaxSpeedMs = MaxForwardSpeed / 100.0f;

    // ---- 推进力:沿船头方向(GetActorForwardVector)+X)----
    if (!FMath::IsNearlyZero(ThrottleInput))
    {
        const bool bOverLimit = FMath::Abs(ForwardSpeedMs) >= MaxSpeedMs;
        // 没超速,或超速了但想减速(反向输入),都允许施力
        const bool bSameSign = FMath::Sign(ThrottleInput) == FMath::Sign(ForwardSpeedCmS);
        if (!bOverLimit || !bSameSign)
        {
            // 推力:正油门用 ForwardThrust,负油门用 ReverseThrust(后退通常弱一些)
            const float Thrust = (ThrottleInput >= 0.0f) ? ForwardThrust : ReverseThrust;
            // 力 = 沿船头方向 * 推力大小 * 油门符号(负号表示后退方向)
            const float ForceMag = Thrust * ThrottleInput;
            const FVector Force = GetActorForwardVector() * ForceMag;
            BoatMesh->AddForce(Force);
        }
    }

    // ---- 转向力矩:绕 Z 轴(UpVector)----
    if (!FMath::IsNearlyZero(SteerInput))
    {
        const float DirectionSign = (ForwardSpeedCmS < 0.0f) ? -1.0f : 1.0f;

        const FVector Torque = FVector(0.0f, 0.0f, TurnTorque * SteerInput * DirectionSign);
        BoatMesh->AddTorqueInDegrees(Torque, NAME_None, /*bAccelChange=*/true);
    }
}

void AOCBoatBase::ApplyRollAndRighting()
{
    if (!BoatMesh || (!bEnableTurnRoll && !bEnableSelfRighting))
    {
        return;
    }

    const FVector ForwardAxis = GetActorForwardVector();
    const FVector UpAxis = GetActorUpVector();
    // 世界系角速度(度/秒),用于两个 PD 的阻尼项
    const FVector AngVelDeg = BoatMesh->GetPhysicsAngularVelocityInDegrees();

    // 倾斜量 + 门控:大幅倾斜时把主导权从"侧倾"平滑交给"回正",避免两者互相打架
    const float CosTilt = FVector::DotProduct(UpAxis, FVector::UpVector);
    const float TiltAngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosTilt, -1.0f, 1.0f)));
    const float RightGate = bEnableSelfRighting
        ? FMath::SmoothStep(RightingThresholdDeg, 90.0f, TiltAngleDeg)
        : 0.0f;

    FVector TotalTorque = FVector::ZeroVector;

    // ---- 转向侧倾:绕前方轴的 PD(仅在小倾角有效,大倾角淡出交给回正)----
    if (bEnableTurnRoll && RightGate < 1.0f)
    {
        const FVector VelocityCmS = BoatMesh->GetPhysicsLinearVelocity();
        const float ForwardSpeedCmS = FVector::DotProduct(VelocityCmS, ForwardAxis);
        const float SpeedFactor = FMath::Clamp(FMath::Abs(ForwardSpeedCmS) / BankRefSpeedCmS, 0.0f, 1.0f);

        // 向转弯内侧倒:右转(SteerInput>0) => 右舷下沉 => 目标 Roll 取正。
        // 若实机方向相反,勾 bInvertBankDirection 翻符号。
        const float BankSign = bInvertBankDirection ? -1.0f : 1.0f;
        const float TargetRollDeg = BankSign * MaxBankAngleDeg * SteerInput * SpeedFactor;

        const float CurrentRollDeg = GetActorRotation().Roll;
        const float RollRateDeg = FVector::DotProduct(AngVelDeg, ForwardAxis);

        // 关键:绕 +ForwardAxis 的力矩会让 UE 的 Roll *减小*(右舷抬升),
        // 所以刚度项取负号才是负反馈(否则正反馈会越转越狠直到翻船);
        // 阻尼项 -Kd*RollRate 直接压制物理角速度,与手性无关,恒为负反馈。
        const float RollAccel = -BankStiffness * (TargetRollDeg - CurrentRollDeg) - BankDamping * RollRateDeg;
        TotalTorque += ForwardAxis * (RollAccel * (1.0f - RightGate));
    }

    // ---- 翻船回正:Up 向量叉乘法,倾斜超阈值才逐渐生效 ----
    if (RightGate > KINDA_SMALL_NUMBER)
    {
        // 叉乘给出把船头 Up 转回世界 Up 的旋转轴
        const FVector CrossAxis = FVector::CrossProduct(UpAxis, FVector::UpVector);

        // 接近倒扣(±180°)时叉乘轴趋于零、方向随抖动乱漂,船只会原地晃不肯翻过去。
        // 叠加一个绕前方轴的固定方向"掀翻"分量:前方轴永远稳定,绕它翻滚正是
        // 180°->0° 翻正的自然方向,给一个确定的翻越侧,打破倒扣平衡点。
        const float FlipBias = FMath::SmoothStep(120.0f, 178.0f, TiltAngleDeg);
        FVector RightAxis = (CrossAxis + ForwardAxis * FlipBias).GetSafeNormal();
        if (RightAxis.IsNearlyZero())
        {
            RightAxis = ForwardAxis;
        }

        const float RightRateDeg = FVector::DotProduct(AngVelDeg, RightAxis);
        const float RightAccel = RightingStrength * RightGate - RightingDamping * RightRateDeg;
        TotalTorque += RightAxis * RightAccel;
    }

    if (!TotalTorque.IsNearlyZero())
    {
        BoatMesh->AddTorqueInDegrees(TotalTorque, NAME_None, /*bAccelChange=*/true);
    }
}
