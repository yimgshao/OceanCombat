// OceanCombat. Copyright(c) All rights reserved.

#include "UI/OCVictoryMenuWidget.h"

#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UOCVictoryMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (RestartButton)
    {
        RestartButton->OnClicked.AddDynamic(this, &UOCVictoryMenuWidget::HandleRestartClicked);
    }

    if (MainMenuButton)
    {
        MainMenuButton->OnClicked.AddDynamic(this, &UOCVictoryMenuWidget::HandleMainMenuClicked);
    }

    if (QuitButton)
    {
        QuitButton->OnClicked.AddDynamic(this, &UOCVictoryMenuWidget::HandleQuitClicked);
    }
}

void UOCVictoryMenuWidget::HandleRestartClicked()
{
    // 切地图前先解除暂停,避免把暂停状态带进重载的地图
    UGameplayStatics::SetGamePaused(this, false);

    // bRemovePrefixString=true:PIE 下地图名带 "UEDPIE_0_" 前缀,去掉才能正确 OpenLevel
    const FString CurrentMapName = UGameplayStatics::GetCurrentLevelName(this, true);
    UGameplayStatics::OpenLevel(this, FName(*CurrentMapName));
}

void UOCVictoryMenuWidget::HandleMainMenuClicked()
{
    UGameplayStatics::SetGamePaused(this, false);
    UGameplayStatics::OpenLevel(this, MainMenuMapName);
}

void UOCVictoryMenuWidget::HandleQuitClicked()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
