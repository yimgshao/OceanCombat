// OceanCombat. Copyright(c) All rights reserved.

#include "Procedural/OCInfiniteHeightfield.h"

#include "Procedural/OCNoiseUtil.h"

/** 各用途噪声种子偏移(与 WorldSeed 相加,替代旧实现的"坐标加常量偏移"去相关写法) */
namespace OCInfiniteNoiseSeeds
{
    constexpr int32 WarpX = 101;
    constexpr int32 WarpY = 102;
    constexpr int32 Detail = 201;
    constexpr int32 Seabed = 301;
}

namespace
{
    /** [0,1] 上的 smoothstep(两端导数为 0),入参自动 clamp */
    FORCEINLINE float SmoothStep01(float X)
    {
        X = FMath::Clamp(X, 0.0f, 1.0f);
        return X * X * (3.0f - 2.0f * X);
    }

    /** polynomial smooth-min(Inigo Quilez),同旧实现 */
    FORCEINLINE float SMin(float A, float B, float K)
    {
        if (K <= KINDA_SMALL_NUMBER)
        {
            return FMath::Min(A, B);
        }
        const float H = FMath::Clamp(0.5f + 0.5f * (B - A) / K, 0.0f, 1.0f);
        return FMath::Lerp(B, A, H) - K * H * (1.0f - H);
    }

    /** 格坐标字典序(归属优先级:小者优先) */
    FORCEINLINE bool CellLess(const FIntPoint& A, const FIntPoint& B)
    {
        return A.X != B.X ? A.X < B.X : A.Y < B.Y;
    }
}

void FOCInfiniteHeightfield::Initialize(int32 InWorldSeed, const FOCFieldParams& InFieldParams, const FOCInfiniteLayoutParams& InLayoutParams)
{
    WorldSeed = InWorldSeed;
    FieldParams = InFieldParams;
    FieldParams.DetailOctaves = FMath::Max(1, InFieldParams.DetailOctaves);
    LayoutParams = InLayoutParams;
    BaseHeight = FieldParams.SeaLevelZ - FieldParams.DeepSeaDepth;

    // 3×3 聚落格充分性校验:任一聚落的 blob 中心不越出其格 ±Spill,
    // 而 blob 对点的影响距离 ≤ MaxBlobInfluence;两者之和 < 格边长时 3×3 收集必然完备
    const float Spill = LayoutParams.ClusterRadiusRange.Y
                      + LayoutParams.SatelliteOffsetFractionRange.Y * LayoutParams.MainIslandRadiusRange.Y;
    const float MaxBlobInfluence = LayoutParams.MainIslandRadiusRange.Y
                                 + FieldParams.ShelfWidth + FieldParams.WarpAmplitude + 500.0f;
    if (Spill + MaxBlobInfluence >= LayoutParams.ClusterCellSize)
    {
        UE_LOG(LogTemp, Warning, TEXT("[OCInfiniteHeightfield] 聚落格可能过小:外溢 %.0f + 影响距离 %.0f >= 格边长 %.0f,岛屿可能在格界处被截断。请调大 ClusterCellSize 或调小聚落/岛屿参数"),
            Spill, MaxBlobInfluence, LayoutParams.ClusterCellSize);
    }
}

