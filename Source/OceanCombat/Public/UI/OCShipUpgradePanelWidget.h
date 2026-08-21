// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OCShipUpgradePanelWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class AOCGameMode;
class UOCShipUpgradeComponent;
class UOCUpgradeEntryWidget;

/**
 * 小船升级面板 Widget 基类:读 DT_ShipUpgrades,每行生成一个升级条目。
 * 由 OCPlayerController 按 Tab 创建(此时游戏已 SetGamePaused 全局暂停)。
 * 打开状态下再按 Tab / Esc 关闭面板。
 * 布局/样式全部在蓝图子类(WBP_ShipUpgradePanel)里做,控件命名需与 BindWidget 一致。
 */
UCLASS()
class OCEANCOMBAT_API UOCShipUpgradePanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 刷新余额文本与所有条目。购买后整体刷新(买贵的会让其他条目余额不足变灰) */
    void RefreshAll();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /**
     * Tab / Esc 关闭面板。必须用 PreviewKeyDown 而不是 OnKeyDown:
     * 1) 暂停时 PlayerController 不 tick,EnhancedInput 收不到按键,只能走 Slate;
     * 2) Tab 是 Slate 的焦点导航键,焦点在按钮上时会被按钮的 OnKeyDown 当作导航吃掉。
     *    PreviewKeyDown 是自根向叶的 tunnel 阶段,早于 OnKeyDown 的冒泡,能稳定拦到。
     */
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    /** 升级条目的容器,蓝图里放 VerticalBox / ScrollBox 并命名一致 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UPanelWidget> EntryContainer;

    /** 当前余额文本(可选) */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> RemainingScoreText;

    /** 关闭按钮(可选,Tab/Esc 也能关) */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> CloseButton;

    /** 条目的 Widget 类,蓝图里配 WBP_UpgradeEntry */
    UPROPERTY(EditDefaultsOnly, Category = "Upgrade")
    TSubclassOf<UOCUpgradeEntryWidget> EntryWidgetClass;

private:
    /** 按表行生成条目并填入 EntryContainer */
    void BuildEntries();

    /** 关闭面板:交还给 PlayerController 解除暂停并恢复输入 */
    UFUNCTION()
    void HandleCloseClicked();

    /** 购买成功回调(绑定组件的 OnUpgradePurchased):整体刷新 */
    UFUNCTION()
    void HandleUpgradePurchased();

    /**
     * 得分变化回调(绑定 GameMode 的 OnScoreChanged):整体刷新。
     * 光绑 OnUpgradePurchased 不够 —— 外部来源的加分(作弊命令 OCAddScore、
     * 或将来面板不暂停时的击杀加分)也要让按钮的灰/亮态跟着变。
     */
    UFUNCTION()
    void HandleScoreChanged(int32 NewTotalScore, int32 NewRemainingScore);

    /** 已生成的条目,RefreshAll 时逐个刷 */
    UPROPERTY()
    TArray<TObjectPtr<UOCUpgradeEntryWidget>> Entries;

    /** 数据源(弱引用;组件挂在 PC 上,与本 Widget 同生共死) */
    TWeakObjectPtr<UOCShipUpgradeComponent> UpgradeComponent;

    /** 分数数据源(弱引用;GameMode 整局常驻,实际不会失效) */
    TWeakObjectPtr<AOCGameMode> ScoreSource;
};
