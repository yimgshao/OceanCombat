// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OCClusterContent.generated.h"

class UStaticMesh;
class UHierarchicalInstancedStaticMeshComponent;

/** 一类装饰(树/花草/石头)的散布规则(POD,按值传给后台线程;有效条目已剔除空 mesh/零权重) */
struct FOCDecoRule
{
    /** 有效条目的权重,与 FOCDecoMeshList 的网格列表一一对应 */
    TArray<float> EntryWeights;

    /** 有效条目的随机均匀缩放范围 */
    TArray<FVector2D> EntryScaleRanges;

    float Spacing = 400.0f;
    float Jitter = 0.7f;
    float MinGroundClearance = 40.0f;
    float MaxSlopeDeg = 25.0f;
    bool bAlignToNormal = false;
    bool bCollision = false;

    /** 每岛组实例上限(0 = 不限) */
    int32 MaxPerGroup = 0;
};

/** 一个装饰实例(后台线程产出的纯数据;网格在游戏线程按 规则+条目 索引解析) */
struct FOCDecoInstance
{
    int32 Rule = 0;
    int32 Entry = 0;
    FTransform Transform;
};

/** 一个装饰规则的有效网格列表(UHT 不支持 TArray 嵌套,包一层) */
USTRUCT()
struct FOCDecoMeshList
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMesh>> Meshes;
};

/**
 * 一个聚落格的内容载体:
 * 该格"拥有"的岛组上的装饰 HISM 挂在这个 Actor 下(建筑是自由 Actor,由管理器另行跟踪)。
 * 出窗口即整个 Actor Destroy,装饰实例一并回收。
 */
UCLASS()
class OCEANCOMBAT_API AOCClusterContent : public AActor
{
    GENERATED_BODY()

public:
    AOCClusterContent();

    /** 游戏线程:按规则/网格列表把后台算好的装饰实例写入 HISM */
    void ApplyDecorations(const TArray<FOCDecoInstance>& Instances,
        const TArray<FOCDecoRule>& DecoRules, const TArray<FOCDecoMeshList>& DecoMeshes);

private:
    /** 取(或惰性创建)某 mesh 的 HISM 组件,挂在根组件下,按类别设置碰撞 */
    UHierarchicalInstancedStaticMeshComponent* GetOrCreateDecoComponent(UStaticMesh* Mesh, bool bCollision);

    /** 装饰 mesh → HISM 组件 */
    UPROPERTY(Transient)
    TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> DecoComponents;
};
