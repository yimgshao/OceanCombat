// OceanCombat. Copyright(c) All rights reserved.

#include "Procedural/OCInfiniteMapManager.h"

#include "Async/Async.h"
#include "Components/StaticMeshComponent.h"
#include "Data/OCDifficultyTierRow.h"
#include "Engine/StaticMesh.h"
#include "GameFlow/OCGameMode.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h"
#include "Pawns/Buildings/OCDefenseBuilding.h"
#include "Pawns/Boats/OCEnemyBoat.h"
#include "Hazards/OCExplosiveBarrel.h"
#include "Procedural/OCMapGenConfig.h"
#include "Procedural/OCMapGenKernels.h"

// 纯生成逻辑(落点/撒点/聚落内容)已抽取到 OCMapGenKernels,与菜单烘焙器共用同一条代码路径。


AOCInfiniteMapManager::AOCInfiniteMapManager()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AOCInfiniteMapManager::BeginPlay()
{
    Super::BeginPlay();
    InitializeGenerator();
}

void AOCInfiniteMapManager::InitializeGenerator()
{
    ActiveConfig = Config ? Config.Get() : GetDefault<UOCMapGenConfig>();

    // 参数管道与菜单烘焙器共用(OCMapGenKernels):同种子下烘焙结果与实机逐位一致
    const FOCMapGenRuntimeParams GenParams = FOCMapGenRuntimeParams::FromConfig(ActiveConfig);
    WorldSeed = GenParams.WorldSeed;

    Heightfield = MakeShared<FOCInfiniteHeightfield>();
    Heightfield->Initialize(WorldSeed, GenParams.FieldParams, GenParams.LayoutParams);

    // 窗口参数
    ChunkSize = ActiveConfig->ChunkSize;
    ActiveRadius = ActiveConfig->ActiveRadius;
    UnloadMargin = ActiveConfig->UnloadMargin;
    MaxCommitsPerFrame = ActiveConfig->MaxChunkCommitsPerFrame;
    bAsyncBuild = ActiveConfig->bAsyncChunkBuild;

    BuildParams = GenParams.ChunkBuildParams;
    BuildingParams = GenParams.BuildingParams;

    BoatParams = GenParams.BoatParams;
    BoatDespawnDistance = ActiveConfig->EnemyBoatDespawnDistance;

    BarrelParams = GenParams.BarrelParams;

    DecoRules = GenParams.DecoRules;
    DecoMeshes = GenParams.DecoMeshes;

    UE_LOG(LogTemp, Log, TEXT("[OCInfiniteMap] 无限地图已启动:种子=%d,区块=%.0fcm,窗口=%d×%d(半径%d+滞后%d),顶点间距=%.1fcm,异步构建=%s"),
        WorldSeed, ChunkSize, ActiveRadius * 2 + 1, ActiveRadius * 2 + 1, ActiveRadius, UnloadMargin,
        BuildParams.VertexSpacing, bAsyncBuild ? TEXT("开") : TEXT("关"));
}

void AOCInfiniteMapManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    DrainCompletedChunks();
    DrainCompletedClusters();

    TimeSinceLastUpdate += DeltaSeconds;
    if (TimeSinceLastUpdate >= 0.25f)
    {
        TimeSinceLastUpdate = 0.0f;
        UpdateChunks();
    }
}

/** 获取玩家的区块坐标<OutCoord>和世界坐标<OutPos> **/
bool AOCInfiniteMapManager::GetPlayerChunkCoord(FIntPoint& OutCoord, FVector& OutPos) const
{
    const UWorld* World = GetWorld();
    const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn)
    {
        return false;
    }
    OutPos = Pawn->GetActorLocation();
    OutCoord = FIntPoint(
        FMath::FloorToInt(OutPos.X / ChunkSize),
        FMath::FloorToInt(OutPos.Y / ChunkSize));
    return true;
}

