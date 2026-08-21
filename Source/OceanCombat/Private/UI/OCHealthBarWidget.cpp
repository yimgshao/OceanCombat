// OceanCombat. Copyright(c) All rights reserved.

#include "UI/OCHealthBarWidget.h"

#include "Components/ProgressBar.h"
#include "Components/OCHealthComponent.h"

void UOCHealthBarWidget::InitWithHealthComponent(UOCHealthComponent* InHealthComponent)
{
    if (!InHealthComponent)
    {
        return;
    }

    HealthComp = InHealthComponent;
    InHealthComponent->OnHealthChanged.AddDynamic(this, &UOCHealthBarWidget::HandleHealthChanged);
    InHealthComponent->OnDeath.AddDynamic(this, &UOCHealthBarWidget::HandleDeath);

    RefreshBar();
}

void UOCHealthBarWidget::NativeDestruct()
{
    if (UOCHealthComponent* Comp = HealthComp.Get())
    {
        Comp->OnHealthChanged.RemoveDynamic(this, &UOCHealthBarWidget::HandleHealthChanged);
        Comp->OnDeath.RemoveDynamic(this, &UOCHealthBarWidget::HandleDeath);
    }
    HealthComp.Reset();

    Super::NativeDestruct();
}

void UOCHealthBarWidget::HandleHealthChanged(float OldHealth, float NewHealth)
{
    RefreshBar();
}

void UOCHealthBarWidget::HandleDeath(AActor* DeadActor, AController* KillerController)
{
    SetVisibility(ESlateVisibility::Hidden);
}

void UOCHealthBarWidget::RefreshBar()
{
    UOCHealthComponent* Comp = HealthComp.Get();
    if (!Comp || !HealthBar)
    {
        return;
    }

    const float Percent = Comp->GetHealthPercent();
    HealthBar->SetPercent(Percent);

    if (bHideWhenFullHealth)
    {
        SetVisibility(Percent >= 1.0f ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
    }
}
