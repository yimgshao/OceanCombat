// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OCInfiniteOceanManager.generated.h"

class AWaterBodyCustom;

/**
 * 无限海面管理器。
 * 本项目关卡里没有 WaterZone:海面是一个 AWaterBodyCustom(静态网格平面,
 * 完全在 WaterZone/四叉树体系之外)。无限海面 = 让这个水面 Actor 跟随玩家低频平移:
 *   - 渲染:只是一个静态网格,平移无重建开销;
 *   - 波形:Gerstner 波是世界坐标+时间的纯函数,平移不带波形,无断层;
 *   - 浮力:碰撞挂在同一个网格组件上,随 Actor 一起平移,玩家永远在平面中心区域。
 */
UCLASS()
class OCEANCOMBAT_API AOCInfiniteOceanManager : public AActor
{
    GENERATED_BODY()

public:
    AOCInfiniteOceanManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** 按 Actor Tag 查找要管理的水面 Actor(找不到时回退用关卡里第一个 AWaterBodyCustom) */
    UPROPERTY(EditAnywhere, Category = "InfiniteOcean")
    FName OceanActorTag = TEXT("InfiniteOcean");

    /** 玩家偏离水面 Actor 超过该距离时,把水面平移过来;移动量按该值吸附,保持原始网格相位(cm) */
    UPROPERTY(EditAnywhere, Category = "InfiniteOcean", meta = (ClampMin = "100"))
    float FollowSnapSize = 50000.0f;

    /** 玩家位置检测间隔(s) */
    UPROPERTY(EditAnywhere, Category = "InfiniteOcean", meta = (ClampMin = "0.05"))
    float CheckInterval = 0.25f;

    /** 自动跟随开关(默认开:水面区域随玩家船移动) */
    UPROPERTY(EditAnywhere, Category = "InfiniteOcean")
    bool bAutoFollow = true;

private:
    /** 按 Tag 查找(回退:第一个)AWaterBodyCustom,成功后缓存 */
    AWaterBodyCustom* FindOceanBody();

    /** 关卡里摆放的水体默认是 Static mobility,运行时移动前必须改为 Movable(网格组件一并改,Static 不能挂在 Movable 下) */
    void EnsureBodyMovable(AWaterBodyCustom* Body) const;

    /** 把水面 Actor 平移 DeltaXY(可选吸附) */
    void MoveOceanByDelta(FVector2D DeltaXY, bool bSnap);

    TWeakObjectPtr<AWaterBodyCustom> CachedBody;

    float TimeSinceLastCheck = 0.0f;
};
