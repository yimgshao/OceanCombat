// OceanCombat. Copyright(c) All rights reserved.

#include "Controllers/OCAIBoatController.h"

#include "Components/OCHealthComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Pawns/Boats/OCBoatBase.h"
#include "WaterBodyActor.h"

float AOCAIBoatController::SignedAngleDegXY(const FVector& A, const FVector& B)
{
    // 绕 +Z:cross.Z = A.X*B.Y - A.Y*B.X,dot = A·B(仅 XY)
    const float Cross = A.X * B.Y - A.Y * B.X;
    const float Dot = A.X * B.X + A.Y * B.Y;
    return FMath::RadiansToDegrees(FMath::Atan2(Cross, Dot));
}

void AOCAIBoatController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // 抓出生点作为敌方半场巡逻锚点(扇区中心方向 = 玩家看向该点的方向)
    if (InPawn)
    {
        HomeAnchor = InPawn->GetActorLocation();
        bHasHomeAnchor = true;
    }
}

bool AOCAIBoatController::UpdateActivation(float DistToTarget)
{
    // 滞回:未激活时看 ActivationDistance,已激活时看 DeactivationDistance。
    // 两个阈值不同,船就不会在单一边界上反复启停抽搐。
    // 配置填反时(Deactivation <= Activation)取 Activation 兜底,保证滞回带非负。
    const float StopDist = FMath::Max(DeactivationDistance, ActivationDistance);

    if (bActivated)
    {
        if (DistToTarget > StopDist)
        {
            bActivated = false;
        }
    }
    else if (DistToTarget <= ActivationDistance)
    {
        bActivated = true;
    }

    return bActivated;
}

FVector AOCAIBoatController::ComputeDesiredDir(const FVector& SelfLoc, const FVector& TargetLoc, float AttackRange)
{
    // ---- 基础量(全部投影到 XY 平面)----
    FVector ToPlayer = TargetLoc - SelfLoc;
    ToPlayer.Z = 0.0f;
    const float Dist = ToPlayer.Size();
    DebugLastDist = Dist;

    // 玩家→本船 方位(向外)。距离退化时无有效方向
    FVector BoatDir = (SelfLoc - TargetLoc);
    BoatDir.Z = 0.0f;
    BoatDir = BoatDir.GetSafeNormal();
    if (BoatDir.IsNearlyZero())
    {
        return FVector::ZeroVector;
    }

    // 扇区中心方向 = 玩家看向敌方 home 锚点的方向;锚点缺失/与玩家重合时退化为当前方位
    FVector AnchorFromPlayer = (bHasHomeAnchor ? HomeAnchor : SelfLoc) - TargetLoc;
    AnchorFromPlayer.Z = 0.0f;
    FVector ArcCenterDir = AnchorFromPlayer.GetSafeNormal();
    if (ArcCenterDir.IsNearlyZero())
    {
        ArcCenterDir = BoatDir;
    }

    // ---- 弧内来回摆(核心)----
    const float Theta = SignedAngleDegXY(ArcCenterDir, BoatDir);
    DebugLastTheta = Theta;
    if (Theta >= ArcHalfAngle)
    {
        SweepDir = -1;   // 荡到一端,掉头
    }
    else if (Theta <= -ArcHalfAngle)
    {
        SweepDir = 1;    // 荡到另一端,掉头
    }
    const FVector Tangent(-BoatDir.Y, BoatDir.X, 0.0f);   // 垂直连线的切向

    // 接近/交战分档:玩家在射程外时以"朝玩家直冲"为主(切向弱,留点摆动更自然),
    // 进入射程带后切向恢复满权重、变回"左右摇摆保持侧舷阵线"。
    // Engage: 1=已进入射程(Dist<=AttackRange),0=远(Dist>=AttackRange+ApproachBlendRange),中间线性过渡
    const float Engage = FMath::Clamp(1.0f - (Dist - AttackRange) / ApproachBlendRange, 0.0f, 1.0f);
    const float TangentScale = FMath::Lerp(ApproachWobble, 1.0f, Engage);
    const FVector TangentComp = Tangent * static_cast<float>(SweepDir) * TangentScale;

    // ---- 径向修正(距离保持)----
    const float RadialError = Dist - PreferredDist;
    const float RadialScalar = FMath::Clamp(RadialError / RadialScale, -1.0f, 1.0f);
    // 太远(Error>0)→ -BoatDir 指向玩家(靠近);太近(Error<0)→ +BoatDir(退开)
    const FVector RadialComp = (-BoatDir) * RadialScalar;

    // ---- 合成 ----
    const FVector Desired = TangentComp * TangentWeight + RadialComp * RadialWeight;
    return Desired.GetSafeNormal();
}

