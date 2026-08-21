// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OCMapGenConfig.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UDataTable;
class AOCDefenseBuilding;
class AOCEnemyBoat;
class AOCExplosiveBarrel;

/**
 * 单个装饰模型条目:静态网格 + 同类别内被选中的权重 + 随机均匀缩放范围。
 * 同一类别(树/花草/石头)可配任意数量条目,撒点时按 Weight 加权随机选用。
 */
USTRUCT(BlueprintType)
struct FOCDecoEntry
{
    GENERATED_BODY()

    /** 装饰静态网格。留空的条目在撒点时跳过 */
    UPROPERTY(EditAnywhere)
    TObjectPtr<UStaticMesh> Mesh;

    /** 同类别内加权随机选中的权重,越大越常出现 */
    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
    float Weight = 1.0f;

    /** 随机均匀缩放范围(X=最小, Y=最大) */
    UPROPERTY(EditAnywhere)
    FVector2D ScaleRange = FVector2D(0.9, 1.1);
};

/**
 * 一类装饰(树/花草/石头)的完整规则:模型列表 + 撒点规则。
 * 每类独立一套参数:树稀而高、花草密而小、石头可上陡坡且带碰撞。
 */
USTRUCT(BlueprintType)
struct FOCDecoCategory
{
    GENERATED_BODY()

    /** 该类别的装饰模型列表,任意数量;空数组 = 该类别不生成 */
    UPROPERTY(EditAnywhere)
    TArray<FOCDecoEntry> Meshes;

    /** 候选点抖动网格步长(cm),越小越密 */
    UPROPERTY(EditAnywhere, meta = (ClampMin = "50.0"))
    float Spacing = 400.0f;

    /** 候选点在格内的随机抖动比例(0~1),避免机械排列 */
    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Jitter = 0.7f;

    /** 落点最小离水高度(cm):落点须高于 海平面 + 此值,避免泡水/贴岸 */
    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
    float MinGroundClearance = 40.0f;

    /** 坡度上限(度):地表法线与竖直夹角超过则不放 */
    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float MaxSlopeDeg = 25.0f;

    /** 是否对齐地表法线:石头 true(贴地自然),树/花草 false(保持竖直向上) */
    UPROPERTY(EditAnywhere)
    bool bAlignToNormal = false;

    /** 是否开启碰撞:石头 true(挡船/挡弹),植被 false(省开销) */
    UPROPERTY(EditAnywhere)
    bool bCollision = false;

    /** 单岛该类别实例上限(0 = 不限),防超密 */
    UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
    int32 MaxPerIsland = 0;
};

/**
 * 程序化地图生成配置(高度场优先模型)。全部生成参数集中在这里,GameMode 持有一个引用,调参不改代码。
 * 未配置时(生成器 Config 为空)走本类 CDO 默认值,可直接跑通测试。
 */
