// OceanCombat. Copyright(c) All rights reserved.

#include "UI/OCPauseMenuWidget.h"

#include "Components/Button.h"
#include "Controllers/OCPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UOCPauseMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 需要接收键盘焦点才能用 Esc 关闭菜单
    SetIsFocusable(true);

    if (ResumeButton)
    {
        ResumeButton->OnClicked.AddDynamic(this, &UOCPauseMenuWidget::HandleResumeClicked);
    }

    if (MainMenuButton)
    {
        MainMenuButton->OnClicked.AddDynamic(this, &UOCPauseMenuWidget::HandleMainMenuClicked);
    }

    if (QuitButton)
    {
        QuitButton->OnClicked.AddDynamic(this, &UOCPauseMenuWidget::HandleQuitClicked);
    }
}

FReply UOCPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    // 暂停时 EnhancedInput 不工作,Esc 关闭菜单在这里处理
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        HandleResumeClicked();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UOCPauseMenuWidget::HandleResumeClicked()
{
    if (AOCPlayerController* PC = GetOwningPlayer<AOCPlayerController>())
    {
        PC->ClosePauseMenu();
    }
}

void UOCPauseMenuWidget::HandleMainMenuClicked()
{
    // 切地图前先解除暂停,避免把暂停状态带进下一张地图
    UGameplayStatics::SetGamePaused(this, false);
    UGameplayStatics::OpenLevel(this, MainMenuMapName);
}

void UOCPauseMenuWidget::HandleQuitClicked()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
