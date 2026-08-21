// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 一个"团块"(blob):岛屿的构成基元。
 * 高度场把所有 blob 用平滑最小距离场(smin)融合:靠得近的连成有机大块(主岛),
 * 离得远的自然独立成离岸小岛。一座"岛"= 一组 blob;群岛/离岸小岛只是 blob 分布的结果。
 */
struct FOCBlobDef
{
    /** blob 中心(世界 XY) */
    FVector2D Center = FVector2D::ZeroVector;

    /** blob 半径(cm):有符号距离场 d = |P-Center| - Radius,负值在 blob 内 */
    float Radius = 0.0f;

    /** 该 blob 贡献的内陆基准高度(相对海平面,cm):做出岛屿高矮参差 */
    float CoreHeight = 0.0f;
};

/**
 * 高度场场参数集:一次性打包传入 Initialize,避免超长参数列表。
 */
struct FOCFieldParams
{
    // ---- 通用 ----
    float SeaLevelZ = 0.0f;
    float DeepSeaDepth = 800.0f;
    float ShallowWaterDepth = 300.0f;

    // ---- 团块融合(层0) ----
    /** smin 融合软度(cm):越大团块过渡越圆滑、越易连成群岛 */
    float BlobSmoothK = 800.0f;

    // ---- 场→高度映射 ----
    /** 近岸浅海大陆架宽(cm):海岸线向海多宽降到深海底。大=缓坡浅滩多 */
    float ShelfWidth = 1200.0f;

    /** 海岸→内陆抬升过渡宽(cm):大=海岸平缓像沙洲,小=近岸陡峭 */
    float LandRiseWidth = 1400.0f;

    // ---- 域扭曲(层1:海岸线有机化) ----
    /** 采样坐标扭曲强度(cm):越大海岸越扭曲破碎。过大会甩散小岛 */
    float WarpAmplitude = 500.0f;

    /** 域扭曲频率(1/cm):越大扭动越细碎 */
    float WarpFrequency = 0.00035f;

    // ---- 内陆起伏(层2:陆地丘陵/山脊) ----
    /** 内陆起伏主旋钮(cm):0=光滑穹顶,中低=开阔海面基调,高=明显丘陵山脊 */
    float DetailAmplitude = 90.0f;

    /** 起伏基频(1/cm) */
    float DetailFrequency = 0.0004f;

    /** 起伏 fBm 倍频数 */
    int32 DetailOctaves = 3;

    /** 起伏向海岸衰减带宽(cm):保证海岸带干净,不被噪声戳出零碎小坑 */
    float DetailFalloffWidth = 700.0f;

    // ---- 深海海床底噪 ----
    /** 深海海床噪声振幅(cm):远离岛屿处的海床底噪 */
    float SeabedNoiseAmplitude = 150.0f;

    /** 深海海床噪声频率(1/cm) */
    float SeabedNoiseFrequency = 0.00025f;
};

/**
 * 无限布局参数:岛屿聚落生成规则。
 * 世界划分为聚落格(默认 410m),每格按概率生成一个聚落:
 * 一座大岛(母 blob + 卫星)+ 若干小岛散布在聚落半径内;空格 = 聚落间距。
 */
struct FOCInfiniteLayoutParams
{
    /** 聚落格边长(cm) */
    float ClusterCellSize = 40960.0f;

    /** 每格生成聚落的概率:< 1 时空格形成聚落间距 */
    float ClusterChance = 0.8f;

    /** 聚落半径范围(cm):小岛散布在主岛周围此半径内 */
    FVector2D ClusterRadiusRange = FVector2D(15000.0, 25000.0);

    /** 每个聚落的小岛数量范围 */
    FIntPoint ClusterSmallIslandCountRange = FIntPoint(2, 5);

    /** 主岛的卫星 blob 数范围 */
    FIntPoint SatellitesPerIslandRange = FIntPoint(2, 4);

