// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OCPauseMenuWidget.generated.h"

class UButton;

/**
 * 局内暂停菜单 Widget 基类:继续游戏 + 返回主菜单 + 退出程序。
 * 由 OCPlayerController 按 Esc 创建(此时游戏已 SetGamePaused 全局暂停)。
 * 打开状态下再按 Esc 等于"继续游戏"(暂停时 PlayerController 不 tick,
 * EnhancedInput 收不到 Esc,所以按键走这里的 NativeOnKeyDown)。
 * 布局/样式全部在蓝图子类(WBP_PauseMenu)里做,按钮命名需与 BindWidget 一致。
 */
UCLASS()
class OCEANCOMBAT_API UOCPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    /** 继续游戏按钮 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> ResumeButton;

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
    /** 继续游戏:交还给 PlayerController 关闭菜单并恢复输入 */
    UFUNCTION()
    void HandleResumeClicked();

    /** 返回主菜单:先解除暂停再 OpenLevel */
    UFUNCTION()
    void HandleMainMenuClicked();

    /** 退出程序 */
    UFUNCTION()
    void HandleQuitClicked();
};