void FOCInfiniteHeightfield::GenerateClusterRaw(FIntPoint Cell, TArray<FOCBlobDef>& OutBlobs) const
{
    const uint32 H = HashCombine(HashCombine(GetTypeHash(WorldSeed), GetTypeHash(Cell.X)), GetTypeHash(Cell.Y));
    FRandomStream Rng(static_cast<int32>(H));

    // 空格:聚落之间的距离(聚落感的关键——内容成簇而非均匀散布)
    if (Rng.FRand() >= LayoutParams.ClusterChance)
    {
        return;
    }

    const float CellSize = LayoutParams.ClusterCellSize;
    const FVector2D CellMin = FVector2D(static_cast<double>(Cell.X), static_cast<double>(Cell.Y)) * CellSize;

    // 聚落中心与半径;小岛散布在中心周围聚落半径内(内容可轻微越出格界,3×3 收集兜底)
    const float ClusterRadius = Rng.FRandRange(
        LayoutParams.ClusterRadiusRange.X, LayoutParams.ClusterRadiusRange.Y);
    const float CenterMargin = LayoutParams.MainIslandRadiusRange.Y;
    const FVector2D Center = CellMin + FVector2D(
        Rng.FRandRange(CenterMargin, CellSize - CenterMargin),
        Rng.FRandRange(CenterMargin, CellSize - CenterMargin));

    auto RandomCoreHeight = [&]()
    {
        return FieldParams.SeaLevelZ
             + Rng.FRandRange(LayoutParams.CoreHeightRange.X, LayoutParams.CoreHeightRange.Y);
    };

    // 1. 主岛(聚落里"大一点的岛"):母 blob + 若干卫星 blob,融合成有机大块
    {
        const float MotherRadius = Rng.FRandRange(
            LayoutParams.MainIslandRadiusRange.X, LayoutParams.MainIslandRadiusRange.Y);
        const float IslandCoreHeight = RandomCoreHeight();

        FOCBlobDef Mother;
        Mother.Center = Center;
        Mother.Radius = MotherRadius;
        Mother.CoreHeight = IslandCoreHeight;
        OutBlobs.Add(Mother);

        const int32 SatCount = Rng.RandRange(
            LayoutParams.SatellitesPerIslandRange.X, LayoutParams.SatellitesPerIslandRange.Y);
        for (int32 s = 0; s < SatCount; ++s)
        {
            const float OffsetFrac = Rng.FRandRange(
                LayoutParams.SatelliteOffsetFractionRange.X, LayoutParams.SatelliteOffsetFractionRange.Y);
            const float RadiusFrac = Rng.FRandRange(
                LayoutParams.SatelliteRadiusFractionRange.X, LayoutParams.SatelliteRadiusFractionRange.Y);
            const float Angle = Rng.FRandRange(0.0f, 2.0f * PI);

            FOCBlobDef Sat;
            Sat.Center = Center
                       + OffsetFrac * MotherRadius * FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
            Sat.Radius = MotherRadius * RadiusFrac;
            Sat.CoreHeight = IslandCoreHeight * Rng.FRandRange(0.7f, 1.1f);
            OutBlobs.Add(Sat);
        }
    }

    // 2. 小岛:聚落半径内均匀散布(sqrt 使圆盘内密度均匀),靠近主岛的由 smin 自然融成群岛
    const int32 SmallCount = Rng.RandRange(
        LayoutParams.ClusterSmallIslandCountRange.X, LayoutParams.ClusterSmallIslandCountRange.Y);
    for (int32 i = 0; i < SmallCount; ++i)
    {
        const float Angle = Rng.FRandRange(0.0f, 2.0f * PI);
        const float Dist = ClusterRadius * FMath::Sqrt(Rng.FRand());

        FOCBlobDef Small;
        Small.Center = Center + Dist * FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
        Small.Radius = Rng.FRandRange(
            LayoutParams.OffshoreRadiusRange.X, LayoutParams.OffshoreRadiusRange.Y);
        // 小岛基准高按半径比例取(而非与大小无关的随机):否则小岛抽到高基准会陡成锥
        Small.CoreHeight = FieldParams.SeaLevelZ
                         + Small.Radius * Rng.FRandRange(
                             LayoutParams.OffshoreCoreHeightFractionRange.X,
                             LayoutParams.OffshoreCoreHeightFractionRange.Y);
        OutBlobs.Add(Small);
    }
}

void FOCInfiniteHeightfield::GatherBlobsForRegion(FVector2D WorldMin, FVector2D WorldMax, FOCRegionBlobTable& OutTable) const
{
    const float CellSize = LayoutParams.ClusterCellSize;
    const FIntPoint MinCell(
        FMath::FloorToInt(WorldMin.X / CellSize),
        FMath::FloorToInt(WorldMin.Y / CellSize));
    const FIntPoint MaxCell(
        FMath::FloorToInt(WorldMax.X / CellSize),
        FMath::FloorToInt(WorldMax.Y / CellSize));

    OutTable.OriginCell = MinCell;
    OutTable.NumCells = MaxCell - MinCell + FIntPoint(1, 1);
    OutTable.CellSize = CellSize;
    OutTable.Cells.SetNum(OutTable.NumCells.X * OutTable.NumCells.Y);

    for (int32 Y = 0; Y < OutTable.NumCells.Y; ++Y)
    {
        for (int32 X = 0; X < OutTable.NumCells.X; ++X)
        {
            GenerateClusterRaw(MinCell + FIntPoint(X, Y), OutTable.Cells[X + Y * OutTable.NumCells.X]);
        }
    }
}

