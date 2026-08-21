// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Procedural/OCInfiniteHeightfield.h"
#include "Procedural/OCMapChunk.h"
#include "Procedural/OCClusterContent.h"
#include "Procedural/OCMapGenKernels.h"
#include "OCInfiniteMapManager.generated.h"

class UOCMapGenConfig;
class AOCDefenseBuilding;
class AOCEnemyBoat;
class AOCExplosiveBarrel;
struct FOCDifficultyTierRow;

/**
 * 无限地图管理器:区块窗口 + 聚落内容调度。
 * 由 AOCGameMode::InitGame spawn。
 *
 * 每 0.25s 以玩家为中心维持 (2R+1)^2 的区块窗口:
 *   - 缺失区块:后台线程构建网格(任务局部 blob 表),游戏线程每帧限量提交;
 *   - 超出 窗口半径 + UnloadMargin 的区块:Destroy 回收(内容为种子纯函数,回收即丢弃)。
 * 聚落格进窗口 → 后台算"拥有的岛组"的建筑落点与装饰(归属判定,天然无重叠),
 * 等地形就绪后 spawn;出窗口整批回收。
 *
 * 调试控制台命令:
 *   OC.TestHeight <X> <Y>  —— 打印该世界坐标的海床高度/水深
 *   OC.MapStats            —— 打印常驻/加载中区块数(验证资源无累积)
 *   OC.ChunkAsync <0|1>    —— 切换异步构建(默认开)
 */
UCLASS()
class OCEANCOMBAT_API AOCInfiniteMapManager : public AActor
{
    GENERATED_BODY()

public:
    AOCInfiniteMapManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** 生成配置,由 GameMode 注入。留空走 UOCMapGenConfig 的 CDO 默认值 */
    UPROPERTY(EditAnywhere, Category = "InfiniteMap")
    TObjectPtr<UOCMapGenConfig> Config;

    /** 聚落内容计算完成结果(归属判定后的成品,纯数据);结构本体在 OCMapGenKernels.h,烘焙器共用 */
    using FCompletedCluster = FOCClusterContentData;

private:
    /** 后台任务完成结果:游戏线程按每帧预算消费 */
    struct FCompletedChunk
    {
        FIntPoint Coord;
        TSharedPtr<FOCChunkMeshData> Data;
    };

    /** 一个聚落格的内容状态 */
    struct FClusterContent
    {
        /** 已 spawn 标记(false = 等地形就绪) */
        bool bSpawned = false;

        TArray<FBox2D> GroupBounds;
        TArray<FOCBuildingSiteInfo> Sites;
        TArray<FOCBoatSpawnInfo> BoatSpawns;
        TArray<FOCBarrelSpawnInfo> BarrelSpawns;
        TArray<FOCDecoInstance> Decorations;

        /** 内容 Actor(装饰 HISM 宿主;弱引用) */
        TWeakObjectPtr<AOCClusterContent> ContentActor;

        /** 已 spawn 的建筑(弱引用:战斗中被打掉的回收时跳过) */
        TArray<TWeakObjectPtr<AOCDefenseBuilding>> Buildings;

        /** 已 spawn 的炸药桶(弱引用:被炸掉的回收时跳过)。随聚落回收(桶是静物,不像船会追玩家) */
        TArray<TWeakObjectPtr<AOCExplosiveBarrel>> Barrels;
    };

    /** 解析配置/种子,初始化高度场与构建参数 */
    void InitializeGenerator();

    /** 每 0.25s:回收窗口外区块 + 启动缺失区块的构建;并调度聚落内容 */
    void UpdateChunks();

    /** 每帧:从完成队列按预算提交区块 */
    void DrainCompletedChunks();

    /** 游戏线程:spawn 区块 Actor 并提交网格(空数据 = 全深海,登记 nullptr 不再重试) */
    void SpawnChunk(FIntPoint Coord, const TSharedPtr<FOCChunkMeshData>& Data);

    /** 深海平面:惰性创建(平面网格 + Movable),按吸附步长跟随玩家 */
    void UpdateSeabedPlane(const FVector& PlayerPos);

    /** 聚落内容调度:窗口内聚落格后台算内容,窗口外整批销毁;并重试待 spawn 的格子 */
    void UpdateClusterBuildings(const FVector& PlayerPos);

    /** 每 0.25s:回收离玩家过远的敌船,并清掉已失效的弱引用(被打沉的船) */
    void UpdateEnemyBoats(const FVector& PlayerPos);

    /** 每帧:按预算接收聚落内容(登记为待 spawn) */
    void DrainCompletedClusters();

