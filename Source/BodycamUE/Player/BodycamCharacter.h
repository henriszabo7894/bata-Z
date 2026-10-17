#pragma once

#include "CoreMinimal.h"
#include "Core/BodycamTypes.h"
#include "GameFramework/Character.h"
#include "BodycamCharacter.generated.h"

class UBodycamCameraComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UNavigationInvokerComponent;
class ABodycamWeapon;
class ABodycamZombie;
class ABodycamDrone;
struct FInputActionValue;

UENUM()
enum class ENVGState : uint8
{
	Up,
	Lowering,
	Down,
	Raising
};

/**
 * Joueur : déplacement réaliste (stamina, penché, accroupi), arme, vie sur 3 coups,
 * contamination, attaques de zombies avec dégagement (touche F), jumelles de vision nocturne
 * avec batterie, téléphone pour commander des munitions et piloter le drone.
 */
UCLASS(config=Game)
class BODYCAMUE_API ABodycamCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABodycamCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UBodycamCameraComponent* GetBodycamCamera() const { return BodycamCamera; }
	float GetStamina() const { return Stamina; }
	bool IsSprinting() const { return bIsSprinting; }
	bool IsExhausted() const { return bExhausted; }
	float GetLeanTarget() const { return LeanTarget; }

	// --- Arme ---
	void EquipWeapon(EBodycamWeaponType Type, int32 ReserveMags);
	ABodycamWeapon* GetWeapon() const { return Weapon; }

	// --- Vie / attaques ---
	/** Appelé par un zombie au moment où son attaque touche. Renvoie false si l'attaque est ignorée. */
	bool ReceiveZombieAttack(ABodycamZombie* Zombie, EZombieAttack Mode);
	void OnGrabberKilled(ABodycamZombie* Zombie);
	bool IsDead() const { return bDead; }
	int32 GetHits() const { return Hits; }
	int32 GetMaxHits() const { return MaxHits; }
	bool IsInfected() const { return bInfected; }
	float GetInfection() const { return Infection; }
	EZombieAttack GetGrabMode() const { return GrabMode; }
	float GetGrabTimeLeft() const { return GrabTimeLeft; }
	float GetEscapeProgress() const { return EscapeProgress; }
	float GetHurtFlash() const { return HurtFlash; }

	// --- Vision nocturne ---
	bool IsNightVisionOn() const { return NVGState == ENVGState::Down; }
	ENVGState GetNVGState() const { return NVGState; }
	float GetNVGAlpha() const { return NVGAlpha; }
	float GetNVGWarm() const { return NVGWarm; }
	bool WasNeckBite() const { return bNeckBiteDeath; }
	float GetNVGBattery() const { return NVGBattery; }
	void RechargeNVG() { NVGBattery = 100.f; }
	void DebugSetBattery(float Percent) { NVGBattery = FMath::Clamp(Percent, 0.f, 100.f); }
	void SetNVGDrainRate(float PerSecond) { NVGDrainPerSecond = PerSecond; }
	void ToggleNightVision();

	// --- Téléphone / drone ---
	bool IsPhoneOpen() const { return bPhoneOpen; }
	bool IsPilotingDrone() const { return bPilotingDrone; }
	ABodycamDrone* GetDrone() const { return Drone; }
	void OnDroneFinished(bool bDestroyed);
	/** Casque FPV : 0 = rien, 1 = écran noir (mise / retrait du casque). */
	float GetHeadsetFade() const { return HeadsetFade; }
	bool IsDeployingDrone() const { return DeployTimer >= 0.f; }
	bool IsDeployingNewDrone() const { return DeployTimer >= 0.f && bDeployNew; }

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void Landed(const FHitResult& Hit) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<UBodycamCameraComponent> BodycamCamera;

	/** Suit exactement la vue (placée par la caméra à chaque image) : l'arme et les jumelles y sont fixées. */
	UPROPERTY(VisibleAnywhere, Category = "Bodycam")
	TObjectPtr<USceneComponent> ViewModelRoot;

	UPROPERTY(VisibleAnywhere, Category = "Bodycam")
	TObjectPtr<USceneComponent> NVGPivot;

	UPROPERTY(VisibleAnywhere, Category = "Bodycam")
	TObjectPtr<UStaticMeshComponent> NVGMesh;

	/** Illuminateur infrarouge : n'éclaire que lorsque la JVN est baissée. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<USpotLightComponent> IRIlluminator;

	UPROPERTY(VisibleAnywhere, Category = "Bodycam")
	TObjectPtr<UNavigationInvokerComponent> NavInvoker;

	UPROPERTY() TObjectPtr<ABodycamWeapon> Weapon;
	UPROPERTY() TObjectPtr<ABodycamWeapon> BareHands;
	UPROPERTY() TObjectPtr<ABodycamDrone> Drone;
	UPROPERTY() TObjectPtr<ABodycamZombie> Grabber;

	// --- Déplacement (cm/s) ---
	UPROPERTY(EditAnywhere, Category = "Bodycam|Movement") float WalkSpeed = 260.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Movement") float SprintSpeed = 480.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Movement") float CrouchSpeed = 130.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Movement") float BackwardSpeedScale = 0.7f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Movement") float StrafeSpeedScale = 0.85f;

	// --- Stamina ---
	UPROPERTY(EditAnywhere, Category = "Bodycam|Stamina") float MaxStamina = 100.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Stamina") float SprintDrainPerSecond = 14.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Stamina") float JumpStaminaCost = 20.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Stamina") float RegenPerSecond = 12.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Stamina") float RegenDelay = 1.2f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Stamina") float ExhaustedRecoverThreshold = 30.f;

	// --- Hauteur caméra (au-dessus du centre de la capsule) ---
	UPROPERTY(EditAnywhere, Category = "Bodycam|Camera") float CameraStandHeight = 70.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Camera") float CameraCrouchHeight = 38.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Camera") float CameraForwardOffset = 10.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Camera") float CameraHeightInterpSpeed = 10.f;

private:
	// Entrées
	void OnMove(const FInputActionValue& Value);
	void OnMoveCompleted();
	void OnLook(const FInputActionValue& Value);
	void OnLookGamepad(const FInputActionValue& Value);
	void OnSprintPressed()    { bSprintHeld = true; }
	void OnSprintReleased()   { bSprintHeld = false; }
	void OnLeanLeftPressed()  { bLeanLeftHeld = true; }
	void OnLeanLeftReleased() { bLeanLeftHeld = false; }
	void OnLeanRightPressed()  { bLeanRightHeld = true; }
	void OnLeanRightReleased() { bLeanRightHeld = false; }
	void OnCrouchPressed();
	void OnCrouchReleased();
	void OnJumpPressed();
	void OnJumpReleased();
	void OnNightVisionPressed() { ToggleNightVision(); }
	void OnFirePressed();
	void OnFireReleased();
	void OnReloadPressed();
	void OnInteractPressed();
	void OnPhonePressed();
	void OnFlashlightPressed();
	void OnMeleePressed();
	void OnAimPressed()  { bAimHeld = true; }
	void OnAimReleased() { bAimHeld = false; }

	void ApplyNightLighting();
	void UpdateSprintAndLean();
	void UpdateStamina(float DeltaSeconds);
	void UpdateCameraHeight(float DeltaSeconds, bool bInstant = false);
	void UpdateNVG(float DeltaSeconds);
	void UpdateGrab(float DeltaSeconds);
	void UpdateInfection(float DeltaSeconds);
	void UpdateDeath(float DeltaSeconds);
	void UpdateDronePiloting();
	void UpdateDroneDeploy(float DeltaSeconds);
	void SpawnDroneOnFloor();
	bool IsCrouchToggle() const;
	bool CanAct() const;

	void TakeHit(bool bCanInfect, float InfectChance);
	void StartGrab(ABodycamZombie* Zombie, EZombieAttack Mode);
	void EndGrab(bool bEscaped);
	void Die(bool bNeckBite);
	void LaunchDrone();
	void StopPiloting();

	void SetSprinting(bool bNewSprinting);
	void SetLean(float NewLean);

	UFUNCTION(Server, Reliable) void ServerSetSprinting(bool bNewSprinting);
	UFUNCTION(Server, Reliable) void ServerSetLean(float NewLean);

	UPROPERTY(Replicated) bool bIsSprinting = false;
	UPROPERTY(Replicated) float LeanTarget = 0.f;

	FVector2D MoveInput = FVector2D::ZeroVector;
	bool bSprintHeld = false;
	bool bLeanLeftHeld = false;
	bool bLeanRightHeld = false;
	bool bJumpHeld = false;
	bool bCrouchHeld = false;
	bool bAimHeld = false;

	// Se dégager : bras qui repoussent le zombie
	float StruggleAlpha = 0.f;
	float StruggleThrust = 0.f;
	float ShoveTime = 0.f;   // poussée finale après la réussite
	void UpdateStruggle(float DeltaSeconds);

	// Mort : façon de tomber (0 arrière, 1 avant, 2 gauche, 3 droite)
	int32 DeathStyle = 0;
	FRotator DeathStartRot = FRotator::ZeroRotator;
	bool bDeathImpact = false;

	float Stamina = 100.f;
	float TimeSinceStaminaUse = 0.f;
	bool bExhausted = false;

	float CurrentCameraHeight = 70.f;
	float LastFallSpeed = 0.f;

	// Vie
	int32 Hits = 0;
	int32 MaxHits = 3;
	bool bDead = false;
	float DeathTime = 0.f;
	bool bNeckBiteDeath = false;
	bool bInfected = false;
	float Infection = 0.f;
	float HurtFlash = 0.f;
	float LastHurtSound = -10.f;
	void PlayHurtSound();
	float CoughTimer = 20.f;

	// Agrippé
	EZombieAttack GrabMode = EZombieAttack::None;
	float GrabTimeLeft = 0.f;
	float EscapeProgress = 0.f;
	float KickAnim = 0.f;
	float GrabImmunity = 0.f;

	// Vision nocturne
	ENVGState NVGState = ENVGState::Up;
	float NVGAlpha = 0.f;          // 0 = relevées, 1 = devant les yeux
	float NVGWarm = 0.f;           // montée en luminosité des tubes après allumage
	float NVGBattery = 100.f;
	float NVGDrainPerSecond = 0.75f;

	// Téléphone / drone
	bool bPhoneOpen = false;
	bool bPilotingDrone = false;
	FVector2D DroneLook = FVector2D::ZeroVector;
	float DeployTimer = -1.f;      // >= 0 : on pose le drone / on met le casque
	bool bDeployNew = false;       // true = on sort un nouveau drone du sac
	float HeadsetFade = 0.f;
	float DeployStartPitch = 0.f;
};