void FOCInfiniteHeightfield::GetIslandGroupsAroundCell(FIntPoint Cell, TArray<FOCIslandGroup>& OutGroups) const
{
    // 1. 收集 Cell + 8 邻居的全部 blob,记录每个 blob 的来源格
    struct FBlobWithCell
    {
        FOCBlobDef Blob;
        FIntPoint Cell;
    };
    TArray<FBlobWithCell> All;
    for (int32 DY = -1; DY <= 1; ++DY)
    {
        for (int32 DX = -1; DX <= 1; ++DX)
        {
            const FIntPoint C = Cell + FIntPoint(DX, DY);
            TArray<FOCBlobDef> Blobs;
            GenerateClusterRaw(C, Blobs);
            for (const FOCBlobDef& B : Blobs)
            {
                All.Add(FBlobWithCell{ B, C });
            }
        }
    }
    const int32 N = All.Num();
    if (N == 0)
    {
        return;
    }

    // 2. 并查集:blob 边缘间隙 < BlobSmoothK 视为连通(与 smin 焊接范围一致,跨格自然成组)
    TArray<int32> Parent;
    Parent.SetNumUninitialized(N);
    for (int32 i = 0; i < N; ++i)
    {
        Parent[i] = i;
    }
    TFunction<int32(int32)> Find = [&](int32 X)
    {
        while (Parent[X] != X)
        {
            Parent[X] = Parent[Parent[X]];
            X = Parent[X];
        }
        return X;
    };
    const float FuseGap = FMath::Max(FieldParams.BlobSmoothK, 1.0f);
    for (int32 i = 0; i < N; ++i)
    {
        for (int32 j = i + 1; j < N; ++j)
        {
            const float Gap = FVector2D::Distance(All[i].Blob.Center, All[j].Blob.Center)
                            - All[i].Blob.Radius - All[j].Blob.Radius;
            if (Gap < FuseGap)
            {
                Parent[Find(i)] = Find(j);
            }
        }
    }

    // 3. 归集成组:主人格 = 字典序最小来源格;包围圆 = blob 圆并集 + 域扭曲余量
    const float Margin = FieldParams.WarpAmplitude + 200.0f;
    TMap<int32, int32> RootToIndex;
    for (int32 i = 0; i < N; ++i)
    {
        const int32 Root = Find(i);
        int32* FoundIdx = RootToIndex.Find(Root);
        if (!FoundIdx)
        {
            FoundIdx = &RootToIndex.Add(Root, OutGroups.Num());
            FOCIslandGroup& NewGroup = OutGroups.AddDefaulted_GetRef();
            NewGroup.OwnerCell = All[i].Cell;
            NewGroup.Center = All[i].Blob.Center;
            NewGroup.Radius = All[i].Blob.Radius;
        }
        FOCIslandGroup& Group = OutGroups[*FoundIdx];
        Group.Blobs.Add(All[i].Blob);
        if (CellLess(All[i].Cell, Group.OwnerCell))
        {
            Group.OwnerCell = All[i].Cell;
        }
        // 包围圆:圆心固定在首个 blob,半径取"到最远 blob 边缘"的并集(与旧逻辑一致)
        Group.Radius = FMath::Max(Group.Radius,
            FVector2D::Distance(Group.Center, All[i].Blob.Center) + All[i].Blob.Radius);
    }
    for (FOCIslandGroup& Group : OutGroups)
    {
        Group.Radius += Margin;
    }

    // 4. 可选互斥(MinClusterSeparation > 0):与更高优先级组的影响圈冲突的组整组删除
    if (LayoutParams.MinClusterSeparation > 0.0f)
    {
        const float Extra = FieldParams.ShelfWidth + FieldParams.WarpAmplitude + LayoutParams.MinClusterSeparation;
        // 先标记(谓词里遍历同一数组再 RemoveAll 不安全)
        TArray<bool> Deleted;
        Deleted.SetNumZeroed(OutGroups.Num());
        for (int32 g = 0; g < OutGroups.Num(); ++g)
        {
            for (int32 h = 0; h < OutGroups.Num() && !Deleted[g]; ++h)
            {
                if (g == h || !CellLess(OutGroups[h].OwnerCell, OutGroups[g].OwnerCell))
                {
                    continue;
                }
                for (const FOCBlobDef& BG : OutGroups[g].Blobs)
                {
                    for (const FOCBlobDef& BH : OutGroups[h].Blobs)
                    {
                        const float Reach = (BG.Radius + Extra) + (BH.Radius + Extra);
                        if (FVector2D::DistSquared(BG.Center, BH.Center) < Reach * Reach)
                        {
                            Deleted[g] = true; // G 与更高优先级的 H 冲突,删 G
                            break;
                        }
                    }
                    if (Deleted[g])
                    {
                        break;
                    }
                }
            }
        }
        for (int32 i = OutGroups.Num() - 1; i >= 0; --i)
        {
            if (Deleted[i])
            {
                OutGroups.RemoveAtSwap(i);
            }
        }
    }
}

