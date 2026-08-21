// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OCGameMode.generated.h"

/** 游戏状态。当前只有"游戏中/胜利";失败(玩家方全灭)后续阶段再加 */
UENUM(BlueprintType)
enum class EOCGameState : uint8
{
    Playing,
    Victory,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOCVictory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOCScoreChanged, int32, TotalScore, int32, RemainingScore);

class UOCMapGenConfig;
class AOCPawnBase;

/**
 * 游戏模式。负责游戏流程与胜负判定。
 * 胜利条件:关卡内所有带 "Castle" 标签的敌方城堡被摧毁。
 * 实现:BeginPlay 按标签收集所有城堡并绑定各自血量组件的 OnDeath,
 * 存活数归零时置状态为 Victory,延迟 VictoryDelay 秒后广播 OnVictory(UI/流程由监听方处理)。
 */
UCLASS()
class OCEANCOMBAT_API AOCGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;

    /** 地图生成时序入口:早于 PostLogin(玩家船生成)与 BeginPlay(城堡收集),在这里触发程序化生成 */
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

    /** 胜利广播(所有城堡被摧毁 VictoryDelay 秒后触发一次)。监听方显示胜利 UI */
    UPROPERTY(BlueprintAssignable, Category = "Game")
    FOnOCVictory OnVictory;

    /** 当前游戏状态 */
    UFUNCTION(BlueprintPure, Category = "Game")
    EOCGameState GetGameState() const { return GameState; }

    /** 玩家复活等待时间(秒):玩家船血量归零后,等待该时长在初始出生点重生 */
    UPROPERTY(EditDefaultsOnly, Category = "Game", meta = (ClampMin = "0.0"))
    float RespawnDelay = 3.0f;

    /** 胜利菜单延迟(秒):最后一座城堡被摧毁后,等待该时长再广播 OnVictory(期间不暂停,可看到摧毁演出) */
    UPROPERTY(EditDefaultsOnly, Category = "Game", meta = (ClampMin = "0.0"))
    float VictoryDelay = 3.0f;

    /** 地图生成配置。留空走 UOCMapGenConfig 的 CDO 默认值 */
    UPROPERTY(EditDefaultsOnly, Category = "MapGen")
    TObjectPtr<UOCMapGenConfig> MapGenConfig;

    // ---- 得分 ----
    /** 对局总得分(只增不减,结算用) */
    UPROPERTY(BlueprintReadOnly, Category = "Score")
    int32 TotalScore = 0;

    /** 剩余得分(购买升级的余额,消费时扣减) */
    UPROPERTY(BlueprintReadOnly, Category = "Score")
    int32 RemainingScore = 0;

    /** 得分变化广播(加分/消费都会触发),HUD 绑定刷新 */
    UPROPERTY(BlueprintAssignable, Category = "Score")
    FOnOCScoreChanged OnScoreChanged;

    UFUNCTION(BlueprintPure, Category = "Score")
    int32 GetTotalScore() const { return TotalScore; }

    UFUNCTION(BlueprintPure, Category = "Score")
    int32 GetRemainingScore() const { return RemainingScore; }

    /** 加分:总分与余额同时增加,并广播 OnScoreChanged */
    UFUNCTION(BlueprintCallable, Category = "Score")
    void AddScore(int32 Amount);

    /** 消费得分(购买升级预留):余额足够则只扣余额并广播,返回是否成功;总分不受影响 */
    UFUNCTION(BlueprintCallable, Category = "Score")
    bool TrySpendScore(int32 Amount);

    /** 注册一个敌方单位:绑定其死亡回调,被玩家击杀时按 ScoreValue 加分。由地图生成器在 spawn 后调用 */
    void RegisterCombatant(AOCPawnBase* Pawn);

    /** 难度分档的零点(玩家初始出生点),供生成器按距离查档 */
    FVector GetDifficultyOrigin() const { return PlayerRespawnTransform.GetLocation(); }

private:
    /** 城堡死亡回调:存活数 -1,归零触发胜利 */
    UFUNCTION()
    void HandleCastleDeath(AActor* DeadActor, AController* KillerController);

    /** 绑定玩家船死亡监听;首次绑定时记录初始出生点(BeginPlay 与每次复活后调用) */
    void BindPlayerBoat();

    /** 玩家船死亡回调:销毁残骸,启动复活倒计时 */
    UFUNCTION()
    void HandlePlayerDeath(AActor* DeadActor, AController* KillerController);

    /** 敌方单位死亡回调(RegisterCombatant 注册):玩家击杀才加分,敌人互殴/无来源不计 */
    UFUNCTION()
    void HandleCombatantDeath(AActor* DeadActor, AController* KillerController);

    /** 复活:在初始出生点重新生成玩家船并让 PlayerController Possess(相机/HUD 由 OnPossess 自动接管) */
    void RespawnPlayer();

    /** 胜利延迟计时结束:广播 OnVictory(UI 弹出胜利菜单) */
    void BroadcastVictory();

    EOCGameState GameState = EOCGameState::Playing;

    /** 存活城堡数,BeginPlay 时统计 */
    int32 AliveCastleCount = 0;

    /**
     * 初始出生点(游戏开始时玩家船的变换)。
     * 注意:这**不是**复活点 —— 复活在阵亡原地(见 PendingRespawnTransform)。
     * 它的用途是 GetDifficultyOrigin():难度分档按"离初始出生点的距离"分档,
     * 这个零点必须全局固定,不能跟着玩家漂移,否则越走越远时难度会被重置。
     */
    FTransform PlayerRespawnTransform;

    /** 复活用的变换:阵亡瞬间在原地记录(Z 归到海平面,避免在沉没深度重生) */
    FTransform PendingRespawnTransform;

    /** 玩家船类(从初始船记录,复活时用同一类重新生成)。同时作为"是否已记录出生点"的标记 */
    TSubclassOf<APawn> PlayerBoatClass;

    /** 复活倒计时 */
    FTimerHandle RespawnTimerHandle;

    /** 胜利广播延迟计时 */
    FTimerHandle VictoryTimerHandle;
};
