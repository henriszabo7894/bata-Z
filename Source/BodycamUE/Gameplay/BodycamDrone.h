#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BodycamDrone.generated.h"

class USphereComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UCameraComponent;
class USpotLightComponent;
class UAnimSequence;
class UMaterialInstanceDynamic;
class ABodycamExterior;

UENUM()
enum class EDroneState : uint8
{
	Landed,      // posé par terre, moteurs coupés
	TakingOff,   // monte tout seul à hauteur de vol
	Flying,      // piloté au casque FPV
	AutoReturn   // revient seul jusqu'au joueur avec la caisse
};

/**
 * Drone de ravitaillement : le joueur le pose par terre, met le casque FPV et le pilote jusqu'à la caisse
 * de munitions dehors. [F] = pince, [R] = retour automatique (par le chemin parcouru).
 * Toucher un mur trop vite le fait exploser.
 */
UCLASS()
class BODYCAMUE_API ABodycamDrone : public APawn
{
	GENERATED_BODY()

public:
	ABodycamDrone();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Commandes envoyées chaque image par le joueur qui pilote. */
	void SetMoveInput(const FVector2D& InMove) { MoveInput = InMove; }
	void SetVerticalInput(float InVertical) { VerticalInput = InVertical; }
	void AddLookInput(const FVector2D& Delta);
	void ClearInput() { MoveInput = FVector2D::ZeroVector; VerticalInput = 0.f; }

	/** Casque mis / retiré : le drone décolle la première fois qu'on le pilote. */
	void SetPiloted(bool bInPiloted);

	/** [F] : ouvre / ferme la pince. Attrape la caisse si elle est juste dessous. */
	void TryGrab();
	/** [R] : retour automatique vers le joueur (uniquement avec la caisse). */
	void StartAutoReturn();

	bool IsCarrying() const { return bCarrying; }
	bool IsGrabbing() const { return GrabTimer >= 0.f; }
	bool CanGrabNow() const;
	EDroneState GetState() const { return State; }
	float GetSpeed() const { return Velocity.Size(); }
	float GetAltitude() const { return Altitude; }
	float GetYaw() const { return Yaw; }
	const FString& GetHint() const { return Hint; }
	float GetHintTime() const { return HintTime; }
	float GetReturnDistance() const;
	/** Encore dans le couloir : position de la fenêtre de sortie la plus proche (marqueur). */
	bool GetExitMarker(FVector& Out, bool& bBroken) const;

	void Explode();

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> CarriedCrate;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> Lamp;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UPointLightComponent> StatusLed;

	UPROPERTY() TObjectPtr<UAnimSequence> AnimFlight;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimOpen;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimClose;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FeedMID;
	UPROPERTY() TObjectPtr<ABodycamExterior> Exterior;
	UPROPERTY() TObjectPtr<class UAudioComponent> RotorSound;

private:
	void SnapToFloor();
	void Move(float DeltaSeconds, const FVector& Desired, bool bCanCrash);
	void UpdateManual(float DeltaSeconds);
	void UpdateTakeOff(float DeltaSeconds);
	void UpdateAutoReturn(float DeltaSeconds);
	void UpdateGrab(float DeltaSeconds);
	void RecordPath();
	void Deliver();
	void SetHint(const FString& InHint) { Hint = InHint; HintTime = 2.5f; }

	EDroneState State = EDroneState::Landed;
	bool bPiloted = false;

	FVector Velocity = FVector::ZeroVector;
	FVector2D MoveInput = FVector2D::ZeroVector;
	float VerticalInput = 0.f;
	float Yaw = 0.f;
	float CamPitch = -10.f;
	float Altitude = 0.f;
	float TakeOffZ = 0.f;
	float LandHeight = 32.f;

	bool bCarrying = false;
	bool bExploded = false;
	float GrabTimer = -1.f;          // >= 0 : pince en mouvement
	bool bGrabCaught = false;
	float CloseAnimTimer = -1.f;

	TArray<FVector> Path;            // trajet enregistré (pour le retour automatique)
	int32 ReturnIndex = 0;
	float StuckTime = 0.f;

	FString Hint;
	float HintTime = 0.f;

	float MaxSpeed = 950.f;
	float CrashSpeed = 250.f;
	float ReturnSpeed = 650.f;
	float GrabRange = 170.f;         // distance horizontale max pour attraper
	float GrabHeight = 260.f;        // hauteur max au-dessus de la caisse
	FRotator MeshBaseRotation = FRotator(0.f, -90.f, 0.f);
	FVector MeshBaseOffset = FVector::ZeroVector;
	float MeshScale = 1.f;
};
