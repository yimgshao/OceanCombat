// OceanCombat. Copyright(c) All rights reserved.

#include "Procedural/OCMapGenKernels.h"

#include "Hazards/OCExplosiveBarrel.h"
#include "Pawns/Boats/OCEnemyBoat.h"
#include "Procedural/OCMapGenConfig.h"

FOCMapGenRuntimeParams FOCMapGenRuntimeParams::FromConfig(const UOCMapGenConfig* Config, int32 SeedOverride)
{
    const UOCMapGenConfig* C = Config ? Config : GetDefault<UOCMapGenConfig>();

    FOCMapGenRuntimeParams P;

    // 种子:命令/调用方显式指定优先;否则 0 = 每局随机;非 0 = 固定世界(可复现)
    P.WorldSeed = SeedOverride != 0 ? SeedOverride : (C->Seed != 0 ? C->Seed : FMath::Rand());

    // 场参数(与旧管线同一套配置字段)
    P.FieldParams.SeaLevelZ = C->SeaLevelZ;
    P.FieldParams.DeepSeaDepth = C->DeepSeaDepth;
    P.FieldParams.ShallowWaterDepth = C->ShallowWaterDepth;
    P.FieldParams.BlobSmoothK = C->BlobSmoothK;
    P.FieldParams.ShelfWidth = C->ShelfWidth;
    P.FieldParams.LandRiseWidth = C->LandRiseWidth;
    P.FieldParams.WarpAmplitude = C->WarpAmplitude;
    P.FieldParams.WarpFrequency = C->WarpFrequency;
    P.FieldParams.DetailAmplitude = C->DetailAmplitude;
    P.FieldParams.DetailFrequency = C->DetailFrequency;
    P.FieldParams.DetailOctaves = C->DetailOctaves;
    P.FieldParams.DetailFalloffWidth = C->DetailFalloffWidth;
    P.FieldParams.SeabedNoiseAmplitude = C->SeabedNoiseAmplitude;
    P.FieldParams.SeabedNoiseFrequency = C->SeabedNoiseFrequency;

    // 无限布局参数(聚落;形状范围沿用 Layout 分类)
    P.LayoutParams.ClusterCellSize = C->ClusterCellSize;
    P.LayoutParams.ClusterChance = C->ClusterChance;
    P.LayoutParams.ClusterRadiusRange = C->ClusterRadiusRange;
    P.LayoutParams.ClusterSmallIslandCountRange = C->ClusterSmallIslandCountRange;
    P.LayoutParams.SatellitesPerIslandRange = C->SatellitesPerIslandRange;
    P.LayoutParams.MainIslandRadiusRange = C->MainIslandRadiusRange;
    P.LayoutParams.SatelliteRadiusFractionRange = C->SatelliteRadiusFractionRange;
    P.LayoutParams.SatelliteOffsetFractionRange = C->SatelliteOffsetFractionRange;
    P.LayoutParams.OffshoreRadiusRange = C->OffshoreRadiusRange;
    P.LayoutParams.OffshoreCoreHeightFractionRange = C->OffshoreCoreHeightFractionRange;
    P.LayoutParams.CoreHeightRange = C->CoreHeightRange;
    P.LayoutParams.MinClusterSeparation = C->MinClusterSeparation;

    // 区块构建参数;顶点间距按区块边长整除对齐 → 区块边界顶点逐位重合,无缝无隙
    P.ChunkSize = C->ChunkSize;
    P.ChunkBuildParams.ChunkSize = C->ChunkSize;
    P.ChunkBuildParams.VertexSpacing = C->ChunkSize / FMath::Max(1, FMath::RoundToInt(C->ChunkSize / C->ChunkVertexSpacing));
    P.ChunkBuildParams.SeaLevelZ = C->SeaLevelZ;
    P.ChunkBuildParams.LandThresholdOffset = C->LandThresholdOffset;
    P.ChunkBuildParams.ShorelineBlendWidth = C->ShorelineBlendWidth;
    P.ChunkBuildParams.ShorelineJitter = C->ShorelineJitter;
    P.ChunkBuildParams.ShorelineJitterFrequency = C->SeabedNoiseFrequency;
    P.ChunkBuildParams.NoiseSeed = P.WorldSeed;
    P.ChunkBuildParams.MeshCullDepth = C->MeshCullDepth;

    // 建筑落点参数(沿用 Buildings 分类)
    P.BuildingParams.FootprintRadius = C->BuildingFootprintRadius;
    P.BuildingParams.MinGroundClearance = C->BuildingMinGroundClearance;
    P.BuildingParams.SampleStep = C->BuildingSampleStep;
    P.BuildingParams.SeaLevelZ = C->SeaLevelZ;
    P.BuildingParams.MaxTurretsPerCluster = C->MaxTurretsPerCluster;
    P.BuildingParams.EmbedDepth = C->BuildingEmbedDepth;
    P.BuildingParams.DecoBuildingClearance = C->DecorationBuildingClearance;

    // 敌船生成参数(总开关同时看蓝图是否配置:没配蓝图就不用白算点)
    P.BoatParams.bEnabled = C->bSpawnEnemyBoats && C->EnemyBoatClass != nullptr;
    P.BoatParams.CountRange = C->EnemyBoatCountRange;
    P.BoatParams.RingRadiusMin = C->EnemyBoatRingRadiusMin;
    P.BoatParams.RingRadiusMax = C->EnemyBoatRingRadiusMax;
    P.BoatParams.MinWaterDepth = C->EnemyBoatMinWaterDepth;
    P.BoatParams.MinSeparation = C->EnemyBoatMinSeparation;
    P.BoatParams.SeaLevelZ = C->SeaLevelZ;

    // 炸药桶生成参数(总开关同时看蓝图是否配置:没配蓝图就不用白算点)
    P.BarrelParams.bEnabled = C->bSpawnBarrels && C->BarrelClass != nullptr;
    P.BarrelParams.CountRange = C->BarrelCountRange;
    P.BarrelParams.RingRadiusMin = C->BarrelRingRadiusMin;
    P.BarrelParams.RingRadiusMax = C->BarrelRingRadiusMax;
    P.BarrelParams.MinWaterDepth = C->BarrelMinWaterDepth;
    P.BarrelParams.MinSeparation = C->BarrelMinSeparation;
    P.BarrelParams.SeaLevelZ = C->SeaLevelZ;

    // 装饰规则(树/花草/石头;剔除空 mesh/零权重条目,MaxPerIsland 复用为每组上限)
    auto BuildDecoRule = [](const FOCDecoCategory& Category, FOCDecoRule& OutRule,
        TArray<TObjectPtr<UStaticMesh>>& OutMeshes)
    {
        for (const FOCDecoEntry& Entry : Category.Meshes)
        {
            if (Entry.Mesh && Entry.Weight > 0.0f)
            {
                OutRule.EntryWeights.Add(Entry.Weight);
                OutRule.EntryScaleRanges.Add(Entry.ScaleRange);
                OutMeshes.Add(Entry.Mesh);
            }
        }
        OutRule.Spacing = Category.Spacing;
        OutRule.Jitter = Category.Jitter;
        OutRule.MinGroundClearance = Category.MinGroundClearance;
        OutRule.MaxSlopeDeg = Category.MaxSlopeDeg;
        OutRule.bAlignToNormal = Category.bAlignToNormal;
        OutRule.bCollision = Category.bCollision;
        OutRule.MaxPerGroup = Category.MaxPerIsland;
    };
    if (C->bScatterDecorations)
    {
        BuildDecoRule(C->Trees, P.DecoRules.Emplace_GetRef(), P.DecoMeshes.Emplace_GetRef().Meshes);
        BuildDecoRule(C->Flowers, P.DecoRules.Emplace_GetRef(), P.DecoMeshes.Emplace_GetRef().Meshes);
        BuildDecoRule(C->Rocks, P.DecoRules.Emplace_GetRef(), P.DecoMeshes.Emplace_GetRef().Meshes);
    }

    return P;
}