void AOCInfiniteMapManager::UpdateChunks()
{
    FIntPoint PlayerChunk;
    FVector PlayerPos;
    if (!GetPlayerChunkCoord(PlayerChunk, PlayerPos))
    {
        return;
    }
    LastPlayerChunk = PlayerChunk;
    bHavePlayerChunk = true;

    // 1. 回收:超出 窗口半径+滞后 的区块直接 Destroy(内容为种子纯函数,重进时再生成)
    const int32 KeepDistance = ActiveRadius + UnloadMargin;
    for (auto It = ActiveChunks.CreateIterator(); It; ++It)
    {
        if (ChunkDistance(It.Key(), PlayerChunk) > KeepDistance)
        {
            if (It.Value())
            {
                It.Value()->Destroy();
            }
            It.RemoveCurrent();
        }
    }

    // 2. 加载:窗口内缺失的区块启动构建
    for (int32 DY = -ActiveRadius; DY <= ActiveRadius; ++DY)
    {
        for (int32 DX = -ActiveRadius; DX <= ActiveRadius; ++DX)
        {
            const FIntPoint Coord = PlayerChunk + FIntPoint(DX, DY);
            if (ActiveChunks.Contains(Coord) || LoadingChunks.Contains(Coord))
            {
                continue;
            }

            if (bAsyncBuild)
            {
                LoadingChunks.Add(Coord);
                const TSharedPtr<FOCInfiniteHeightfield> HeightfieldCopy = Heightfield; // 共享所有权,防悬垂
                const FOCChunkBuildParams Params = BuildParams;
                const float ClusterCellSize = ActiveConfig->ClusterCellSize;
                const float LocalChunkSize = ChunkSize;
                const TSharedPtr<TQueue<FCompletedChunk, EQueueMode::Mpsc>> Queue = CompletedChunks; // 共享所有权,防悬垂
                Async(EAsyncExecution::ThreadPool, [Coord, HeightfieldCopy, Params, ClusterCellSize, LocalChunkSize, Queue]()
                {
                    FOCRegionBlobTable Table;
                    OCMapGenKernels::BuildChunkBlobTable(*HeightfieldCopy, Coord, LocalChunkSize, ClusterCellSize, Params.VertexSpacing, Table);
                    TSharedPtr<FOCChunkMeshData> Data = AOCMapChunk::BuildMeshData(Coord, *HeightfieldCopy, Table, Params);
                    // 无条件入队:TWeakObjectPtr 不能在工作线程解引用(GC 标记期会误报空,导致结果丢失、LoadingChunks 永久卡死)。
                    // Manager 销毁时游戏线程不再消费,纯数据随队列引用计数自动释放。
                    Queue->Enqueue(FCompletedChunk{ Coord, MoveTemp(Data) });
                });
            }
            else
            {
                FOCRegionBlobTable Table;
                OCMapGenKernels::BuildChunkBlobTable(*Heightfield, Coord, ChunkSize, ActiveConfig->ClusterCellSize, BuildParams.VertexSpacing, Table);
                TSharedPtr<FOCChunkMeshData> Data = AOCMapChunk::BuildMeshData(Coord, *Heightfield, Table, BuildParams);
                SpawnChunk(Coord, Data);
            }
        }
    }

    // 3. 聚落内容调度(拥有的岛组:建筑 + 装饰)
    UpdateClusterBuildings(PlayerPos);

    // 4. 敌船回收(按船自身离玩家距离,不随聚落回收)
    UpdateEnemyBoats(PlayerPos);

    // 5. 深海平面跟随(兜住被剔除的深海区域)
    UpdateSeabedPlane(PlayerPos);
}

