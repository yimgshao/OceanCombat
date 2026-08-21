// OceanCombat. Copyright(c) All rights reserved.

#include "UI/OCMainMenuWidget.h"

#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UOCMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (StartButton)
    {
        StartButton->OnClicked.AddDynamic(this, &UOCMainMenuWidget::HandleStartClicked);
    }

    if (QuitButton)
    {
        QuitButton->OnClicked.AddDynamic(this, &UOCMainMenuWidget::HandleQuitClicked);
    }
}

void UOCMainMenuWidget::HandleStartClicked()
{
    UGameplayStatics::OpenLevel(this, GameMapName);
}

void UOCMainMenuWidget::HandleQuitClicked()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
