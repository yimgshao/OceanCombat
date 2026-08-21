// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/OCCombatConfig.h"
#include "OCWeaponTurret.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class AOCProjectileBase;

/**
 * 炮塔武器。独立 Actor,通过 Child Actor 挂到船/建筑上。
 * 结构:底座(预留 Yaw) → BarrelPivot(铰接轴心,转 Pitch) → 炮管。
 * 本阶段只管挂载和旋转,瞄准输入和开火在后续阶段接入。
 */
UCLASS()
class OCEANCOMBAT_API AOCWeaponTurret : public AActor
{
    GENERATED_BODY()

public:
    AOCWeaponTurret();

    /** 设置炮管俯仰角(度),内部按 PitchLimit 做 Clamp */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Turret")
    void SetBarrelPitch(float Pitch);

    /** 在当前俯仰角基础上累加(度),内部按 PitchLimit 做 Clamp */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Turret")
    void AddBarrelPitch(float DeltaPitch);

    /** 获取当前炮管俯仰角(度) */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Turret")
    float GetBarrelPitch() const;

    /** 设置底座朝向(度,局部空间 Yaw),预留 */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Turret")
    void SetTurretYaw(float Yaw);

    /** 获取炮口世界位置(预留,Fire 时生成炮弹用) */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Turret")
    FVector GetMuzzleLocation() const;

    /** 炮口初速(cm/s),供瞄准线预测读取(与 Fire 用的是同一个值) */
    UFUNCTION(BlueprintPure, Category = "Weapon|Turret")
    float GetMuzzleVelocity() const { return MuzzleVelocity; }

    /**
     * 预测本次射击的炮弹轨迹,与真实炮弹严格一致:同炮口变换/初速/重力缩放,
     * 逐段球扫硬物(忽略自己的船)并查水面终止。供瞄准线绘制。
     * @param OutPoints    轨迹折线点(含起点),命中处截断
     * @param OutEnd       落点(命中硬物点 / 水面点 / 达到最大步数的末点)
     * @param bOutHitWater true=落水终止, false=命中硬物或到达最大射程
     */
    void PredictTrajectory(TArray<FVector>& OutPoints, FVector& OutEnd, bool& bOutHitWater) const;

    /** 瞄准世界坐标点:底座转 Yaw(360° 无限制),炮管转 Pitch(自动 Clamp)。瞄准数学在 UOCAimStatics */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Turret")
    void AimAt(const FVector& TargetLocation);

    /**
     * 弹道瞄准:按炮弹初速度和重力解算仰角,让炮弹实际命中目标点(而非直线指向)。
     * @return false=目标超出弹道最大射程(此时仍按直线瞄准摆好炮口,但不建议开火)
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Turret")
    bool AimAtBallistic(const FVector& TargetLocation);

    /** 开火 CD 剩余比例:1=刚开火,0=可开火(线性递减)。供 UI 每帧读取 */
    UFUNCTION(BlueprintPure, Category = "Weapon|Turret")
    float GetFireCooldownPercent() const;

    /** 开火:在炮口生成炮弹并注入初速度。受 FireRate 节流。由 Controller(玩家/AI)调用
     *  @return true=本次真的打出了炮弹;CD 中/未配置炮弹/生成失败返回 false */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Turret")
    bool Fire();

    /** 应用战斗配置:覆盖射速/初速,并保存伤害配置供 Fire() 注入炮弹。由拥有单位的 BeginPlay 调用 */
    void ApplyConfig(const FOCCombatConfigRow& Config);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    /** 底座 Mesh */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> TurretBaseMesh;

    /** 铰接轴心:纯变换节点,转它的 Pitch 让炮管绕正确的轴俯仰 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> BarrelPivot;

    /** 炮管 Mesh */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> TurretBarrelMesh;

    /** 炮口位置标记(预留) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> MuzzlePoint;

    /** 俯仰角下限(度) */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Turret")
    float PitchLimitMin = -5.0f;

    /** 俯仰角上限(度) */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Turret")
    float PitchLimitMax = 45.0f;

    /** 发射的炮弹类型(蓝图里配具体炮弹蓝图) */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Fire")
    TSubclassOf<AOCProjectileBase> ProjectileClass;

    /** 炮口初速(cm/s) */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Fire", meta = (ClampMin = "0.0"))
    float MuzzleVelocity = 3000.0f;

    /** 每秒最大发射次数 */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Fire", meta = (ClampMin = "0.0"))
    float FireRate = 1.0f;

private:
    /** 上次开火的世界时间(秒)。初值保证第一发立即可打 */
    float LastFireTime = -1000.0f;

    /** 战斗配置备份(伤害字段供 Fire() 注入炮弹);bHasCombatConfig=false 时不注入,炮弹用自己的默认值 */
    FOCCombatConfigRow CombatConfig;
    bool bHasCombatConfig = false;
};
