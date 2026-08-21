// OceanCombat. Copyright(c) All rights reserved.

#include "Controllers/OCPlayerController.h"

#include "Pawns/OCPawnBase.h"
#include "Pawns/Boats/OCBoatBase.h"
#include "Camera/OCFollowCamera.h"
#include "Components/OCAimLineComponent.h"
#include "Components/OCShipUpgradeComponent.h"
#include "Weapons/OCWeaponTurret.h"
#include "UI/OCPlayerHUDWidget.h"
#include "UI/OCPauseMenuWidget.h"
#include "UI/OCShipUpgradePanelWidget.h"
#include "UI/OCVictoryMenuWidget.h"
#include "GameFlow/OCGameMode.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"

AOCPlayerController::AOCPlayerController()
{
    // 注意:不要把 PrimaryActorTick.bCanEverTick 设为 false。
    // PlayerController 的输入处理(ProcessPlayerInput → EnhancedInput 评估)
    // 完全依赖 TickActor 驱动;基类 AController 特意把它设为 true。
    // 关掉 tick 会让所有键盘/手柄输入都收不到。

    // 升级组件挂在 PC 上:船会死亡重建,已购等级得活得比船久
    UpgradeComponent = CreateDefaultSubobject<UOCShipUpgradeComponent>(TEXT("UpgradeComponent"));
}

void AOCPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // 把默认输入映射上下文推入 EnhancedInput 子系统
    if (DefaultContext)
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultContext, 0);
        }
    }

    // ---- 创建跟随相机并设为 ViewTarget ----
    if (!FollowCamera && InPawn)
    {
        if (UWorld* World = GetWorld())
        {
            // 在玩家附近 Spawn 相机(位置不重要,Tick 会立即 snap 到正确位置)
            const FVector CamSpawnLocation = InPawn->GetActorLocation();
            const FRotator CamSpawnRotation = InPawn->GetActorRotation();
            FollowCamera = World->SpawnActor<AOCFollowCamera>(AOCFollowCamera::StaticClass(), CamSpawnLocation, CamSpawnRotation);
        }
    }

    if (FollowCamera)
    {
        FollowCamera->SetFollowTarget(InPawn);
        // SetViewTargetWithBlend:blend time=0 表示立即切换(不淡入)
        SetViewTargetWithBlend(FollowCamera, 0.0f);

        // 切换 ViewTarget 到非 Pawn 时,确保 Controller 仍接收游戏输入
        SetInputMode(FInputModeGameOnly());
        bShowMouseCursor = false;
        EnableInput(this);
    }

    // ---- 创建屏幕 HUD(血条 + 开火 CD)----
    if (HUDWidgetClass && !HUDWidget && IsLocalController())
    {
        if (AOCBoatBase* Boat = GetPawn<AOCBoatBase>())
        {
            HUDWidget = CreateWidget<UOCPlayerHUDWidget>(this, HUDWidgetClass);
            if (HUDWidget)
            {
                HUDWidget->InitWithSources(Boat->GetHealthComponent(), Boat->GetTurret());
                HUDWidget->BindScoreSource(GetWorld()->GetAuthGameMode<AOCGameMode>());
                HUDWidget->AddToViewport();
            }
        }
    }

    // ---- 绑定胜利事件:所有城堡被摧毁时弹出胜利菜单 ----
    if (IsLocalController())
    {
        if (AOCGameMode* GameMode = GetWorld()->GetAuthGameMode<AOCGameMode>())
        {
            GameMode->OnVictory.AddUniqueDynamic(this, &AOCPlayerController::HandleVictory);
        }
    }

    // ---- 已购升级重新下发到(新)船 ----
    // 复活时 GameMode 会 Spawn 一艘全新的船,BeginPlay 把配置表数值重新下发了一遍,
    // 升级加成得在这里补上。开局首次 Possess 时还没买任何升级,是空操作。
    if (UpgradeComponent)
    {
        UpgradeComponent->ApplyToPawn(Cast<AOCPawnBase>(InPawn));
    }
}

