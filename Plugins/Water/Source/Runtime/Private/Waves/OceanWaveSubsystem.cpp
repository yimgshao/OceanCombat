// OCEANCOMBAT-MOD: 波源注册表子系统实现（自研动态水面核心）。

#include "Waves/OceanWaveSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Waves/OceanWaveRTManager.h"
#include "Waves/WaveEvaluation.h"
#include "WaterSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogOceanWaves, Log, All);

static TAutoConsoleVariable<int32> CVarWaveDebug(
	TEXT("wave.Debug"),
	0,
	TEXT("绘制波源调试球（半径=波前位置，颜色区分波型）。0=关闭 1=开启"),
	ECVF_Default);

void UOceanWaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RTManager = NewObject<UOceanWaveRTManager>(this);
}

double UOceanWaveSubsystem::GetWaveTime() const
{
	if (const UWaterSubsystem* WaterSubsystem = UWaterSubsystem::GetWaterSubsystem(GetWorld()))
	{
		return static_cast<double>(WaterSubsystem->GetWaterTimeSeconds());
	}
	return 0.0;
}

int32 UOceanWaveSubsystem::AddWaveSource(const TSharedPtr<FWaveSourceBase>& Source)
{
	if (!Source.IsValid())
	{
		return INDEX_NONE;
	}
	Source->SourceID = NextSourceID++;
	const int32 Index = Sources.Add(Source);
	bSnapshotDirty = true;
	return Index;
}

int32 UOceanWaveSubsystem::AddShipWakeSource(FVector2D Pos, float Heading, float InAmplitude,
	float InWaveLength, float InWaveSpeed, float InDecayRate, float InCutoffRadius)
{
	TSharedPtr<FShipWakeSource> Source = MakeShared<FShipWakeSource>();
	Source->Position = Pos;
	Source->Heading = Heading;
	Source->StartTime = GetWaveTime();
	Source->Amplitude = InAmplitude;
	Source->WaveLength = InWaveLength;
	Source->WaveSpeed = InWaveSpeed;
	Source->DecayRate = InDecayRate;
	Source->CutoffRadius = InCutoffRadius;
	return AddWaveSource(Source);
}

int32 UOceanWaveSubsystem::AddSplashSource(FVector2D Pos, float InAmplitude,
	float InWaveLength, float InWaveSpeed, float InDecayRate, float InCutoffRadius)
{
	TSharedPtr<FSplashWaveSource> Source = MakeShared<FSplashWaveSource>();
	Source->Position = Pos;
	Source->StartTime = GetWaveTime();
	Source->Amplitude = InAmplitude;
	Source->WaveLength = InWaveLength;
	Source->WaveSpeed = InWaveSpeed;
	Source->DecayRate = InDecayRate;
	Source->CutoffRadius = InCutoffRadius;
	return AddWaveSource(Source);
}

TSharedPtr<const TArray<TSharedPtr<FWaveSourceBase>>, ESPMode::ThreadSafe> UOceanWaveSubsystem::GetSnapshot() const
{
	return Snapshot;
}

void UOceanWaveSubsystem::Tick(float DeltaTime)
{
	const double Now = GetWaveTime();

	// 淘汰：有效振幅不足或超寿命
	const int32 Removed = Sources.RemoveAll([this, Now](const TSharedPtr<FWaveSourceBase>& Source)
	{
		return !Source.IsValid()
			|| Source->GetEffectiveAmplitude(Now) < MinAmplitude
			|| (Now - Source->StartTime) > static_cast<double>(MaxLifetime);
	});
	if (Removed > 0)
	{
		bSnapshotDirty = true;
	}

	// 数量超限：按当前有效振幅升序淘汰最弱者
	if (Sources.Num() > MaxSources)
	{
		Sources.Sort([Now](const TSharedPtr<FWaveSourceBase>& A, const TSharedPtr<FWaveSourceBase>& B)
		{
			return A->GetEffectiveAmplitude(Now) < B->GetEffectiveAmplitude(Now);
		});
		Sources.RemoveAt(0, Sources.Num() - MaxSources, EAllowShrinking::No);
		bSnapshotDirty = true;
	}

	// 快照发布：仅在增删后重建，平时零拷贝复用
	if (bSnapshotDirty)
	{
		Snapshot = MakeShared<TArray<TSharedPtr<FWaveSourceBase>>, ESPMode::ThreadSafe>(Sources);
		bSnapshotDirty = false;
	}

	// 波纹 RT 渲染（步骤 3）
	if (RTManager)
	{
		RTManager->RenderWaves(GetWorld(), Sources, Now);
	}

	if (CVarWaveDebug.GetValueOnGameThread() != 0)
	{
		DrawDebugSources(Now);
	}
}