void AOCInfiniteMapManager::DrainCompletedChunks()
{
    for (int32 Count = 0; Count < MaxCommitsPerFrame; ++Count)
    {
        FCompletedChunk Completed;
        if (!CompletedChunks->Dequeue(Completed))
        {
            break;
        }

        LoadingChunks.Remove(Completed.Coord);

        // 完成后已飞出窗口(玩家跑远了):丢弃数据,等下次进入窗口再生成
        if (bHavePlayerChunk && ChunkDistance(Completed.Coord, LastPlayerChunk) > ActiveRadius + UnloadMargin)
        {
            continue;
        }
        if (ActiveChunks.Contains(Completed.Coord))
        {
            continue; // 已存在(理论不发生,防御)
        }
        SpawnChunk(Completed.Coord, Completed.Data);
    }
}

void AOCInfiniteMapManager::SpawnChunk(FIntPoint Coord, const TSharedPtr<FOCChunkMeshData>& Data)
{
    if (!Data.IsValid())
    {
        return;
    }
    if (Data->Vertices.Num() == 0)
    {
        // 全深海区块:不 spawn,登记空值表示"已就绪"(深海平面兜底;避免每 tick 重复构建)
        ActiveChunks.Add(Coord, nullptr);
        return;
    }
    AOCMapChunk* Chunk = GetWorld()->SpawnActor<AOCMapChunk>();
    if (!Chunk)
    {
        return;
    }
    Chunk->ApplyMeshData(Data, ActiveConfig->TerrainMaterial.Get());
    ActiveChunks.Add(Coord, Chunk);
}

void AOCInfiniteMapManager::UpdateSeabedPlane(const FVector& PlayerPos)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 惰性创建:平面网格(默认引擎基本平面)+ 深色材质,Movable,阻挡碰撞(接住下沉物体)、无投影
    if (!SeabedPlane)
    {
        UStaticMesh* PlaneMesh = ActiveConfig->SeabedPlaneMesh.Get();
        if (!PlaneMesh)
        {
            PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
        }
        if (!PlaneMesh)
        {
            return;
        }

        SeabedPlane = World->SpawnActor<AActor>();
        if (!SeabedPlane)
        {
            return;
        }
        UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(SeabedPlane, TEXT("SeabedPlaneMesh"));
        SeabedPlane->SetRootComponent(Comp);
        Comp->SetStaticMesh(PlaneMesh);
        Comp->SetMobility(EComponentMobility::Movable);
        // 与 terrain chunk(AOCMapChunk::SeabedMesh)一致:阻挡所有,接住深海区下沉的物体,不让其穿透
        Comp->SetCollisionProfileName(TEXT("BlockAll"));
        Comp->SetCastShadow(false);
        Comp->RegisterComponent();

        UMaterialInterface* Mat = ActiveConfig->SeabedPlaneMaterial.Get();
        if (!Mat)
        {
            UE_LOG(LogTemp, Warning, TEXT("[OCInfiniteMap] 深海平面材质未配置,使用引擎默认材质。请在 DA_MapGenConfig 配置 SeabedPlaneMaterial(深色)"));
            Mat = UMaterial::GetDefaultMaterial(MD_Surface);
        }
        Comp->SetMaterial(0, Mat);

        // 引擎基本平面为 100×100cm,缩放到覆盖范围
        const float Scale = ActiveConfig->SeabedPlaneExtent / 100.0f;
        SeabedPlane->SetActorScale3D(FVector(Scale));
        UE_LOG(LogTemp, Log, TEXT("[OCInfiniteMap] 深海平面已创建(覆盖 %.0f cm)"), ActiveConfig->SeabedPlaneExtent);
    }

    // 吸附步长跟随:偏离超过步长才平移,保持网格相位
    const float Snap = ActiveConfig->SeabedPlaneSnapSize;
    const FVector2D Snapped(
        FMath::GridSnap(PlayerPos.X, Snap),
        FMath::GridSnap(PlayerPos.Y, Snap));

    // Z 每次检测都校准(配置改了 SeabedPlaneZ 也能即时生效)
    const float PlaneZ = ActiveConfig->SeabedPlaneZ;

    const bool bMoved = !Snapped.Equals(SeabedPlaneCenter);
    const bool bZDirty = !FMath::IsNearlyEqual(static_cast<float>(SeabedPlane->GetActorLocation().Z), PlaneZ, 0.1f);
    if (!bMoved && !bZDirty)
    {
        return;
    }
    SeabedPlaneCenter = Snapped;
    SeabedPlane->SetActorLocation(FVector(Snapped.X, Snapped.Y, PlaneZ),
        /*bSweep=*/false, /*OutHit=*/nullptr, ETeleportType::TeleportPhysics);
}

