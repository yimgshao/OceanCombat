// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Procedural/OCInfiniteHeightfield.h"
#include "Procedural/OCMapChunk.h"
#include "Procedural/OCClusterContent.h"

class UOCMapGenConfig;

/** 建筑落点(后台线程产出,纯数据) */
struct FOCBuildingSiteInfo
{
    FVector Location = FVector::ZeroVector;

    /** true = 城堡(拥有的最大岛组);false = 防御塔(其余岛组) */
    bool bIsCastle = false;
};

/** 建筑落点搜索参数(POD,按值传给后台线程) */
struct FOCBuildingSiteParams
{
    float FootprintRadius = 350.0f;
    float MinGroundClearance = 60.0f;
    float SampleStep = 120.0f;
    float SeaLevelZ = 0.0f;
    int32 MaxTurretsPerCluster = 3;

    /** 落点下沉深度(cm):Z = 足印内最低点 - 此值,坡上底座低侧落实、高侧微埋,避免悬浮 */
    float EmbedDepth = 30.0f;

    /** 装饰避开建筑落点的排除半径(cm) */
    float DecoBuildingClearance = 600.0f;
};

/** 敌船生成点(后台线程产出,纯数据) */
struct FOCBoatSpawnInfo
{
    FVector Location = FVector::ZeroVector;

    /** 初始朝向(度):由主岛中心指向本点,让船一出生就朝外,不会立刻冲上岸 */
    float YawDeg = 0.0f;
};

/** 敌船生成参数(POD,按值传给后台线程) */
struct FOCBoatSpawnParams
{
    bool bEnabled = true;

    /** 数量区间(含两端) */
    FIntPoint CountRange = FIntPoint(2, 4);

    float RingRadiusMin = 4000.0f;
    float RingRadiusMax = 9000.0f;

    /** 生成点最小水深(cm):低于此值(陆地/浅滩)的候选点淘汰 */
    float MinWaterDepth = 400.0f;

    /** 同聚落内两船最小间距(cm) */
    float MinSeparation = 1500.0f;

    float SeaLevelZ = 0.0f;
};

/** 炸药桶生成点(后台线程产出,纯数据) */
struct FOCBarrelSpawnInfo
{
    FVector Location = FVector::ZeroVector;

    /** 初始朝向(度):纯随机,仅为外观错开 */
    float YawDeg = 0.0f;
};

/** 炸药桶生成参数(POD,按值传给后台线程) */
struct FOCBarrelSpawnParams
{
    bool bEnabled = true;

    /** 数量区间(含两端) */
    FIntPoint CountRange = FIntPoint(2, 5);

    float RingRadiusMin = 3000.0f;
    float RingRadiusMax = 8000.0f;

    /** 生成点最小水深(cm):低于此值(陆地/浅滩)的候选点淘汰 */
    float MinWaterDepth = 400.0f;

    /** 同聚落内两桶最小间距(cm) */
    float MinSeparation = 800.0f;

    float SeaLevelZ = 0.0f;
};

/** 一个聚落格的内容计算结果(归属判定后的成品,纯数据) */
struct FOCClusterContentData
{
    FIntPoint Cell;

    /** 拥有组包围盒(等地形就绪用) */
    TArray<FBox2D> GroupBounds;
    TArray<FOCBuildingSiteInfo> Sites;
    TArray<FOCBoatSpawnInfo> BoatSpawns;
    TArray<FOCBarrelSpawnInfo> BarrelSpawns;
    TArray<FOCDecoInstance> Decorations;
};

/**
 * 生成器运行时参数:FromConfig 一次打包。
 * AOCInfiniteMapManager(运行时)与菜单烘焙器(编辑器)共用同一条参数管道,
 * 保证同种子下烘焙结果与实机逐位一致。
 */
struct FOCMapGenRuntimeParams
{
    int32 WorldSeed = 0;
    float ChunkSize = 25600.0f;