namespace OCMapGenKernels
{

bool FindFlattestSite(const FOCInfiniteHeightfield& Heightfield, const FOCIslandGroup& Group,
    const FOCBuildingSiteParams& Params, FVector& OutLocation)
{
    const float MinGroundZ = Params.SeaLevelZ + Params.MinGroundClearance;
    const float FootR = Params.FootprintRadius;
    const float Step = Params.SampleStep;

    FVector2D FootOffsets[9];
    FootOffsets[0] = FVector2D::ZeroVector;
    for (int32 k = 0; k < 8; ++k)
    {
        const float Angle = (2.0f * PI) * k / 8.0f;
        FootOffsets[k + 1] = FootR * FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
    }

    bool bFound = false;
    float BestScore = UE_BIG_NUMBER;
    FVector2D BestCenter = FVector2D::ZeroVector;
    float BestMinZ = Params.SeaLevelZ;

    for (float Y = -Group.Radius; Y <= Group.Radius; Y += Step)
    {
        for (float X = -Group.Radius; X <= Group.Radius; X += Step)
        {
            const FVector2D Center = Group.Center + FVector2D(X, Y);

            float MinH = UE_BIG_NUMBER;
            float MaxH = -UE_BIG_NUMBER;
            bool bValid = true;
            for (const FVector2D& Off : FootOffsets)
            {
                const float H = Heightfield.GetHeight(Center + Off);
                if (H < MinGroundZ) // 足印任一点泡水/贴海岸悬空 → 淘汰
                {
                    bValid = false;
                    break;
                }
                MinH = FMath::Min(MinH, H);
                MaxH = FMath::Max(MaxH, H);
            }
            if (!bValid)
            {
                continue;
            }

            const float Score = MaxH - MinH;
            if (Score < BestScore)
            {
                BestScore = Score;
                BestCenter = Center;
                BestMinZ = MinH;
                bFound = true;
            }
        }
    }

    if (bFound)
    {
        // Z 取足印最低点再下沉 EmbedDepth:坡上底座低侧落实、高侧微埋,避免一边悬浮
        OutLocation = FVector(BestCenter.X, BestCenter.Y, BestMinZ - Params.EmbedDepth);
    }
    return bFound;
}

FVector GetNormalFromTable(const FOCInfiniteHeightfield& Heightfield, const FOCRegionBlobTable& Table, FVector2D P)
{
    const float Eps = 50.0f;
    const float DX = Heightfield.GetHeight(P + FVector2D(Eps, 0.0), Table) - Heightfield.GetHeight(P - FVector2D(Eps, 0.0), Table);
    const float DY = Heightfield.GetHeight(P + FVector2D(0.0, Eps), Table) - Heightfield.GetHeight(P - FVector2D(0.0, Eps), Table);
    return FVector(-DX, -DY, 2.0 * Eps).GetSafeNormal();
}

void ScatterDecorationsForGroup(const FOCIslandGroup& Group, FIntPoint Cell, int32 GroupIndex,
    const FOCDecoRule& Rule, int32 RuleIndex, int32 WorldSeed,
    const TArray<FOCBuildingSiteInfo>& Sites, float ClearanceSq,
    const FOCInfiniteHeightfield& Heightfield, const FOCRegionBlobTable& Table,
    TArray<FOCDecoInstance>& OutInstances)
{
    float TotalWeight = 0.0f;
    for (float W : Rule.EntryWeights)
    {
        TotalWeight += W;
    }
    if (TotalWeight <= 0.0f)
    {
        return;
    }

    const uint32 SeedHash = HashCombine(
        HashCombine(GetTypeHash(WorldSeed), GetTypeHash(Cell.X)),
        HashCombine(GetTypeHash(Cell.Y), HashCombine(GetTypeHash(GroupIndex), GetTypeHash(RuleIndex))));
    FRandomStream Rng(static_cast<int32>(SeedHash));

    const float SeaLevelZ = Heightfield.GetSeaLevelZ();
    const float MinHeight = SeaLevelZ + Rule.MinGroundClearance;
    const float MinNormalZ = FMath::Cos(FMath::DegreesToRadians(Rule.MaxSlopeDeg));
    const float JitterAmp = Rule.Spacing * 0.5f * Rule.Jitter;
    int32 Placed = 0;

    for (float Y = -Group.Radius; Y <= Group.Radius; Y += Rule.Spacing)
    {
        for (float X = -Group.Radius; X <= Group.Radius; X += Rule.Spacing)
        {
            if (Rule.MaxPerGroup > 0 && Placed >= Rule.MaxPerGroup)
            {
                return;
            }

            const FVector2D P = Group.Center + FVector2D(X, Y)
                + FVector2D(Rng.FRandRange(-JitterAmp, JitterAmp), Rng.FRandRange(-JitterAmp, JitterAmp));

            // 筛选1:在陆地且离水足够
            const float GroundZ = Heightfield.GetHeight(P, Table);
            if (GroundZ < MinHeight)
            {
                continue;
            }
            // 筛选2:坡度合格
            const FVector Normal = GetNormalFromTable(Heightfield, Table, P);
            if (Normal.Z < MinNormalZ)
            {
                continue;
            }
            // 筛选3:避开建筑落点
            bool bNearBuilding = false;
            for (const FOCBuildingSiteInfo& Site : Sites)
            {
                if (FVector2D::DistSquared(P, FVector2D(Site.Location.X, Site.Location.Y)) < ClearanceSq)
                {
                    bNearBuilding = true;
                    break;
                }
            }
            if (bNearBuilding)
            {
                continue;
            }

            // 加权随机选条目
            float Pick = Rng.FRandRange(0.0f, TotalWeight);
            int32 EntryIdx = Rule.EntryWeights.Num() - 1;
            for (int32 e = 0; e < Rule.EntryWeights.Num(); ++e)
            {
                Pick -= Rule.EntryWeights[e];
                if (Pick <= 0.0f)
                {
                    EntryIdx = e;
                    break;
                }
            }

            // 随机 Yaw / 均匀缩放 / 可选贴地;缩放下限防零缩放(零缩放会让渲染矩阵不可逆)
            const float Yaw = Rng.FRandRange(0.0f, 360.0f);
            const FVector2D ScaleRange = Rule.EntryScaleRanges.IsValidIndex(EntryIdx)
                ? Rule.EntryScaleRanges[EntryIdx] : FVector2D(1.0f, 1.0f);
            const float Scale = FMath::Max(Rng.FRandRange(ScaleRange.X, ScaleRange.Y), 0.01f);
            FQuat Rotation = FRotator(0.0f, Yaw, 0.0f).Quaternion();
            if (Rule.bAlignToNormal)
            {
                Rotation = FQuat::FindBetweenVectors(FVector::UpVector, Normal) * Rotation;
            }

            FOCDecoInstance Inst;
            Inst.Rule = RuleIndex;
            Inst.Entry = EntryIdx;
            Inst.Transform = FTransform(Rotation, FVector(P.X, P.Y, GroundZ), FVector(Scale));
            OutInstances.Add(MoveTemp(Inst));
            ++Placed;
        }
    }
}

void BuildChunkBlobTable(const FOCInfiniteHeightfield& Heightfield, FIntPoint Coord,
    float ChunkSize, float ClusterCellSize, float VertexSpacing, FOCRegionBlobTable& OutTable)
{
    // 区域 = 本区块 ±(1 聚落格 + 1 顶点间距):顶点范围含一圈边界,查询还需 3×3 邻格在表内
    const FVector2D Origin = FVector2D(static_cast<double>(Coord.X), static_cast<double>(Coord.Y)) * ChunkSize;
    const float Margin = ClusterCellSize + VertexSpacing;
    Heightfield.GatherBlobsForRegion(Origin - FVector2D(Margin, Margin),
        Origin + FVector2D(ChunkSize + Margin, ChunkSize + Margin), OutTable);
}

void PickBoatSpawns(const FOCInfiniteHeightfield& Heightfield, const FOCIslandGroup& MainGroup,
    FIntPoint Cell, const FOCBoatSpawnParams& Params, TArray<FOCBoatSpawnInfo>& OutSpawns)
{
    // Clamp 保证区间合法(配置里可能填反),下限不小于 0
    const int32 MinCount = FMath::Max(0, FMath::Min(Params.CountRange.X, Params.CountRange.Y));
    const int32 MaxCount = FMath::Max(0, FMath::Max(Params.CountRange.X, Params.CountRange.Y));
    if (MaxCount <= 0)
    {
        return;
    }

    // 盐值 0x8A17 让本随机流与装饰撒点(用 GroupIndex/RuleIndex)错开,互不干扰
    const uint32 SeedHash = HashCombine(
        HashCombine(GetTypeHash(Heightfield.GetWorldSeed()), GetTypeHash(Cell.X)),
        HashCombine(GetTypeHash(Cell.Y), GetTypeHash(0x8A17)));
    FRandomStream Rng(static_cast<int32>(SeedHash));

    const int32 Count = Rng.RandRange(MinCount, MaxCount);
    if (Count <= 0)
    {
        return;
    }

    // 环带半径也 Clamp,并保证外半径不小于内半径
    const float RadiusMin = FMath::Max(0.0f, FMath::Min(Params.RingRadiusMin, Params.RingRadiusMax));
    const float RadiusMax = FMath::Max(Params.RingRadiusMin, Params.RingRadiusMax);
    const float MinSepSq = Params.MinSeparation * Params.MinSeparation;

    // 每个点最多试这么多次:摇不到合格水域(如聚落被大陆包住)就少放几艘,不死循环
    constexpr int32 MaxAttemptsPerBoat = 24;

    OutSpawns.Reserve(Count);
    for (int32 i = 0; i < Count; ++i)
    {
        for (int32 Attempt = 0; Attempt < MaxAttemptsPerBoat; ++Attempt)
        {
            const float Angle = Rng.FRandRange(0.0f, 2.0f * PI);
            const float Radius = Rng.FRandRange(RadiusMin, RadiusMax);
            const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
            const FVector2D P = MainGroup.Center + Dir * Radius;

            // 筛选1:水深足够(GetWaterDepth 为负表示陆地露出水面)
            if (Heightfield.GetWaterDepth(P) < Params.MinWaterDepth)
            {
                continue;
            }

            // 筛选2:与已选点保持间距
            bool bTooClose = false;
            for (const FOCBoatSpawnInfo& Existing : OutSpawns)
            {
                if (FVector2D::DistSquared(FVector2D(Existing.Location), P) < MinSepSq)
                {
                    bTooClose = true;
                    break;
                }
            }
            if (bTooClose)
            {
                continue;
            }

            // Z 取海平面:水面是固定高度的平面(海面管理器只做 XY 平移),浮力组件会接管姿态
            FOCBoatSpawnInfo Info;
            Info.Location = FVector(P.X, P.Y, Params.SeaLevelZ);
            Info.YawDeg = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)); // 朝聚落外侧
            OutSpawns.Add(Info);
            break;
        }
    }
}

