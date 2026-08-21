// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/OCShipUpgradeRow.h"
#include "OCUpgradeEntryWidget.generated.h"

class UButton;
class UTextBlock;
class UOCShipUpgradeComponent;

/**
 * 升级面板里的单个升级项条目(属性名 + 等级 + 消耗 + 升级按钮)。
 * 由 UOCShipUpgradePanelWidget 按表行动态创建,不需要在面板蓝图里手摆。
 * 布局/样式在蓝图子类(WBP_UpgradeEntry)里做,控件命名需与 BindWidget 一致。
 */
UCLASS()
class OCEANCOMBAT_API UOCUpgradeEntryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 绑定数据源并刷新一次。由面板在创建条目后立即调用 */
    void InitWithUpgrade(UOCShipUpgradeComponent* InComponent, const FOCShipUpgradeRow& Row);

    /** 按当前等级与余额刷新所有文本和按钮可用态 */
    void Refresh();

protected:
    virtual void NativeConstruct() override;

    /** 属性显示名(取自表行的 DisplayName) */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> NameText;

    /** 等级文本,形如 "Lv 2 / 5" */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> LevelText;

    /** 消耗文本,已满级时显示"已满级" */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> CostText;

    /** 升级按钮。满级或余额不足时置灰 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> UpgradeButton;

    /** 数值预览,形如 "1500 → 1650"(可选) */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ValueText;

    /** 表行的 Description(可选) */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> DescriptionText;

    /** 已满级时 CostText 显示的文本 */
    UPROPERTY(EditDefaultsOnly, Category = "Upgrade")
    FText MaxLevelText = NSLOCTEXT("OceanCombat", "UpgradeMaxLevel", "已满级");

private:
    /** 点击升级:走组件消费得分,成功与否都由组件侧决定,这里只负责触发 */
    UFUNCTION()
    void HandleUpgradeClicked();

    /** 数据源(弱引用;组件挂在 PC 上,与本 Widget 同生共死,实际不会失效) */
    TWeakObjectPtr<UOCShipUpgradeComponent> UpgradeComponent;

    /** 本条目对应的属性种类 */
    EOCShipUpgradeType UpgradeType = EOCShipUpgradeType::MoveSpeed;
};