    /** 主岛母 blob 半径范围(cm) */
    FVector2D MainIslandRadiusRange = FVector2D(2000.0, 3500.0);

    /** 卫星 blob 半径 = 母 blob 半径 × 此比例范围 */
    FVector2D SatelliteRadiusFractionRange = FVector2D(0.4, 0.7);

    /** 卫星 blob 相对母 blob 的中心偏移 = 母半径 × 此比例范围(需 < 1 才能融合) */
    FVector2D SatelliteOffsetFractionRange = FVector2D(0.4, 0.75);

    /** 聚落内小岛 blob 半径范围(cm):下限别太小,否则成尖刺岛 */
    FVector2D OffshoreRadiusRange = FVector2D(700.0, 1300.0);

    /** 小岛内陆基准高 = 半径 × 此比例范围:坡度与岛大小解耦,小岛不会因抽到高基准而陡成锥 */
    FVector2D OffshoreCoreHeightFractionRange = FVector2D(0.2, 0.35);

    /** 岛屿内陆基准高度范围(海平面往上,cm):用于主岛 */
    FVector2D CoreHeightRange = FVector2D(150.0, 600.0);

    /**
     * 岛组最小间隔(cm,默认 0 = 关闭)。> 0 时:与更高优先级岛组的影响圈距离小于此值的
     * 岛组整组删除(互斥删除)。想要聚落间硬间隔时启用。
     */
    float MinClusterSeparation = 0.0f;
};

/**
 * 岛组:并查集融合的一组 blob(= 一座岛),以及它的归属主人格。
 * 跨聚落格融合的 blob 自然归为一组;每组只有一个主人格(组内 blob 来源格中字典序最小者),
 * 该岛的内容(mesh/建筑/装饰)只由主人格的聚落任务处理 → 天然零重叠。
 */
struct FOCIslandGroup
{
    /** 组内所有 blob(可能来自多个聚落格) */
    TArray<FOCBlobDef> Blobs;

    /** 主人格:组内 blob 来源格中字典序最小者 */
    FIntPoint OwnerCell = FIntPoint::ZeroValue;

    /** 包围圆中心(世界 XY,供落点扫描/撒点界定范围) */
    FVector2D Center = FVector2D::ZeroVector;

    /** 包围圆半径(cm,含域扭曲外扩余量) */
    float Radius = 0.0f;
};

/**
 * 任务局部 blob 表:
 * 覆盖一片矩形区域的所有聚落格的预生成结果。任务开始建一次,任务内查询走数组下标,
 * 用完即弃——无共享状态、无锁、无缓存管理。
 */
struct FOCRegionBlobTable
{
    /** 左下角的格子坐标 */
    FIntPoint OriginCell = FIntPoint::ZeroValue;

    /** 格子数(X × Y) */
    FIntPoint NumCells = FIntPoint::ZeroValue;

    /** 聚落格边长(cm) */
    float CellSize = 0.0f;

    /** NumCells.X * NumCells.Y 个格子的 blob 列表(按 X + Y*NumCells.X 索引) */
    TArray<TArray<FOCBlobDef>> Cells;

    bool IsValid() const { return NumCells.X > 0 && NumCells.Y > 0 && CellSize > 0.0f; }

    /** 取格子(须在表范围内,调用方保证) */
    const TArray<FOCBlobDef>& GetCell(FIntPoint Cell) const
    {
        const int32 Idx = (Cell.X - OriginCell.X) + (Cell.Y - OriginCell.Y) * NumCells.X;
        return Cells[Idx];
    }
};

