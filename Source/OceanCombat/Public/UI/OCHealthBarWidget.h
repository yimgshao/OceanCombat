// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OCHealthBarWidget.generated.h"

class UProgressBar;
class UOCHealthComponent;

/**
 * 头顶血条 Widget 基类。蓝图子类(WBP_HealthBar)里放一个同名 ProgressBar 即自动绑定。
 * 职责单一:把 UOCHealthComponent 的血量显示成进度条,不认识任何单位类型。
 * 颜色/样式全部在蓝图里配。
 */
UCLASS()
class OCEANCOMBAT_API UOCHealthBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 绑定血量数据源:立即刷新一次,并监听血量变化/死亡 */
    void InitWithHealthComponent(UOCHealthComponent* InHealthComponent);

protected:
    virtual void NativeDestruct() override;

    /** 进度条。蓝图子类必须放一个同名 ProgressBar(BindWidget 按名字绑定) */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> HealthBar;

    /** 满血时隐藏整条血条(敌人平时不显示,挨打了才出现) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HealthBar")
    bool bHideWhenFullHealth = false;

private:
    UFUNCTION()
    void HandleHealthChanged(float OldHealth, float NewHealth);

    UFUNCTION()
    void HandleDeath(AActor* DeadActor, AController* KillerController);

    /** 按当前血量刷新进度条和显隐 */
    void RefreshBar();

    /** 血量数据源(弱引用,随单位销毁自动失效) */
    TWeakObjectPtr<UOCHealthComponent> HealthComp;
};
