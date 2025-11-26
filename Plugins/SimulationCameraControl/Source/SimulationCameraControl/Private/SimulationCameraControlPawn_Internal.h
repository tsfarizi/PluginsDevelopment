#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSimulationCameraControl, Log, All);

namespace SimulationCameraControl
{
	namespace Private
	{
		static constexpr float KINDA_SMALL_NUMBER_CM = 0.01f;

		static bool IsVectorFinite(const FVector& V)
		{
			return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
		}
	}
}
