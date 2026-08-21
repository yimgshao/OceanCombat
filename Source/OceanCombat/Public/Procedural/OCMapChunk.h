// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OCMapChunk.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
struct FOCInfiniteHeightfield;
struct FOCRegionBlobTable;

/** 区块网格构建参数(POD,按值传给后台线程) */
struct FOCChunkBuildParams
{
    /** 区块边长(cm) */
    float ChunkSize = 25600.0f;

    /** 有效顶点间距(cm,已按区块边长整除对齐,保证区块边界顶点逐位重合) */
    float VertexSpacing = 100.0f;

    /** 海平面高度(顶点色混合分界基准) */
    float SeaLevelZ = 0.0f;

    /** 混合分界中心 = 海平面 + 此值 */
    float LandThresholdOffset = 30.0f;

    /** 混合带高度宽(cm) */
    float ShorelineBlendWidth = 120.0f;

    /** 混合分界噪声抖动幅度(cm) */
    float ShorelineJitter = 80.0f;

    /** 混合分界噪声频率(1/cm) */
    float ShorelineJitterFrequency = 0.00025f;

    /** 噪声种子(世界种子派生) */
    int32 NoiseSeed = 0;

    /** 深海剔除深度(cm):四角全低于 SeaLevelZ-此值 的四边形不生成三角形 */
    float MeshCullDepth = 500.0f;
};

/** 后台线程构建的区块网格数据(纯数据,无 UObject) */
struct FOCChunkMeshData
{
    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> VertexColors;
    TArray<int32> Triangles;
};

/**
 * 无限地图的一个区块:纯地形网格。
 * 根组件 = 海床 ProceduralMesh(世界坐标顶点,Actor 位于原点),材质槽 0 = 地形(顶点色混合)。
 * 网格数据在后台线程构建(BuildMeshData,只读高度场),游戏线程提交(ApplyMeshData)。
 * 出窗口即整个 Actor Destroy,网格/碰撞一并回收。
 * 建筑/装饰等"岛上的内容"不归这里管,由聚落系统(AOCClusterContent)负责。
 */
UCLASS()
class OCEANCOMBAT_API AOCMapChunk : public AActor
{
    GENERATED_BODY()

public:
    AOCMapChunk();

    /** 海床网格组件(根组件) */
    UPROPERTY(VisibleAnywhere, Category = "Chunk")
    TObjectPtr<UProceduralMeshComponent> SeabedMesh;

    /**
     * 后台线程安全:构建区块网格数据。只读调用高度场(纯函数),不碰任何 UObject。
     * BlobTable 为任务局部 blob 表(须覆盖本区块 + 一圈边界 + 一格余量)。
     * 顶点取世界坐标:相邻区块共享同一条边界列,同一高度函数 → 边界顶点逐位一致,无接缝。
     */
    static TSharedPtr<FOCChunkMeshData> BuildMeshData(
        FIntPoint ChunkCoord, const FOCInfiniteHeightfield& Heightfield,
        const FOCRegionBlobTable& BlobTable, const FOCChunkBuildParams& Params);

    /** 游戏线程:提交网格 section 并设置材质(TerrainMat 为空时回退引擎默认材质) */
    void ApplyMeshData(const TSharedPtr<FOCChunkMeshData>& Data, UMaterialInterface* TerrainMat);

private:
    /** 材质回退警告只打一次(每个区块都打会刷屏) */
    static bool bLoggedMaterialFallback;
};
