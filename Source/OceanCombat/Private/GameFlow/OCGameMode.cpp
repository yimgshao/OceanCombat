// OceanCombat. Copyright(c) All rights reserved.

#include "GameFlow/OCGameMode.h"

#include "Components/OCHealthComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Pawns/OCPawnBase.h"
#include "Procedural/OCInfiniteMapManager.h"
#include "Procedural/OCInfiniteOceanManager.h"
#include "Procedural/OCMapGenConfig.h"
#include "TimerManager.h"

/** 胜利目标(城堡)的识别标签。在城堡蓝图/关卡实例上打这个 Actor Tag 即可参与胜利判定 */
static const FName CastleActorTag(TEXT("Castle"));

void AOCGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    // 无限地图生成:必须早于 PostLogin(玩家船生成)与 BeginPlay(城堡收集)
    AOCInfiniteMapManager* MapManager = GetWorld()->SpawnActor<AOCInfiniteMapManager>();
    MapManager->Config = MapGenConfig; // 留空时管理器内部走 CDO 默认值

    // 无限海面管理器:水面 Actor 跟随玩家平移
    GetWorld()->SpawnActor<AOCInfiniteOceanManager>();
}

void AOCGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 收集关卡内所有城堡(按 "Castle" 标签,与具体类解耦),绑定死亡回调
    TArray<AActor*> Castles;
    UGameplayStatics::GetAllActorsWithTag(this, CastleActorTag, Castles);

    AliveCastleCount = 0;
    for (AActor* Castle : Castles)
    {
        AOCPawnBase* CastleBuilding = Cast<AOCPawnBase>(Castle);
        UOCHealthComponent* Health = CastleBuilding ? CastleBuilding->GetHealthComponent() : nullptr;
        if (Health)
        {
            Health->OnDeath.AddDynamic(this, &AOCGameMode::HandleCastleDeath);
            ++AliveCastleCount;
        }
    }
    // 注:无限模式下城堡由聚落任务在游戏过程中动态 spawn,BeginPlay 时 AliveCastleCount 恒为 0,
    // 胜利判定在无限模式下暂不生效(纯探索)。

    BindPlayerBoat();
}

void AOCGameMode::BindPlayerBoat()
{
    const APlayerController* PC = GetWorld()->GetFirstPlayerController();
    AOCPawnBase* PlayerPawn = PC ? Cast<AOCPawnBase>(PC->GetPawn()) : nullptr;
    if (!PlayerPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("[OCGameMode] 找不到玩家船,复活逻辑未绑定(玩家船未摆放或未设置 Auto Possess)"));
        return;
    }

    // 仅首次绑定记录初始出生点:它只作难度分档的零点(GetDifficultyOrigin),
    // 不是复活点 —— 复活在阵亡原地。所以复活后的新船不更新这个值,零点全局固定
    if (!PlayerBoatClass)
    {
        PlayerBoatClass = PlayerPawn->GetClass();
        PlayerRespawnTransform = PlayerPawn->GetActorTransform();
        PendingRespawnTransform = PlayerRespawnTransform; // 兜底:万一还没记录过阵亡点就复活
        UE_LOG(LogTemp, Log, TEXT("[OCGameMode] 记录难度分档零点(初始出生点) @ (%.0f, %.0f, %.0f)"),
            PlayerRespawnTransform.GetLocation().X, PlayerRespawnTransform.GetLocation().Y, PlayerRespawnTransform.GetLocation().Z);
    }

    if (UOCHealthComponent* Health = PlayerPawn->GetHealthComponent())
    {
        Health->OnDeath.AddDynamic(this, &AOCGameMode::HandlePlayerDeath);
    }
}

void AOCGameMode::HandlePlayerDeath(AActor* DeadActor, AController* KillerController)
{
    if (GameState != EOCGameState::Playing)
    {
        return;
    }

    // 原地复活:必须在 Destroy 之前抓阵亡位置。
    // Z 归到海平面并清掉 Pitch/Roll —— 船沉没时 Z 是负的、姿态是倾覆的,
    // 照搬会让新船在水下歪着重生(浮力要把它顶上来,期间穿模/翻滚)。
    if (DeadActor)
    {
        const FVector DeathLocation = DeadActor->GetActorLocation();
        const float SeaLevelZ = MapGenConfig ? MapGenConfig->SeaLevelZ : 0.0f;
        PendingRespawnTransform = FTransform(
            FRotator(0.0f, DeadActor->GetActorRotation().Yaw, 0.0f),
            FVector(DeathLocation.X, DeathLocation.Y, SeaLevelZ));
    }

    UE_LOG(LogTemp, Log, TEXT("[OCGameMode] 玩家船被击沉,%.0f 秒后在阵亡原地 (%.0f, %.0f) 复活"),
        RespawnDelay, PendingRespawnTransform.GetLocation().X, PendingRespawnTransform.GetLocation().Y);

    // 销毁残骸:死亡的船留着挡弹道,且仍被 Controller Possess 着
    if (DeadActor)
    {
        DeadActor->Destroy();
    }

    GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AOCGameMode::RespawnPlayer, RespawnDelay, false);
}

