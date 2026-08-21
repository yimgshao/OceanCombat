// OceanCombat. Copyright(c) All rights reserved.

#include "UI/OCPlayerHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFlow/OCGameMode.h"
#include "UI/OCHealthBarWidget.h"
#include "Weapons/OCWeaponTurret.h"

void UOCPlayerHUDWidget::InitWithSources(UOCHealthComponent* Health, AOCWeaponTurret* InTurret)
{
    // 血量:复用敌人血条同一套绑定(委托驱动)
    if (HealthBarWidget && Health)
    {
        HealthBarWidget->InitWithHealthComponent(Health);
    }

    // CD:存炮塔引用,NativeTick 每帧读
    Turret = InTurret;
}

void UOCPlayerHUDWidget::BindScoreSource(AOCGameMode* GameMode)
{
    if (!GameMode)
    {
        return;
    }

    ScoreSource = GameMode;
    GameMode->OnScoreChanged.AddDynamic(this, &UOCPlayerHUDWidget::HandleScoreChanged);

    // 立即刷新一次(HUD 复活重建时能显示当前分,而不是等下一次加分)
    HandleScoreChanged(GameMode->GetTotalScore(), GameMode->GetRemainingScore());
}

void UOCPlayerHUDWidget::NativeDestruct()
{
    if (AOCGameMode* GameMode = ScoreSource.Get())
    {
        GameMode->OnScoreChanged.RemoveDynamic(this, &UOCPlayerHUDWidget::HandleScoreChanged);
    }
    ScoreSource.Reset();

    Super::NativeDestruct();
}

void UOCPlayerHUDWidget::HandleScoreChanged(int32 NewTotalScore, int32 NewRemainingScore)
{
    if (TotalScoreText)
    {
        TotalScoreText->SetText(FText::AsNumber(NewTotalScore));
    }
    if (RemainingScoreText)
    {
        RemainingScoreText->SetText(FText::AsNumber(NewRemainingScore));
    }
}

void UOCPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (CooldownBar)
    {
        // 显示装填进度(1 - CD 剩余比例):满条 = 可开火;炮塔没了视为就绪
        const float ReloadPercent = Turret.IsValid() ? 1.0f - Turret->GetFireCooldownPercent() : 1.0f;
        CooldownBar->SetPercent(ReloadPercent);
    }
}