void PickBarrelSpawns(const FOCInfiniteHeightfield& Heightfield, const FOCIslandGroup& MainGroup,
    FIntPoint Cell, const FOCBarrelSpawnParams& Params, TArray<FOCBarrelSpawnInfo>& OutSpawns)
{
    const int32 MinCount = FMath::Max(0, FMath::Min(Params.CountRange.X, Params.CountRange.Y));
    const int32 MaxCount = FMath::Max(0, FMath::Max(Params.CountRange.X, Params.CountRange.Y));
    if (MaxCount <= 0)
    {
        return;
    }

    // 盐值 0xB27E 与敌船(0x8A17)/装饰错开,让炸药桶随机流独立
    const uint32 SeedHash = HashCombine(
        HashCombine(GetTypeHash(Heightfield.GetWorldSeed()), GetTypeHash(Cell.X)),
        HashCombine(GetTypeHash(Cell.Y), GetTypeHash(0xB27E)));
    FRandomStream Rng(static_cast<int32>(SeedHash));

    const int32 Count = Rng.RandRange(MinCount, MaxCount);
    if (Count <= 0)
    {
        return;
    }

    const float RadiusMin = FMath::Max(0.0f, FMath::Min(Params.RingRadiusMin, Params.RingRadiusMax));
    const float RadiusMax = FMath::Max(Params.RingRadiusMin, Params.RingRadiusMax);
    const float MinSepSq = Params.MinSeparation * Params.MinSeparation;

    constexpr int32 MaxAttemptsPerBarrel = 24;

    OutSpawns.Reserve(Count);
    for (int32 i = 0; i < Count; ++i)
    {
        for (int32 Attempt = 0; Attempt < MaxAttemptsPerBarrel; ++Attempt)
        {
            const float Angle = Rng.FRandRange(0.0f, 2.0f * PI);
            const float Radius = Rng.FRandRange(RadiusMin, RadiusMax);
            const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
            const FVector2D P = MainGroup.Center + Dir * Radius;

            // 筛选1:水深足够(GetWaterDepth 为负表示陆地露出水面)
            if (Heightfield.GetWaterDepth(P) < Params.MinWaterDepth)
            {
                continue;
            }

            // 筛选2:与已选桶保持间距(避免生成时重叠被物理弹飞)
            bool bTooClose = false;
            for (const FOCBarrelSpawnInfo& Existing : OutSpawns)
            {
                if (FVector2D::DistSquared(FVector2D(Existing.Location), P) < MinSepSq)
                {
                    bTooClose = true;
                    break;
                }
            }
            if (bTooClose)
            {
                continue;
            }

            FOCBarrelSpawnInfo Info;
            Info.Location = FVector(P.X, P.Y, Params.SeaLevelZ);
            Info.YawDeg = Rng.FRandRange(0.0f, 360.0f); // 纯随机朝向,外观错开
            OutSpawns.Add(Info);
            break;
        }
    }
}

