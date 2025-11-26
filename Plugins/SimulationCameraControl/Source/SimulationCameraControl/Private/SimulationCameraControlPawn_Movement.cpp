#include "SimulationCameraControlPawn.h"
#include "SimulationCameraControlPawn_Internal.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

using SimulationCameraControl::Private::IsVectorFinite;
using SimulationCameraControl::Private::KINDA_SMALL_NUMBER_CM;

void ASimulationCameraControl::Zoom(float AxisValue)
{
	const float CurrentArm = SpringArm ? SpringArm->TargetArmLength : -1.0f;
	UE_LOG(LogSimulationCameraControl, Verbose, TEXT("Zoom: Axis=%.3f Arm=%.2f Input=%s"),
		AxisValue, CurrentArm, bInputEnabled ? TEXT("true") : TEXT("false"));

	if (!bInputEnabled || !SpringArm || FMath::IsNearlyZero(AxisValue, KINDA_SMALL_NUMBER_CM))
	{
		if (!SpringArm)
		{
			UE_LOG(LogSimulationCameraControl, Warning, TEXT("Zoom aborted: SpringArm not available."));
		}
		return;
	}

	const float Direction = bInvertZoom ? -AxisValue : AxisValue;
	const float DesiredArmLength = SpringArm->TargetArmLength - Direction * ZoomStep;

	const FVector FocusPoint = GetStableFocusPoint();
	ApplyZoom(DesiredArmLength, FocusPoint);
}

void ASimulationCameraControl::Orbit(FVector2D AxisValue)
{
	const FRotator CurrentRotation = SpringArm ? SpringArm->GetRelativeRotation() : FRotator::ZeroRotator;
	UE_LOG(LogSimulationCameraControl, Verbose, TEXT("Orbit: Axis=(%.3f, %.3f) Rot=%s Input=%s"),
		AxisValue.X, AxisValue.Y, *CurrentRotation.ToCompactString(), bInputEnabled ? TEXT("true") : TEXT("false"));

	if (!bInputEnabled || !SpringArm || AxisValue.IsNearlyZero())
	{
		if (!SpringArm)
		{
			UE_LOG(LogSimulationCameraControl, Warning, TEXT("Orbit aborted: SpringArm not available."));
		}
		return;
	}

	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	FRotator NewRotation = SpringArm->GetRelativeRotation();
	NewRotation.Yaw   += AxisValue.X * OrbitYawSpeed   * DeltaSeconds;
	NewRotation.Pitch  = FMath::Clamp(NewRotation.Pitch + AxisValue.Y * OrbitPitchSpeed * DeltaSeconds, MinPitch, MaxPitch);
	NewRotation.Roll   = 0.0f;

	SpringArm->SetRelativeRotation(NewRotation);
	UE_LOG(LogSimulationCameraControl, Verbose, TEXT("Orbit result: NewRot=%s Arm=%.2f"),
		*NewRotation.ToCompactString(), SpringArm->TargetArmLength);
}

void ASimulationCameraControl::Pan(FVector2D AxisValue)
{
	const FVector CurrentLocation = GetActorLocation();
	UE_LOG(LogSimulationCameraControl, Verbose, TEXT("Pan: Axis=(%.3f, %.3f) Loc=%s Input=%s"),
		AxisValue.X, AxisValue.Y, *CurrentLocation.ToCompactString(), bInputEnabled ? TEXT("true") : TEXT("false"));

	if (!bInputEnabled || !SpringArm || AxisValue.IsNearlyZero())
	{
		if (!SpringArm)
		{
			UE_LOG(LogSimulationCameraControl, Warning, TEXT("Pan aborted: SpringArm not available."));
		}
		return;
	}

	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	FVector Forward = SpringArm->GetForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}

	FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	if (!Right.Normalize())
	{
		Right = FVector::RightVector;
	}

	FVector Movement = -(Forward * AxisValue.Y + Right * AxisValue.X) * PanSpeed * DeltaSeconds;
	Movement.Z = 0.0f;

	if (Movement.IsNearlyZero())
	{
		return;
	}

	const FVector NewLocation = CurrentLocation + Movement;
	if (!IsVectorFinite(NewLocation))
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("Pan aborted: computed non-finite location."));
		return;
	}

	SetActorLocation(NewLocation);

	if (bHasCachedFocus)
	{
		LastValidHitLocation += Movement;
	}

	FVector ImmediateFocus;
	if (GetCursorWorldPoint(ImmediateFocus))
	{
		LastValidHitLocation = ImmediateFocus;
		bHasCachedFocus = true;
	}

	UE_LOG(LogSimulationCameraControl, Verbose, TEXT("Pan result: Movement=%s NewLoc=%s"),
		*Movement.ToCompactString(), *NewLocation.ToCompactString());
}

