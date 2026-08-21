// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OCMainMenuGameMode.generated.h"

class UOCMainMenuWidget;

/**
 * 主菜单地图的游戏模式。不生成 Pawn,BeginPlay 时创建主菜单 Widget 并切到 UI 输入模式。
 * 在 MainMenu 地图的 World Settings 里把 GameMode Override 设为本类(或其蓝图子类)。
 */
UCLASS()
class OCEANCOMBAT_API AOCMainMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AOCMainMenuGameMode();

    virtual void BeginPlay() override;

protected:
    /** 主菜单 Widget 类,蓝图里配 WBP_MainMenu */
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UOCMainMenuWidget> MainMenuWidgetClass;
};