void AOCPlayerController::OnUnPossess()
{
    if (HUDWidget)
    {
        HUDWidget->RemoveFromParent();
        HUDWidget = nullptr;
    }

    // 拥有对象丢失时(如暂停中船只被击沉)顺手关掉暂停菜单,避免 UI 卡死、游戏停在暂停态
    ClosePauseMenu();
    CloseUpgradePanel();

    if (VictoryMenuWidget)
    {
        VictoryMenuWidget->RemoveFromParent();
        VictoryMenuWidget = nullptr;
    }

    Super::OnUnPossess();
}

void AOCPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // 用 EnhancedInputComponent 绑定(从基类 InputComponent 向上转型)
    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInput)
    {
        return;
    }

    if (ThrottleAction)
    {
        EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AOCPlayerController::HandleThrottle);
    }

    if (SteerAction)
    {
        EnhancedInput->BindAction(SteerAction, ETriggerEvent::Triggered, this, &AOCPlayerController::HandleSteer);
    }

    if (BarrelPitchAction)
    {
        EnhancedInput->BindAction(BarrelPitchAction, ETriggerEvent::Triggered, this, &AOCPlayerController::HandleBarrelPitch);
    }

    if (FireAction)
    {
        // Started:按下瞬间触发一次,点一下打一发
        EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AOCPlayerController::HandleFire);
    }

    if (AimAction)
    {
        // 长按右键:按下显示瞄准线,松开隐藏
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &AOCPlayerController::HandleAimStart);
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &AOCPlayerController::HandleAimStop);
    }

    if (PauseMenuAction)
    {
        // Started:按下瞬间打开暂停菜单(暂停后 PC 不 tick,关闭走 Widget 的 OnKeyDown/按钮)
        EnhancedInput->BindAction(PauseMenuAction, ETriggerEvent::Started, this, &AOCPlayerController::HandlePauseMenu);
    }

    if (UpgradePanelAction)
    {
        // Started:按下瞬间打开升级面板(暂停后 PC 不 tick,关闭走 Widget 的 PreviewKeyDown/按钮)
        EnhancedInput->BindAction(UpgradePanelAction, ETriggerEvent::Started, this, &AOCPlayerController::HandleUpgradePanel);
    }
}

void AOCPlayerController::HandleThrottle(const FInputActionValue& Value)
{
    // 我们用的 IA 是 Axis1D,取一维浮点值
    const float ThrottleValue = Value.Get<float>();
    if (FMath::IsNearlyZero(ThrottleValue))
    {
        return;
    }

    if (AOCBoatBase* Boat = GetPawn<AOCBoatBase>())
    {
        Boat->AddThrottleInput(ThrottleValue);
    }
}

void AOCPlayerController::HandleSteer(const FInputActionValue& Value)
{
    const float SteerValue = Value.Get<float>();
    if (FMath::IsNearlyZero(SteerValue))
    {
        return;
    }

    if (AOCBoatBase* Boat = GetPawn<AOCBoatBase>())
    {
        Boat->AddSteerInput(SteerValue);
    }
}

void AOCPlayerController::HandleBarrelPitch(const FInputActionValue& Value)
{
    const float WheelValue = Value.Get<float>();
    if (FMath::IsNearlyZero(WheelValue))
    {
        return;
    }

    if (AOCBoatBase* Boat = GetPawn<AOCBoatBase>())
    {
        if (AOCWeaponTurret* Turret = Boat->GetTurret())
        {
            Turret->AddBarrelPitch(WheelValue * BarrelPitchStep);
        }
    }
}

void AOCPlayerController::HandleFire(const FInputActionValue& Value)
{
    if (AOCBoatBase* Boat = GetPawn<AOCBoatBase>())
    {
        if (AOCWeaponTurret* Turret = Boat->GetTurret())
        {
            Turret->Fire();
        }
    }
}

void AOCPlayerController::HandleAimStart(const FInputActionValue& Value)
{
    if (AOCBoatBase* Boat = GetPawn<AOCBoatBase>())
    {
        if (UOCAimLineComponent* AimLine = Boat->FindComponentByClass<UOCAimLineComponent>())
        {
            AimLine->SetShowing(true);
        }
    }
}