    FOCFieldParams FieldParams;
    FOCInfiniteLayoutParams LayoutParams;
    FOCChunkBuildParams ChunkBuildParams;
    FOCBuildingSiteParams BuildingParams;
    FOCBoatSpawnParams BoatParams;
    FOCBarrelSpawnParams BarrelParams;
    TArray<FOCDecoRule> DecoRules;
    TArray<FOCDecoMeshList> DecoMeshes;

    /**
     * 从配置资产构建全部参数。Config 为空走 CDO 默认值。
     * SeedOverride 非 0 时优先于 Config->Seed;两者都为 0 时随机(与 Manager 原行为一致)。
     */
    static FOCMapGenRuntimeParams FromConfig(const UOCMapGenConfig* Config, int32 SeedOverride = 0);
};

/**
 * 地图生成内核:全部是与世界状态无关的纯函数(同输入恒同输出,线程安全)。
 * 运行时的 AOCInfiniteMapManager 在后台线程调用;编辑器烘焙器在游戏线程同步调用。
 */
namespace OCMapGenKernels
{
    /**
     * 在单个岛组的包围圆内按步长扫格,选"足印半径内全在平地、且高于水位余量"的最平坦落点。
     * 评分 = 足印内高度极差(越小越平)。纯扫描无随机消耗 → 同组每次算出相同落点。
     */
    bool FindFlattestSite(const FOCInfiniteHeightfield& Heightfield, const FOCIslandGroup& Group,
        const FOCBuildingSiteParams& Params, FVector& OutLocation);

    /** 表查法线(装饰撒点用):中心差分,与 FOCInfiniteHeightfield::GetNormal 同公式 */
    FVector GetNormalFromTable(const FOCInfiniteHeightfield& Heightfield, const FOCRegionBlobTable& Table, FVector2D P);

    /**
     * 在单个岛组包围盒内散布一个类别的装饰。
     * 种子 = Hash(世界种子, 格, 组, 类别) → 确定性;候选点排除建筑落点周围 ClearanceSq 范围。
     */
    void ScatterDecorationsForGroup(const FOCIslandGroup& Group, FIntPoint Cell, int32 GroupIndex,
        const FOCDecoRule& Rule, int32 RuleIndex, int32 WorldSeed,
        const TArray<FOCBuildingSiteInfo>& Sites, float ClearanceSq,
        const FOCInfiniteHeightfield& Heightfield, const FOCRegionBlobTable& Table,
        TArray<FOCDecoInstance>& OutInstances);

    /** 为区块构建任务局部 blob 表(参数按值传入,线程安全) */
    void BuildChunkBlobTable(const FOCInfiniteHeightfield& Heightfield, FIntPoint Coord,
        float ChunkSize, float ClusterCellSize, float VertexSpacing, FOCRegionBlobTable& OutTable);

    /**
     * 在聚落主岛周围的水域环带上摇敌船生成点。
     * 纯函数(种子由 WorldSeed + 聚落格决定),同一聚落每次算出相同结果 —— 与建筑落点、装饰撒点一致。
     */
    void PickBoatSpawns(const FOCInfiniteHeightfield& Heightfield, const FOCIslandGroup& MainGroup,
        FIntPoint Cell, const FOCBoatSpawnParams& Params, TArray<FOCBoatSpawnInfo>& OutSpawns);

    /** 炸药桶撒点:与 PickBoatSpawns 同构,盐值与敌船/装饰错开,随机流独立 */
    void PickBarrelSpawns(const FOCInfiniteHeightfield& Heightfield, const FOCIslandGroup& MainGroup,
        FIntPoint Cell, const FOCBarrelSpawnParams& Params, TArray<FOCBarrelSpawnInfo>& OutSpawns);

    /**
     * 算一个聚落格的全部内容:岛组划分 + 归属判定 → 只对"我是主人"的组做建筑落点、船/桶摇点与装饰撒点。
     */
    FOCClusterContentData ComputeClusterContent(
        FIntPoint Cell, const FOCInfiniteHeightfield& Heightfield,
        const FOCBuildingSiteParams& BuildingParams, const FOCBoatSpawnParams& BoatParams,
        const FOCBarrelSpawnParams& BarrelParams,
        const TArray<FOCDecoRule>& DecoRules);
}