UCLASS()
class OCEANCOMBAT_API UOCMapGenConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    // ---- 通用 ----
    /** 随机种子。0 = 每局随机;非 0 = 固定布局(便于复现/调试) */
    UPROPERTY(EditAnywhere, Category = "General")
    int32 Seed = 0;

    /** 海平面高度(世界 Z),固定不变;海浪仅为视觉 */
    UPROPERTY(EditAnywhere, Category = "General")
    float SeaLevelZ = 0.0f;

    // ---- 地形:高度映射 ----
    /** 深海海床深度(海平面往下) */
    UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "0.0"))
    float DeepSeaDepth = 800.0f;

    /** 浅海阈值:水深小于此值为浅海 */
    UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "0.0"))
    float ShallowWaterDepth = 300.0f;

    /** 近岸浅海大陆架宽(cm):海岸线向海多宽降到深海底。大 = 缓坡浅滩多,小 = 近岸即深 */
    UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "1.0"))
    float ShelfWidth = 1200.0f;

    /** 海岸→内陆抬升过渡宽(cm):大 = 海岸平缓像沙洲,小 = 近岸陡峭 */
    UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "1.0"))
    float LandRiseWidth = 1400.0f;

    /** 深海剔除深度(cm):四角全低于 海平面-此值 的四边形不生成三角形(深海平面兜底)。
     *  调参原则:洞缘必须深到看不见;须显著小于 DeepSeaDepth 以免浅滩被挖穿 */
    UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "100.0"))
    float MeshCullDepth = 500.0f;

    // ---- 团块融合(层0:群岛/离岸小岛涌现) ----
    /** smin 融合软度(cm):越大团块过渡越圆滑、越易连成群岛;越小越易分裂成独立小岛 */
    UPROPERTY(EditAnywhere, Category = "Field", meta = (ClampMin = "0.0"))
    float BlobSmoothK = 800.0f;

    // ---- 域扭曲(层1:海岸线有机化) ----
    /** 采样坐标扭曲强度(cm):越大海岸越扭曲破碎。过大会甩散小岛,建议 ≤ 最小 blob 半径的 0.6 */
    UPROPERTY(EditAnywhere, Category = "Field", meta = (ClampMin = "0.0"))
    float WarpAmplitude = 500.0f;

    /** 域扭曲频率(1/cm):越大扭动越细碎 */
    UPROPERTY(EditAnywhere, Category = "Field", meta = (ClampMin = "0.0"))
    float WarpFrequency = 0.00035f;

    // ---- 内陆起伏(层2:陆地丘陵/山脊) ----
    /** 内陆起伏主旋钮(cm):0 = 光滑穹顶,中低 = 开阔海面基调,高 = 明显丘陵山脊 */
    UPROPERTY(EditAnywhere, Category = "SurfaceDetail", meta = (ClampMin = "0.0"))
    float DetailAmplitude = 90.0f;

    /** 起伏基频(1/cm):波长(1/频率)应明显小于岛屿尺度。越大起伏越密(搓衣板),越小越舒缓 */
    UPROPERTY(EditAnywhere, Category = "SurfaceDetail", meta = (ClampMin = "0.0"))
    float DetailFrequency = 0.0004f;

    /** 起伏 fBm 倍频数:最细倍频波长别低于约 3 倍网格间距,否则起伏细过网格分辨率会出锯齿 */
    UPROPERTY(EditAnywhere, Category = "SurfaceDetail", meta = (ClampMin = "1", ClampMax = "6"))
    int32 DetailOctaves = 3;

    /** 起伏向海岸衰减带宽(cm):保证海岸带干净,不被噪声戳出零碎小坑 */
    UPROPERTY(EditAnywhere, Category = "SurfaceDetail", meta = (ClampMin = "1.0"))
    float DetailFalloffWidth = 700.0f;

    // ---- 深海海床底噪 ----
    /** 深海海床噪声振幅(cm):远离岛屿处的海床底噪。应明显小于 DeepSeaDepth */
    UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "0.0"))
    float SeabedNoiseAmplitude = 150.0f;

    /** 深海海床噪声频率(1/cm),数值越小起伏越平缓 */
    UPROPERTY(EditAnywhere, Category = "Terrain", meta = (ClampMin = "0.0"))
    float SeabedNoiseFrequency = 0.00025f;

    // ---- 无限地图 ----
    /** 区块边长(cm) */
    UPROPERTY(EditAnywhere, Category = "Infinite", meta = (ClampMin = "1600.0"))
    float ChunkSize = 25600.0f;

    /** 加载窗口半径(区块数):窗口 = (2R+1)² */
    UPROPERTY(EditAnywhere, Category = "Infinite", meta = (ClampMin = "1"))
    int32 ActiveRadius = 3;

    /** 卸载滞后(区块数):超出 窗口半径+此值 才回收,防边界反复生成/销毁 */
    UPROPERTY(EditAnywhere, Category = "Infinite", meta = (ClampMin = "0"))
    int32 UnloadMargin = 1;

    /** 每帧游戏线程提交区块上限(防一帧卡死) */
    UPROPERTY(EditAnywhere, Category = "Infinite", meta = (ClampMin = "1"))
    int32 MaxChunkCommitsPerFrame = 2;

    /** 区块顶点间距(cm,实际值按区块边长整除对齐)。调小海岸线更精细,构建成本按平方增长 */
    UPROPERTY(EditAnywhere, Category = "Infinite", meta = (ClampMin = "20.0"))
    float ChunkVertexSpacing = 50.0f;

    /** 聚落格边长(cm):每格按概率生成一个岛屿聚落(一座大岛+若干小岛)。
     *  约束:ClusterRadiusRange.Y + 卫星偏移 + 岛屿影响距离 必须小于此值(Initialize 有校验) */
    UPROPERTY(EditAnywhere, Category = "Infinite", meta = (ClampMin = "25600.0"))
    float ClusterCellSize = 40960.0f;

    /** 每聚落格生成聚落的概率:调小聚落更稀疏、间距更大 */
    UPROPERTY(EditAnywhere, Category = "Infinite", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ClusterChance = 0.8f;

    /** 聚落半径范围(cm):聚落内小岛散布在主岛周围此半径内 */
    UPROPERTY(EditAnywhere, Category = "Infinite")
    FVector2D ClusterRadiusRange = FVector2D(15000.0, 25000.0);

    /** 每个聚落的小岛数量范围 */
    UPROPERTY(EditAnywhere, Category = "Infinite")
    FIntPoint ClusterSmallIslandCountRange = FIntPoint(2, 5);

    /** 岛组最小间隔(cm,默认 0 = 关闭,允许跨聚落融合)。> 0 时:与更高优先级岛组
     *  影响圈距离小于此值的岛组整组删除(互斥),想要聚落间硬间隔时启用 */
    UPROPERTY(EditAnywhere, Category = "Infinite", meta = (ClampMin = "0.0"))
    float MinClusterSeparation = 0.0f;

    /** 小岛内陆基准高 = blob 半径 × 此比例范围:坡度与岛大小解耦,小岛不会因抽到高基准而陡成锥 */
    UPROPERTY(EditAnywhere, Category = "Infinite")
    FVector2D OffshoreCoreHeightFractionRange = FVector2D(0.2, 0.35);

    /** 区块网格异步构建开关:true = 后台线程构建(默认,航行不卡);false = 同步构建(调试用,会卡) */
    UPROPERTY(EditAnywhere, Category = "Infinite")
    bool bAsyncChunkBuild = true;

    /** 难度分档表(DT_DifficultyTiers,行结构 FOCDifficultyTierRow):按离出生点距离分档,
     *  spawn 敌人时查表把血量/伤害/得分系数烘焙到实例上。留空则所有敌人按基础数值 */
    UPROPERTY(EditAnywhere, Category = "Infinite")
    TObjectPtr<UDataTable> DifficultyTierTable;

    // ---- 深海平面 ----
    /** 深海平面网格(留空用引擎基本平面) */
    UPROPERTY(EditAnywhere, Category = "Infinite")
    TObjectPtr<UStaticMesh> SeabedPlaneMesh;

    /** 深海平面材质(深色;留空回退引擎默认材质并打 warning) */
    UPROPERTY(EditAnywhere, Category = "Infinite")
    TObjectPtr<UMaterialInterface> SeabedPlaneMaterial;

    /** 平面跟随吸附步长(cm):玩家偏离超过此值才把平面平移过来 */
    UPROPERTY(EditAnywhere, Category = "Infinite", meta = (ClampMin = "1000.0"))
    float SeabedPlaneSnapSize = 50000.0f;

    /** 平面覆盖边长(cm,须大于 视野 + 吸附步长×2) */
    UPROPERTY(EditAnywhere, Category = "Infinite", meta = (ClampMin = "10000.0"))
    float SeabedPlaneExtent = 400000.0f;

    /** 深海平面的世界 Z 坐标(cm):应高于深海床(盖住被剔除的洞)、低于 海平面-MeshCullDepth(不戳进保留的地形) */
    UPROPERTY(EditAnywhere, Category = "Infinite")
    float SeabedPlaneZ = -600.0f;

    // ---- 布局:blob 撒点(聚落内岛屿形状;数量/概率见 Infinite 分类)----
    /** 每座主岛的卫星 blob 数范围(母 blob 附近抖动的小 blob,融合成有机大块) */
    UPROPERTY(EditAnywhere, Category = "Layout")
    FIntPoint SatellitesPerIslandRange = FIntPoint(2, 4);

    /** 主岛母 blob 半径范围(cm) */
    UPROPERTY(EditAnywhere, Category = "Layout")
    FVector2D MainIslandRadiusRange = FVector2D(2000.0, 3500.0);

    /** 卫星 blob 半径 = 母 blob 半径 × 此比例范围 */
    UPROPERTY(EditAnywhere, Category = "Layout")
    FVector2D SatelliteRadiusFractionRange = FVector2D(0.4, 0.7);

    /** 卫星 blob 相对母 blob 的中心偏移 = 母半径 × 此比例范围(需 < 1 才能融合) */
    UPROPERTY(EditAnywhere, Category = "Layout")
    FVector2D SatelliteOffsetFractionRange = FVector2D(0.4, 0.75);

    /** 离岸小岛 blob 半径范围(cm):下限别太小,否则会成尖刺 */
    UPROPERTY(EditAnywhere, Category = "Layout")
    FVector2D OffshoreRadiusRange = FVector2D(700.0, 1300.0);

    /** 岛屿内陆基准高度范围(海平面往上,cm):做出岛屿高矮参差 */
    UPROPERTY(EditAnywhere, Category = "Layout")
    FVector2D CoreHeightRange = FVector2D(150.0, 600.0);

    // ---- 材质 ----
    /**
     * 地形混合材质(单一材质,水下褐↔水上绿由顶点色平滑混合)。
     * 网格现为单 section,每顶点写一个 0~1 的"陆地权重"到顶点色(RGB),
     * 材质里用 VertexColor 作 Lerp alpha 在海床/陆地两套贴图间过渡 → 边界无几何锯齿。
     * 未配置时回退引擎默认材质并 UE_LOG warning。
     */
    UPROPERTY(EditAnywhere, Category = "Material")
    TObjectPtr<UMaterialInterface> TerrainMaterial;

    /** 水下海床材质(褐色)。混合材质就绪前的回退/参考;混合方案下不直接用于渲染 */
    UPROPERTY(EditAnywhere, Category = "Material")
    TObjectPtr<UMaterialInterface> SeabedMaterial;

    /** 水上岛屿材质(绿色)。混合材质就绪前的回退/参考;混合方案下不直接用于渲染 */
    UPROPERTY(EditAnywhere, Category = "Material")
    TObjectPtr<UMaterialInterface> LandMaterial;

    /** 绿/褐混合分界中心高度 = 海平面 + 此值("水面之上稍微高一点点"处过渡) */
    UPROPERTY(EditAnywhere, Category = "Material", meta = (ClampMin = "0.0"))
    float LandThresholdOffset = 30.0f;

    /**
     * 混合带高度宽(cm):以分界中心为中,上下各半带内从褐平滑过渡到绿。
     * 越大过渡越柔;越小越接近硬边(仍无方块锯齿,因为是逐顶点权重)。
     */
    UPROPERTY(EditAnywhere, Category = "Material", meta = (ClampMin = "1.0"))
    float ShorelineBlendWidth = 120.0f;

    /** 混合分界噪声抖动幅度(cm),让绿/褐过渡带蜿蜒如自然海岸,而非等高直线 */
    UPROPERTY(EditAnywhere, Category = "Material", meta = (ClampMin = "0.0"))
    float ShorelineJitter = 80.0f;

    // ---- 建筑放置(城堡/防御塔;须在稳定高度场上落点)----
    /** 总开关:是否在岛上放置城堡/防御塔 */
    UPROPERTY(EditAnywhere, Category = "Buildings")
    bool bPlaceBuildings = true;

    /** 城堡蓝图(放在最大的岛上,自动打 "Castle" 标签参与胜利判定)。留空则不放城堡 */
    UPROPERTY(EditAnywhere, Category = "Buildings")
    TSubclassOf<AOCDefenseBuilding> CastleClass;

    /** 防御塔蓝图(放在其余岛上)。留空则不放防御塔 */
    UPROPERTY(EditAnywhere, Category = "Buildings")
    TSubclassOf<AOCDefenseBuilding> TurretClass;

    /** 每个聚落最多放多少座防御塔(城堡不受限,恒放在拥有的最大岛组上) */
    UPROPERTY(EditAnywhere, Category = "Buildings", meta = (ClampMin = "0"))
    int32 MaxTurretsPerCluster = 3;

    /** 建筑足印半径(cm):落点检测时以此为半径取一圈采样点,要求整片足印都在平地上、不出水 */
    UPROPERTY(EditAnywhere, Category = "Buildings", meta = (ClampMin = "1.0"))
    float BuildingFootprintRadius = 350.0f;

    /** 落点最小离水高度(cm):足印内所有点须高于 海平面 + 此值,避免建筑泡水/贴海岸悬空 */
    UPROPERTY(EditAnywhere, Category = "Buildings", meta = (ClampMin = "0.0"))
    float BuildingMinGroundClearance = 60.0f;

    /** 选点扫描步长(cm):在岛包围盒内按此步长扫格找最平坦落点。越小越精细但越慢 */
    UPROPERTY(EditAnywhere, Category = "Buildings", meta = (ClampMin = "20.0"))
    float BuildingSampleStep = 120.0f;

    /** 落点下沉深度(cm):Z = 足印内最低点 - 此值。坡上建筑底座低侧落实、高侧微埋,避免一边悬浮 */
    UPROPERTY(EditAnywhere, Category = "Buildings", meta = (ClampMin = "0.0"))
    float BuildingEmbedDepth = 20.0f;

    // ---- 敌方船只(每个聚落在周围水域生成一支小队)----
    /** 总开关:是否在聚落周围生成敌船 */
    UPROPERTY(EditAnywhere, Category = "EnemyBoats")
    bool bSpawnEnemyBoats = true;

    /** 敌船蓝图(留空则不生成)。蓝图自带 AI 控制器,spawn 即自主巡弋 */
    UPROPERTY(EditAnywhere, Category = "EnemyBoats")
    TSubclassOf<AOCEnemyBoat> EnemyBoatClass;

    /** 每个聚落生成的敌船数量区间(X=最小, Y=最大, 含两端)。实际数量按聚落格种子确定性摇取 */
    UPROPERTY(EditAnywhere, Category = "EnemyBoats", meta = (ClampMin = "0"))
    FIntPoint EnemyBoatCountRange = FIntPoint(2, 4);

    /** 生成环带内半径(cm):离主岛中心至少这么远,避免贴岸卡浅滩 */
    UPROPERTY(EditAnywhere, Category = "EnemyBoats", meta = (ClampMin = "0.0"))
    float EnemyBoatRingRadiusMin = 4000.0f;

    /** 生成环带外半径(cm) */
    UPROPERTY(EditAnywhere, Category = "EnemyBoats", meta = (ClampMin = "0.0"))
    float EnemyBoatRingRadiusMax = 9000.0f;

    /** 生成点最小水深(cm):水深不足的点(陆地/浅滩)淘汰重摇,保证船不卡礁 */
    UPROPERTY(EditAnywhere, Category = "EnemyBoats", meta = (ClampMin = "0.0"))
    float EnemyBoatMinWaterDepth = 400.0f;

    /** 同一聚落内两船的最小间距(cm),避免挤在一起 */
    UPROPERTY(EditAnywhere, Category = "EnemyBoats", meta = (ClampMin = "0.0"))
    float EnemyBoatMinSeparation = 1500.0f;

    /**
     * 回收距离(cm):船离玩家超过此距离即销毁。
     * 船会追着玩家跑出出生聚落,所以不跟聚落一起回收;此值应显著大于区块窗口半径
     * (ActiveRadius × ChunkSize),否则正在交战的船会凭空消失。
     */
    UPROPERTY(EditAnywhere, Category = "EnemyBoats", meta = (ClampMin = "0.0"))
    float EnemyBoatDespawnDistance = 120000.0f;

    // ---- 炸药桶(每个聚落在周围水域散布若干漂浮炸药桶,零散单个)----
    /** 总开关:是否在聚落周围水面生成炸药桶 */
    UPROPERTY(EditAnywhere, Category = "ExplosiveBarrels")
    bool bSpawnBarrels = true;

    /** 炸药桶蓝图(留空则不生成) */
    UPROPERTY(EditAnywhere, Category = "ExplosiveBarrels")
    TSubclassOf<AOCExplosiveBarrel> BarrelClass;

    /** 每个聚落生成的炸药桶数量区间(X=最小, Y=最大, 含两端)。按聚落格种子确定性摇取 */
    UPROPERTY(EditAnywhere, Category = "ExplosiveBarrels", meta = (ClampMin = "0"))
    FIntPoint BarrelCountRange = FIntPoint(2, 5);

    /** 生成环带内半径(cm):离主岛中心至少这么远(比敌船环带略靠岛) */
    UPROPERTY(EditAnywhere, Category = "ExplosiveBarrels", meta = (ClampMin = "0.0"))
    float BarrelRingRadiusMin = 3000.0f;

    /** 生成环带外半径(cm) */
    UPROPERTY(EditAnywhere, Category = "ExplosiveBarrels", meta = (ClampMin = "0.0"))
    float BarrelRingRadiusMax = 8000.0f;

    /** 生成点最小水深(cm):水深不足的点(陆地/浅滩)淘汰重摇,保证桶漂在水上不卡岸 */
    UPROPERTY(EditAnywhere, Category = "ExplosiveBarrels", meta = (ClampMin = "0.0"))
    float BarrelMinWaterDepth = 400.0f;

    /** 同一聚落内两桶的最小间距(cm):避免生成时重叠被物理弹飞 */
    UPROPERTY(EditAnywhere, Category = "ExplosiveBarrels", meta = (ClampMin = "0.0"))
    float BarrelMinSeparation = 800.0f;

    // ---- 装饰散布(树/花草/石头;HISM 实例化渲染,纯装饰不写高度场)----
    /** 总开关:是否在岛上程序化散布装饰 */
    UPROPERTY(EditAnywhere, Category = "Decorations")
    bool bScatterDecorations = true;

    /** 树:稀而高,要求平地,无碰撞。Meshes 可配任意数量预设模型 */
    UPROPERTY(EditAnywhere, Category = "Decorations")
    FOCDecoCategory Trees{ {}, 700.0f, 0.7f, 80.0f, 25.0f, false, false, 0 };

    /** 花草:密而小,无碰撞。Meshes 可配任意数量预设模型 */
    UPROPERTY(EditAnywhere, Category = "Decorations")
    FOCDecoCategory Flowers{ {}, 250.0f, 0.7f, 40.0f, 30.0f, false, false, 0 };

    /** 石头:可上陡坡,贴地摆放,开碰撞(挡船/挡弹)。Meshes 可配任意数量预设模型 */
    UPROPERTY(EditAnywhere, Category = "Decorations")
    FOCDecoCategory Rocks{ {}, 500.0f, 0.7f, 20.0f, 45.0f, true, true, 0 };

    /** 装饰避开城堡/防御塔落点的排除半径(cm) */
    UPROPERTY(EditAnywhere, Category = "Decorations", meta = (ClampMin = "0.0"))
    float DecorationBuildingClearance = 600.0f;
};
