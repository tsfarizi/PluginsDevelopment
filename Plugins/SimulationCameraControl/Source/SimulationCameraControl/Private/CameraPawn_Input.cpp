#include "CameraPawn.h"

#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"

void ACameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	ResolveInputAssets();

	if (!PlayerInputComponent)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("SetupPlayerInputComponent: PlayerInputComponent null."));
		return;
	}

	UEnhancedInputComponent* EnhancedComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedComponent)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("SetupPlayerInputComponent: expected EnhancedInputComponent but received %s."),
			*PlayerInputComponent->GetName());
		return;
	}

	if (!DefaultInputMapping)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("SetupPlayerInputComponent: DefaultInputMapping is not set."));
		return;
	}

	for (const FEnhancedActionKeyMapping& Mapping : DefaultInputMapping->GetMappings())
	{
		if (Mapping.Action)
		{
			const FName ActionName = Mapping.Action->GetFName();

			if (ActionName == ZoomActionName)
			{
				EnhancedComponent->BindAction(Mapping.Action, ETriggerEvent::Triggered, this, &ACameraPawn::HandleZoomAction);
			}
			else if (ActionName == OrbitActionName)
			{
				EnhancedComponent->BindAction(Mapping.Action, ETriggerEvent::Triggered, this, &ACameraPawn::HandleOrbitAction);
			}
			else if (ActionName == PanActionName)
			{
				EnhancedComponent->BindAction(Mapping.Action, ETriggerEvent::Triggered, this, &ACameraPawn::HandlePanAction);
			}
		}
	}
}

void ACameraPawn::HandleZoomAction(const FInputActionInstance& Instance)
{
	const EInputActionValueType ValueType = Instance.GetValue().GetValueType();
	if (ValueType != EInputActionValueType::Axis1D)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("HandleZoomAction: Expected Axis1D but received %d."),
			static_cast<int32>(ValueType));
		return;
	}

	Zoom(Instance.GetValue().Get<float>());
}

void ACameraPawn::HandleOrbitAction(const FInputActionInstance& Instance)
{
	const EInputActionValueType ValueType = Instance.GetValue().GetValueType();
	if (ValueType != EInputActionValueType::Axis2D)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("HandleOrbitAction: Expected Axis2D but received %d."),
			static_cast<int32>(ValueType));
		return;
	}

	Orbit(Instance.GetValue().Get<FVector2D>());
}

void ACameraPawn::HandlePanAction(const FInputActionInstance& Instance)
{
	const EInputActionValueType ValueType = Instance.GetValue().GetValueType();
	if (ValueType != EInputActionValueType::Axis2D)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("HandlePanAction: Expected Axis2D but received %d."),
			static_cast<int32>(ValueType));
		return;
	}

	Pan(Instance.GetValue().Get<FVector2D>());
}

void ACameraPawn::SetInputEnabled(bool bInEnabled)
{
	const bool bOldState = bInputEnabled;
	bInputEnabled = bInEnabled;
	UE_LOG(LogCameraPawn, Verbose, TEXT("SetInputEnabled: %s -> %s"),
		bOldState ? TEXT("true") : TEXT("false"),
		bInputEnabled ? TEXT("true") : TEXT("false"));
}

/* ===== Setter BP-callable ===== */

void ACameraPawn::SetDefaultInputMapping(UInputMappingContext* InContext)
{
	DefaultInputMapping = InContext;
	InitializeInputMapping();
}

void ACameraPawn::SetInputMappingPriority(int32 InPriority)
{
	InputMappingPriority = FMath::Max(0, InPriority);
	InitializeInputMapping();
}