void AOCInfiniteMapManager::UpdateClusterBuildings(const FVector& PlayerPos)
{
    // 聚落内容管线服务于建筑/装饰/敌船/炸药桶 —— 只要有一类启用就需要运行
    if (!ActiveConfig->bPlaceBuildings && DecoRules.Num() == 0
        && !BoatParams.bEnabled && !BarrelParams.bEnabled)
    {
        return;
    }

    const float CellSize = ActiveConfig->ClusterCellSize;
    // 加载范围 = 区块窗口;卸载范围 = 窗口+滞后(与区块回收同一滞回逻辑)
    const float LoadDist = ActiveRadius * ChunkSize;
    const float KeepDist = (ActiveRadius + UnloadMargin) * ChunkSize;

    auto CellRangeFromDist = [CellSize](const FVector& P, float Dist, FIntPoint& OutMin, FIntPoint& OutMax)
    {
        OutMin = FIntPoint(FMath::FloorToInt((P.X - Dist) / CellSize) - 1, FMath::FloorToInt((P.Y - Dist) / CellSize) - 1);
        OutMax = FIntPoint(FMath::FloorToInt((P.X + Dist) / CellSize) + 1, FMath::FloorToInt((P.Y + Dist) / CellSize) + 1);
    };

    // 1. 回收:飞出 窗口+滞后 的聚落,整批销毁(内容为种子纯函数,重进时再生成)
    FIntPoint KeepMin, KeepMax;
    CellRangeFromDist(PlayerPos, KeepDist, KeepMin, KeepMax);
    for (auto It = ClusterContents.CreateIterator(); It; ++It)
    {
        const FIntPoint Cell = It.Key();
        if (Cell.X >= KeepMin.X && Cell.X <= KeepMax.X && Cell.Y >= KeepMin.Y && Cell.Y <= KeepMax.Y)
        {
            continue;
        }
        if (AOCClusterContent* Content = It.Value().ContentActor.Get())
        {
            Content->Destroy();
        }
        for (const TWeakObjectPtr<AOCDefenseBuilding>& Weak : It.Value().Buildings)
        {
            if (AOCDefenseBuilding* Building = Weak.Get())
            {
                Building->Destroy();
            }
        }
        for (const TWeakObjectPtr<AOCExplosiveBarrel>& Weak : It.Value().Barrels)
        {
            if (AOCExplosiveBarrel* Barrel = Weak.Get())
            {
                Barrel->Destroy();
            }
        }
        It.RemoveCurrent();
    }

    // 2. 加载:窗口内缺失的聚落格,后台线程算内容(岛组归属 + 落点 + 撒点)
    FIntPoint LoadMin, LoadMax;
    CellRangeFromDist(PlayerPos, LoadDist, LoadMin, LoadMax);
    for (int32 Y = LoadMin.Y; Y <= LoadMax.Y; ++Y)
    {
        for (int32 X = LoadMin.X; X <= LoadMax.X; ++X)
        {
            const FIntPoint Cell(X, Y);
            if (ClusterContents.Contains(Cell) || LoadingClusters.Contains(Cell))
            {
                continue;
            }
            LoadingClusters.Add(Cell);
            const TSharedPtr<FOCInfiniteHeightfield> HeightfieldCopy = Heightfield; // 共享所有权,防悬垂
            const FOCBuildingSiteParams BParams = BuildingParams;
            const FOCBoatSpawnParams BoatP = BoatParams;
            const FOCBarrelSpawnParams BarrelP = BarrelParams;
            const TArray<FOCDecoRule> DRules = DecoRules;
            const TSharedPtr<TQueue<FCompletedCluster, EQueueMode::Mpsc>> Queue = CompletedClusters; // 共享所有权,防悬垂
            Async(EAsyncExecution::ThreadPool, [Cell, HeightfieldCopy, BParams, BoatP, BarrelP, DRules, Queue]()
            {
                FCompletedCluster Done = OCMapGenKernels::ComputeClusterContent(Cell, *HeightfieldCopy, BParams, BoatP, BarrelP, DRules);
                // 无条件入队,原因同区块任务(见 UpdateChunks)
                Queue->Enqueue(MoveTemp(Done));
            });
        }
    }

    // 3. 重试待 spawn 的格子(地形可能已就绪)
    for (auto& Pair : ClusterContents)
    {
        if (!Pair.Value.bSpawned)
        {
            TrySpawnClusterContent(Pair.Key);
        }
    }
}