void AOCPlayerController::HandleAimStop(const FInputActionValue& Value)
{
    if (AOCBoatBase* Boat = GetPawn<AOCBoatBase>())
    {
        if (UOCAimLineComponent* AimLine = Boat->FindComponentByClass<UOCAimLineComponent>())
        {
            AimLine->SetShowing(false);
        }
    }
}

void AOCPlayerController::HandlePauseMenu(const FInputActionValue& Value)
{
    OpenPauseMenu();
}

void AOCPlayerController::OpenPauseMenu()
{
    // 已打开、升级面板已弹出、胜利菜单已弹出或未配置 Widget 类时忽略
    if (PauseMenuWidget || UpgradePanelWidget || VictoryMenuWidget || !PauseMenuWidgetClass || !IsLocalController())
    {
        return;
    }

    PauseMenuWidget = CreateWidget<UOCPauseMenuWidget>(this, PauseMenuWidgetClass);
    if (!PauseMenuWidget)
    {
        return;
    }

    PauseMenuWidget->AddToViewport();

    // 全局暂停:物理/AI/计时器全部冻结。先上屏再暂停,顺序不影响,但输入模式切换要在暂停后也能生效
    UGameplayStatics::SetGamePaused(this, true);

    SetInputMode(FInputModeUIOnly());
    bShowMouseCursor = true;

    // 键盘焦点给菜单,NativeOnKeyDown 才能收到 Esc
    PauseMenuWidget->SetKeyboardFocus();
}

void AOCPlayerController::ClosePauseMenu()
{
    if (PauseMenuWidget)
    {
        PauseMenuWidget->RemoveFromParent();
        PauseMenuWidget = nullptr;
    }

    UGameplayStatics::SetGamePaused(this, false);

    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;
}

void AOCPlayerController::HandleUpgradePanel(const FInputActionValue& Value)
{
    OpenUpgradePanel();
}

void AOCPlayerController::OpenUpgradePanel()
{
    // 已打开、暂停菜单已弹出、胜利菜单已弹出或未配置 Widget 类时忽略
    if (UpgradePanelWidget || PauseMenuWidget || VictoryMenuWidget || !UpgradePanelWidgetClass || !IsLocalController())
    {
        return;
    }

    UpgradePanelWidget = CreateWidget<UOCShipUpgradePanelWidget>(this, UpgradePanelWidgetClass);
    if (!UpgradePanelWidget)
    {
        return;
    }

    UpgradePanelWidget->AddToViewport();

    // 全局暂停:升级时冻结战场,避免边挑属性边被打
    UGameplayStatics::SetGamePaused(this, true);

    SetInputMode(FInputModeUIOnly());
    bShowMouseCursor = true;

    // 键盘焦点给面板,NativeOnPreviewKeyDown 才能收到 Tab/Esc
    UpgradePanelWidget->SetKeyboardFocus();
}

void AOCPlayerController::CloseUpgradePanel()
{
    if (!UpgradePanelWidget)
    {
        return; // 面板没开时不要顺手解除暂停(可能是暂停菜单/胜利菜单在用)
    }

    UpgradePanelWidget->RemoveFromParent();
    UpgradePanelWidget = nullptr;

    UGameplayStatics::SetGamePaused(this, false);

    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;
}

void AOCPlayerController::HandleVictory()
{
    // 已弹出或未配置 Widget 类时忽略
    if (VictoryMenuWidget || !VictoryMenuWidgetClass || !IsLocalController())
    {
        return;
    }

    // 暂停菜单/升级面板若开着先摘掉(不清暂停状态,下面统一暂停),胜利菜单优先
    if (PauseMenuWidget)
    {
        PauseMenuWidget->RemoveFromParent();
        PauseMenuWidget = nullptr;
    }
    if (UpgradePanelWidget)
    {
        UpgradePanelWidget->RemoveFromParent();
        UpgradePanelWidget = nullptr;
    }

    VictoryMenuWidget = CreateWidget<UOCVictoryMenuWidget>(this, VictoryMenuWidgetClass);
    if (!VictoryMenuWidget)
    {
        return;
    }

    VictoryMenuWidget->AddToViewport();

    // 全局暂停,冻结战场画面作为胜利菜单背景
    UGameplayStatics::SetGamePaused(this, true);

    SetInputMode(FInputModeUIOnly());
    bShowMouseCursor = true;
}
