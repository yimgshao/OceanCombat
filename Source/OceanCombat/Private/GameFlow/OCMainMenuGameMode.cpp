// OceanCombat. Copyright(c) All rights reserved.

#include "GameFlow/OCMainMenuGameMode.h"

#include "GameFramework/PlayerController.h"
#include "UI/OCMainMenuWidget.h"

AOCMainMenuGameMode::AOCMainMenuGameMode()
{
    // 菜单地图不需要 Pawn
    DefaultPawnClass = nullptr;
}

void AOCMainMenuGameMode::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[OCMainMenuGameMode] 找不到 PlayerController,主菜单未创建"));
        return;
    }

    if (MainMenuWidgetClass)
    {
        if (UOCMainMenuWidget* MenuWidget = CreateWidget<UOCMainMenuWidget>(PC, MainMenuWidgetClass))
        {
            MenuWidget->AddToViewport();
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[OCMainMenuGameMode] MainMenuWidgetClass 未配置,主菜单未创建"));
    }

    // 菜单只接收 UI 输入,显示鼠标
    PC->SetInputMode(FInputModeUIOnly());
    PC->bShowMouseCursor = true;
}