void AOCInfiniteMapManager::DrainCompletedClusters()
{
    for (int32 Count = 0; Count < MaxCommitsPerFrame; ++Count)
    {
        FCompletedCluster Completed;
        if (!CompletedClusters->Dequeue(Completed))
        {
            break;
        }

        LoadingClusters.Remove(Completed.Cell);

        // 完成后已飞出窗口(玩家跑远了):丢弃,等下次进入再算
        if (bHavePlayerChunk)
        {
            const float CellSize = ActiveConfig->ClusterCellSize;
            const FVector2D CellCenter = (FVector2D(static_cast<double>(Completed.Cell.X), static_cast<double>(Completed.Cell.Y)) + 0.5) * CellSize;
            const FVector2D PlayerCenter = (FVector2D(static_cast<double>(LastPlayerChunk.X), static_cast<double>(LastPlayerChunk.Y)) + 0.5) * ChunkSize;
            if (static_cast<float>((CellCenter - PlayerCenter).GetAbsMax()) > (ActiveRadius + UnloadMargin) * ChunkSize + CellSize)
            {
                continue;
            }
        }

        FClusterContent& Entry = ClusterContents.FindOrAdd(Completed.Cell);
        Entry.GroupBounds = MoveTemp(Completed.GroupBounds);
        Entry.Sites = MoveTemp(Completed.Sites);
        Entry.BoatSpawns = MoveTemp(Completed.BoatSpawns);
        Entry.BarrelSpawns = MoveTemp(Completed.BarrelSpawns);
        Entry.Decorations = MoveTemp(Completed.Decorations);

        TrySpawnClusterContent(Completed.Cell);
    }
}

