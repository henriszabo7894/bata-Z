#include "Camera/BodycamCameraComponent.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"

UBodycamCameraComponent::UBodycamCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	// Grand angle type caméra corporelle (FOV horizontal).
	FieldOfView = 100.f;
	PostProcessBlendWeight = 1.f;
}

void UBodycamCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// Nuit : exposition bloquée (sinon Unreal éclaircit tout seul l'obscurité). Réglable dans DefaultGame.ini.
	float NightEV = 1.5f, NVGEV = -1.f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("Bodycam.Camera"), TEXT("NightEV"), NightEV, GGameIni);
		GConfig->GetFloat(TEXT("Bodycam.Camera"), TEXT("NVGEV"), NVGEV, GGameIni);
	}
	MinExposureEV = MaxExposureEV = NightEV;
	NightMinExposureEV = NVGEV;
	NightExposureBias = 0.f;

	// Matériaux post-process créés par Scripts/create_pp_materials.py.
	if (UMaterialInterface* NVG = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/PP/M_PP_NVG.M_PP_NVG")))
	{
		NVGMID = UMaterialInstanceDynamic::Create(NVG, this);
		NVGBlendIndex = PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(0.f, NVGMID));
	}
	if (UMaterialInterface* Inf = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/PP/M_PP_Infection.M_PP_Infection")))
	{
		InfectionMID = UMaterialInstanceDynamic::Create(Inf, this);
		InfectionBlendIndex = PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(0.f, InfectionMID));
	}
	ApplyEffects();
}

void UBodycamCameraComponent::SetNVGPower(float Power)
{
	if (NVGMID) NVGMID->SetScalarParameterValue(TEXT("Power"), Power);
}

void UBodycamCameraComponent::SetInfection(float Amount, float Hurt)
{
	if (!InfectionMID || InfectionBlendIndex == INDEX_NONE) return;
	const bool bActive = Amount > 0.001f || Hurt > 0.001f;
	PostProcessSettings.WeightedBlendables.Array[InfectionBlendIndex].Weight = bActive ? 1.f : 0.f;
	InfectionMID->SetScalarParameterValue(TEXT("Amount"), Amount);
	InfectionMID->SetScalarParameterValue(TEXT("Hurt"), Hurt);
}

void UBodycamCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// La distorsion est une variable globale du moteur : on la remet à zéro en quittant.
	if (IConsoleVariable* Panini = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Upscale.Panini.D")))
	{
		Panini->Set(0.f, ECVF_SetByCode);
	}
	Super::EndPlay(EndPlayReason);
}

void UBodycamCameraComponent::SetEffectsEnabled(bool bEnabled)
{
	bEffectsEnabled = bEnabled;
	ApplyEffects();
}

void UBodycamCameraComponent::SetNightVision(bool bEnabled)
{
	bNightVision = bEnabled;
	ApplyEffects();
}

void UBodycamCameraComponent::ApplyEffects()
{
	FPostProcessSettings& PP = PostProcessSettings;
	const bool bOn = bEffectsEnabled;

	// Bornes d'exposition : sans elles, Unreal éclaircit la nuit jusqu'à en faire le jour.
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.AutoExposureMinBrightness = bNightVision ? NightMinExposureEV : MinExposureEV;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMaxBrightness = bNightVision ? NightMinExposureEV : MaxExposureEV;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = bNightVision ? NightExposureBias : 0.f;

	// Vision nocturne : le matériau M_PP_NVG fait le phosphore blanc et les deux tubes ;
	// ici on remonte l'exposition et on pousse le bloom (halos autour des lampes, typiques des JVN).
	PP.bOverride_BloomIntensity = bNightVision;
	PP.BloomIntensity = 2.2f;
	PP.bOverride_BloomThreshold = bNightVision;
	PP.BloomThreshold = 0.6f;
	if (NVGBlendIndex != INDEX_NONE)
	{
		PP.WeightedBlendables.Array[NVGBlendIndex].Weight = bNightVision ? 1.f : 0.f;
	}

	PP.bOverride_FilmGrainIntensity = bOn || bNightVision;
	PP.FilmGrainIntensity = bNightVision ? NightFilmGrain : FilmGrain;
	PP.bOverride_SceneFringeIntensity = bOn;
	PP.SceneFringeIntensity = ChromaticAberration;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = bOn ? MotionBlur : 0.f;
	PP.bOverride_MotionBlurMax = bOn;
	PP.MotionBlurMax = 4.f;
	PP.bOverride_VignetteIntensity = bOn;
	PP.VignetteIntensity = Vignette;
	PP.bOverride_AutoExposureSpeedUp = bOn;
	PP.AutoExposureSpeedUp = AutoExposureSpeedUp;
	PP.bOverride_AutoExposureSpeedDown = bOn;
	PP.AutoExposureSpeedDown = AutoExposureSpeedDown;

	if (IConsoleVariable* Panini = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Upscale.Panini.D")))
	{
		Panini->Set(bOn ? LensDistortion : 0.f, ECVF_SetByCode);
	}
}

void UBodycamCameraComponent::AddLandingImpact(float Strength01)
{
	LandOffset -= LandingDip * FMath::Clamp(Strength01, 0.f, 1.f);
}

void UBodycamCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	const FRotator ControlRot = Pawn ? Pawn->GetControlRotation() : GetComponentRotation();

	UpdateLean(DeltaTime, ControlRot);
	UpdateBob(DeltaTime);
	UpdateTurnRoll(DeltaTime, ControlRot);
	LandOffset = FMath::FInterpTo(LandOffset, 0.f, DeltaTime, LandingRecoverSpeed);
	ShakeAmount = FMath::FInterpTo(ShakeAmount, 0.f, DeltaTime, 3.f);
	ShakeTime += DeltaTime;
}