void ASimulationCameraControl::ApplyZoom(float DesiredArmLength, const FVector& FocusPoint)
{
	if (!SpringArm)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("ApplyZoom aborted: SpringArm not available."));
		return;
	}
	if (!Camera)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("ApplyZoom aborted: Camera not available."));
		return;
	}

	const FVector PawnLocation   = GetActorLocation();
	const FVector CameraLocation = Camera->GetComponentLocation();

	const float CurrentArm  = SpringArm->TargetArmLength;
	const float ClampedArm  = FMath::Clamp(DesiredArmLength, MinArmLength, MaxArmLength);
	const float ArmDelta    = ClampedArm - CurrentArm;

	UE_LOG(LogSimulationCameraControl, Verbose, TEXT("ApplyZoom: CurrentArm=%.2f Desired=%.2f Clamped=%.2f ArmDelta=%.2f Focus=%s Cam=%s Pawn=%s"),
		CurrentArm, DesiredArmLength, ClampedArm, ArmDelta,
		*FocusPoint.ToCompactString(), *CameraLocation.ToCompactString(), *PawnLocation.ToCompactString());

	if (FMath::IsNearlyZero(ArmDelta, KINDA_SMALL_NUMBER_CM))
	{
		SpringArm->TargetArmLength = ClampedArm;
		return;
	}

	FVector RayDir = FocusPoint - CameraLocation;
	if (!RayDir.Normalize())
	{
		RayDir = SpringArm->GetForwardVector();
		if (!RayDir.Normalize())
		{
			UE_LOG(LogSimulationCameraControl, Warning, TEXT("ApplyZoom: unable to determine ray direction."));
			SpringArm->TargetArmLength = ClampedArm;
			return;
		}
	}

	const FVector NewCameraLocation = CameraLocation - RayDir * ArmDelta;
	const FVector ArmForward = SpringArm->GetForwardVector();
	FVector NewPawnLocation = NewCameraLocation + ArmForward * ClampedArm;
	NewPawnLocation.Z = PawnLocation.Z;

	if (!IsVectorFinite(NewPawnLocation))
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("ApplyZoom aborted: computed non-finite pawn location."));
		SpringArm->TargetArmLength = ClampedArm;
		return;
	}

	SetActorLocation(NewPawnLocation);
	SpringArm->TargetArmLength = ClampedArm;

	LastValidHitLocation = FocusPoint;
	bHasCachedFocus = true;

	UE_LOG(LogSimulationCameraControl, Verbose, TEXT("ApplyZoom result: Pawn %s -> %s, Cam'=%s"),
		*PawnLocation.ToCompactString(), *NewPawnLocation.ToCompactString(), *NewCameraLocation.ToCompactString());

	if (bDebug && GetWorld())
	{
		DrawDebugLine(GetWorld(), CameraLocation, FocusPoint, FColor::Cyan, false, 0.05f, 0, 1.5f);
		DrawDebugLine(GetWorld(), NewCameraLocation, FocusPoint, FColor::Blue, false, 0.05f, 0, 1.5f);
	}
}
