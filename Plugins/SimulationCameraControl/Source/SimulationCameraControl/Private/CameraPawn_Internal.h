#pragma once

#include "CoreMinimal.h"

namespace CameraPawn
{
        namespace Private
        {
                inline constexpr float KINDA_SMALL_NUMBER_CM = 1.0e-3f;

                inline bool IsVectorFinite(const FVector& V)
                {
                        return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
                }
        }
}
