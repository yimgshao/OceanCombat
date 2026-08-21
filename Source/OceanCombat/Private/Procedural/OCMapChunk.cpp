// OceanCombat. Copyright(c) All rights reserved.

#include "Procedural/OCMapChunk.h"

#include "Materials/Material.h"
#include "Procedural/OCInfiniteHeightfield.h"
#include "Procedural/OCNoiseUtil.h"
#include "ProceduralMeshComponent.h"

bool AOCMapChunk::bLoggedMaterialFallback = false;

AOCMapChunk::AOCMapChunk()
{
    PrimaryActorTick.bCanEverTick = false;

    SeabedMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SeabedMesh"));
    RootComponent = SeabedMesh;
    SeabedMesh->bUseAsyncCooking = true; // 碰撞异步 cook,不卡游戏线程
    SeabedMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

TSharedPtr<FOCChunkMeshData> AOCMapChunk::BuildMeshData(
    FIntPoint ChunkCoord, const FOCInfiniteHeightfield& Heightfield,
    const FOCRegionBlobTable& BlobTable, const FOCChunkBuildParams& Params)
{
    TSharedPtr<FOCChunkMeshData> Data = MakeShared<FOCChunkMeshData>();

    const float Spacing = Params.VertexSpacing;
    const int32 NumX = FMath::RoundToInt(Params.ChunkSize / Spacing) + 1;
    const int32 NumY = NumX;
    const FVector2D Origin = FVector2D(static_cast<double>(ChunkCoord.X), static_cast<double>(ChunkCoord.Y)) * Params.ChunkSize;

    // 高度一次性采样到网格(多采一圈边界):法线直接用相邻格点中心差分
    const int32 HNum = NumX + 2;
    TArray<float> Heights;
    Heights.SetNumUninitialized(HNum * HNum);
    for (int32 j = 0; j < HNum; ++j)
    {
        for (int32 i = 0; i < HNum; ++i)
        {
            const FVector2D P(Origin.X + (i - 1) * Spacing, Origin.Y + (j - 1) * Spacing);
            Heights[i + j * HNum] = Heightfield.GetHeight(P, BlobTable);
        }
    }
    auto HeightAt = [&Heights, HNum](int32 i, int32 j) // 顶点 (i,j) 的高度(i,j ∈ [-1, NumX])
    {
        return Heights[(i + 1) + (j + 1) * HNum];
    };

    // ---- 深海剔除:四角全低于剔除面的四边形不生成三角形 ----
    // 顶点压缩:只保留被保留四边形引用的顶点(顶点数据也只算这些,顺带省掉深水区的法线/顶点色计算)
    const float CullZ = Params.SeaLevelZ - Params.MeshCullDepth;

    TArray<bool> KeepQuad;
    KeepQuad.SetNumZeroed((NumX - 1) * (NumY - 1));
    TArray<int32> VertexRemap;
    VertexRemap.Init(-1, NumX * NumY);

    int32 KeptQuads = 0;
    for (int32 j = 0; j < NumY - 1; ++j)
    {
        for (int32 i = 0; i < NumX - 1; ++i)
        {
            const bool bKeep = HeightAt(i, j) >= CullZ || HeightAt(i + 1, j) >= CullZ
                            || HeightAt(i, j + 1) >= CullZ || HeightAt(i + 1, j + 1) >= CullZ;
            KeepQuad[i + j * (NumX - 1)] = bKeep;
            if (bKeep)
            {
                ++KeptQuads;
                VertexRemap[i + j * NumX] = 0;       // 标记引用(暂存 0,稍后统一编号)
                VertexRemap[i + 1 + j * NumX] = 0;
                VertexRemap[i + (j + 1) * NumX] = 0;
                VertexRemap[i + 1 + (j + 1) * NumX] = 0;
            }
        }
    }

    if (KeptQuads == 0)
    {
        return Data; // 全深海区块:空数据,管理器不 spawn(深海平面兜底)
    }

    // 顶点色混合带参数
    const float BlendCenter = Params.SeaLevelZ + Params.LandThresholdOffset;
    const float HalfBand = FMath::Max(Params.ShorelineBlendWidth * 0.5f, 1.0f);

    // 为被引用的顶点统一编号并填充数据
    Data->Vertices.Reserve(NumX * NumY);
    Data->Normals.Reserve(NumX * NumY);
    Data->UVs.Reserve(NumX * NumY);
    Data->VertexColors.Reserve(NumX * NumY);
    for (int32 j = 0; j < NumY; ++j)
    {
        for (int32 i = 0; i < NumX; ++i)
        {
            int32& Slot = VertexRemap[i + j * NumX];
            if (Slot < 0)
            {
                continue; // 未被任何保留四边形引用
            }
            const FVector2D P(Origin.X + i * Spacing, Origin.Y + j * Spacing);
            const float H = HeightAt(i, j);
            const float DX = HeightAt(i + 1, j) - HeightAt(i - 1, j);
            const float DY = HeightAt(i, j + 1) - HeightAt(i, j - 1);

            float Center = BlendCenter;
            if (Params.ShorelineJitter > 0.0f)
            {
                Center += Params.ShorelineJitter
                        * OCNoise::GradientNoise2D(P * Params.ShorelineJitterFrequency, Params.NoiseSeed);
            }
            const float LandWeight = FMath::SmoothStep(Center - HalfBand, Center + HalfBand, H);

            Slot = Data->Vertices.Num();
            Data->Vertices.Add(FVector(P.X, P.Y, H));
            Data->Normals.Add(FVector(-DX, -DY, 2.0 * Spacing).GetSafeNormal());
            Data->UVs.Add(FVector2D(P * 0.001)); // 世界平面映射,1000cm 平铺一次
            Data->VertexColors.Add(FLinearColor(LandWeight, LandWeight, LandWeight, 1.0f));
        }
    }

    // 三角形
    Data->Triangles.Reserve(KeptQuads * 6);
    for (int32 j = 0; j < NumY - 1; ++j)
    {
        for (int32 i = 0; i < NumX - 1; ++i)
        {
            if (!KeepQuad[i + j * (NumX - 1)])
            {
                continue;
            }
            const int32 A = VertexRemap[i + j * NumX];
            const int32 B = VertexRemap[i + 1 + j * NumX];
            const int32 C = VertexRemap[i + (j + 1) * NumX];
            const int32 D = VertexRemap[i + 1 + (j + 1) * NumX];
            Data->Triangles.Add(C); Data->Triangles.Add(D); Data->Triangles.Add(A);
            Data->Triangles.Add(D); Data->Triangles.Add(B); Data->Triangles.Add(A);
        }
    }

    return Data;
}

void AOCMapChunk::ApplyMeshData(const TSharedPtr<FOCChunkMeshData>& Data, UMaterialInterface* TerrainMat)
{
    if (!Data.IsValid() || Data->Vertices.Num() == 0)
    {
        return;
    }

    const TArray<FProcMeshTangent> EmptyTangents;
    SeabedMesh->CreateMeshSection_LinearColor(0, Data->Vertices, Data->Triangles, Data->Normals,
        Data->UVs, Data->VertexColors, EmptyTangents, /*bCreateCollision=*/true);

    if (!TerrainMat)
    {
        if (!bLoggedMaterialFallback)
        {
            bLoggedMaterialFallback = true;
            UE_LOG(LogTemp, Warning, TEXT("[OCMapChunk] 地形混合材质未配置,使用引擎默认材质。请在 DA_MapGenConfig 配置 TerrainMaterial"));
        }
        TerrainMat = UMaterial::GetDefaultMaterial(MD_Surface);
    }
    SeabedMesh->SetMaterial(0, TerrainMat);
}