void AOCGameMode::RespawnPlayer()
{
    if (GameState != EOCGameState::Playing || !PlayerBoatClass)
    {
        return;
    }

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
    {
        return;
    }

    // AlwaysSpawn:复活点可能有残骸/礁石,不因碰撞拒绝生成
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APawn* NewBoat = GetWorld()->SpawnActor<APawn>(PlayerBoatClass, PendingRespawnTransform, Params);
    if (!NewBoat)
    {
        UE_LOG(LogTemp, Error, TEXT("[OCGameMode] 玩家复活失败:SpawnActor 返回空"));
        return;
    }

    PC->Possess(NewBoat); // OnPossess 会自动接管跟随相机与 HUD
    BindPlayerBoat();     // 给新船重新绑死亡监听(难度分档零点保持不变)
    UE_LOG(LogTemp, Log, TEXT("[OCGameMode] 玩家已在阵亡原地复活"));
}

void AOCGameMode::HandleCastleDeath(AActor* DeadActor, AController* KillerController)
{
    if (GameState != EOCGameState::Playing)
    {
        return;
    }

    --AliveCastleCount;
    UE_LOG(LogTemp, Log, TEXT("[OCGameMode] 城堡被摧毁:%s,剩余 %d 座"), *GetNameSafe(DeadActor), AliveCastleCount);

    if (AliveCastleCount <= 0)
    {
        // 立即置 Victory 状态,防止后续城堡死亡回调重复进入;
        // OnVictory 延迟 VictoryDelay 秒再广播,留出时间看最后一座城堡的摧毁演出
        GameState = EOCGameState::Victory;
        UE_LOG(LogTemp, Log, TEXT("[OCGameMode] 所有城堡已被摧毁,%.0f 秒后判定胜利"), VictoryDelay);
        GetWorldTimerManager().SetTimer(VictoryTimerHandle, this, &AOCGameMode::BroadcastVictory, VictoryDelay, false);
    }
}

void AOCGameMode::BroadcastVictory()
{
    UE_LOG(LogTemp, Log, TEXT("[OCGameMode] 胜利!"));
    OnVictory.Broadcast();
}

void AOCGameMode::AddScore(int32 Amount)
{
    if (Amount <= 0)
    {
        return;
    }

    TotalScore += Amount;
    RemainingScore += Amount;
    OnScoreChanged.Broadcast(TotalScore, RemainingScore);
}

bool AOCGameMode::TrySpendScore(int32 Amount)
{
    if (Amount < 0 || RemainingScore < Amount)
    {
        return false;
    }

    RemainingScore -= Amount;
    OnScoreChanged.Broadcast(TotalScore, RemainingScore);
    return true;
}

void AOCGameMode::RegisterCombatant(AOCPawnBase* Pawn)
{
    if (!Pawn)
    {
        return;
    }

    if (UOCHealthComponent* Health = Pawn->GetHealthComponent())
    {
        Health->OnDeath.AddDynamic(this, &AOCGameMode::HandleCombatantDeath);
    }
}

void AOCGameMode::HandleCombatantDeath(AActor* DeadActor, AController* KillerController)
{
    if (GameState != EOCGameState::Playing)
    {
        return;
    }

    // 只有玩家击杀才得分;敌人互殴、无来源伤害(如未来环境伤害)不计
    if (!KillerController || !KillerController->IsPlayerController())
    {
        return;
    }

    const AOCPawnBase* Pawn = Cast<AOCPawnBase>(DeadActor);
    if (!Pawn || Pawn->ScoreValue <= 0)
    {
        return;
    }

    AddScore(Pawn->ScoreValue);
    UE_LOG(LogTemp, Log, TEXT("[Score] 击毁 %s +%d 分(总分 %d,余额 %d)"),
        *GetNameSafe(DeadActor), Pawn->ScoreValue, TotalScore, RemainingScore);
}
