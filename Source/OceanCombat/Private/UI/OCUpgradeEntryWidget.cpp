// OceanCombat. Copyright(c) All rights reserved.

#include "UI/OCUpgradeEntryWidget.h"

#include "Components/Button.h"
#include "Components/OCShipUpgradeComponent.h"
#include "Components/TextBlock.h"
#include "Pawns/OCPawnBase.h"

/** 属性数值的显示格式:最多一位小数,整数时不显示小数点(航速是整数级,转向力矩带 .5) */
static FNumberFormattingOptions MakeStatNumberFormat()
{
    FNumberFormattingOptions Options;
    Options.MinimumFractionalDigits = 0;
    Options.MaximumFractionalDigits = 1;
    return Options;
}

void UOCUpgradeEntryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (UpgradeButton)
    {
        UpgradeButton->OnClicked.AddDynamic(this, &UOCUpgradeEntryWidget::HandleUpgradeClicked);
    }
}

void UOCUpgradeEntryWidget::InitWithUpgrade(UOCShipUpgradeComponent* InComponent, const FOCShipUpgradeRow& Row)
{
    UpgradeComponent = InComponent;
    UpgradeType = Row.UpgradeType;

    // 静态文本(表里配好后不再变)只在这里设一次,Refresh 只管随等级/余额变化的部分
    if (NameText)
    {
        NameText->SetText(Row.DisplayName);
    }
    if (DescriptionText)
    {
        DescriptionText->SetText(Row.Description);
    }

    Refresh();
}

void UOCUpgradeEntryWidget::Refresh()
{
    UOCShipUpgradeComponent* Component = UpgradeComponent.Get();
    if (!Component)
    {
        return;
    }

    const int32 Level = Component->GetLevel(UpgradeType);
    const int32 MaxLevel = Component->GetMaxLevel(UpgradeType);
    const bool bIsMaxLevel = Component->IsMaxLevel(UpgradeType);

    if (LevelText)
    {
        LevelText->SetText(FText::Format(
            NSLOCTEXT("OceanCombat", "UpgradeLevelFormat", "Lv {0} / {1}"),
            FText::AsNumber(Level), FText::AsNumber(MaxLevel)));
    }

    if (CostText)
    {
        CostText->SetText(bIsMaxLevel
            ? MaxLevelText
            : FText::AsNumber(Component->GetNextCost(UpgradeType)));
    }

    if (ValueText)
    {
        // 当前绝对值由船提供(已包含已应用的加成),下一级 = 当前 + 下一级增量
        const AOCPawnBase* Pawn = Cast<AOCPawnBase>(GetOwningPlayerPawn());
        const float CurrentValue = Pawn ? Pawn->GetUpgradableStatValue(UpgradeType) : 0.0f;
        const FNumberFormattingOptions Format = MakeStatNumberFormat();

        ValueText->SetText(bIsMaxLevel
            ? FText::AsNumber(CurrentValue, &Format)
            : FText::Format(NSLOCTEXT("OceanCombat", "UpgradeValueFormat", "{0} → {1}"),
                FText::AsNumber(CurrentValue, &Format),
                FText::AsNumber(CurrentValue + Component->GetNextDelta(UpgradeType), &Format)));
    }

    if (UpgradeButton)
    {
        // 满级或余额不足都置灰(CanUpgrade 已覆盖两种情况)
        UpgradeButton->SetIsEnabled(Component->CanUpgrade(UpgradeType));
    }
}

void UOCUpgradeEntryWidget::HandleUpgradeClicked()
{
    if (UOCShipUpgradeComponent* Component = UpgradeComponent.Get())
    {
        // 刷新不在这里做:组件购买成功会广播 OnUpgradePurchased,由面板统一整体刷新
        // (买贵的会让其他条目余额不足变灰,只刷自己不够)
        Component->TryUpgrade(UpgradeType);
    }
}