void AOCAIBoatController::EnsureWaterIgnoreList()
{
    if (bWaterIgnoreListReady)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 无限海面只有一个 AWaterBodyCustom(由 AOCInfiniteOceanManager 平移复用),
    // 但这里按基类遍历,兼容后续加湖泊/河流等其它水体
    for (TActorIterator<AWaterBody> It(World); It; ++It)
    {
        IgnoredWaterActors.Add(*It);
    }

    bWaterIgnoreListReady = true;
}

float AOCAIBoatController::MeasureShallowClearance(const FVector& Origin, const FVector& Dir, float MaxDist,
    float WaterZ, AActor* SelfPawn) const
{
    const UWorld* World = GetWorld();
    if (!World || ShallowProbeSamples <= 0)
    {
        return MaxDist;
    }

    // 海床是 ProceduralMesh(BlockAll),向下打线一定能命中;
    // 只有真正的深水(海床在 ShallowProbeDownLength 之外)才会 miss。
    // 同样要忽略水体:测深线从水面出发,不忽略的话第一个命中就是水面自己。
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OCBoatDepthProbe), /*bTraceComplex=*/false, SelfPawn);
    for (const TWeakObjectPtr<AActor>& Weak : IgnoredWaterActors)
    {
        if (AActor* Water = Weak.Get())
        {
            Params.AddIgnoredActor(Water);
        }
    }

    // 从近到远采样:第一个不合格的点就是有效障碍距离
    for (int32 i = 1; i <= ShallowProbeSamples; ++i)
    {
        const float Dist = MaxDist * static_cast<float>(i) / static_cast<float>(ShallowProbeSamples);
        const FVector Sample = Origin + Dir * Dist;

        // 起点抬到船上方再往下打:船体随浪起伏(实测 Z 在 -42 ~ +15 之间跳),
        // 直接从船的 Z 出发会让"水面基准"抖动,量出的水深忽大忽小。
        const FVector TraceStart(Sample.X, Sample.Y, WaterZ + ShallowProbeStartHeight);
        const FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, ShallowProbeStartHeight + ShallowProbeDownLength);

        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ObstacleChannel.GetValue(), Params))
        {
            continue; // 打不到海床 = 比测深长度还深,安全
        }

        // 水深 = 水面到海床的垂直距离(以 WaterZ 为水面基准,不含抬高量)。
        // 负值表示海床高出水面(露出的小岛),同样会被判为障碍
        const float WaterDepth = WaterZ - Hit.ImpactPoint.Z;
        if (WaterDepth < MinKeelClearance)
        {
            return Dist;
        }
    }

    return MaxDist;
}