void UBodycamCameraComponent::UpdateLean(float DeltaTime, const FRotator& ControlRot)
{
	LeanAlpha = FMath::FInterpTo(LeanAlpha, LeanTarget, DeltaTime, LeanSpeed);

	float Desired = LeanAlpha * LeanDistance;
	if (FMath::Abs(Desired) > 0.5f)
	{
		// On vérifie qu'on ne passe pas la tête à travers un mur.
		const FVector Right = FRotator(0.f, ControlRot.Yaw, 0.f).RotateVector(FVector::RightVector);
		const FVector Start = GetComponentLocation();
		const FVector End = Start + Right * Desired;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(BodycamLean), false, GetOwner());
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeSphere(LeanProbeRadius), Params))
		{
			Desired = FMath::Sign(Desired) * FMath::Max(0.f, Hit.Distance - 2.f);
		}
	}

	LeanOffset = Desired;
	LeanRoll = (LeanOffset / LeanDistance) * LeanAngle;
}

void UBodycamCameraComponent::UpdateBob(float DeltaTime)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const bool bGrounded = Movement && Movement->IsMovingOnGround();
	const float Speed = Character ? Character->GetVelocity().Size2D() : 0.f;

	// 0 = immobile, 1 = marche, ~1.85 = course.
	const float TargetBlend = bGrounded ? FMath::Clamp(Speed / ReferenceWalkSpeed, 0.f, 2.f) : 0.f;
	BobBlend = FMath::FInterpTo(BobBlend, TargetBlend, DeltaTime, 6.f);

	const float SprintAlpha = FMath::Clamp(BobBlend - 1.f, 0.f, 1.f);
	const float StepRate = FMath::Lerp(StepsPerSecondWalk, StepsPerSecondSprint, SprintAlpha);
	// Une demi-période de "Phase" par pas : vertical = 1 cycle/pas, latéral = 1 cycle/2 pas.
	BobPhase = FMath::Fmod(BobPhase + DeltaTime * StepRate * PI, 2.f * PI);

	const float Amplitude = HeadBobScale * FMath::Min(BobBlend, 1.f) * FMath::Lerp(1.f, SprintBobMultiplier, SprintAlpha);
	const float Lateral = FMath::Sin(BobPhase);
	const float Vertical = FMath::Sin(2.f * BobPhase);

	BreathTime += DeltaTime;
	const float Breath = FMath::Sin(BreathTime * BreathRate * 2.f * PI) * BreathAmplitude * HeadBobScale;

	BobOffset = FVector(0.f, Lateral * BobHorizontal * Amplitude, Vertical * BobVertical * Amplitude + Breath);
	BobRotation = FRotator(Vertical * 0.3f * Amplitude + Breath * 0.4f, 0.f, Lateral * BobRollDegrees * Amplitude);
}

void UBodycamCameraComponent::UpdateTurnRoll(float DeltaTime, const FRotator& ControlRot)
{
	if (!bHasLastYaw || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		LastYaw = ControlRot.Yaw;
		bHasLastYaw = true;
		return;
	}

	const float YawRate = FMath::FindDeltaAngleDegrees(LastYaw, ControlRot.Yaw) / DeltaTime;
	LastYaw = ControlRot.Yaw;

	const float Target = FMath::Clamp(-YawRate * TurnRollFactor, -MaxTurnRoll, MaxTurnRoll) * HeadBobScale;
	TurnRoll = FMath::FInterpTo(TurnRoll, Target, DeltaTime, 8.f);
}

void UBodycamCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	Super::GetCameraView(DeltaTime, DesiredView);

	// Décalages appliqués au moment du rendu : aucune latence d'une image.
	const FVector Right = FRotator(0.f, DesiredView.Rotation.Yaw, 0.f).RotateVector(FVector::RightVector);
	DesiredView.Location += Right * (LeanOffset + BobOffset.Y) + FVector::UpVector * (BobOffset.Z + LandOffset);
	DesiredView.Rotation.Pitch += BobRotation.Pitch;
	DesiredView.Rotation.Roll += LeanRoll + BobRotation.Roll + TurnRoll;

	if (ShakeAmount > 0.01f)
	{
		const float T = ShakeTime * 23.f;
		DesiredView.Rotation += FRotator(FMath::PerlinNoise1D(T) * 2.5f, FMath::PerlinNoise1D(T + 37.f) * 2.5f, FMath::PerlinNoise1D(T + 71.f) * 3.5f) * ShakeAmount;
		DesiredView.Location += FVector(0.f, 0.f, FMath::PerlinNoise1D(T + 11.f) * 1.5f * ShakeAmount);
	}

	// L'arme et les jumelles suivent la vue exacte (penché, balancement, secousses compris).
	if (USceneComponent* VM = ViewModelRoot.Get())
	{
		VM->SetWorldLocationAndRotation(DesiredView.Location, DesiredView.Rotation);
	}
}
