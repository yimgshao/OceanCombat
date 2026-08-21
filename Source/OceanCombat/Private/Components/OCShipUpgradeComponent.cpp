// OceanCombat. Copyright(c) All rights reserved.

#include "Components/OCShipUpgradeComponent.h"

#include "Engine/DataTable.h"
#include "GameFlow/OCGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Pawns/OCPawnBase.h"

UOCShipUpgradeComponent::UOCShipUpgradeComponent()
{
    // 纯数据组件,不需要 Tick
    PrimaryComponentTick.bCanEverTick = false;
}

void UOCShipUpgradeComponent::GetUpgradeRows(TArray<const FOCShipUpgradeRow*>& OutRows) const
{
    OutRows.Reset();
    if (!UpgradeTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Upgrade] 未指派 UpgradeTable,升级面板将为空"));
        return;
    }

    UpgradeTable->ForeachRow<FOCShipUpgradeRow>(TEXT("UOCShipUpgradeComponent::GetUpgradeRows"),
        [&OutRows](const FName& RowName, const FOCShipUpgradeRow& Row)
        {
            OutRows.Add(&Row);
        });

    // 表的行迭代顺序不保证与编辑器里看到的行序一致,按 SortOrder 显式排
    OutRows.Sort([](const FOCShipUpgradeRow& A, const FOCShipUpgradeRow& B)
        {
            return A.SortOrder < B.SortOrder;
        });
}

const FOCShipUpgradeRow* UOCShipUpgradeComponent::FindRow(EOCShipUpgradeType Type) const
{
    if (!UpgradeTable)
    {
        return nullptr;
    }

    const FOCShipUpgradeRow* Found = nullptr;
    UpgradeTable->ForeachRow<FOCShipUpgradeRow>(TEXT("UOCShipUpgradeComponent::FindRow"),
        [Type, &Found](const FName& RowName, const FOCShipUpgradeRow& Row)
        {
            if (Row.UpgradeType == Type && !Found)
            {
                Found = &Row;
            }
        });

    return Found;
}

int32 UOCShipUpgradeComponent::GetLevel(EOCShipUpgradeType Type) const
{
    const int32* Level = Levels.Find(Type);
    return Level ? *Level : 0;
}

int32 UOCShipUpgradeComponent::GetMaxLevel(EOCShipUpgradeType Type) const
{
    const FOCShipUpgradeRow* Row = FindRow(Type);
    return Row ? Row->Levels.Num() : 0;
}

bool UOCShipUpgradeComponent::IsMaxLevel(EOCShipUpgradeType Type) const
{
    return GetLevel(Type) >= GetMaxLevel(Type);
}

int32 UOCShipUpgradeComponent::GetNextCost(EOCShipUpgradeType Type) const
{
    const FOCShipUpgradeRow* Row = FindRow(Type);
    const int32 NextIndex = GetLevel(Type); // 已购 N 级 → 下一级是 Levels[N]
    if (!Row || !Row->Levels.IsValidIndex(NextIndex))
    {
        return INDEX_NONE;
    }

    return Row->Levels[NextIndex].Cost;
}

float UOCShipUpgradeComponent::GetNextDelta(EOCShipUpgradeType Type) const
{
    const FOCShipUpgradeRow* Row = FindRow(Type);
    const int32 NextIndex = GetLevel(Type);
    if (!Row || !Row->Levels.IsValidIndex(NextIndex))
    {
        return 0.0f;
    }

    return Row->Levels[NextIndex].Delta;
}

bool UOCShipUpgradeComponent::CanUpgrade(EOCShipUpgradeType Type) const
{
    const int32 Cost = GetNextCost(Type);
    if (Cost == INDEX_NONE)
    {
        return false; // 已满级或表里没这一行
    }

    const AOCGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOCGameMode>() : nullptr;
    return GameMode && GameMode->GetRemainingScore() >= Cost;
}

bool UOCShipUpgradeComponent::TryUpgrade(EOCShipUpgradeType Type)
{
    const int32 Cost = GetNextCost(Type);
    if (Cost == INDEX_NONE)
    {
        return false;
    }

    AOCGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOCGameMode>() : nullptr;
    if (!GameMode || !GameMode->TrySpendScore(Cost))
    {
        return false; // 余额不足
    }

    const int32 NewLevel = GetLevel(Type) + 1;
    Levels.Add(Type, NewLevel);

    // 立刻下发到当前控制的船(整体重算,不是只加这一级)
    if (const APlayerController* PC = Cast<APlayerController>(GetOwner()))
    {
        ApplyToPawn(PC->GetPawn<AOCPawnBase>());
    }

    UE_LOG(LogTemp, Log, TEXT("[Upgrade] %s 升到 %d 级,消耗 %d 分(余额 %d)"),
        *UEnum::GetValueAsString(Type), NewLevel, Cost, GameMode->GetRemainingScore());

    OnUpgradePurchased.Broadcast();
    return true;
}

void UOCShipUpgradeComponent::ApplyToPawn(AOCPawnBase* Pawn) const
{
    if (!Pawn || Levels.IsEmpty())
    {
        return; // 开局还没买任何升级时是空操作
    }

    Pawn->ApplyStatBonus(BuildBonus());
}

FOCShipStatBonus UOCShipUpgradeComponent::BuildBonus() const
{
    FOCShipStatBonus Bonus;

    for (const TPair<EOCShipUpgradeType, int32>& Pair : Levels)
    {
        const FOCShipUpgradeRow* Row = FindRow(Pair.Key);
        if (!Row)
        {
            continue; // 表被改动导致行消失:该属性的加成静默归零(数值仍在 Levels 里,改回表就恢复)
        }

        // 已购 N 级 = Levels[0..N-1] 的 Delta 之和
        const int32 OwnedLevel = FMath::Min(Pair.Value, Row->Levels.Num());
        for (int32 Index = 0; Index < OwnedLevel; ++Index)
        {
            Bonus.Add(Pair.Key, Row->Levels[Index].Delta);
        }
    }

    return Bonus;
}