FVector AOCAIBoatController::ApplyAvoidance(const FVector& DesiredDir, const FVector& SelfLoc,
    AActor* Target, bool& bOutEmergencyBrake)
{
    bOutEmergencyBrake = false;

    UWorld* World = GetWorld();
    APawn* SelfPawn = GetPawn();
    if (!World || !SelfPawn)
    {
        return DesiredDir;
    }

    // 前视距离:必须覆盖物理转弯半径,否则"看到"障碍时已经转不开了。
    // 转弯半径 R = v / ω,其中稳态角速度 ω = TurnTorque / AngularDamping(度/秒)。
    // 默认参数下 R ≈ 3800cm,而单靠 速度×反应时间 只有 2400cm —— 这是撞岛的直接原因。
    const float Speed = SelfPawn->GetVelocity().Size2D();
    float LookAhead = FMath::Max(Speed * ReactionTime, MinLookAhead);
    if (const AOCBoatBase* SelfBoat = Cast<AOCBoatBase>(SelfPawn))
    {
        const float TurnRadius = SelfBoat->EstimateTurnRadius(Speed);
        LookAhead = FMath::Max(LookAhead, TurnRadius * TurnRadiusLookAheadFactor);
    }
    LookAhead = FMath::Min(LookAhead, MaxLookAhead);

    // 须的角度偏移(相对 DesiredDir):中间 + 左右两档
    static const float FeelerAngles[] = { 0.0f, 25.0f, -25.0f, 50.0f, -50.0f };

    const FCollisionShape Probe = FCollisionShape::MakeSphere(AvoidProbeRadius);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OCBoatAvoid), /*bTraceComplex=*/false, SelfPawn);
    if (Target)
    {
        Params.AddIgnoredActor(Target);   // 玩家是攻击目标,不当障碍
    }

    // 忽略水体:否则球扫起点就在水面碰撞体内,水体成为距离 0 的首个阻挡命中,
    // 射线止步于此永远扫不到前方岛屿(实测每条须都命中 WaterBodyCustom@0)
    EnsureWaterIgnoreList();
    for (const TWeakObjectPtr<AActor>& Weak : IgnoredWaterActors)
    {
        if (AActor* Water = Weak.Get())
        {
            Params.AddIgnoredActor(Water);
        }
    }

    float BestClearance = -1.0f;          // 最空须的通透距离(被围死时的兜底方向用)
    FVector BestDir = DesiredDir;         // 最空须的方向
    bool bAnyHit = false;
    float CenterHitDist = TNumericLimits<float>::Max();

    // 每条须的结果,供下面累加排斥场
    struct FFeelerResult
    {
        FVector Dir;
        float Clearance;
    };
    TArray<FFeelerResult, TInlineAllocator<8>> Feelers;

    for (const float AngleOffset : FeelerAngles)
    {
        const FVector Dir = DesiredDir.RotateAngleAxis(AngleOffset, FVector::UpVector);
        const FVector End = SelfLoc + Dir * LookAhead;

        FHitResult Hit;
        const bool bRawHit = World->SweepSingleByChannel(
            Hit, SelfLoc, End, FQuat::Identity, ObstacleChannel.GetValue(), Probe, Params);

        // 起点穿透必须忽略:海床是连续网格,从船底下穿过。半径 AvoidProbeRadius 的球体在浅水区
        // 必然与海床相交 → 距离恒为 0。但这不代表前方畅通,真正的地形判定靠下面的向下测深。
        const bool bHit = bRawHit && !Hit.bStartPenetrating;

        float Clearance = bHit ? Hit.Distance : LookAhead;

        // 地形(含浅滩与低矮小岛)一律靠测深判定:
        // 水平球扫在近岛浅水区会因起点穿透而失效,只有露出水面很高的物体才拦得住,
        // 所以测深是地形避障的主力,球扫只兜住建筑/礁石这类"高出水面的独立碰撞体"。
        if (bAvoidShallowWater)
        {
            const float ShallowDist = MeasureShallowClearance(SelfLoc, Dir, LookAhead, SelfLoc.Z, SelfPawn);
            Clearance = FMath::Min(Clearance, ShallowDist);
        }

        const bool bBlocked = Clearance < LookAhead;
        if (bBlocked)
        {
            bAnyHit = true;
            if (AngleOffset == 0.0f)
            {
                CenterHitDist = Clearance;
            }
        }
        if (Clearance > BestClearance)
        {
            BestClearance = Clearance;
            BestDir = Dir;
        }

        Feelers.Add({ Dir, Clearance });

        if (bDrawDebug)
        {
            // 实线画到实际通透距离处:浅滩挡住时线会提前变短变红,能直观看出是浅滩还是实体
            DrawDebugLine(World, SelfLoc, SelfLoc + Dir * Clearance,
                bBlocked ? FColor::Red : FColor::Green, false, 0.0f, 0, 4.0f);
        }
    }

    if (!bAnyHit)
    {
        return DesiredDir; // 全通透
    }

    // 连续排斥场:每条须按 (1 - 通透/前视)² 产生一个"背离该须方向"的分量并累加。
    // 与旧的"在期望方向和最空须之间二选一插值"相比,这里远处就开始轻推、越近越强,
    // 船在离岸很远时就已在偏航 —— 而不是贴上去才急转(那时低速无舵效,必然搁浅)。
    FVector Repulsion = FVector::ZeroVector;
    for (const FFeelerResult& Feeler : Feelers)
    {
        const float Normalized = FMath::Clamp(Feeler.Clearance / LookAhead, 0.0f, 1.0f);
        const float Weight = FMath::Square(1.0f - Normalized);
        Repulsion -= Feeler.Dir * Weight;
    }

    // 期望方向 + 排斥场。排斥为零时结果就是原期望方向
    FVector FinalDir = (DesiredDir + Repulsion * AvoidStrength).GetSafeNormal();

    // 全方向被围死(排斥互相抵消)时退回"最空的那条须",保证总有出路
    if (FinalDir.IsNearlyZero())
    {
        FinalDir = BestDir;
    }

    // 紧急减速只在正前方极近时触发,且下游会保留 MinSteeringThrottle ——
    // 绝不能真的降到 0 油门,否则失去舵效反而卡死在岸边
    if (CenterHitDist < EmergencyDist)
    {
        bOutEmergencyBrake = true;
    }

    return FinalDir;
}

