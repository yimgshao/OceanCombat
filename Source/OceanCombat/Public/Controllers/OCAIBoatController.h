// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controllers/OCAIControllerBase.h"
#include "Engine/EngineTypes.h"
#include "OCAIBoatController.generated.h"

/**
 * 敌方船 AI:侧舷阵线来回巡弋 + 避障。
 *
 * 战斗(索敌/瞄准/开火)完全复用基类 AOCAIControllerBase::Tick;本类只追加移动:
 * 把船约束在"以玩家为圆心、理想交战距离为半径"圆环上的一段弧内(位于玩家的敌方一侧),
 * 在弧内沿切向左右摆动,到弧端就掉头 —— 侧舷对敌、来回荡、不绕整圈、不贴脸。
 * 每帧合成三个意图:切向摆动(主导) + 弧段约束 + 径向修正(距离保持),
 * 再经须状射线避障融合,最后转成油门/转向喂给 AOCBoatBase。
 */
UCLASS()
class OCEANCOMBAT_API AOCAIBoatController : public AOCAIControllerBase
{
    GENERATED_BODY()

public:
    virtual void Tick(float DeltaTime) override;
    virtual void OnPossess(APawn* InPawn) override;

protected:
    // ---- 巡弋参数 ----
    /**
     * 激活距离(cm):玩家进入此距离内才开始移动。
     * 每个聚落都会生成敌船,不设限的话全地图的船会一起朝玩家开过来。
     * 应显著大于 AttackRange,让船有接近的过程,而不是一进射程就凭空启动。
     */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Move", meta = (ClampMin = "0.0"))
    float ActivationDistance = 15000.0f;

    /**
     * 停止距离(cm):已激活的船,玩家跑出此距离才重新静止。
     * 必须大于 ActivationDistance —— 两者的差值就是滞回带,防止船在边界反复启停抽搐。
     */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Move", meta = (ClampMin = "0.0"))
    float DeactivationDistance = 20000.0f;

    /** 理想交战距离(cm),须落在 [MinAttackRange, AttackRange] 内 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Move", meta = (ClampMin = "0.0"))
    float PreferredDist = 3200.0f;

    /** 径向修正满量程对应的距离误差(cm):|Dist-PreferredDist| 达到它时径向修正最强 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Move", meta = (ClampMin = "1.0"))
    float RadialScale = 800.0f;

    /** 扇区半角(度):本船相对玩家的角位置超出 ±ArcHalfAngle 就掉头,决定来回摆动的幅度 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Move", meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float ArcHalfAngle = 50.0f;

    /** 接近→交战的过渡带宽(cm):Dist 从 AttackRange 到 AttackRange+该值之间,行为由"朝玩家直冲"平滑过渡到"左右摇摆" */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Move", meta = (ClampMin = "1.0"))
    float ApproachBlendRange = 1500.0f;

    /** 接近(玩家在射程外)时保留的切向摆动比例(0=笔直冲向玩家,1=和交战时一样摇摆),给点小值让接近更自然 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Move", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ApproachWobble = 0.2f;

    /** 切向(左右摆)权重,应显著大于径向权重以保证"以横荡为主、不正面冲" */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Move", meta = (ClampMin = "0.0"))
    float TangentWeight = 1.0f;

    /** 径向(距离保持)权重 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Move", meta = (ClampMin = "0.0"))
    float RadialWeight = 0.5f;

    /** 满舵对应的航向误差(度):|YawError| 超过它就满舵 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Move", meta = (ClampMin = "1.0"))
    float SteerFullAngle = 40.0f;

    /** 最大油门(0~1) */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Move", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ThrottleMax = 1.0f;

    /** 大角度转向时的最小油门系数(0~1):避免原地打转、保持一点推进以便转头 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Move", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ThrottleMinFactor = 0.15f;

    // ---- 避障参数 ----
    /** 前视时间(秒):前视距离 = 速度 × 该值,再 Clamp 到 [Min,Max]LookAhead */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid", meta = (ClampMin = "0.0"))
    float ReactionTime = 1.5f;

    /**
     * 最小前视距离(cm)。
     * 注意别设太大:它会盖过按转弯半径算出的前视值。实测船的巡航速度约 350cm/s,
     * 对应转弯半径仅约 850cm,若 MinLookAhead 设成 1600 则转弯半径那一项永远不起作用。
     */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid", meta = (ClampMin = "0.0"))
    float MinLookAhead = 1200.0f;

    /** 最大前视距离(cm) */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid", meta = (ClampMin = "0.0"))
    float MaxLookAhead = 8000.0f;

    /**
     * 前视距离至少覆盖 转弯半径 × 此系数。
     * 船的转弯半径 = 速度 / (TurnTorque/AngularDamping),默认参数下满速约 3800cm,
     * 远大于"速度×反应时间"算出的 2400cm —— 只按后者会让船发现障碍时已转不开。
     * 1.5 表示留半个转弯半径的余量。
     */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid", meta = (ClampMin = "1.0"))
    float TurnRadiusLookAheadFactor = 1.5f;

    /** 避障球扫半径(cm),约等于船体半宽 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid", meta = (ClampMin = "0.0"))
    float AvoidProbeRadius = 300.0f;

    /** 正前方障碍近于该距离(cm)则紧急减速 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid", meta = (ClampMin = "0.0"))
    float EmergencyDist = 500.0f;

    /**
     * 排斥强度:障碍对期望方向的推离权重。
     *
     * 避障是连续力场而非"挡住才转":每条须按 (1 - 通透/前视)² 产生一个背离障碍的分量,
     * 远处就开始轻推、越近越强。这样船在离岸很远时就已经在偏航,而不是贴上去才急转。
     * 越大越早远离海岸,过大则难以接近岸边的玩家。
     */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid", meta = (ClampMin = "0.0"))
    float AvoidStrength = 2.5f;

