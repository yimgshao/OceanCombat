#pragma once

#include "CoreMinimal.h"
#include "Pawns/OCPawnBase.h"
#include "OCBoatBase.generated.h"

class UStaticMeshComponent;
class UBuoyancyComponent;
class UOCWeaponMountComponent;

/**
 * 船基类。负责船的物理基础:船体 Mesh + 官方浮力组件 + 移动输入接口。
 * 不区分玩家/AI——输入由外部(Controller)通过 AddThrottleInput/AddSteerInput 注入。
 *
 * 物理驱动:
 * - BoatMesh 为 RootComponent,SimulatePhysics=true
 * - Tick 里把 ThrottleInput 转成沿船头方向的推力(AddForce)
 * - Tick 里把 SteerInput 转成绕 Z 轴的转向力矩(AddTorqueInDegrees)
 *
 * 浮力:
 * - 用官方 UBuoyancyComponent,具体 pontoon 配置在蓝图里做
 * - C++ 不硬编码 pontoon 位置(每艘船形状不同)
 */
UCLASS(Abstract)
class OCEANCOMBAT_API AOCBoatBase : public AOCPawnBase
{
    GENERATED_BODY()

public:
    AOCBoatBase();

    // ---- 输入接口(给 Controller/AI 调用)----
    /** 油门输入:-1(全倒退) ~ +1(全前进),每帧持续调用 */
    UFUNCTION(BlueprintCallable, Category = "Boat|Input")
    void AddThrottleInput(float ThrottleValue);

    /** 转向输入:-1(左满舵) ~ +1(右满舵),每帧持续调用 */
    UFUNCTION(BlueprintCallable, Category = "Boat|Input")
    void AddSteerInput(float SteerValue);

    /** 除基类的血量/伤害外,再处理航速与转向(同样从基础值重算,幂等) */
    virtual void ApplyStatBonus(const FOCShipStatBonus& Bonus) override;

    virtual float GetUpgradableStatValue(EOCShipUpgradeType Type) const override;

    /**
     * 估算给定速度下的转弯半径(cm),供 AI 决定前视距离 —— 前视必须覆盖转弯半径,
     * 否则发现障碍时物理上已经转不开。
     *
     * 推导:AddTorqueInDegrees(bAccelChange=true) 给的是角加速度,与角阻尼平衡后的
     * 稳态角速度 ω = TurnTorque / AngularDamping(度/秒);半径 R = v / ω(ω 转成弧度)。
     * 角阻尼为 0 时无稳态解,返回一个很大的值表示"几乎转不过来"。
     */
    UFUNCTION(BlueprintPure, Category = "Boat|Movement")
    float EstimateTurnRadius(float SpeedCmS) const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ---- 组件 ----
    /** 船体物理 Mesh(也是 RootComponent) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat")
    TObjectPtr<UStaticMeshComponent> BoatMesh;

    /** 官方浮力组件,pontoon 在蓝图里配置 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat|Buoyancy")
    TObjectPtr<UBuoyancyComponent> BuoyancyComp;

    /** 武器挂载组件:炮塔挂载点(Child Actor Component)在蓝图组件树里配,数量/类型随蓝图 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<UOCWeaponMountComponent> WeaponMount;

    // ---- 移动参数(可在蓝图/Details 调)----
    /** 前进最大推力(牛顿) */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Movement", meta = (ClampMin = "0.0"))
    float ForwardThrust;

    /** 倒退最大推力(牛顿) */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Movement", meta = (ClampMin = "0.0"))
    float ReverseThrust;

    /** 转向最大角加速度(度/秒²,AddTorqueInDegrees 语义) */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Movement", meta = (ClampMin = "0.0"))
    float TurnTorque;

    /** 最大前进速度(m/s,用于限速) */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Movement", meta = (ClampMin = "0.0"))
    float MaxForwardSpeed;

    /** 线性阻尼(水阻力) */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Movement", meta = (ClampMin = "0.0"))
    float LinearDamping;

    /** 角阻尼(转向阻力) */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Movement", meta = (ClampMin = "0.0"))
    float AngularDamping;

    // ---- 转向侧倾(Bank)----
    /** 是否启用转向侧倾 */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Roll")
    bool bEnableTurnRoll = true;

