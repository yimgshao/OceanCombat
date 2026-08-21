// OceanCombat. Copyright(c) All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Pawns/Boats/OCBoatBase.h"
#include "OCPlayerBoat.generated.h"

class UOCAimLineComponent;

/**
 * 玩家船。由 AOCPlayerController 拥有,输入由 Controller 转发到基类的
 * AddThrottleInput / AddSteerInput。
 *
 * 持有瞄准线组件(长按右键显示,由玩家控制器切换)。
 */
UCLASS()
class OCEANCOMBAT_API AOCPlayerBoat : public AOCBoatBase
{
    GENERATED_BODY()

public:
    AOCPlayerBoat();

protected:
    /** 瞄准线组件:长按右键显示与炮弹一致的弹道预测线 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boat")
    TObjectPtr<UOCAimLineComponent> AimLine;
};