    /**
     * 贴近障碍时保留的最低油门系数(0~1)。
     *
     * 关键:不能在岸边把油门降到 0 —— 船的转向依赖航速产生的水动力,
     * 零油门等于失去舵效,船会顶着岸转不开(实测 Thr=0.00 时永久卡住)。
     * 所以即使前方很近也要保留推进力,靠"转"而不是靠"停"来脱离。
     */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinSteeringThrottle = 0.45f;

    /** 避障射线通道。v1 用 WorldStatic(命中岛屿/建筑;忽略自身与玩家);后续可切专用 Obstacle 通道 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid")
    TEnumAsByte<ECollisionChannel> ObstacleChannel = ECC_WorldStatic;

    // ---- 浅滩规避 ----
    /**
     * 是否规避浅滩。
     * 必须单独检测:海床在船的下方,船体高度的水平球扫会从浅滩正上方掠过完全不命中,
     * AI 于是认为前方畅通、全速冲上去搁浅。开启后沿每条须向下打线量水深。
     */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid")
    bool bAvoidShallowWater = true;

    /**
     * 龙骨最小水深(cm):船底到海床的安全余量。
     * 采样点的水深低于此值即视为障碍。调大 = 更早远离海岸,调小 = 敢贴岸但可能搁浅。
     */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid", meta = (ClampMin = "0.0"))
    float MinKeelClearance = 400.0f;

    /** 每条须上的水深采样点数(沿前视距离等分)。越多越可靠但射线越多 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid", meta = (ClampMin = "1", ClampMax = "16"))
    int32 ShallowProbeSamples = 6;

    /** 测深线检测的向下长度(cm):超过此深度未命中即视为深水安全 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid", meta = (ClampMin = "1.0"))
    float ShallowProbeDownLength = 2000.0f;

    /** 测深线起点相对水面的抬高量(cm):船随浪起伏,抬高后基准稳定,也能量到露出水面的低矮小岛 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Avoid", meta = (ClampMin = "0.0"))
    float ShallowProbeStartHeight = 1000.0f;

    // ---- 调试 ----
    /** 开启后画期望方向/须射线/扇区,并按 DebugLogInterval 打印埋点日志 */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Debug")
    bool bDrawDebug = false;

    /** 埋点日志间隔(秒) */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Debug", meta = (ClampMin = "0.0"))
    float DebugLogInterval = 0.25f;

private:
    /** 巡逻锚点(敌方半场参考点),OnPossess 时抓出生点 */
    FVector HomeAnchor = FVector::ZeroVector;
    bool bHasHomeAnchor = false;

    /** 是否已激活(玩家进过 ActivationDistance)。带滞回:激活后要跑出 DeactivationDistance 才熄火 */
    bool bActivated = false;

    /** 更新激活状态(滞回判定),返回本帧是否应该移动 */
    bool UpdateActivation(float DistToTarget);

    /**
     * 把关卡内的水体收进射线忽略列表(惰性收集一次)。
     *
     * 必须忽略:船泡在水体内部,球扫起点就在水面碰撞体里,水体会成为距离 0 的首个阻挡命中,
     * 导致射线止步于此、永远扫不到前方的岛屿 —— AI 于是认为畅通并全速撞岛。
     */
    void EnsureWaterIgnoreList();

    /** 水体 Actor(射线要忽略的);惰性收集,弱引用防悬垂 */
    TArray<TWeakObjectPtr<AActor>> IgnoredWaterActors;
    bool bWaterIgnoreListReady = false;

    /** 弧内摆动方向(+1/-1),到扇区边界翻转 */
    int32 SweepDir = 1;

    /** 计算期望巡弋方向(切向摆动 + 弧段约束 + 径向修正);同时更新调试量。AttackRange 用于接近/交战分档 */
    FVector ComputeDesiredDir(const FVector& SelfLoc, const FVector& TargetLoc, float AttackRange);

    /**
     * 须状射线避障:把各须的通透距离转成连续排斥场,叠加到期望方向上。
     * @param bOutEmergencyBrake 正前方极近障碍,需减速(但下游仍保留 MinSteeringThrottle)
     */
    FVector ApplyAvoidance(const FVector& DesiredDir, const FVector& SelfLoc,
        AActor* Target, bool& bOutEmergencyBrake);

    /**
     * 沿一条须测水深:从须上若干采样点向下打线找海床,返回第一个"水深不足"处的距离。
     * 无浅滩返回 MaxDist。水平球扫查不到水下地形,浅滩只能这样查。
     * @param WaterZ  水面高度(取船当前 Z,船浮在水面上)
     */
    float MeasureShallowClearance(const FVector& Origin, const FVector& Dir, float MaxDist,
        float WaterZ, AActor* SelfPawn) const;

    /** 绕 +Z 从 A 到 B 的带符号夹角(度,-180~180),只取 XY */
    static float SignedAngleDegXY(const FVector& A, const FVector& B);

    // ---- 调试运行时缓存 ----
    float DebugTimeAccum = 0.0f;   // 日志节流累加
    float PrevYaw = 0.0f;          // 上帧船头 Yaw,用于估算实际转向角速度
    bool bHasPrevYaw = false;
    float DebugLastTheta = 0.0f;   // 上次 ComputeDesiredDir 的 theta
    float DebugLastDist = 0.0f;    // 上次 ComputeDesiredDir 的 Dist
};