    /** 满舵+满速时的目标横滚角(度) */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Roll", meta = (ClampMin = "0.0"))
    float MaxBankAngleDeg = 12.0f;

    /** 达到满侧倾所需的参考速度(cm/s):船速越接近它,侧倾越接近 MaxBankAngleDeg */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Roll", meta = (ClampMin = "1.0"))
    float BankRefSpeedCmS = 1000.0f;

    /** 侧倾 PD 刚度(角加速度/度) */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Roll", meta = (ClampMin = "0.0"))
    float BankStiffness = 8.0f;

    /** 侧倾 PD 阻尼(角加速度/(度/秒)) */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Roll", meta = (ClampMin = "0.0"))
    float BankDamping = 3.0f;

    /** 侧倾方向反转:默认向转弯内侧倒,若实机方向相反则勾选 */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Roll")
    bool bInvertBankDirection = false;

    // ---- 翻船回正(Self-Righting)----
    /** 是否启用翻船回正 */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Righting")
    bool bEnableSelfRighting = true;

    /**
     * 质心相对物理默认质心的偏移(cm,船体本地坐标)。Z 取负 = 把质心下压向龙骨。
     *
     * 这是翻船能否自动回正的物理根因:降低质心让"正浮"更稳,同时把"倒扣"从
     * 稳定平衡变成不稳定平衡 —— 浮力会自己把船推离肚皮朝天状态,回正力矩只需轻推。
     * 单纯靠回正力矩打不赢浮力的反扭矩(实测被抵消),必须靠这个。
     *
     * 按船体高度调:太小(接近0)倒扣仍稳、翻不回来;过大会削弱转向侧倾手感并可能吃水异常。
     * 建议从船体半高的负值起调,一边翻船一边加大绝对值直到能自动回正。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Righting")
    FVector CenterOfMassOffset = FVector(0.0f, 0.0f, -100.0f);

    /** 回正启动阈值(度):倾斜超过它才逐渐介入,避免干扰正常波浪起伏 */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Righting", meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float RightingThresholdDeg = 55.0f;

    /** 回正力矩强度(角加速度):须大于翻覆时浮力的反扭矩才能翻过来,不够就调大 */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Righting", meta = (ClampMin = "0.0"))
    float RightingStrength = 80.0f;

    /** 回正阻尼(角加速度/(度/秒)) */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Righting", meta = (ClampMin = "0.0"))
    float RightingDamping = 5.0f;

    // ---- 应急上浮(防止沉出水体后官方浮力失效导致永久沉底)----
    /** 是否启用应急上浮 */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Buoyancy")
    bool bEnableEmergencyResurface = true;

    /** 水面高度(cm,世界 Z),一般等于海平面 */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Buoyancy")
    float WaterSurfaceZ = 0.0f;

    /**
     * 触发深度(cm):船低于"水面 - 该值"且官方浮力已失效(脱离水体碰撞体)时,
     * 判定为异常沉没并强制上浮。设得够大以免正常波谷/出水瞬间误触发。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Buoyancy", meta = (ClampMin = "0.0"))
    float ResurfaceTriggerDepth = 200.0f;

    /** 应急上浮速度(cm/s):强制把下沉速度抬到该值,平稳顶回水面 */
    UPROPERTY(EditDefaultsOnly, Category = "Boat|Buoyancy", meta = (ClampMin = "0.0"))
    float ResurfaceRiseSpeed = 400.0f;

private:
    // 当前帧的输入值,由 AddXxxInput 设置,Tick 读取
    float ThrottleInput = 0.0f;
    float SteerInput = 0.0f;

    /** 蓝图默认移动参数的快照(BeginPlay 缓存),升级加成的算式基准 */
    float BaseForwardThrust = 0.0f;
    float BaseReverseThrust = 0.0f;
    float BaseTurnTorque = 0.0f;
    float BaseMaxForwardSpeed = 0.0f;

    /** 把输入转成力/力矩施加到 BoatMesh */
    void ApplyMovementPhysics();

    /**
     * 每帧施加转向侧倾力矩 + 翻船回正力矩。
     * 独立于推进/转向:即使无输入也运行(转弯后拉回水平、翻船后扶正)。
     */
    void ApplyRollAndRighting();
};