FOCClusterContentData ComputeClusterContent(
    FIntPoint Cell, const FOCInfiniteHeightfield& Heightfield,
    const FOCBuildingSiteParams& BuildingParams, const FOCBoatSpawnParams& BoatParams,
    const FOCBarrelSpawnParams& BarrelParams,
    const TArray<FOCDecoRule>& DecoRules)
{
    FOCClusterContentData Result;
    Result.Cell = Cell;

    // 1~3. 分组 + 归属:只留"我是主人"的组,按包围半径降序(最大组放城堡)
    TArray<FOCIslandGroup> Groups;
    Heightfield.GetIslandGroupsAroundCell(Cell, Groups);
    Groups.RemoveAll([Cell](const FOCIslandGroup& G) { return G.OwnerCell != Cell; });
    Groups.Sort([](const FOCIslandGroup& A, const FOCIslandGroup& B) { return A.Radius > B.Radius; });

    if (Groups.Num() == 0)
    {
        return Result;
    }

    // 4. 建筑落点:最大组城堡,其后最多 MaxTurretsPerCluster 个组防御塔
    int32 TurretCount = 0;
    for (int32 g = 0; g < Groups.Num(); ++g)
    {
        const bool bIsCastle = (g == 0);
        if (!bIsCastle && TurretCount >= BuildingParams.MaxTurretsPerCluster)
        {
            break;
        }
        FVector Location;
        if (FindFlattestSite(Heightfield, Groups[g], BuildingParams, Location))
        {
            Result.Sites.Add(FOCBuildingSiteInfo{ Location, bIsCastle });
            if (!bIsCastle)
            {
                ++TurretCount;
            }
        }
    }

    // 4.5 敌船生成点:在最大组(主岛)周围的水域环带上摇点
    if (BoatParams.bEnabled)
    {
        PickBoatSpawns(Heightfield, Groups[0], Cell, BoatParams, Result.BoatSpawns);
    }

    // 4.6 炸药桶生成点:同样在主岛周围水域环带上摇零散单个
    if (BarrelParams.bEnabled)
    {
        PickBarrelSpawns(Heightfield, Groups[0], Cell, BarrelParams, Result.BarrelSpawns);
    }

    // 5. 装饰撒点(有装饰规则时):blob 表覆盖拥有组包围盒并集 + 一格余量(查询 3×3 须在表内)
    if (DecoRules.Num() > 0)
    {
        FBox2D Bounds(ForceInit);
        for (const FOCIslandGroup& G : Groups)
        {
            Bounds += FBox2D(G.Center - FVector2D(G.Radius, G.Radius), G.Center + FVector2D(G.Radius, G.Radius));
        }

        const float Margin = Heightfield.GetClusterCellSize();
        FOCRegionBlobTable Table;
        Heightfield.GatherBlobsForRegion(Bounds.Min - FVector2D(Margin, Margin),
            Bounds.Max + FVector2D(Margin, Margin), Table);

        const float ClearanceSq = BuildingParams.DecoBuildingClearance * BuildingParams.DecoBuildingClearance;
        const int32 WorldSeed = Heightfield.GetWorldSeed();
        for (int32 g = 0; g < Groups.Num(); ++g)
        {
            for (int32 r = 0; r < DecoRules.Num(); ++r)
            {
                ScatterDecorationsForGroup(Groups[g], Cell, g, DecoRules[r], r, WorldSeed,
                    Result.Sites, ClearanceSq, Heightfield, Table, Result.Decorations);
            }
        }
    }

    // 6. 记录拥有组包围盒(主线程等地形就绪用)
    for (const FOCIslandGroup& G : Groups)
    {
        Result.GroupBounds.Add(FBox2D(G.Center - FVector2D(G.Radius, G.Radius), G.Center + FVector2D(G.Radius, G.Radius)));
    }
    return Result;
}

} // namespace OCMapGenKernels