/**
 * 无限高度场:无限世界地形的唯一事实来源。
 *
 * blob 按"聚落格"即时确定性生成:每格用 Hash(WorldSeed, CellX, CellY) 为种子
 * 独立生成一个岛屿聚落(一座大岛 + 若干小岛,空格 = 聚落间距),
 * 任意点的地形只由附近 3×3 聚落格决定 → 天然无限、跨区块连续。
 * 场求值四层管线(域扭曲 → smin 融合 → 场→高度映射 → 内陆 fBm)与旧实现一致,
 * 噪声用 OCNoise(整数格点哈希),远离原点无精度劣化。
 *
 * Initialize 后成员只读,查询是纯函数 → 天然线程安全,可在后台线程算区块。
 */
struct OCEANCOMBAT_API FOCInfiniteHeightfield
{
public:
    void Initialize(int32 InWorldSeed, const FOCFieldParams& InFieldParams, const FOCInfiniteLayoutParams& InLayoutParams);

    // ---- 零散查询(现取现算) ----
    /** 海床高度(世界 Z) */
    float GetHeight(FVector2D P) const;

    /** 水深 = 海平面 - 海床高度;负值表示陆地露出水面 */
    float GetWaterDepth(FVector2D P) const;

    /** 浅海判定:水深不超过 Threshold 即为浅海 */
    bool IsShallow(FVector2D P, float Threshold) const;

    /** 海床法线(中心差分) */
    FVector GetNormal(FVector2D P) const;

    float GetSeaLevelZ() const { return FieldParams.SeaLevelZ; }

    /** 世界种子(供派生用途,如装饰撒点种子) */
    int32 GetWorldSeed() const { return WorldSeed; }

    /** 聚落格边长(cm,供调用方计算任务局部表的区域余量) */
    float GetClusterCellSize() const { return LayoutParams.ClusterCellSize; }

    // ---- 批量查询(任务局部 blob 表,区块/聚落任务用) ----
    /** 生成单个聚落格的原始 blob(纯函数,不含归属/删除逻辑) */
    void GenerateClusterRaw(FIntPoint Cell, TArray<FOCBlobDef>& OutBlobs) const;

    /**
     * 生成覆盖 [WorldMin, WorldMax] 的所有聚落格,存成任务局部 blob 表。
     * 注意:GetHeight(P, 表) 需要 P 的 3×3 邻格都在表内,区域请按查询范围外扩一格。
     */
    void GatherBlobsForRegion(FVector2D WorldMin, FVector2D WorldMax, FOCRegionBlobTable& OutTable) const;

    /** 海床高度(批量版):blob 从预生成表取,不重复生成。P 的 3×3 邻格须在表内 */
    float GetHeight(FVector2D P, const FOCRegionBlobTable& Table) const;

    /**
     * 岛组划分与归属:
     * 收集 Cell + 8 邻居的 blob,并查集分组(间隙 < BlobSmoothK 融合),
     * 每组标注主人格(字典序最小的来源格)。MinClusterSeparation > 0 时另做互斥删除。
     */
    void GetIslandGroupsAroundCell(FIntPoint Cell, TArray<FOCIslandGroup>& OutGroups) const;

private:
    /** 域扭曲:采样前把坐标推歪,让团块边界有机扭动 */
    FVector2D SampleWarp(FVector2D P) const;

    /** 陆地有符号距离场:所有 blob 用 polynomial smooth-min 融合(语义同旧实现) */
    float SampleBaseField(FVector2D P, const TArray<FOCBlobDef>& Blobs, float& OutCoreHeight, float& OutLocalRadius) const;

    /** 场→高度映射(全 smoothstep,C^1 连续) */
    float HeightFromField(float F, float CoreHeight, float LandRiseWidthEff) const;

    /** 第 3~5 步求值:blob 列表 → 高度(两个 GetHeight 共用) */
    float EvaluateHeight(FVector2D P, const TArray<FOCBlobDef>& Blobs) const;

    int32 WorldSeed = 0;
    FOCFieldParams FieldParams;
    FOCInfiniteLayoutParams LayoutParams;

    /** 深海海床基准高度 = SeaLevelZ - DeepSeaDepth(缓存) */
    float BaseHeight = -800.0f;
};
