#include "SimulationCameraControlPawn.h"
#include "SimulationCameraControlPawn_Internal.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

void ASimulationCameraControl::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!PlayerInputComponent)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("SetupPlayerInputComponent: PlayerInputComponent null."));
		return;
	}

	UEnhancedInputComponent* EnhancedComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedComponent)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("SetupPlayerInputComponent: expected EnhancedInputComponent but received %s."),
			*PlayerInputComponent->GetName());
		return;
	}

	if (!DefaultInputMapping)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("SetupPlayerInputComponent: DefaultInputMapping is not set."));
		return;
	}

	for (const FEnhancedActionKeyMapping& Mapping : DefaultInputMapping->GetMappings())
	{
		if (Mapping.Action)
		{
			const FName ActionName = Mapping.Action->GetFName();

			if (ActionName == ZoomActionName)
			{
				EnhancedComponent->BindAction(Mapping.Action, ETriggerEvent::Triggered, this, &ASimulationCameraControl::HandleZoomAction);
			}
			else if (ActionName == OrbitActionName)
			{
				EnhancedComponent->BindAction(Mapping.Action, ETriggerEvent::Triggered, this, &ASimulationCameraControl::HandleOrbitAction);
			}
			else if (ActionName == OrbitModifierActionName)
			{
				EnhancedComponent->BindAction(Mapping.Action, ETriggerEvent::Triggered, this, &ASimulationCameraControl::HandleOrbitModifierAction);
				EnhancedComponent->BindAction(Mapping.Action, ETriggerEvent::Completed, this, &ASimulationCameraControl::HandleOrbitModifierAction);
			}
			else if (ActionName == PanActionName)
			{
				EnhancedComponent->BindAction(Mapping.Action, ETriggerEvent::Triggered, this, &ASimulationCameraControl::HandlePanAction);
			}
			else if (ActionName == PanModifierActionName)
			{
				EnhancedComponent->BindAction(Mapping.Action, ETriggerEvent::Triggered, this, &ASimulationCameraControl::HandlePanModifierAction);
				EnhancedComponent->BindAction(Mapping.Action, ETriggerEvent::Completed, this, &ASimulationCameraControl::HandlePanModifierAction);
			}
		}
	}
}

void ASimulationCameraControl::InitializeInputMapping()
{
	if (!DefaultInputMapping)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("InitializeInputMapping skipped: DefaultInputMapping is null."));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("InitializeInputMapping failed: no controller."));
		return;
	}

	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("InitializeInputMapping failed: controller has no local player."));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("InitializeInputMapping failed: EnhancedInput subsystem unavailable."));
		return;
	}

	if (Subsystem->HasMappingContext(DefaultInputMapping))
	{
		Subsystem->RemoveMappingContext(DefaultInputMapping);
		UE_LOG(LogSimulationCameraControl, VeryVerbose, TEXT("InitializeInputMapping: Removed existing mapping %s before re-adding."),
			*GetNameSafe(DefaultInputMapping));
	}

	Subsystem->AddMappingContext(DefaultInputMapping, InputMappingPriority);
	UE_LOG(LogSimulationCameraControl, Verbose, TEXT("InitializeInputMapping: Added %s with priority %d for %s."),
		*GetNameSafe(DefaultInputMapping), InputMappingPriority, *GetName());
}

void ASimulationCameraControl::HandleZoomAction(const FInputActionInstance& Instance)
{
	const EInputActionValueType ValueType = Instance.GetValue().GetValueType();
	if (ValueType != EInputActionValueType::Axis1D)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("HandleZoomAction: Expected Axis1D but received %d."),
			static_cast<int32>(ValueType));
		return;
	}

	Zoom(Instance.GetValue().Get<float>());
}

void ASimulationCameraControl::HandleOrbitAction(const FInputActionInstance& Instance)
{
	// Only orbit if the modifier key (Right Mouse) is held down
	if (!bIsOrbitModifierDown)
	{
		return;
	}

	const EInputActionValueType ValueType = Instance.GetValue().GetValueType();
	if (ValueType != EInputActionValueType::Axis2D)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("HandleOrbitAction: Expected Axis2D but received %d."),
			static_cast<int32>(ValueType));
		return;
	}

	Orbit(Instance.GetValue().Get<FVector2D>());
}

void ASimulationCameraControl::HandlePanAction(const FInputActionInstance& Instance)
{
	const EInputActionValueType ValueType = Instance.GetValue().GetValueType();
	if (ValueType != EInputActionValueType::Axis2D)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("HandlePanAction: Expected Axis2D but received %d."),
			static_cast<int32>(ValueType));
		return;
	}

	const FVector2D AxisValue = Instance.GetValue().Get<FVector2D>();

	// Pan if modifier is held (Middle Mouse) OR if input is strong (WASD keys usually give +/- 1.0)
	// This allows WASD to work without holding a button, while gating mouse movement.
	const bool bIsKeyInput = FMath::Abs(AxisValue.X) >= 0.5f || FMath::Abs(AxisValue.Y) >= 0.5f;
	
	if (bIsPanModifierDown || bIsKeyInput)
	{
		Pan(AxisValue);
	}
}

void ASimulationCameraControl::HandleOrbitModifierAction(const FInputActionInstance& Instance)
{
	bIsOrbitModifierDown = Instance.GetValue().Get<bool>();
}

void ASimulationCameraControl::HandlePanModifierAction(const FInputActionInstance& Instance)
{
	bIsPanModifierDown = Instance.GetValue().Get<bool>();
}
