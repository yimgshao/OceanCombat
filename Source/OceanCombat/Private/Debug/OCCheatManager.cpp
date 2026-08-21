// OceanCombat. Copyright(c) All rights reserved.

#include "Debug/OCCheatManager.h"

#include "GameFlow/OCGameMode.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Hazards/OCExplosiveBarrel.h"

void UOCCheatManager::OCGod()
{
    APlayerController* PC = GetOuterAPlayerController();
    if (!PC)
    {
        return;
    }

    APawn* Pawn = PC->GetPawn();
    if (!Pawn)
    {
        // 复活倒计时期间没有 Pawn(旧船已 Destroy,新船还没 Spawn)
        PC->ClientMessage(TEXT("[Cheat] 当前没有控制任何船(可能正在复活倒计时)"));
        return;
    }

    // 当前可被伤害 → 本次操作是"开启无敌"
    const bool bEnableGodMode = Pawn->CanBeDamaged();
    Pawn->SetCanBeDamaged(!bEnableGodMode);

    const TCHAR* StateText = bEnableGodMode ? TEXT("开") : TEXT("关");
    PC->ClientMessage(FString::Printf(TEXT("[Cheat] 无敌模式 %s"), StateText));
    UE_LOG(LogTemp, Log, TEXT("[Cheat] 无敌模式 %s (%s)"), StateText, *GetNameSafe(Pawn));
}

void UOCCheatManager::OCAddScore(int32 Amount)
{
    APlayerController* PC = GetOuterAPlayerController();
    if (!PC)
    {
        return;
    }

    if (Amount <= 0)
    {
        // AddScore 内部会忽略非正数,这里提前给出可读的提示
        PC->ClientMessage(TEXT("[Cheat] 用法: OCAddScore <正整数>"));
        return;
    }

    AOCGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AOCGameMode>() : nullptr;
    if (!GameMode)
    {
        PC->ClientMessage(TEXT("[Cheat] 取不到 OCGameMode,加分失败"));
        return;
    }

    // 复用击杀加分的同一路径:内部会广播 OnScoreChanged,HUD 与升级面板自动刷新
    GameMode->AddScore(Amount);

    const FString Message = FString::Printf(TEXT("[Cheat] +%d 分(总分 %d,余额 %d)"),
        Amount, GameMode->GetTotalScore(), GameMode->GetRemainingScore());
    PC->ClientMessage(Message);
    UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
}

