// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OCPlayerHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UOCHealthComponent;
class UOCHealthBarWidget;
class AOCWeaponTurret;
class AOCGameMode;

/**
 * 玩家屏幕 HUD 基类:血量条 + 开火 CD 条。
 * 血量走委托(复用 OCHealthBarWidget,和敌人头顶血条同一套逻辑),
 * CD 每帧轮询炮塔(CD 是连续变化量,轮询比事件自然)。
 * 布局/样式全部在蓝图子类(WBP_PlayerHUD)里做。
 */
UCLASS()
class OCEANCOMBAT_API UOCPlayerHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 绑定数据源:玩家船的血量组件 + 炮塔。由 PlayerController 在 Possess 时调用 */
    void InitWithSources(UOCHealthComponent* Health, AOCWeaponTurret* InTurret);

    /** 绑定分数数据源(GameMode):立即刷新一次,并监听 OnScoreChanged。由 PlayerController 在创建 HUD 时调用 */
    void BindScoreSource(AOCGameMode* GameMode);

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeDestruct() override;

    /** 血条子 Widget:蓝图里放一个 OCHealthBarWidget 子类实例(如 WBP_PlayerHealthBar),命名一致 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UOCHealthBarWidget> HealthBarWidget;

    /** 开火 CD 进度条。显示装填进度:空=刚开火,满=可开火 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> CooldownBar;

    /** 总得分文本(可选:蓝图里放同名 TextBlock 即自动刷新) */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TotalScoreText;

    /** 剩余得分(余额)文本(可选) */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> RemainingScoreText;

private:
    UFUNCTION()
    void HandleScoreChanged(int32 NewTotalScore, int32 NewRemainingScore);

    /** 炮塔弱引用(随单位销毁自动失效) */
    TWeakObjectPtr<AOCWeaponTurret> Turret;

    /** 分数数据源(弱引用;GameMode 整局常驻,实际不会失效) */
    TWeakObjectPtr<AOCGameMode> ScoreSource;
};
