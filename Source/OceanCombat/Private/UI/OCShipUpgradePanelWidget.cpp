// OceanCombat. Copyright(c) All rights reserved.

#include "UI/OCShipUpgradePanelWidget.h"

#include "Components/Button.h"
#include "Components/OCShipUpgradeComponent.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Controllers/OCPlayerController.h"
#include "GameFlow/OCGameMode.h"
#include "UI/OCUpgradeEntryWidget.h"

void UOCShipUpgradePanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 需要接收键盘焦点才能用 Tab/Esc 关闭面板
    SetIsFocusable(true);

    if (CloseButton)
    {
        CloseButton->OnClicked.AddDynamic(this, &UOCShipUpgradePanelWidget::HandleCloseClicked);
    }

    const AOCPlayerController* PC = GetOwningPlayer<AOCPlayerController>();
    UpgradeComponent = PC ? PC->GetUpgradeComponent() : nullptr;

    if (UOCShipUpgradeComponent* Component = UpgradeComponent.Get())
    {
        Component->OnUpgradePurchased.AddDynamic(this, &UOCShipUpgradePanelWidget::HandleUpgradePurchased);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Upgrade] PlayerController 上找不到 UOCShipUpgradeComponent,升级面板无数据"));
    }

    // 外部加分(作弊命令等)也要刷新按钮灰/亮态,不能只靠 OnUpgradePurchased
    if (AOCGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOCGameMode>() : nullptr)
    {
        ScoreSource = GameMode;
        GameMode->OnScoreChanged.AddDynamic(this, &UOCShipUpgradePanelWidget::HandleScoreChanged);
    }

    BuildEntries();
    RefreshAll();
}

void UOCShipUpgradePanelWidget::NativeDestruct()
{
    if (UOCShipUpgradeComponent* Component = UpgradeComponent.Get())
    {
        Component->OnUpgradePurchased.RemoveDynamic(this, &UOCShipUpgradePanelWidget::HandleUpgradePurchased);
    }
    UpgradeComponent.Reset();

    if (AOCGameMode* GameMode = ScoreSource.Get())
    {
        GameMode->OnScoreChanged.RemoveDynamic(this, &UOCShipUpgradePanelWidget::HandleScoreChanged);
    }
    ScoreSource.Reset();

    Super::NativeDestruct();
}

void UOCShipUpgradePanelWidget::BuildEntries()
{
    Entries.Reset();

    UOCShipUpgradeComponent* Component = UpgradeComponent.Get();
    if (!Component || !EntryContainer)
    {
        return;
    }

    if (!EntryWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Upgrade] 未配置 EntryWidgetClass(应指向 WBP_UpgradeEntry),升级面板将为空"));
        return;
    }

    EntryContainer->ClearChildren();

    TArray<const FOCShipUpgradeRow*> Rows;
    Component->GetUpgradeRows(Rows);

    for (const FOCShipUpgradeRow* Row : Rows)
    {
        UOCUpgradeEntryWidget* Entry = CreateWidget<UOCUpgradeEntryWidget>(this, EntryWidgetClass);
        if (!Entry)
        {
            continue;
        }

        // 先入树再 Init:NativeConstruct(按钮委托绑定)要在 AddChild 时才跑
        EntryContainer->AddChild(Entry);
        Entry->InitWithUpgrade(Component, *Row);
        Entries.Add(Entry);
    }
}

void UOCShipUpgradePanelWidget::RefreshAll()
{
    if (RemainingScoreText)
    {
        const AOCGameMode* GameMode = ScoreSource.Get();
        RemainingScoreText->SetText(FText::AsNumber(GameMode ? GameMode->GetRemainingScore() : 0));
    }

    for (UOCUpgradeEntryWidget* Entry : Entries)
    {
        if (Entry)
        {
            Entry->Refresh();
        }
    }
}

FReply UOCShipUpgradePanelWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Tab || Key == EKeys::Escape)
    {
        HandleCloseClicked();
        return FReply::Handled();
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UOCShipUpgradePanelWidget::HandleCloseClicked()
{
    if (AOCPlayerController* PC = GetOwningPlayer<AOCPlayerController>())
    {
        PC->CloseUpgradePanel();
    }
}

void UOCShipUpgradePanelWidget::HandleUpgradePurchased()
{
    RefreshAll();
}

void UOCShipUpgradePanelWidget::HandleScoreChanged(int32 NewTotalScore, int32 NewRemainingScore)
{
    RefreshAll();
}
