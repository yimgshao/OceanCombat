// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OCPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class AOCFollowCamera;
class UOCPlayerHUDWidget;
class UOCPauseMenuWidget;
class UOCVictoryMenuWidget;
class UOCShipUpgradePanelWidget;
class UOCShipUpgradeComponent;
struct FInputActionValue;

/**
 * 玩家控制器。接收 EnhancedInput 输入,翻译成船的 AddThrottleInput/AddSteerInput 调用。
 *
 * 输入资产(在蓝图或 Details 面板里指派):
 * - ThrottleAction: IA_Throttle (Axis1D, W=+1, S=-1)
 * - SteerAction:    IA_Steer    (Axis1D, D=+1, A=-1)
 * - DefaultContext: IMC_Default
 */
UCLASS()
class OCEANCOMBAT_API AOCPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AOCPlayerController();

    /** 拥有 Pawn 时调用:把输入映射上下文推入 EnhancedInput 子系统 */
    virtual void OnPossess(APawn* InPawn) override;

    /** 释放 Pawn 时调用:移除屏幕 HUD */
    virtual void OnUnPossess() override;

    /** 绑定输入动作到处理函数 */
    virtual void SetupInputComponent() override;

    /** 关闭暂停菜单:移除 Widget、解除暂停、恢复游戏输入。由暂停菜单的"继续游戏"按钮/Esc 调用 */
    void ClosePauseMenu();

    /** 关闭升级面板:移除 Widget、解除暂停、恢复游戏输入。由面板的关闭按钮/Tab/Esc 调用 */
    void CloseUpgradePanel();

    /** 升级组件(持有已购等级,活得比船久)。升级面板从这里取数据源 */
    UFUNCTION(BlueprintPure, Category = "Upgrade")
    UOCShipUpgradeComponent* GetUpgradeComponent() const { return UpgradeComponent; }

protected:
    // ---- 输入资产引用(在蓝图/Details 里指派 .uasset)----
    /** 油门动作 (W/S) */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> ThrottleAction;

    /** 转向动作 (A/D) */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> SteerAction;

    /** 炮管俯仰动作 (鼠标滚轮) */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> BarrelPitchAction;

    /** 开火动作 (鼠标左键) */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> FireAction;

    /** 瞄准动作 (鼠标右键,长按显示瞄准线) */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> AimAction;

    /** 暂停菜单动作 (Esc) */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> PauseMenuAction;

    /** 升级面板动作 (Tab) */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> UpgradePanelAction;

    /** 滚轮每格对应的俯仰角步进(度),负值反转滚轮方向 */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    float BarrelPitchStep = -5.0f;

    /** 默认输入映射上下文 */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultContext;

    /** 屏幕 HUD(血条 + 开火 CD)的 Widget 类,蓝图里配 WBP_PlayerHUD */
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UOCPlayerHUDWidget> HUDWidgetClass;

    /** 局内暂停菜单的 Widget 类,蓝图里配 WBP_PauseMenu */
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UOCPauseMenuWidget> PauseMenuWidgetClass;

    /** 胜利菜单的 Widget 类,蓝图里配 WBP_VictoryMenu */
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UOCVictoryMenuWidget> VictoryMenuWidgetClass;

    /** 小船升级面板的 Widget 类,蓝图里配 WBP_ShipUpgradePanel */
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UOCShipUpgradePanelWidget> UpgradePanelWidgetClass;

    // ---- 输入处理函数 ----
    /** 处理油门输入,转发到当前 Pawn 的 AddThrottleInput */
    void HandleThrottle(const FInputActionValue& Value);

    /** 处理转向输入,转发到当前 Pawn 的 AddSteerInput */
    void HandleSteer(const FInputActionValue& Value);

    /** 处理滚轮输入,转发到炮塔的 AddBarrelPitch */
    void HandleBarrelPitch(const FInputActionValue& Value);

    /** 处理开火输入(左键按下),调用炮塔的 Fire */
    void HandleFire(const FInputActionValue& Value);

    /** 右键按下:显示瞄准线 */
    void HandleAimStart(const FInputActionValue& Value);

    /** 右键松开:隐藏瞄准线 */
    void HandleAimStop(const FInputActionValue& Value);

    /** 处理暂停菜单输入(Esc),打开暂停菜单 */
    void HandlePauseMenu(const FInputActionValue& Value);

    /** 打开暂停菜单:创建 Widget、全局暂停、切到 UI 输入模式。重复调用安全(已打开时忽略) */
    void OpenPauseMenu();

    /** 处理升级面板输入(Tab),打开升级面板 */
    void HandleUpgradePanel(const FInputActionValue& Value);

    /** 打开升级面板:创建 Widget、全局暂停、切到 UI 输入模式。重复调用安全(已打开时忽略) */
    void OpenUpgradePanel();

    /** 胜利回调(绑定 OCGameMode::OnVictory):弹出胜利菜单并全局暂停 */
    UFUNCTION()
    void HandleVictory();

private:
    /** 玩家拥有的跟随相机实例(OnPossess 时 Spawn) */
    UPROPERTY()
    TObjectPtr<AOCFollowCamera> FollowCamera;
    /** 屏幕 HUD 实例(OnPossess 时创建,OnUnPossess 时移除) */
    UPROPERTY()
    TObjectPtr<UOCPlayerHUDWidget> HUDWidget;

    /** 暂停菜单实例(Esc 打开,继续游戏/返回主菜单时移除) */
    UPROPERTY()
    TObjectPtr<UOCPauseMenuWidget> PauseMenuWidget;

    /** 胜利菜单实例(OnVictory 时创建) */
    UPROPERTY()
    TObjectPtr<UOCVictoryMenuWidget> VictoryMenuWidget;

    /** 升级面板实例(Tab 打开,再按 Tab/Esc 或点关闭按钮时移除) */
    UPROPERTY()
    TObjectPtr<UOCShipUpgradePanelWidget> UpgradePanelWidget;

    /**
     * 升级组件:构造时创建,挂在 PC 上而不是船上。
     * 船死亡后会被 Destroy 并重新 Spawn,等级数据必须活得比船久(复活后由 OnPossess 重新下发)。
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UOCShipUpgradeComponent> UpgradeComponent;
};
