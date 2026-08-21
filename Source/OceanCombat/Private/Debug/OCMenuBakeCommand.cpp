// OceanCombat. Copyright(c) All rights reserved.

// 菜单地图烘焙命令(仅编辑器):把无限地图生成器在 指定种子+坐标+区块半径 下的产物
// 烘焙成静态资产/普通 Actor,摆进当前打开的菜单关卡(如 MainMenu)。
// 生成走与运行时完全相同的内核代码路径(OCMapGenKernels),同种子下结果逐位一致。
//
// 用法(Output Log 的 Cmd 栏):
//   OC.BakeMenuMap <Seed> <CenterX> <CenterY> <ChunkRadius> [ConfigPath]
// 例:OC.BakeMenuMap 12345 0 0 1
//   Seed        必须非 0(0 = 随机,烘焙无意义)
//   CenterX/Y   取景中心世界坐标(cm)
//   ChunkRadius 区块半径:范围 = (2R+1)^2 个区块
//   ConfigPath  可选,默认 /Game/Core/Config/DA_MapGenConfig
//
// 重复执行幂等:先销毁带 OCMenuBaked 标签的旧 Actor、删除 /Game/Maps/MenuBaked 下旧网格资产再烘。
// 命令只写关卡与资产包;地形网格包自动保存,关卡本身请检查后手动保存。

#if WITH_EDITOR

#include "CoreMinimal.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Hazards/OCExplosiveBarrel.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "ObjectTools.h"
#include "Pawns/Boats/OCEnemyBoat.h"
#include "Pawns/Buildings/OCDefenseBuilding.h"
#include "Procedural/OCClusterContent.h"
#include "Procedural/OCMapChunk.h"
#include "Procedural/OCMapGenConfig.h"
#include "Procedural/OCMapGenKernels.h"
#include "StaticMeshAttributes.h"

namespace OCMenuBake
{

static const FName BakedTag(TEXT("OCMenuBaked"));
static const FName BakedFolder(TEXT("OCMenuBaked"));

/** 给烘焙产物 Actor 统一打标签/文件夹/名字前缀,便于幂等清理与识别 */
static void MarkBakedActor(AActor* Actor, const FString& Label)
{
    Actor->Tags.Add(BakedTag);
    Actor->SetFolderPath(BakedFolder);
    Actor->SetActorLabel(Label);
}

/** 把区块网格数据构建成 UStaticMesh 资产(顶点保持世界坐标,Actor 摆原点即与实机一致) */
static UStaticMesh* CreateStaticMeshFromChunk(UPackage*& OutPackage, FIntPoint Coord,
    const FOCChunkMeshData& Data, UMaterialInterface* TerrainMat)
{
    const FString AssetName = FString::Printf(TEXT("SM_MenuChunk_%d_%d"), Coord.X, Coord.Y);
    const FString PackageName = FString::Printf(TEXT("/Game/Maps/MenuBaked/%s"), *AssetName);
    UPackage* Package = CreatePackage(*PackageName);
    Package->FullyLoad();

    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone);

    FMeshDescription MeshDesc;
    FStaticMeshAttributes Attributes(MeshDesc);
    Attributes.Register();

    TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
    TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
    TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
    TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
    TPolygonGroupAttributesRef<FName> SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
    UVs.SetNumChannels(1);

    MeshDesc.ReserveNewVertices(Data.Vertices.Num());
    MeshDesc.ReserveNewVertexInstances(Data.Triangles.Num());
    MeshDesc.ReserveNewTriangles(Data.Triangles.Num() / 3);

    TArray<FVertexID> VertexIDs;
    VertexIDs.SetNumUninitialized(Data.Vertices.Num());
    for (int32 i = 0; i < Data.Vertices.Num(); ++i)
    {
        VertexIDs[i] = MeshDesc.CreateVertex();
        Positions[VertexIDs[i]] = FVector3f(Data.Vertices[i]);
    }

    const FPolygonGroupID PolyGroup = MeshDesc.CreatePolygonGroup();
    SlotNames[PolyGroup] = TerrainMat ? TerrainMat->GetFName() : NAME_None;

    for (int32 t = 0; t + 2 < Data.Triangles.Num(); t += 3)
    {
        FVertexInstanceID VI[3];
        for (int32 k = 0; k < 3; ++k)
        {
            const int32 SrcIndex = Data.Triangles[t + k];
            VI[k] = MeshDesc.CreateVertexInstance(VertexIDs[SrcIndex]);
            Normals[VI[k]] = FVector3f(Data.Normals[SrcIndex]);
            UVs[VI[k]] = FVector2f(Data.UVs[SrcIndex]);
            const FLinearColor& C = Data.VertexColors[SrcIndex];
            Colors[VI[k]] = FVector4f(C.R, C.G, C.B, C.A);
        }
        MeshDesc.CreateTriangle(PolyGroup, MakeConstArrayView(VI, 3));
    }

    UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
    BuildParams.bBuildSimpleCollision = true;
    StaticMesh->BuildFromMeshDescriptions({ &MeshDesc }, BuildParams);

    // 材质槽 0 = 地形混合材质(与运行时 AOCMapChunk::ApplyMeshData 一致)
    if (StaticMesh->GetStaticMaterials().Num() == 0)
    {
        StaticMesh->GetStaticMaterials().Add(FStaticMaterial(TerrainMat));
    }
    else
    {
        StaticMesh->GetStaticMaterials()[0].MaterialInterface = TerrainMat;
    }

    // 初始化每个材质槽的 UV 密度数据(纹理流送用);不建会在 GetUVChannelData 处触发 ensure
    StaticMesh->UpdateUVChannelData(/*bRebuildAll=*/true);

    OutPackage = Package;
    return StaticMesh;
}

static void Execute(const TArray<FString>& Args)
{
    if (Args.Num() < 4)
    {
        UE_LOG(LogTemp, Error, TEXT("[OCMenuBake] 用法: OC.BakeMenuMap <Seed> <CenterX> <CenterY> <ChunkRadius> [ConfigPath]"));
        return;
    }

    const int32 Seed = FCString::Atoi(*Args[0]);
    const double CenterX = FCString::Atod(*Args[1]);
    const double CenterY = FCString::Atod(*Args[2]);
    const int32 ChunkRadius = FMath::Max(0, FCString::Atoi(*Args[3]));
    const FString ConfigPath = Args.Num() > 4 ? Args[4] : TEXT("/Game/Core/Config/DA_MapGenConfig.DA_MapGenConfig");

    if (Seed == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[OCMenuBake] Seed 必须非 0(0 = 每局随机,烘焙结果不可复现)"));
        return;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("[OCMenuBake] 没有编辑器世界,请先打开菜单关卡(如 MainMenu)"));
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("[OCMenuBake] 目标关卡: %s"), *World->GetOutermost()->GetName());

    const UOCMapGenConfig* Config = LoadObject<UOCMapGenConfig>(nullptr, *ConfigPath);
    if (!Config)
    {
        UE_LOG(LogTemp, Warning, TEXT("[OCMenuBake] 配置 %s 加载失败,使用 CDO 默认值"), *ConfigPath);
    }
    const UOCMapGenConfig* C = Config ? Config : GetDefault<UOCMapGenConfig>();

    // ---- 1. 幂等清理:销毁上次烘焙的 Actor + 删除旧地形网格资产 ----
    int32 RemovedActors = 0;
    for (ULevel* Level : World->GetLevels())
    {
        if (!Level)
        {
            continue;
        }
        for (int32 i = Level->Actors.Num() - 1; i >= 0; --i)
        {
            AActor* Actor = Level->Actors[i];
            if (Actor && Actor->ActorHasTag(BakedTag))
            {
                World->DestroyActor(Actor);
                ++RemovedActors;
            }
        }
    }
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        TArray<FAssetData> OldAssets;
        AssetRegistryModule.Get().GetAssetsByPath(FName(TEXT("/Game/Maps/MenuBaked")), OldAssets, /*bRecursive=*/true);
        if (OldAssets.Num() > 0)
        {
            TArray<UObject*> ToDelete;
            for (const FAssetData& AssetData : OldAssets)
            {
                if (UObject* Obj = AssetData.GetAsset())
                {
                    ToDelete.Add(Obj);
                }
            }
            ObjectTools::ForceDeleteObjects(ToDelete, /*ShowConfirmation=*/false);
        }
        UE_LOG(LogTemp, Log, TEXT("[OCMenuBake] 清理:Actor %d 个,旧网格资产 %d 个"), RemovedActors, OldAssets.Num());
    }

    // ---- 2. 初始化生成参数(与运行时同一条参数管道) ----
    FOCMapGenRuntimeParams P = FOCMapGenRuntimeParams::FromConfig(Config, Seed);
    // 菜单要画面不要局内开关:只要配了蓝图类就摆船/桶
    P.BoatParams.bEnabled = C->EnemyBoatClass != nullptr;
    P.BarrelParams.bEnabled = C->BarrelClass != nullptr;

    FOCInfiniteHeightfield Heightfield;
    Heightfield.Initialize(P.WorldSeed, P.FieldParams, P.LayoutParams);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // ---- 3. 地形区块 → 静态网格资产 + AStaticMeshActor ----
    const FIntPoint CenterChunk(
        FMath::FloorToInt(static_cast<float>(CenterX / P.ChunkSize)),
        FMath::FloorToInt(static_cast<float>(CenterY / P.ChunkSize)));
    const FIntPoint MinChunk = CenterChunk - FIntPoint(ChunkRadius, ChunkRadius);
    const FIntPoint MaxChunk = CenterChunk + FIntPoint(ChunkRadius, ChunkRadius);

    UMaterialInterface* TerrainMat = C->TerrainMaterial ? C->TerrainMaterial.Get() : UMaterial::GetDefaultMaterial(MD_Surface);

    TArray<UPackage*> PackagesToSave;
    int32 NumChunks = 0;
    for (int32 CY = MinChunk.Y; CY <= MaxChunk.Y; ++CY)
    {
        for (int32 CX = MinChunk.X; CX <= MaxChunk.X; ++CX)
        {
            const FIntPoint Coord(CX, CY);
            FOCRegionBlobTable Table;
            OCMapGenKernels::BuildChunkBlobTable(Heightfield, Coord, P.ChunkSize,
                P.LayoutParams.ClusterCellSize, P.ChunkBuildParams.VertexSpacing, Table);
            TSharedPtr<FOCChunkMeshData> Data = AOCMapChunk::BuildMeshData(Coord, Heightfield, Table, P.ChunkBuildParams);
            if (!Data.IsValid() || Data->Vertices.Num() == 0)
            {
                continue; // 全深海区块:由兜底平面覆盖
            }

            UPackage* MeshPackage = nullptr;
            UStaticMesh* Mesh = CreateStaticMeshFromChunk(MeshPackage, Coord, *Data, TerrainMat);
            PackagesToSave.Add(MeshPackage);

            AStaticMeshActor* ChunkActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform::Identity, SpawnParams);
            ChunkActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
            ChunkActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
            MarkBakedActor(ChunkActor, FString::Printf(TEXT("OCBaked_Terrain_%d_%d"), CX, CY));
            ++NumChunks;
        }
    }

    // ---- 4. 聚落格范围(外扩:聚落半径上限 + 一格,防漏掉格外聚落伸进范围的岛组) ----
    const double RegionMinX = static_cast<double>(MinChunk.X) * P.ChunkSize;
    const double RegionMinY = static_cast<double>(MinChunk.Y) * P.ChunkSize;
    const double RegionMaxX = (static_cast<double>(MaxChunk.X) + 1.0) * P.ChunkSize;
    const double RegionMaxY = (static_cast<double>(MaxChunk.Y) + 1.0) * P.ChunkSize;
    const double Expand = P.LayoutParams.ClusterRadiusRange.Y + P.LayoutParams.ClusterCellSize;
    const double CellSize = P.LayoutParams.ClusterCellSize;
    const FIntPoint MinCell(
        FMath::FloorToInt(static_cast<float>((RegionMinX - Expand) / CellSize)),
        FMath::FloorToInt(static_cast<float>((RegionMinY - Expand) / CellSize)));
    const FIntPoint MaxCell(
        FMath::FloorToInt(static_cast<float>((RegionMaxX + Expand) / CellSize)),
        FMath::FloorToInt(static_cast<float>((RegionMaxY + Expand) / CellSize)));

    // ---- 5. 逐聚落格计算内容:装饰合并到一个 HISM 宿主,建筑/船/桶逐个 spawn ----
    TArray<FOCDecoInstance> AllDecorations;
    int32 NumBuildings = 0;
    int32 NumBoats = 0;
    int32 NumBarrels = 0;
    for (int32 CY = MinCell.Y; CY <= MaxCell.Y; ++CY)
    {
        for (int32 CX = MinCell.X; CX <= MaxCell.X; ++CX)
        {
            const FOCClusterContentData Content = OCMapGenKernels::ComputeClusterContent(
                FIntPoint(CX, CY), Heightfield, P.BuildingParams, P.BoatParams, P.BarrelParams, P.DecoRules);
            AllDecorations.Append(Content.Decorations);

            for (const FOCBuildingSiteInfo& Site : Content.Sites)
            {
                TSubclassOf<AOCDefenseBuilding> BuildingClass = Site.bIsCastle ? C->CastleClass : C->TurretClass;
                if (!BuildingClass)
                {
                    continue;
                }
                AActor* Building = World->SpawnActor<AActor>(BuildingClass, FTransform(Site.Location), SpawnParams);
                if (Building)
                {
                    MarkBakedActor(Building, FString::Printf(TEXT("OCBaked_%s"), Site.bIsCastle ? TEXT("Castle") : TEXT("Turret")));
                    ++NumBuildings;
                }
            }

            if (C->EnemyBoatClass)
            {
                for (const FOCBoatSpawnInfo& BoatSpawn : Content.BoatSpawns)
                {
                    const FTransform BoatTransform(FRotator(0.0f, BoatSpawn.YawDeg, 0.0f), BoatSpawn.Location);
                    AOCEnemyBoat* Boat = World->SpawnActor<AOCEnemyBoat>(C->EnemyBoatClass, BoatTransform, SpawnParams);
                    if (!Boat)
                    {
                        continue;
                    }
                    // 关 AI:寻路/移动决策全在 AOCAIBoatController,Controller 不 possessed 船就只随浪起伏
                    Boat->AutoPossessAI = EAutoPossessAI::Disabled;
                    MarkBakedActor(Boat, TEXT("OCBaked_EnemyBoat"));
                    ++NumBoats;
                }
            }

            if (C->BarrelClass)
            {
                for (const FOCBarrelSpawnInfo& BarrelSpawn : Content.BarrelSpawns)
                {
                    const FTransform BarrelTransform(FRotator(0.0f, BarrelSpawn.YawDeg, 0.0f), BarrelSpawn.Location);
                    AActor* Barrel = World->SpawnActor<AActor>(C->BarrelClass, BarrelTransform, SpawnParams);
                    if (Barrel)
                    {
                        MarkBakedActor(Barrel, TEXT("OCBaked_Barrel"));
                        ++NumBarrels;
                    }
                }
            }
        }
    }

    if (AllDecorations.Num() > 0)
    {
        AOCClusterContent* DecoActor = World->SpawnActor<AOCClusterContent>(AOCClusterContent::StaticClass(), FTransform::Identity, SpawnParams);
        DecoActor->ApplyDecorations(AllDecorations, P.DecoRules, P.DecoMeshes);
        MarkBakedActor(DecoActor, TEXT("OCBaked_Decorations"));
    }

    // ---- 6. 深海兜底平面(兜住被剔除的深海区域,与运行时 UpdateSeabedPlane 同配置) ----
    // 用 AStaticMeshActor(原生组件,随关卡序列化),不要 NewObject 组件(实例组件需额外标记才序列化)
    if (UStaticMesh* PlaneMesh = C->SeabedPlaneMesh ? C->SeabedPlaneMesh.Get()
        : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
    {
        AStaticMeshActor* Plane = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform::Identity, SpawnParams);
        UStaticMeshComponent* Comp = Plane->GetStaticMeshComponent();
        Comp->SetStaticMesh(PlaneMesh);
        Comp->SetMobility(EComponentMobility::Static);
        Comp->SetCollisionProfileName(TEXT("BlockAll"));
        Comp->SetCastShadow(false);
        Comp->SetMaterial(0, C->SeabedPlaneMaterial ? C->SeabedPlaneMaterial.Get() : UMaterial::GetDefaultMaterial(MD_Surface));

        // 引擎基本平面为 100×100cm,缩放到覆盖烘焙范围 + 外扩余量
        const double Extent = (ChunkRadius * 2 + 1) * static_cast<double>(P.ChunkSize) + 2.0 * Expand;
        Plane->SetActorLocation(FVector(CenterX, CenterY, C->SeabedPlaneZ));
        Plane->SetActorScale3D(FVector(Extent / 100.0));
        MarkBakedActor(Plane, TEXT("OCBaked_SeabedPlane"));
    }

    // ---- 7. 保存地形网格资产包;关卡标脏,由用户检查后手动保存 ----
    UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, /*bOnlyDirty=*/true);
    World->GetCurrentLevel()->MarkPackageDirty();

    UE_LOG(LogTemp, Log, TEXT("[OCMenuBake] 烘焙完成:种子=%d 区块=%d 装饰实例=%d 建筑=%d 敌船=%d 炸药桶=%d。请检查后手动保存关卡(Ctrl+S)。"),
        P.WorldSeed, NumChunks, AllDecorations.Num(), NumBuildings, NumBoats, NumBarrels);
}

static FAutoConsoleCommand GBakeMenuMapCommand(
    TEXT("OC.BakeMenuMap"),
    TEXT("将无限地图的一块区域烘焙进当前打开的菜单关卡。用法: OC.BakeMenuMap <Seed> <CenterX> <CenterY> <ChunkRadius> [ConfigPath]"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&OCMenuBake::Execute));

} // namespace OCMenuBake

#endif // WITH_EDITOR