    /** 游戏线程:地形就绪后 spawn 一个聚落格的内容(内容 Actor + 装饰 + 建筑) */
    void TrySpawnClusterContent(FIntPoint Cell);

    /** 按世界坐标查难度分档行:取 MinDistance <= 离出生点距离 的最后一行。无表/零点未就绪返回 nullptr */
    const FOCDifficultyTierRow* FindTierForLocation(const FVector& WorldLocation);

    /** 玩家所在区块坐标与世界位置;无玩家时返回 false */
    bool GetPlayerChunkCoord(FIntPoint& OutCoord, FVector& OutPos) const;

    /** 切比雪夫距离(区块格数) */
    static int32 ChunkDistance(FIntPoint A, FIntPoint B)
    {
        return FMath::Max(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y));
    }

    const UOCMapGenConfig* ActiveConfig = nullptr;

    /** TSharedPtr 持有:后台任务捕获,Manager 销毁也不悬垂 */
    TSharedPtr<FOCInfiniteHeightfield> Heightfield;

    FOCChunkBuildParams BuildParams;
    int32 WorldSeed = 0;
    bool bAsyncBuild = false;

    // 窗口参数缓存(自配置)
    float ChunkSize = 25600.0f;
    int32 ActiveRadius = 2;
    int32 UnloadMargin = 1;
    int32 MaxCommitsPerFrame = 2;

    UPROPERTY(Transient)
    TMap<FIntPoint, TObjectPtr<AOCMapChunk>> ActiveChunks;

    /** 深海平面 Actor(一块跟随玩家的深色平面,兜住被剔除的深海区域) */
    UPROPERTY(Transient)
    TObjectPtr<AActor> SeabedPlane;

    /** 平面当前的吸附中心(未移动时不重复 SetActorLocation) */
    FVector2D SeabedPlaneCenter = FVector2D(1e30, 1e30);

    TSet<FIntPoint> LoadingChunks;

    /** 区块完成队列(TSharedPtr 持有:后台任务捕获,Manager 销毁也不悬垂,与 Heightfield 同一套路) */
    TSharedPtr<TQueue<FCompletedChunk, EQueueMode::Mpsc>> CompletedChunks =
        MakeShared<TQueue<FCompletedChunk, EQueueMode::Mpsc>>();

    // ---- 聚落内容(拥有的岛组:建筑 + 装饰)----
    FOCBuildingSiteParams BuildingParams;

    /** 敌船生成参数(InitializeGenerator 从配置构建一次) */
    FOCBoatSpawnParams BoatParams;

    /** 炸药桶生成参数(InitializeGenerator 从配置构建一次) */
    FOCBarrelSpawnParams BarrelParams;

    /** 敌船回收距离(cm),自配置缓存 */
    float BoatDespawnDistance = 120000.0f;

    /**
     * 已生成的敌船(弱引用)。刻意不挂在 FClusterContent 下:
     * 船会追着玩家跑出出生聚落很远,跟聚落一起回收会让正在交战的船凭空消失。
     * 回收判据是船自身离玩家的距离(见 UpdateEnemyBoats)。
     */
    TArray<TWeakObjectPtr<AOCEnemyBoat>> SpawnedBoats;

    /** 聚落格 → 内容状态(含未 spawn 的,表示"已加载") */
    TMap<FIntPoint, FClusterContent> ClusterContents;

    TSet<FIntPoint> LoadingClusters;

    /** 聚落完成队列(TSharedPtr 持有,原因同上) */
    TSharedPtr<TQueue<FCompletedCluster, EQueueMode::Mpsc>> CompletedClusters =
        MakeShared<TQueue<FCompletedCluster, EQueueMode::Mpsc>>();

    /** 建筑蓝图缺失的警告只打一次 */
    bool bLoggedMissingBuildingClass = false;

    /** 难度分档零点(玩家初始出生点,首次查档时从 GameMode 懒取并缓存,避免 BeginPlay 时序问题) */
    TOptional<FVector2D> DifficultyOrigin;

    // ---- 装饰散布(树/花草/石头;规则在 InitializeGenerator 从配置构建一次)----
    TArray<FOCDecoRule> DecoRules;

    /** 每个规则的有效网格列表(与 DecoRules.EntryWeights 一一对应) */
    UPROPERTY(Transient)
    TArray<FOCDecoMeshList> DecoMeshes;

    FIntPoint LastPlayerChunk = FIntPoint::ZeroValue;
    bool bHavePlayerChunk = false;

    float TimeSinceLastUpdate = 0.0f;
};
