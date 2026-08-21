// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OCMainMenuWidget.generated.h"

class UButton;

/**
 * 主菜单 Widget 基类:开始游戏 + 退出程序。
 * 开始游戏:OpenLevel 加载主场景地图(默认 OC01)。
 * 布局/样式全部在蓝图子类(WBP_MainMenu)里做,按钮命名需与 BindWidget 一致。
 */
UCLASS()
class OCEANCOMBAT_API UOCMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    /** 开始游戏按钮 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> StartButton;

    /** 退出程序按钮 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> QuitButton;

    /** 点击开始游戏后加载的主场景地图名 */
    UPROPERTY(EditDefaultsOnly, Category = "Menu")
    FName GameMapName = TEXT("OC01");

private:
    /** 开始游戏:加载主场景 */
    UFUNCTION()
    void HandleStartClicked();

    /** 退出程序 */
    UFUNCTION()
    void HandleQuitClicked();
};