void UOceanWaveSubsystem::DrawDebugSources(double Now) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const TSharedPtr<FWaveSourceBase>& Source : Sources)
	{
		if (!Source.IsValid())
		{
			continue;
		}

		// 波前半径 = WaveSpeed × age
		const float FrontRadius = Source->WaveSpeed
			* static_cast<float>(FWaveSourceBase::ComputeAge(Now, Source->StartTime));

		FColor Color = FColor::White;
		switch (Source->GetSourceType())
		{
		case EWaveSourceType::ShipWake: Color = FColor::Cyan; break;
		case EWaveSourceType::Splash:   Color = FColor::Orange; break;
		default: break;
		}

		DrawDebugSphere(World, FVector(Source->Position, 0.0), FMath::Max(FrontRadius, 1.0f),
			16, Color, false, -1.0f, 0, 2.0f);
	}
}

// ---------------------------------------------------------------- 调试控制台命令

namespace OceanWaveDebug
{
	static UOceanWaveSubsystem* GetSubsystem(UWorld* InWorld)
	{
		return InWorld ? InWorld->GetSubsystem<UOceanWaveSubsystem>() : nullptr;
	}

	static bool GetViewPoint(UWorld* InWorld, FVector& OutLocation, FRotator& OutRotation)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(InWorld, 0))
		{
			PC->GetPlayerViewPoint(OutLocation, OutRotation);
			return true;
		}
		return false;
	}

	static void TestWave(const TArray<FString>& Args, UWorld* InWorld, bool bShipWake)
	{
		UOceanWaveSubsystem* Subsystem = GetSubsystem(InWorld);
		FVector ViewLocation;
		FRotator ViewRotation;
		if (!Subsystem || !GetViewPoint(InWorld, ViewLocation, ViewRotation))
		{
			UE_LOG(LogOceanWaves, Warning, TEXT("wave.Test: 无可用子系统或玩家视点"));
			return;
		}

		const float Amplitude = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 50.0f;
		const FVector SpawnPos3D = ViewLocation + ViewRotation.Vector() * 2000.0;
		const FVector2D SpawnPos(SpawnPos3D.X, SpawnPos3D.Y);

		// 直接持有新源指针做锚点验证（快照要到下次 Tick 才重建，不能从快照取）
		TSharedPtr<FWaveSourceBase> Source;
		if (bShipWake)
		{
			TSharedPtr<FShipWakeSource> ShipWake = MakeShared<FShipWakeSource>();
			ShipWake->Heading = FMath::DegreesToRadians(ViewRotation.Yaw);
			ShipWake->WaveLength = 300.0f;
			ShipWake->WaveSpeed = 600.0f;
			ShipWake->DecayRate = 0.08f;
			ShipWake->CutoffRadius = 2000.0f;
			Source = ShipWake;
		}
		else
		{
			TSharedPtr<FSplashWaveSource> Splash = MakeShared<FSplashWaveSource>();
			Splash->WaveLength = 150.0f;
			Splash->WaveSpeed = 300.0f;
			Splash->DecayRate = 1.0f;
			Splash->CutoffRadius = 1000.0f;
			Source = Splash;
		}
		Source->Position = SpawnPos;
		Source->Amplitude = Amplitude;
		Source->StartTime = Subsystem->GetWaveTime();
		Subsystem->AddWaveSource(Source);

		// 公式锚点：直接求 t0+1s 时源后方 200cm 处的高度（无需等待实时流逝）
		const float BehindAngle = bShipWake
			? static_cast<const FShipWakeSource&>(*Source).Heading + PI
			: 0.0f;
		const FVector2D SamplePos = Source->Position
			+ FVector2D(FMath::Cos(BehindAngle), FMath::Sin(BehindAngle)) * 200.0f;
		const float H = FWaveEvaluation::EvaluateSource(*Source, SamplePos, Source->StartTime + 1.0);
		UE_LOG(LogOceanWaves, Log,
			TEXT("wave.Test: 类型=%d Amplitude=%.1f | t0+1s 后方200cm 高度 = %.3f cm（请与手算公式对照）"),
			(int32)Source->GetSourceType(), Amplitude, H);
	}

	static FAutoConsoleCommand TestShipWakeCmd(
		TEXT("wave.TestShipWake"),
		TEXT("在镜头前方 20m 生成标准船尾迹波源并输出公式锚点日志。用法: wave.TestShipWake [Amplitude]"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* InWorld, FOutputDevice& /*Ar*/) { TestWave(Args, InWorld, true); }));

	static FAutoConsoleCommand TestSplashCmd(
		TEXT("wave.TestSplash"),
		TEXT("在镜头前方 20m 生成标准落水冲击波源并输出公式锚点日志。用法: wave.TestSplash [Amplitude]"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
			[](const TArray<FString>& Args, UWorld* InWorld, FOutputDevice& /*Ar*/) { TestWave(Args, InWorld, false); }));
}