void AOCInfiniteMapManager::TrySpawnClusterContent(FIntPoint Cell)
{
    FClusterContent* Entry = ClusterContents.Find(Cell);
    if (!Entry || Entry->bSpawned)
    {
        return;
    }

    // 地形就绪检查:拥有组覆盖的区块须全部激活(不浮空)
    bool bTerrainReady = true;
    for (const FBox2D& Bounds : Entry->GroupBounds)
    {
        const FIntPoint MinChunk(
            FMath::FloorToInt(Bounds.Min.X / ChunkSize),
            FMath::FloorToInt(Bounds.Min.Y / ChunkSize));
        const FIntPoint MaxChunk(
            FMath::FloorToInt(Bounds.Max.X / ChunkSize),
            FMath::FloorToInt(Bounds.Max.Y / ChunkSize));
        for (int32 Y = MinChunk.Y; Y <= MaxChunk.Y && bTerrainReady; ++Y)
        {
            for (int32 X = MinChunk.X; X <= MaxChunk.X; ++X)
            {
                if (!ActiveChunks.Contains(FIntPoint(X, Y)))
                {
                    bTerrainReady = false;
                    break;
                }
            }
        }
        if (!bTerrainReady)
        {
            break;
        }
    }
    if (!bTerrainReady)
    {
        return; // 等地形,下个 tick 重试
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 装饰:spawn 内容 Actor 并写入 HISM
    if (Entry->Decorations.Num() > 0)
    {
        AOCClusterContent* Content = World->SpawnActor<AOCClusterContent>();
        if (Content)
        {
            Content->ApplyDecorations(Entry->Decorations, DecoRules, DecoMeshes);
            Entry->ContentActor = Content;
        }
    }

    // 建筑:最大组城堡(打 Castle 标签),其余防御塔
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (const FOCBuildingSiteInfo& Site : Entry->Sites)
    {
        TSubclassOf<AOCDefenseBuilding> BuildingClass =
            Site.bIsCastle ? ActiveConfig->CastleClass : ActiveConfig->TurretClass;
        if (!BuildingClass)
        {
            if (!bLoggedMissingBuildingClass)
            {
                bLoggedMissingBuildingClass = true;
                UE_LOG(LogTemp, Warning, TEXT("[OCInfiniteMap] 建筑蓝图未配置(CastleClass/TurretClass),请在 DA_MapGenConfig 配置"));
            }
            continue;
        }

        AOCDefenseBuilding* Building = World->SpawnActor<AOCDefenseBuilding>(BuildingClass, FTransform(Site.Location), SpawnParams);
        if (!Building)
        {
            continue;
        }
        if (Site.bIsCastle)
        {
            Building->Tags.Add(TEXT("Castle")); // 与旧管线一致,参与胜利判定
        }

        // 难度分档:按离出生点距离查表,血量/伤害/得分系数一次性烘焙到实例上
        if (const FOCDifficultyTierRow* Tier = FindTierForLocation(Site.Location))
        {
            Building->ApplyDifficultyScaling(Tier->HealthMultiplier, Tier->DamageMultiplier, Tier->ScoreMultiplier);
            UE_LOG(LogTemp, Log, TEXT("[OCInfiniteMap] %s 距离出生点 %.0fcm,命中分档(MinDist=%.0f):血量x%.2f 伤害x%.2f 得分x%.2f"),
                *Building->GetName(), FVector2D::Distance(FVector2D(Site.Location), DifficultyOrigin.Get(FVector2D::ZeroVector)),
                Tier->MinDistance, Tier->HealthMultiplier, Tier->DamageMultiplier, Tier->ScoreMultiplier);
        }

        // 注册到 GameMode:被玩家击杀时按 ScoreValue 加分
        if (AOCGameMode* GameMode = World->GetAuthGameMode<AOCGameMode>())
        {
            GameMode->RegisterCombatant(Building);
        }

        Entry->Buildings.Add(Building);
    }

    // 敌船:自带 AI 控制器(AutoPossessAI),spawn 即自主巡弋。
    // 刻意不记入 Entry:船会追着玩家跑出本聚落,回收按船自身离玩家距离(UpdateEnemyBoats)
    if (ActiveConfig->bSpawnEnemyBoats && ActiveConfig->EnemyBoatClass)
    {
        for (const FOCBoatSpawnInfo& BoatSpawn : Entry->BoatSpawns)
        {
            const FTransform BoatTransform(FRotator(0.0f, BoatSpawn.YawDeg, 0.0f), BoatSpawn.Location);
            AOCEnemyBoat* Boat = World->SpawnActor<AOCEnemyBoat>(ActiveConfig->EnemyBoatClass, BoatTransform, SpawnParams);
            if (!Boat)
            {
                continue;
            }

            // 难度分档与加分注册:与建筑完全同一套
            if (const FOCDifficultyTierRow* Tier = FindTierForLocation(BoatSpawn.Location))
            {
                Boat->ApplyDifficultyScaling(Tier->HealthMultiplier, Tier->DamageMultiplier, Tier->ScoreMultiplier);
                UE_LOG(LogTemp, Log, TEXT("[OCInfiniteMap] %s 距离出生点 %.0fcm,命中分档(MinDist=%.0f):血量x%.2f 伤害x%.2f 得分x%.2f"),
                    *Boat->GetName(), FVector2D::Distance(FVector2D(BoatSpawn.Location), DifficultyOrigin.Get(FVector2D::ZeroVector)),
                    Tier->MinDistance, Tier->HealthMultiplier, Tier->DamageMultiplier, Tier->ScoreMultiplier);
            }

            if (AOCGameMode* GameMode = World->GetAuthGameMode<AOCGameMode>())
            {
                GameMode->RegisterCombatant(Boat);
            }

            SpawnedBoats.Add(Boat);
        }
    }

    // 炸药桶:中立漂浮障碍。记入 Entry->Barrels(弱引用),随聚落一起回收(桶是静物,不追玩家)。
    // 不做难度分档、不注册 GameMode(桶不是敌人,不给击杀分;它炸死的敌人归属由爆炸 instigator 传递)。
    if (ActiveConfig->bSpawnBarrels && ActiveConfig->BarrelClass)
    {
        for (const FOCBarrelSpawnInfo& BarrelSpawn : Entry->BarrelSpawns)
        {
            const FTransform BarrelTransform(FRotator(0.0f, BarrelSpawn.YawDeg, 0.0f), BarrelSpawn.Location);
            AOCExplosiveBarrel* Barrel = World->SpawnActor<AOCExplosiveBarrel>(ActiveConfig->BarrelClass, BarrelTransform, SpawnParams);
            if (Barrel)
            {
                Entry->Barrels.Add(Barrel);
            }
        }
    }

    Entry->bSpawned = true;
}

void AOCInfiniteMapManager::UpdateEnemyBoats(const FVector& PlayerPos)
{
    const FVector2D PlayerXY(PlayerPos);
    const float DespawnDistSq = BoatDespawnDistance * BoatDespawnDistance;

    // 弱引用失效(已被打沉销毁)的直接摘掉;离玩家过远的销毁后摘掉。
    // 用 2D 距离:船恒在水面,Z 差固定,与 FindTierForLocation 的距离语义一致
    SpawnedBoats.RemoveAllSwap([&PlayerXY, DespawnDistSq](const TWeakObjectPtr<AOCEnemyBoat>& Weak)
    {
        AOCEnemyBoat* Boat = Weak.Get();
        if (!Boat)
        {
            return true;
        }
        if (FVector2D::DistSquared(FVector2D(Boat->GetActorLocation()), PlayerXY) > DespawnDistSq)
        {
            Boat->Destroy();
            return true;
        }
        return false;
    });
}

const FOCDifficultyTierRow* AOCInfiniteMapManager::FindTierForLocation(const FVector& WorldLocation)
{
    const UDataTable* TierTable = ActiveConfig ? ActiveConfig->DifficultyTierTable : nullptr;
    if (!TierTable)
    {
        return nullptr;
    }

    // 零点懒取:GameMode 在 BeginPlay 记录出生点,首次查档时缓存(避免初始化时序依赖)
    if (!DifficultyOrigin.IsSet())
    {
        const AOCGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOCGameMode>() : nullptr;
        if (!GameMode)
        {
            return nullptr;
        }
        DifficultyOrigin = FVector2D(GameMode->GetDifficultyOrigin());
    }

    const float Distance = FVector2D::Distance(FVector2D(WorldLocation), DifficultyOrigin.GetValue());

    // 取 MinDistance <= 距离 的最后一行(表按 MinDistance 升序填写,首行 0 兜底)
    const FOCDifficultyTierRow* Best = nullptr;
    for (const TPair<FName, uint8*>& Pair : TierTable->GetRowMap())
    {
        const FOCDifficultyTierRow* Row = reinterpret_cast<const FOCDifficultyTierRow*>(Pair.Value);
        if (Row && Row->MinDistance <= Distance && (!Best || Row->MinDistance > Best->MinDistance))
        {
            Best = Row;
        }
    }
    return Best;
}
