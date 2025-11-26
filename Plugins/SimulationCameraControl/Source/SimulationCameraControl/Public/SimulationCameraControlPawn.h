#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "UObject/SoftObjectPath.h"
#include "SimulationCameraControlPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class USceneComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionInstance;

/**
 * Specialized camera pawn for simulation controls.
 * Implements RTS-style camera controls (Zoom, Orbit, Pan).
 */
UCLASS()
class SIMULATIONCAMERACONTROL_API ASimulationCameraControl : public APawn
{
	GENERATED_BODY()

public:
	ASimulationCameraControl();

	//~ Begin APawn Interface
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void PawnClientRestart() override;
	//~ End APawn Interface

	/**
	 * Enables or disables all camera input (e.g., when cursor hovers UI widgets).
	 * Caller: Enhanced Input IA_* bound to UI focus events.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|Input")
	void SetInputEnabled(bool bInEnabled);

	/**
	 * Zooms by adjusting spring arm length while sliding pawn to keep cursor focus steady.
	 * Axis source: mouse wheel (+/-1). Flip sign via bInvertZoom if required.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|Input")
	void Zoom(float AxisValue);

	/**
	 * Orbits the spring arm around the pawn using yaw/pitch deltas.
	 * Axis.X = MouseX, Axis.Y = MouseY, values typically in +/-1 per Enhanced Input sample.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|Input")
	void Orbit(FVector2D AxisValue);

	/**
	 * Pans the pawn in world X/Y based on camera yaw so controls remain screen-relative.
	 * Suggested bindings: WASD or middle-mouse drag; expects continuous IA_* Triggered events.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|Input")
	void Pan(FVector2D AxisValue);

	//Setter BP-callable
	UFUNCTION(BlueprintCallable, Category="Camera|Input")
	void SetDefaultInputMapping(UInputMappingContext* InContext);
	UFUNCTION(BlueprintCallable, Category="Camera|Input")
	void SetInputMappingPriority(int32 InPriority);

protected:
	/** Root component - keeps explicit hierarchy Root -> SpringArm -> Camera. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Spring arm driving orbital rotation; collision test disabled for unobstructed control. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArm;

	/** Active camera placed at spring arm tip. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

	/** Minimum boom length in centimeters (cm). Safe range: 200-800. Smaller risks clipping geometry when zooming in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom", meta = (ClampMin = "10.0"))
	float MinArmLength = 400.0f;

	/** Maximum boom length in centimeters (cm). Safe range: 1200-4000. Larger values exaggerate parallax and increase skybox exposure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom", meta = (ClampMin = "100.0"))
	float MaxArmLength = 2500.0f;

	/** Zoom step in centimeters (cm) per wheel tick. Safe range: 25-250. Bigger steps feel snappier but reduce precision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom", meta = (ClampMin = "1.0"))
	float ZoomStep = 120.0f;

	/** Optional inversion for zoom axis; set true to swap wheel direction. Side effect: affects all input devices uniformly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Zoom")
	bool bInvertZoom = false;

	/** Yaw orbit speed in degrees/second. Safe range: 45-360. Values too high can induce motion sickness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit", meta = (ClampMin = "0.0"))
	float OrbitYawSpeed = 120.0f;

	/** Pitch orbit speed in degrees/second. Safe range: 30-180. Higher speeds risk overshooting clamps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit", meta = (ClampMin = "0.0"))
	float OrbitPitchSpeed = 90.0f;

	/** Minimum pitch in degrees (negative keeps top-down). Safe range: -89 to -10. Prevents flipping underneath the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit", meta = (ClampMax = "0.0"))
	float MinPitch = -75.0f;

	/** Maximum pitch in degrees (negative for downward tilt). Safe range: -89 to -5. Clamped to avoid gimbal flip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit", meta = (ClampMax = "0.0"))
	float MaxPitch = -30.0f;

	/** Pan speed in centimeters/second. Safe range: 300-3000. Directly scales WASD traversal across the ground plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pan", meta = (ClampMin = "0.0"))
	float PanSpeed = 1500.0f;

	/** Ray length in centimeters (cm) for cursor focus traces. Safe range: 5000-200000. Longer rays cover tall levels but cost trace time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Focus", meta = (ClampMin = "100.0"))
	float RayLength = 50000.0f;

	/** World Z plane (cm) used when traces miss. Choose ground height; fallback plane stabilizes zoom over empty space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Focus")
	float GroundZ = 0.0f;

	/** Distance tolerance in centimeters to accept new focus hits. Safe range: 1-500. Higher tolerates cursor jumps; lower keeps micro precision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Focus", meta = (ClampMin = "0.0"))
	float JumpThreshold = 100.0f;

	/** Master input gate. False disables Zoom/Orbit/Pan; use when interacting with UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Input")
	bool bInputEnabled = true;

	/** Optional debug visualizations (draws rays/spheres). Safe to toggle at runtime; may spam logs/lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Debug")
	bool bDebug = false;

	/** Mapping context applied via code. Fallback path allows loading IMC_CameraPawn without touching project settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultInputMapping;

	/** Name of the Enhanced Input action driving Zoom(float). */
	FName ZoomActionName = "IA_Zoom";

	/** Name of the Enhanced Input action driving Orbit(FVector2D). */
	FName OrbitActionName = "IA_Orbit";

	/** Name of the Enhanced Input action driving Orbit Modifier (bool). */
	FName OrbitModifierActionName = "IA_Orbit_Modifier";

	/** Name of the Enhanced Input action driving Pan(FVector2D). */
	FName PanActionName = "IA_Pan";

	/** Name of the Enhanced Input action driving Pan Modifier (bool). */
	FName PanModifierActionName = "IA_Pan_Modifier";

	/** Priority applied when registering the mapping context; higher values win conflicts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Input", meta = (ClampMin = "0"))
	int32 InputMappingPriority = 0;

private:
	/** Returns cursor world point, preferring hits then falling back to GroundZ plane; logs failure reasons. */
	bool GetCursorWorldPoint(FVector& OutPoint);

	/** Provides a stable focus by caching previous hits and rejecting large jumps. */
	FVector GetStableFocusPoint();

	/** Applies zoom by clamping arm length and repositioning pawn along focus direction. */
	void ApplyZoom(float DesiredArmLength, const FVector& FocusPoint);

	/** Registers DefaultInputMapping with the local player's Enhanced Input subsystem. */
	void InitializeInputMapping();

	/** Wrapper pulling float axis from Enhanced Input action and forwarding to Zoom. */
	void HandleZoomAction(const FInputActionInstance& Instance);

	/** Wrapper pulling 2D axis from Enhanced Input action and forwarding to Orbit. */
	void HandleOrbitAction(const FInputActionInstance& Instance);

	/** Wrapper tracking Orbit Modifier state. */
	void HandleOrbitModifierAction(const FInputActionInstance& Instance);

	/** Wrapper pulling 2D axis from Enhanced Input action and forwarding to Pan. */
	void HandlePanAction(const FInputActionInstance& Instance);

	/** Wrapper tracking Pan Modifier state. */
	void HandlePanModifierAction(const FInputActionInstance& Instance);

	/** Cached focus location to smooth zoom operations. */
	FVector LastValidHitLocation = FVector::ZeroVector;

	/** Tracks whether LastValidHitLocation is initialized. */
	bool bHasCachedFocus = false;

	/** Tracks whether the Orbit Modifier (Right Mouse) is held down. */
	bool bIsOrbitModifierDown = false;

	/** Tracks whether the Pan Modifier (Middle Mouse) is held down. */
	bool bIsPanModifierDown = false;
};
