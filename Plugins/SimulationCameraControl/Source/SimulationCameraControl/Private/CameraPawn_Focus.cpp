#include "CameraPawn.h"
#include "CameraPawn_Internal.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

using CameraPawn::Private::IsVectorFinite;

bool ACameraPawn::GetCursorWorldPoint(FVector& OutPoint)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("GetCursorWorldPoint failed: no controller."));
		return false;
	}

	FHitResult Hit;
	const bool bHit = PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit);
	if (bHit && Hit.bBlockingHit)
	{
		OutPoint = Hit.ImpactPoint;
		UE_LOG(LogCameraPawn, Verbose, TEXT("GetCursorWorldPoint: CursorHit %s"), *OutPoint.ToCompactString());

		if (bDebug && GetWorld())
		{
			DrawDebugSphere(GetWorld(), OutPoint, 25.0f, 12, FColor::Green, false, 0.05f);
		}

		return true;
	}

	float MouseX = 0.0f, MouseY = 0.0f;
	if (!PC->GetMousePosition(MouseX, MouseY))
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("GetCursorWorldPoint fallback failed: mouse position unavailable."));
		return false;
	}

	FVector WorldOrigin, WorldDirection;
	if (!PC->DeprojectScreenPositionToWorld(MouseX, MouseY, WorldOrigin, WorldDirection))
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("GetCursorWorldPoint fallback failed: deprojection failed (Mouse %.2f, %.2f)."),
			MouseX, MouseY);
		return false;
	}

	if (!WorldDirection.Normalize())
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("GetCursorWorldPoint fallback failed: zero world direction."));
		return false;
	}

	const float Denominator = FVector::DotProduct(WorldDirection, FVector::UpVector);
	if (FMath::IsNearlyZero(Denominator))
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("GetCursorWorldPoint fallback failed: ray parallel to plane Z=%.2f."), GroundZ);
		return false;
	}

	const float DistanceAlongRay = (GroundZ - WorldOrigin.Z) / Denominator;
	if (DistanceAlongRay < 0.0f)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("GetCursorWorldPoint fallback failed: plane intersection behind origin (%.2f cm)."),
			DistanceAlongRay);
		return false;
	}

	if (DistanceAlongRay > RayLength)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("GetCursorWorldPoint fallback failed: intersection %.2f exceeds RayLength %.2f."),
			DistanceAlongRay, RayLength);
		return false;
	}

	const FVector Intersection = WorldOrigin + WorldDirection * DistanceAlongRay;
	if (!IsVectorFinite(Intersection))
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("GetCursorWorldPoint fallback failed: intersection non-finite."));
		return false;
	}

	OutPoint = Intersection;
	UE_LOG(LogCameraPawn, Verbose, TEXT("GetCursorWorldPoint: FallbackPlane %s"), *OutPoint.ToCompactString());

	if (bDebug && GetWorld())
	{
		DrawDebugLine(GetWorld(), WorldOrigin, WorldOrigin + WorldDirection * DistanceAlongRay, FColor::Yellow, false, 0.05f, 0, 1.0f);
		DrawDebugSphere(GetWorld(), OutPoint, 25.0f, 12, FColor::Yellow, false, 0.05f);
	}

	return true;
}

FVector ACameraPawn::GetStableFocusPoint()
{
	FVector SamplePoint = FVector::ZeroVector;
	const bool bHasSample = GetCursorWorldPoint(SamplePoint);

	if (!bHasSample)
	{
		UE_LOG(LogCameraPawn, Verbose, TEXT("GetStableFocusPoint: cursor sample unavailable."));
	}

	if (!bHasCachedFocus)
	{
		LastValidHitLocation = bHasSample ? SamplePoint : GetActorLocation();
		bHasCachedFocus = true;
		UE_LOG(LogCameraPawn, Verbose, TEXT("GetStableFocusPoint: initialized cache at %s (HasSample=%s)"),
			*LastValidHitLocation.ToCompactString(), bHasSample ? TEXT("true") : TEXT("false"));
		return LastValidHitLocation;
	}

	if (bHasSample)
	{
		if (!IsVectorFinite(SamplePoint))
		{
			UE_LOG(LogCameraPawn, Warning, TEXT("GetStableFocusPoint: sample non-finite, keeping cache %s."),
				*LastValidHitLocation.ToCompactString());
			return LastValidHitLocation;
		}

		const float Distance = FVector::Dist(SamplePoint, LastValidHitLocation);
		const bool bUpdate   = Distance <= JumpThreshold;
		UE_LOG(LogCameraPawn, Verbose, TEXT("GetStableFocusPoint: Dist=%.2f UpdatedCache=%s"),
			Distance, bUpdate ? TEXT("true") : TEXT("false"));

		if (bUpdate)
		{
			LastValidHitLocation = SamplePoint;
		}
	}

	return LastValidHitLocation;
}