void AOCAIBoatController::Tick(float DeltaTime)
{
    // 基类战斗循环:索敌 → 射程判定 → 弹道瞄准(炮塔 Yaw+Pitch 追踪) → 带散布开火
    Super::Tick(DeltaTime);

    AOCBoatBase* Boat = Cast<AOCBoatBase>(GetPawn());
    if (!Boat)
    {
        return;
    }

    // 自己死了就不再移动(与基类停火一致)
    const UOCHealthComponent* Health = Boat->GetHealthComponent();
    if (Health && Health->IsDead())
    {
        return;
    }

    AActor* Target = AcquireTarget();
    if (!Target)
    {
        return;
    }

    const FVector SelfLoc = Boat->GetActorLocation();
    const FVector TargetLoc = Target->GetActorLocation();

    // 激活判定:玩家太远就不动(水平距离)。
    // 每个聚落都生成敌船,不设限的话全地图的船会一起朝玩家开过来。
    // 不移动 = 不喂 AddThrottleInput/AddSteerInput:AOCBoatBase::Tick 每帧消费后清零,
    // 船会被阻尼自然停住并随浪漂浮,不需要额外的刹车逻辑。
    // 注意:炮塔开火不受此限(基类 Tick 已按 AttackRange 独立判定),
    // 所以静止的船若被玩家远程点到、且在射程内,仍会反击。
    const float DistToTarget = FVector2D(TargetLoc.X - SelfLoc.X, TargetLoc.Y - SelfLoc.Y).Size();
    if (!UpdateActivation(DistToTarget))
    {
        return;
    }

    // ① 期望巡弋方向
    const FVector DesiredDir = ComputeDesiredDir(SelfLoc, TargetLoc, Boat->AttackRange);
    if (DesiredDir.IsNearlyZero())
    {
        return;
    }

    // ② 避障融合
    bool bEmergencyBrake = false;
    const FVector FinalDir = ApplyAvoidance(DesiredDir, SelfLoc, Target, bEmergencyBrake);

    // ③ 方向 → 油门/转向
    FVector Forward = Boat->GetActorForwardVector();
    Forward.Z = 0.0f;
    Forward = Forward.GetSafeNormal();
    const float YawError = SignedAngleDegXY(Forward, FinalDir);

    const float SteerInput = FMath::Clamp(YawError / SteerFullAngle, -1.0f, 1.0f);

    // 油门:大角度转向时减速(便于转头),但**绝不降到 0**。
    // 船的转向依赖航速产生的水动力 —— 零油门等于失去舵效,会顶着岸转不开。
    // 之前那套"刹停 + 倒车脱困"正是因此陷入"撞岸→倒车→再撞"的来回抖动;
    // 现在靠 MinSteeringThrottle 保住舵效,由排斥场把船"推着转开",不再需要脱困状态机。
    const float TurnFactor = FMath::Clamp(1.0f - FMath::Abs(YawError) / 90.0f, ThrottleMinFactor, 1.0f);
    const float ThrottleFloor = ThrottleMax * MinSteeringThrottle;
    const float ThrottleInput = bEmergencyBrake
        ? ThrottleFloor
        : FMath::Max(ThrottleMax * TurnFactor, ThrottleFloor);

    Boat->AddSteerInput(SteerInput);
    Boat->AddThrottleInput(ThrottleInput);

    // ---- 调试埋点 ----
    if (bDrawDebug)
    {
        // 实际转向角速度(度/秒),用船头 Yaw 帧间差分估算
        const float CurYaw = Boat->GetActorRotation().Yaw;
        float YawRate = 0.0f;
        if (bHasPrevYaw && DeltaTime > KINDA_SMALL_NUMBER)
        {
            YawRate = FRotator::NormalizeAxis(CurYaw - PrevYaw) / DeltaTime;
        }
        PrevYaw = CurYaw;
        bHasPrevYaw = true;

        DrawDebugDirectionalArrow(GetWorld(), SelfLoc, SelfLoc + FinalDir * 1000.0f,
            120.0f, FColor::Cyan, false, 0.0f, 0, 6.0f);

        DebugTimeAccum += DeltaTime;
        if (DebugTimeAccum >= DebugLogInterval)
        {
            DebugTimeAccum = 0.0f;
            UE_LOG(LogTemp, Log,
                TEXT("[OCAIBoat] Dist=%.0f theta=%.1f Sweep=%d YawErr=%.1f Steer=%.2f Thr=%.2f YawRate=%.1f%s"),
                DebugLastDist, DebugLastTheta, SweepDir, YawError, SteerInput, ThrottleInput, YawRate,
                bEmergencyBrake ? TEXT(" [BRAKE]") : TEXT(""));
        }
    }
}
