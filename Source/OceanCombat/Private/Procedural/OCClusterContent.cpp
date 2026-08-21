// OceanCombat. Copyright(c) All rights reserved.

#include "Procedural/OCClusterContent.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"

AOCClusterContent::AOCClusterContent()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AOCClusterContent::ApplyDecorations(const TArray<FOCDecoInstance>& Instances,
    const TArray<FOCDecoRule>& DecoRules, const TArray<FOCDecoMeshList>& DecoMeshes)
{
    for (const FOCDecoInstance& Inst : Instances)
    {
        if (!DecoRules.IsValidIndex(Inst.Rule) || !DecoMeshes.IsValidIndex(Inst.Rule))
        {
            continue;
        }
        const TArray<TObjectPtr<UStaticMesh>>& Meshes = DecoMeshes[Inst.Rule].Meshes;
        if (!Meshes.IsValidIndex(Inst.Entry) || !Meshes[Inst.Entry])
        {
            continue;
        }
        if (UHierarchicalInstancedStaticMeshComponent* Comp =
            GetOrCreateDecoComponent(Meshes[Inst.Entry], DecoRules[Inst.Rule].bCollision))
        {
            Comp->AddInstance(Inst.Transform, /*bWorldSpace=*/true);
        }
    }
}

UHierarchicalInstancedStaticMeshComponent* AOCClusterContent::GetOrCreateDecoComponent(UStaticMesh* Mesh, bool bCollision)
{
    if (!Mesh)
    {
        return nullptr;
    }
    if (TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* Existing = DecoComponents.Find(Mesh))
    {
        return *Existing;
    }

    UHierarchicalInstancedStaticMeshComponent* Comp = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
    if (!Comp)
    {
        return nullptr;
    }
    Comp->SetStaticMesh(Mesh);
    Comp->SetupAttachment(RootComponent);
    // Mobility 保持默认 Movable,与根组件一致
    if (bCollision)
    {
        Comp->SetCollisionProfileName(TEXT("BlockAll")); // 石头:挡船/挡弹
    }
    else
    {
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 植被:纯视觉,省开销
    }
    Comp->RegisterComponent();
    // 标记为实例组件:编辑器烘焙(OC.BakeMenuMap)时才会随关卡序列化保存;运行时无副作用
    AddInstanceComponent(Comp);

    DecoComponents.Add(Mesh, Comp);
    return Comp;
}
