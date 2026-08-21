// OceanCombat. Copyright(c) All rights reserved.

#include "Pawns/Boats/OCPlayerBoat.h"

#include "Components/OCAimLineComponent.h"

AOCPlayerBoat::AOCPlayerBoat()
{
    // 玩家船的移动参数(覆盖基类默认值)
    // 这些值是初值,后续在蓝图或实测时调整
    ForwardThrust = 60000.0f;     // 玩家船比基类稍强
    ReverseThrust = 35000.0f;
    TurnTorque = 6.0f;            // 度/秒²(AddTorqueInDegrees 语义)
    MaxForwardSpeed = 2000.0f;    // cm/s(= 20 m/s),玩家船稍快
    LinearDamping = 0.5f;
    AngularDamping = 2.5f;

    // 瞄准线组件(长按右键显示;根组件由基类构造时已设为 BoatMesh)
    AimLine = CreateDefaultSubobject<UOCAimLineComponent>(TEXT("AimLine"));
    AimLine->SetupAttachment(GetRootComponent());
}
