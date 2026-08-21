// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OCDestructionComponent.generated.h"

class UGeometryCollection;
class UNiagaraSystem;
class USoundBase;

/**
 * 死亡碎裂表现组件。挂在任意 AOCPawnBase(船/建筑)上,监听其血量组件的 OnDeath,
 * 死亡时统一处理:隐藏本体所有 Mesh + 炮塔 → 原位生成 Geometry Collection 残骸
 * → 在受击点做局部断裂(整体四周碎开塌落)+ 受击点一小撮碎片从爆心向四周炸开
 * → 可选 Niagara 爆炸 / 音效 → 本体延时销毁。
 *
 * 组合优于继承:船和建筑都复用同一套逻辑,差异只在参数(碎裂资产、崩飞强度、是否继承速度等)。
 * WreckCollection 留空则不生成残骸(只隐藏本体并延时销毁),向后兼容。
 */
UCLASS(ClassGroup=(OceanCombat), meta=(BlueprintSpawnableComponent))
class OCEANCOMBAT_API UOCDestructionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOCDestructionComponent();

    /** 死亡碎裂用的 Geometry Collection 资产(编辑器 Fracture 生成)。留空则死亡时本体消失,不碎裂 */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction")
    TObjectPtr<UGeometryCollection> WreckCollection;

    /**
     * true=按受击点做局部断裂 + 受击点崩飞:落点附近断开、结构整体四周碎开塌落,并把落点一小撮碎片顺炮弹方向崩飞。
     * 无受击信息(非爆炸/点伤致死、拿不到落点)时自动回退到整块炸开。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction")
    bool bUseImpactFracture = true;

    /**
     * 受击点断裂应变强度。以爆心为中心、断裂半径内对子块施加应变;
     * **需超过 GC 资产根簇的 Damage Threshold 才会断**。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction", meta = (ClampMin = "0.0", EditCondition = "bUseImpactFracture"))
    float FractureStrainMagnitude = 1000000.0f;

    /**
     * 断裂半径系数:断裂作用半径 = 爆炸外半径 × 本系数。爆炸越大断裂范围越大(炮弹/炸药桶自动不同)。
     * 1.0 = 断裂范围等于爆炸范围;<1 收紧、>1 放大。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction", meta = (ClampMin = "0.0", EditCondition = "bUseImpactFracture"))
    float FractureRadiusScale = 1.0f;

    /** 断裂沿连接图向外传播的层数(0=只碎半径内的块;越大裂纹沿相邻块扩散越远,更自然但范围更大) */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction", meta = (ClampMin = "0", EditCondition = "bUseImpactFracture"))
    int32 FracturePropagationDepth = 2;

    /** 传播每向外一层,应变乘以的衰减系数(<1 让裂纹越传越弱) */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bUseImpactFracture"))
    float FracturePropagationFactor = 0.5f;

    /**
     * 爆炸冲击冲量系数:**爆心处的峰值冲量 = 爆炸外半径(=爆炸大小) × 本系数**,并从爆心沿半径**线性衰减**
     */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction", meta = (ClampMin = "0.0", EditCondition = "bUseImpactFracture"))
    float ScatterImpulseScale = 2000.0f;

    /**
     * 爆开半径系数:冲量的衰减半径 = 爆炸外半径 × 本系数(爆心满值 → 此半径处衰减到 0,半径外不受力)。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction", meta = (ClampMin = "0.0", EditCondition = "bUseImpactFracture"))
    float ScatterRadiusScale = 0.6f;

    /** 死亡爆炸特效(可选,留空跳过) */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction")
    TObjectPtr<UNiagaraSystem> ExplosionEffect;

    /** 死亡爆炸音效(可选,留空跳过) */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction")
    TObjectPtr<USoundBase> ExplosionSound;

    /** 残骸存活时间(秒),超时销毁 */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction", meta = (ClampMin = "0.0"))
    float WreckLifeSpan = 15.0f;

    /** 本体死亡后延时销毁时间(秒) */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction", meta = (ClampMin = "0.0"))
    float OwnerLifeSpan = 3.0f;

    /** 碎块是否继承本体死亡瞬间的物理速度(移动单位如船需要,静态建筑保持 false) */
    UPROPERTY(EditDefaultsOnly, Category = "Destruction")
    bool bInheritVelocity = false;

protected:
    virtual void BeginPlay() override;

private:
    /** 死亡处理:隐藏本体 → 生成碎裂残骸 → 爆炸表现 → 本体延时销毁 */
    UFUNCTION()
    void HandleDeath(AActor* DeadActor, AController* KillerController);
};