FVector2D FOCInfiniteHeightfield::SampleWarp(FVector2D P) const
{
    if (FieldParams.WarpAmplitude <= 0.0f)
    {
        return FVector2D::ZeroVector;
    }
    const FVector2D Base = P * FieldParams.WarpFrequency;
    const float QX = OCNoise::GradientNoise2D(Base, WorldSeed + OCInfiniteNoiseSeeds::WarpX);
    const float QY = OCNoise::GradientNoise2D(Base, WorldSeed + OCInfiniteNoiseSeeds::WarpY);
    return FieldParams.WarpAmplitude * FVector2D(QX, QY);
}

float FOCInfiniteHeightfield::SampleBaseField(FVector2D P, const TArray<FOCBlobDef>& Blobs, float& OutCoreHeight, float& OutLocalRadius) const
{
    if (Blobs.Num() == 0)
    {
        OutCoreHeight = 0.0f;
        OutLocalRadius = 0.0f;
        return UE_BIG_NUMBER;
    }

    // 融合场 Field 与最近距离 Dmin;CoreHeight/LocalRadius 按距离 softmax 加权(同旧实现,避免突变台阶)
    const float K = FMath::Max(FieldParams.BlobSmoothK, 1.0f);
    float Field = UE_BIG_NUMBER;
    float Dmin = UE_BIG_NUMBER;
    for (const FOCBlobDef& Blob : Blobs)
    {
        const float D = FVector2D::Distance(P, Blob.Center) - Blob.Radius;
        Field = SMin(Field, D, FieldParams.BlobSmoothK);
        Dmin = FMath::Min(Dmin, D);
    }

    float WeightSum = 0.0f;
    float CoreSum = 0.0f;
    float RadiusSum = 0.0f;
    for (const FOCBlobDef& Blob : Blobs)
    {
        const float D = FVector2D::Distance(P, Blob.Center) - Blob.Radius;
        const float W = FMath::Exp(-(D - Dmin) / K);
        WeightSum += W;
        CoreSum += W * Blob.CoreHeight;
        RadiusSum += W * Blob.Radius;
    }
    OutCoreHeight = WeightSum > 0.0f ? CoreSum / WeightSum : 0.0f;
    OutLocalRadius = WeightSum > 0.0f ? RadiusSum / WeightSum : 0.0f;

    return Field;
}

float FOCInfiniteHeightfield::HeightFromField(float F, float CoreHeight, float LandRiseWidthEff) const
{
    if (F >= 0.0f)
    {
        const float T = SmoothStep01(F / FMath::Max(FieldParams.ShelfWidth, 1.0f));
        return FieldParams.SeaLevelZ - FieldParams.DeepSeaDepth * T;
    }
    const float T = SmoothStep01(-F / FMath::Max(LandRiseWidthEff, 1.0f));
    return FieldParams.SeaLevelZ + CoreHeight * T;
}

