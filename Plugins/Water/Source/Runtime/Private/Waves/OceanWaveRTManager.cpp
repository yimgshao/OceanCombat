// OCEANCOMBAT-MOD: 波纹 RT 渲染管理器实现（自研动态水面核心）。

#include "Waves/OceanWaveRTManager.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogOceanWaveRT, Log, All);

namespace OceanWaveRTPaths
{
	const FString WakeRT = TEXT("/Water/InteractiveWaves/RT_WakeWaves.RT_WakeWaves");
	const FString MPC = TEXT("/Water/InteractiveWaves/MPC_OceanWaves.MPC_OceanWaves");
	const FString StampMaterial = TEXT("/Water/InteractiveWaves/M_WakeStamp.M_WakeStamp");
}

bool UOceanWaveRTManager::EnsureAssets()
{
	if (!WakeRT)
	{
		WakeRT = LoadObject<UTextureRenderTarget2D>(nullptr, *OceanWaveRTPaths::WakeRT);
	}
	if (!MPC)
	{
		MPC = LoadObject<UMaterialParameterCollection>(nullptr, *OceanWaveRTPaths::MPC);
	}
	if (!StampMaterial)
	{
		StampMaterial = LoadObject<UMaterialInterface>(nullptr, *OceanWaveRTPaths::StampMaterial);
	}

	if (!WakeRT || !MPC || !StampMaterial)
	{
		if (!bWarnedMissingAsset)
		{
			bWarnedMissingAsset = true;
			UE_LOG(LogOceanWaveRT, Warning,
				TEXT("波纹渲染资产缺失（请在插件 Water Content/InteractiveWaves/ 下创建，见 docs/步骤3-操作手册.md）：RT=%s MPC=%s Stamp=%s"),
				WakeRT ? TEXT("OK") : *OceanWaveRTPaths::WakeRT,
				MPC ? TEXT("OK") : *OceanWaveRTPaths::MPC,
				StampMaterial ? TEXT("OK") : *OceanWaveRTPaths::StampMaterial);
		}
		return false;
	}
	return true;
}

