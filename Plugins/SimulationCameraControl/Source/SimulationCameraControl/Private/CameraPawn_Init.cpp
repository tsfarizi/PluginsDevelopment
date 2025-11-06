#include "CameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputMappingContext.h"

ACameraPawn::ACameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(SceneRoot);
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = false;
	SpringArm->TargetArmLength = 1200.0f;
	SpringArm->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw  = false;
	bUseControllerRotationRoll = false;

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ACameraPawn::BeginPlay()
{
	Super::BeginPlay();

	ensureMsgf(SceneRoot, TEXT("CameraPawn %s missing SceneRoot"), *GetName());
	ensureMsgf(SpringArm, TEXT("CameraPawn %s missing SpringArm"), *GetName());
	ensureMsgf(Camera,    TEXT("CameraPawn %s missing Camera"),    *GetName());

	if (MinPitch > MaxPitch)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("BeginPlay: MinPitch %.2f > MaxPitch %.2f. Swapping values to preserve clamp."),
			MinPitch, MaxPitch);
		Swap(MinPitch, MaxPitch);
	}

	if (SpringArm)
	{
		SpringArm->TargetArmLength = FMath::Clamp(SpringArm->TargetArmLength, MinArmLength, MaxArmLength);
	}

	InitializeInputMapping();
}

void ACameraPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeInputMapping();
}

void ACameraPawn::PawnClientRestart()
{
	Super::PawnClientRestart();
	InitializeInputMapping();
}

bool ACameraPawn::ResolveInputAssets()
{
	if (!DefaultInputMapping)
	{
		if (DefaultInputMappingPath.IsValid())
		{
			if (UInputMappingContext* LoadedContext = Cast<UInputMappingContext>(DefaultInputMappingPath.TryLoad()))
			{
				DefaultInputMapping = LoadedContext;
				UE_LOG(LogCameraPawn, Verbose, TEXT("ResolveInputAssets: Loaded mapping context from %s"),
					*DefaultInputMappingPath.ToString());
			}
			else
			{
				UE_LOG(LogCameraPawn, Warning, TEXT("ResolveInputAssets: Failed to load mapping context from %s"),
					*DefaultInputMappingPath.ToString());
			}
		}
	}

	return DefaultInputMapping != nullptr;
}

void ACameraPawn::InitializeInputMapping()
{
	if (!ResolveInputAssets())
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("InitializeInputMapping skipped: mapping context unresolved."));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("InitializeInputMapping failed: no controller."));
		return;
	}

	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("InitializeInputMapping failed: controller has no local player."));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogCameraPawn, Warning, TEXT("InitializeInputMapping failed: EnhancedInput subsystem unavailable."));
		return;
	}

	if (Subsystem->HasMappingContext(DefaultInputMapping))
	{
		Subsystem->RemoveMappingContext(DefaultInputMapping);
		UE_LOG(LogCameraPawn, VeryVerbose, TEXT("InitializeInputMapping: Removed existing mapping %s before re-adding."),
			*GetNameSafe(DefaultInputMapping));
	}

	Subsystem->AddMappingContext(DefaultInputMapping, InputMappingPriority);
	UE_LOG(LogCameraPawn, Verbose, TEXT("InitializeInputMapping: Added %s with priority %d for %s."),
		*GetNameSafe(DefaultInputMapping), InputMappingPriority, *GetName());
}