float FOCInfiniteHeightfield::EvaluateHeight(FVector2D P, const TArray<FOCBlobDef>& Blobs) const
{
    // 层1:域扭曲
    const FVector2D WarpedP = P + SampleWarp(P);

    // 层0:融合场 → 基准海床高度
    float CoreHeight = 0.0f;
    float LocalRadius = 0.0f;
    const float F = SampleBaseField(WarpedP, Blobs, CoreHeight, LocalRadius);

    // 小岛收窄抬升过渡宽,避免中心冲成尖刺
    const float LandRiseWidthEff = FMath::Min(FieldParams.LandRiseWidth, LocalRadius * 0.85f);
    float Height = HeightFromField(F, CoreHeight, LandRiseWidthEff);

    // 层2:内陆起伏 fBm,向海岸衰减
    if (FieldParams.DetailAmplitude > 0.0f && F < 0.0f)
    {
        const float Falloff = SmoothStep01(-F / FMath::Max(FieldParams.DetailFalloffWidth, 1.0f));
        if (Falloff > 0.0f)
        {
            const FVector2D DetailP = WarpedP * FieldParams.DetailFrequency;
            Height += FieldParams.DetailAmplitude
                    * OCNoise::FBm2D(DetailP, FieldParams.DetailOctaves, WorldSeed + OCInfiniteNoiseSeeds::Detail)
                    * Falloff;
        }
    }

    // 深海海床底噪:仅在海里且随入海深度淡入
    if (FieldParams.SeabedNoiseAmplitude > 0.0f && F > 0.0f)
    {
        const float SeaFade = SmoothStep01(F / FMath::Max(FieldParams.ShelfWidth, 1.0f));
        Height += FieldParams.SeabedNoiseAmplitude
                * OCNoise::GradientNoise2D(P * FieldParams.SeabedNoiseFrequency, WorldSeed + OCInfiniteNoiseSeeds::Seabed)
                * SeaFade;
    }

    return Height;
}

float FOCInfiniteHeightfield::GetHeight(FVector2D P) const
{
    // 收集 P 周围 3×3 聚落格的 blob(充分性见 Initialize 校验)
    const float CellSize = LayoutParams.ClusterCellSize;
    const FIntPoint CenterCell(
        FMath::FloorToInt(P.X / CellSize),
        FMath::FloorToInt(P.Y / CellSize));

    TArray<FOCBlobDef> Blobs;
    Blobs.Reserve(48);
    for (int32 DY = -1; DY <= 1; ++DY)
    {
        for (int32 DX = -1; DX <= 1; ++DX)
        {
            GenerateClusterRaw(CenterCell + FIntPoint(DX, DY), Blobs);
        }
    }

    return EvaluateHeight(P, Blobs);
}

float FOCInfiniteHeightfield::GetHeight(FVector2D P, const FOCRegionBlobTable& Table) const
{
    const FIntPoint CenterCell(
        FMath::FloorToInt(P.X / Table.CellSize),
        FMath::FloorToInt(P.Y / Table.CellSize));

    TArray<FOCBlobDef> Blobs;
    Blobs.Reserve(48);
    for (int32 DY = -1; DY <= 1; ++DY)
    {
        for (int32 DX = -1; DX <= 1; ++DX)
        {
            const FIntPoint C = CenterCell + FIntPoint(DX, DY);
            // 表外格子视为空(调用方应按查询范围外扩一格建表,正常不会发生)
            if (C.X >= Table.OriginCell.X && C.X < Table.OriginCell.X + Table.NumCells.X
             && C.Y >= Table.OriginCell.Y && C.Y < Table.OriginCell.Y + Table.NumCells.Y)
            {
                Blobs.Append(Table.GetCell(C));
            }
        }
    }

    return EvaluateHeight(P, Blobs);
}

float FOCInfiniteHeightfield::GetWaterDepth(FVector2D P) const
{
    return FieldParams.SeaLevelZ - GetHeight(P);
}

bool FOCInfiniteHeightfield::IsShallow(FVector2D P, float Threshold) const
{
    return GetWaterDepth(P) <= Threshold;
}

FVector FOCInfiniteHeightfield::GetNormal(FVector2D P) const
{
    const float Eps = 50.0f;
    const float DX = GetHeight(P + FVector2D(Eps, 0.0)) - GetHeight(P - FVector2D(Eps, 0.0));
    const float DY = GetHeight(P + FVector2D(0.0, Eps)) - GetHeight(P - FVector2D(0.0, Eps));
    return FVector(-DX, -DY, 2.0 * Eps).GetSafeNormal();
}
