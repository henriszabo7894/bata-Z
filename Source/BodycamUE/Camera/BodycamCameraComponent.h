#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "BodycamCameraComponent.generated.h"

class UMaterialInstanceDynamic;

/**
 * Caméra corporelle : grand angle, penché, balancement de buste, à-coups d'atterrissage,
 * roulis en tournant, et post-process "vidéo" (grain, aberration, flou, vignette, expo lente).
 * Tous les effets sont réglables et désactivables (options du jeu à l'étape 12).
 */
UCLASS(ClassGroup = (Bodycam), meta = (BlueprintSpawnableComponent))
class BODYCAMUE_API UBodycamCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	UBodycamCameraComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

	/** -1 gauche, 0 droit, 1 droite. */
	void SetLeanTarget(float Target) { LeanTarget = FMath::Clamp(Target, -1.f, 1.f); }
	void AddLandingImpact(float Strength01);

	void SetEffectsEnabled(bool bEnabled);
	bool AreEffectsEnabled() const { return bEffectsEnabled; }

	/** Vision nocturne militaire (jumelles baissées) : matériau post-process M_PP_NVG. */
	void SetNightVision(bool bEnabled);
	bool IsNightVision() const { return bNightVision; }
	/** Luminosité des tubes (coupure auto au tir, batterie faible...). */
	void SetNVGPower(float Power);

	/** Contamination (0-1) et flash de blessure (0-1). */
	void SetInfection(float Amount, float Hurt);

	/** Secousse (attaques, toux, coups de pied). */
	void AddShake(float Amount) { ShakeAmount = FMath::Min(ShakeAmount + Amount, 4.f); }

	/** Composant replacé à chaque image sur la vue exacte (arme, jumelles). */
	void SetViewModelRoot(USceneComponent* Root) { ViewModelRoot = Root; }

	// --- Exposition : bornes de l'auto-exposition (EV100). Plus bas = la nuit reste sombre. ---
	UPROPERTY(EditAnywhere, Category = "Bodycam|Effects") float MinExposureEV = -1.5f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Effects") float MaxExposureEV = 14.f;

	// --- Infrarouge ---
	UPROPERTY(EditAnywhere, Category = "Bodycam|NightVision") float NightMinExposureEV = -6.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|NightVision") float NightExposureBias = 1.5f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|NightVision") float NightFilmGrain = 0.65f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|NightVision") FLinearColor NightTint = FLinearColor(0.92f, 1.f, 0.95f, 1.f);

	// --- Penché ---
	UPROPERTY(EditAnywhere, Category = "Bodycam|Lean") float LeanDistance = 32.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Lean") float LeanAngle = 12.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Lean") float LeanSpeed = 7.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Lean") float LeanProbeRadius = 12.f;

	// --- Balancement (0 = désactivé, 1 = normal) ---
	UPROPERTY(EditAnywhere, Category = "Bodycam|Bob", meta = (ClampMin = "0", ClampMax = "2")) float HeadBobScale = 1.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Bob") float BobVertical = 1.1f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Bob") float BobHorizontal = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Bob") float BobRollDegrees = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Bob") float SprintBobMultiplier = 2.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Bob") float StepsPerSecondWalk = 1.9f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Bob") float StepsPerSecondSprint = 2.7f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Bob") float ReferenceWalkSpeed = 260.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Bob") float BreathAmplitude = 0.25f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Bob") float BreathRate = 0.25f;

	// --- Roulis quand on tourne vite (caméra fixée au torse) ---
	UPROPERTY(EditAnywhere, Category = "Bodycam|Sway") float TurnRollFactor = 0.012f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Sway") float MaxTurnRoll = 2.5f;

	// --- Atterrissage ---
	UPROPERTY(EditAnywhere, Category = "Bodycam|Landing") float LandingDip = 8.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Landing") float LandingRecoverSpeed = 6.f;

	// --- Effets vidéo ---
	UPROPERTY(EditAnywhere, Category = "Bodycam|Effects") bool bEffectsEnabled = true;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Effects") float FilmGrain = 0.25f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Effects") float ChromaticAberration = 1.2f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Effects") float MotionBlur = 0.3f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Effects") float Vignette = 0.55f;
	/** Projection Panini : courbe légèrement les bords comme un objectif grand angle (0 = off). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Effects") float LensDistortion = 0.35f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Effects") float AutoExposureSpeedUp = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Effects") float AutoExposureSpeedDown = 0.5f;

private:
	void ApplyEffects();

	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> NVGMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> InfectionMID;
	int32 NVGBlendIndex = INDEX_NONE;
	int32 InfectionBlendIndex = INDEX_NONE;
	TWeakObjectPtr<USceneComponent> ViewModelRoot;
	float ShakeAmount = 0.f;
	float ShakeTime = 0.f;

	bool bNightVision = false;
	void UpdateLean(float DeltaTime, const FRotator& ControlRot);
	void UpdateBob(float DeltaTime);
	void UpdateTurnRoll(float DeltaTime, const FRotator& ControlRot);

	float LeanTarget = 0.f;
	float LeanAlpha = 0.f;
	float LeanOffset = 0.f;
	float LeanRoll = 0.f;

	float BobPhase = 0.f;
	float BobBlend = 0.f;
	float BreathTime = 0.f;
	FVector BobOffset = FVector::ZeroVector;
	FRotator BobRotation = FRotator::ZeroRotator;

	float LandOffset = 0.f;

	float TurnRoll = 0.f;
	float LastYaw = 0.f;
	bool bHasLastYaw = false;
};
