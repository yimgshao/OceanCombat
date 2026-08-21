// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OCVictoryMenuWidget.generated.h"

class UButton;

/**
 * 胜利菜单 Widget 基类:重新开始 + 返回主菜单 + 退出程序。
 * 由 OCPlayerController 在收到 OCGameMode::OnVictory(所有城堡被摧毁)时创建,
 * 此时游戏已 SetGamePaused 全局暂停。
 * 胜利文本与布局/样式全部在蓝图子类(WBP_VictoryMenu)里做,按钮命名需与 BindWidget 一致。
 */
UCLASS()
class OCEANCOMBAT_API UOCVictoryMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    /** 重新开始按钮 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> RestartButton;

    /** 返回主菜单按钮 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> MainMenuButton;

    /** 退出程序按钮 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> QuitButton;

    /** 返回主菜单时加载的地图名 */
    UPROPERTY(EditDefaultsOnly, Category = "Menu")
    FName MainMenuMapName = TEXT("MainMenu");

private:
    /** 重新开始:解除暂停后重新加载当前地图 */
    UFUNCTION()
    void HandleRestartClicked();

    /** 返回主菜单:先解除暂停再 OpenLevel */
    UFUNCTION()
    void HandleMainMenuClicked();

    /** 退出程序 */
    UFUNCTION()
    void HandleQuitClicked();
};
