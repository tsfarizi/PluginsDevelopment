#include "SimulationCameraControlPawn.h"
#include "SimulationCameraControlPawn_Internal.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputMappingContext.h"

DEFINE_LOG_CATEGORY(LogSimulationCameraControl);

ASimulationCameraControl::ASimulationCameraControl()
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

	// Set default input mapping to IMC_BaseSimulation
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContext(TEXT("/Game/Input/IMC_BaseSimulation.IMC_BaseSimulation"));
	if (DefaultContext.Succeeded())
	{
		DefaultInputMapping = DefaultContext.Object;
	}
}

void ASimulationCameraControl::BeginPlay()
{
	Super::BeginPlay();

	if (MinPitch > MaxPitch)
	{
		UE_LOG(LogSimulationCameraControl, Warning, TEXT("BeginPlay: MinPitch %.2f > MaxPitch %.2f. Swapping values to preserve clamp."),
			MinPitch, MaxPitch);
		Swap(MinPitch, MaxPitch);
	}

	if (SpringArm)
	{
		SpringArm->TargetArmLength = FMath::Clamp(SpringArm->TargetArmLength, MinArmLength, MaxArmLength);
	}

	InitializeInputMapping();
}

void ASimulationCameraControl::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeInputMapping();
}

void ASimulationCameraControl::PawnClientRestart()
{
	Super::PawnClientRestart();
	InitializeInputMapping();
}

void ASimulationCameraControl::SetInputEnabled(bool bInEnabled)
{
	const bool bOldState = bInputEnabled;
	bInputEnabled = bInEnabled;
	UE_LOG(LogSimulationCameraControl, Verbose, TEXT("SetInputEnabled: %s -> %s"),
		bOldState ? TEXT("true") : TEXT("false"),
		bInputEnabled ? TEXT("true") : TEXT("false"));
}

void ASimulationCameraControl::SetDefaultInputMapping(UInputMappingContext* InContext)
{
	DefaultInputMapping = InContext;
	InitializeInputMapping();
}

void ASimulationCameraControl::SetInputMappingPriority(int32 InPriority)
{
	InputMappingPriority = FMath::Max(0, InPriority);
	InitializeInputMapping();
}