UMaterialInstanceDynamic* UOceanWaveRTManager::GetOrCreateMID(const FWaveSourceBase& Source)
{
	if (TObjectPtr<UMaterialInstanceDynamic>* Existing = MIDCache.Find(Source.SourceID))
	{
		return *Existing;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(StampMaterial, this);
	MIDCache.Add(Source.SourceID, MID);

	// 参数仅在创建时设置一次（波源不可变）；WaveTime 走 MPC 全局参数
	MID->SetVectorParameterValue(TEXT("SourcePos"), FLinearColor(Source.Position.X, Source.Position.Y, 0.0f, 0.0f));
	MID->SetScalarParameterValue(TEXT("StartTime"), static_cast<float>(Source.StartTime));
	MID->SetScalarParameterValue(TEXT("Amplitude"), Source.Amplitude);
	MID->SetScalarParameterValue(TEXT("WaveLength"), Source.WaveLength);
	MID->SetScalarParameterValue(TEXT("WaveSpeed"), Source.WaveSpeed);
	MID->SetScalarParameterValue(TEXT("DecayRate"), Source.DecayRate);
	MID->SetScalarParameterValue(TEXT("CutoffRadius"), Source.CutoffRadius);

	if (Source.GetSourceType() == EWaveSourceType::ShipWake)
	{
		const FShipWakeSource& ShipWake = static_cast<const FShipWakeSource&>(Source);
		MID->SetScalarParameterValue(TEXT("Heading"), ShipWake.Heading);
		MID->SetScalarParameterValue(TEXT("OmniWeight"), ShipWake.OmniWeight);
		MID->SetScalarParameterValue(TEXT("WedgeHalfAngleDeg"), ShipWake.WedgeHalfAngleDeg);
	}
	else
	{
		// Splash / Generic：OmniWeight=1 使楔形权重退化为全向（与 CPU 侧 FSplashWaveSource 等价）
		MID->SetScalarParameterValue(TEXT("Heading"), 0.0f);
		MID->SetScalarParameterValue(TEXT("OmniWeight"), 1.0f);
		MID->SetScalarParameterValue(TEXT("WedgeHalfAngleDeg"), 0.0f);
	}
	return MID;
}

void UOceanWaveRTManager::RenderWaves(UWorld* World, const TArray<TSharedPtr<FWaveSourceBase>>& Sources, double WaveTime)
{
	if (!World || Sources.IsEmpty())
	{
		return;
	}
	if (!EnsureAssets())
	{
		return;
	}

	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(World, 0);
	if (!CameraManager)
	{
		return;
	}

	// 镜头跟随 + 纹素对齐（防 RT 内容游动）
	const FVector CameraLocation = CameraManager->GetCameraLocation();
	const double Texel = 2.0 * static_cast<double>(Extent) / RTSize;
	const double CenterX = FMath::FloorToDouble(CameraLocation.X / Texel + 0.5) * Texel;
	const double CenterY = FMath::FloorToDouble(CameraLocation.Y / Texel + 0.5) * Texel;

	// 同步 MPC 全局参数
	if (UMaterialParameterCollectionInstance* MPCI = World->GetParameterCollectionInstance(MPC))
	{
		MPCI->SetVectorParameterValue(TEXT("WakeRT_Center"), FLinearColor(CenterX, CenterY, 0.0, 0.0));
		MPCI->SetScalarParameterValue(TEXT("WakeRT_Extent"), Extent);
		MPCI->SetScalarParameterValue(TEXT("WaveTime"), static_cast<float>(WaveTime));
	}

	// 清空并逐源图章绘制
	UKismetRenderingLibrary::ClearRenderTarget2D(World, WakeRT, FLinearColor::Black);

	UCanvas* Canvas = nullptr;
	FVector2D CanvasSize;
	FDrawToRenderTargetContext DrawContext;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, WakeRT, Canvas, CanvasSize, DrawContext);

	TSet<int32> AliveIDs;
	const double InvDoubleExtent = 1.0 / (2.0 * static_cast<double>(Extent));
	for (const TSharedPtr<FWaveSourceBase>& Source : Sources)
	{
		if (!Source.IsValid())
		{
			continue;
		}
		AliveIDs.Add(Source->SourceID);

		// 世界矩形 → 画布像素矩形
		const double Cutoff = Source->CutoffRadius;
		const FVector2D QuadCenterPx(
			((Source->Position.X - CenterX) * InvDoubleExtent + 0.5) * CanvasSize.X,
			((Source->Position.Y - CenterY) * InvDoubleExtent + 0.5) * CanvasSize.Y);
		const FVector2D QuadSizePx(
			2.0 * Cutoff * InvDoubleExtent * CanvasSize.X,
			2.0 * Cutoff * InvDoubleExtent * CanvasSize.Y);

		// 完全在 RT 覆盖区外的源跳过
		const FVector2D QuadMin = QuadCenterPx - QuadSizePx * 0.5f;
		const FVector2D QuadMax = QuadCenterPx + QuadSizePx * 0.5f;
		if (QuadMax.X < 0.0f || QuadMax.Y < 0.0f || QuadMin.X > CanvasSize.X || QuadMin.Y > CanvasSize.Y)
		{
			continue;
		}

		if (UMaterialInstanceDynamic* MID = GetOrCreateMID(*Source))
		{
			Canvas->K2_DrawMaterial(MID, QuadMin, QuadSizePx, FVector2D::ZeroVector, FVector2D::UnitVector);
		}
	}

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, DrawContext);

	// 惰性清理已淘汰波源的 MID
	for (auto It = MIDCache.CreateIterator(); It; ++It)
	{
		if (!AliveIDs.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}
