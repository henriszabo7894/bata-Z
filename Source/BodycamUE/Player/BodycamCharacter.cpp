#include "Player/BodycamCharacter.h"

#include "Camera/BodycamCameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "EngineUtils.h"
#include "Engine/SkyLight.h"
#include "World/BodycamFlickerLight.h"
#include "World/BodycamAlarmLight.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/BodycamGameMode.h"
#include "EnhancedInputComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/BodycamDrone.h"
#include "Gameplay/BodycamSounds.h"
#include "InputActionValue.h"
#include "Misc/ConfigCacheIni.h"
#include "NavigationInvokerComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/BodycamPlayerController.h"
#include "Weapons/BodycamWeapon.h"
#include "World/BodycamExterior.h"
#include "Zombies/BodycamZombie.h"

ABodycamCharacter::ABodycamCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Capsule d'un adulte équipé : 1,84 m, 68 cm de large.
	GetCapsuleComponent()->InitCapsuleSize(34.f, 92.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	BaseEyeHeight = CameraStandHeight;
	CrouchedEyeHeight = CameraCrouchHeight;

	BodycamCamera = CreateDefaultSubobject<UBodycamCameraComponent>(TEXT("BodycamCamera"));
	BodycamCamera->SetupAttachment(GetCapsuleComponent());
	BodycamCamera->SetRelativeLocation(FVector(CameraForwardOffset, 0.f, CameraStandHeight));
	BodycamCamera->bUsePawnControlRotation = true;

	ViewModelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ViewModelRoot"));
	ViewModelRoot->SetupAttachment(GetCapsuleComponent());
	ViewModelRoot->SetUsingAbsoluteLocation(true);
	ViewModelRoot->SetUsingAbsoluteRotation(true);

	// Jumelles de vision nocturne : charnière au-dessus des yeux, basculent devant la vue.
	NVGPivot = CreateDefaultSubobject<USceneComponent>(TEXT("NVGPivot"));
	NVGPivot->SetupAttachment(ViewModelRoot);
	NVGPivot->SetRelativeLocation(FVector(4.f, 0.f, 11.f));

	NVGMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NVGMesh"));
	NVGMesh->SetupAttachment(NVGPivot);
	NVGMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NVGMesh->CastShadow = false;
	NVGMesh->SetVisibility(false);

	// Projecteur IR : éclaire les 15-25 m devant quand la JVN est baissée.
	IRIlluminator = CreateDefaultSubobject<USpotLightComponent>(TEXT("IRIlluminator"));
	IRIlluminator->SetupAttachment(BodycamCamera);
	IRIlluminator->SetMobility(EComponentMobility::Movable);
	IRIlluminator->SetIntensityUnits(ELightUnits::Candelas);
	IRIlluminator->SetIntensity(22.f);
	IRIlluminator->SetAttenuationRadius(1800.f);
	IRIlluminator->SetOuterConeAngle(38.f);
	IRIlluminator->SetInnerConeAngle(14.f);
	IRIlluminator->SetCastShadows(false);
	IRIlluminator->SetVisibility(false);

	// Génère la navigation des zombies autour du joueur (navmesh dynamique).
	NavInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavInvoker"));
	NavInvoker->SetGenerationRadii(4500.f, 5500.f);

	GetMesh()->SetOwnerNoSee(true);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->NavAgentProps.bCanCrouch = true;
	Movement->SetCrouchedHalfHeight(62.f);
	Movement->bCanWalkOffLedgesWhenCrouching = true;
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxWalkSpeedCrouched = CrouchSpeed;
	Movement->MaxAcceleration = 1100.f;
	Movement->BrakingDecelerationWalking = 900.f;
	Movement->GroundFriction = 6.f;
	Movement->JumpZVelocity = 330.f;
	Movement->AirControl = 0.15f;

	CurrentCameraHeight = CameraStandHeight;
	SetCanBeDamaged(false);
}

void ABodycamCharacter::BeginPlay()
{
	Super::BeginPlay();

	BodycamCamera->SetViewModelRoot(ViewModelRoot);

	if (UStaticMesh* NVG = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Bodycam/Gear/NVG/SM_NVG.SM_NVG")))
	{
		NVGMesh->SetStaticMesh(NVG);
	}
	FVector NVGOffset(9.f, 0.f, -12.f);
	FRotator NVGRot(0.f, 90.f, 0.f);
	float NVGScale = 0.18f;
	if (GConfig)
	{
		GConfig->GetVector(TEXT("Bodycam.NVG"), TEXT("MeshOffset"), NVGOffset, GGameIni);
		GConfig->GetRotator(TEXT("Bodycam.NVG"), TEXT("MeshRotation"), NVGRot, GGameIni);
		GConfig->GetFloat(TEXT("Bodycam.NVG"), TEXT("MeshScale"), NVGScale, GGameIni);
	}
	NVGMesh->SetRelativeLocationAndRotation(NVGOffset, NVGRot);
	NVGMesh->SetRelativeScale3D(FVector(NVGScale));
	NVGPivot->SetRelativeRotation(FRotator(105.f, 0.f, 0.f));

	ApplyNightLighting();

	// Zone extérieure (où le drone va chercher la caisse) : construite une seule fois par carte.
	bool bHasExterior = false;
	for (TActorIterator<ABodycamExterior> It(GetWorld()); It; ++It) { bHasExterior = true; break; }
	if (!bHasExterior) GetWorld()->SpawnActor<ABodycamExterior>(ABodycamExterior::StaticClass(), FTransform::Identity);

	// Bras nus (sans arme) pour repousser un zombie qui nous agrippe.
	FActorSpawnParameters HP;
	HP.Owner = this;
	HP.Instigator = this;
	BareHands = GetWorld()->SpawnActor<ABodycamWeapon>(ABodycamWeapon::StaticClass(), GetActorTransform(), HP);
	if (BareHands)
	{
		BareHands->AttachToComponent(ViewModelRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		BareHands->InitWeapon(EBodycamWeaponType::Pistol);
		BareHands->MakeBareHands();
	}
}

void ABodycamCharacter::UpdateStruggle(float DeltaSeconds)
{
	const bool bStruggling = GrabMode == EZombieAttack::Front || GrabMode == EZombieAttack::Back;
	ShoveTime = FMath::Max(0.f, ShoveTime - DeltaSeconds);
	StruggleAlpha = FMath::FInterpTo(StruggleAlpha, (bStruggling || ShoveTime > 0.f) ? 1.f : 0.f, DeltaSeconds, bStruggling ? 9.f : 5.f);
	StruggleThrust = FMath::FInterpTo(StruggleThrust, ShoveTime > 0.f ? 1.8f : 0.f, DeltaSeconds, ShoveTime > 0.f ? 14.f : 7.f);
	if (BareHands) BareHands->SetStruggle(StruggleAlpha, StruggleThrust);
}

void ABodycamCharacter::ApplyNightLighting()
{
	// La carte est éclairée "de jour" (lumière du ciel + lampes fortes) : on l'assombrit au lancement.
	// Réglable sans recompiler : DefaultGame.ini [Bodycam.Night].
	float SkyScale = 0.03f, LampScale = 0.3f, SunScale = 0.f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("Bodycam.Night"), TEXT("SkyLightScale"), SkyScale, GGameIni);
		GConfig->GetFloat(TEXT("Bodycam.Night"), TEXT("LampScale"), LampScale, GGameIni);
		GConfig->GetFloat(TEXT("Bodycam.Night"), TEXT("SunScale"), SunScale, GGameIni);
	}
	int32 NSky = 0, NLamp = 0, NStatic = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (!A || A->IsA<APawn>() || A->IsA<ABodycamWeapon>() || A->IsA<ABodycamFlickerLight>()) continue;
		TArray<ULightComponentBase*> Lights;
		A->GetComponents<ULightComponentBase>(Lights);
		for (ULightComponentBase* L : Lights)
		{
			if (L->Mobility == EComponentMobility::Static)
			{
				++NStatic;   // éclairage précalculé : ne peut pas changer en jeu
				continue;
			}
			if (USkyLightComponent* Sky = Cast<USkyLightComponent>(L))
			{
				Sky->SetIntensity(Sky->Intensity * SkyScale);
				++NSky;
			}
			else if (ULightComponent* LC = Cast<ULightComponent>(L))
			{
				const bool bSun = LC->GetLightType() == LightType_Directional;
				LC->SetIntensity(LC->Intensity * (bSun ? SunScale : LampScale));
				++NLamp;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Nuit : %d lumiere(s) du ciel, %d lampe(s) assombries, %d lumiere(s) statiques non modifiables"), NSky, NLamp, NStatic);

	// Dehors (derrière les fenêtres) : ciel, brouillard lumineux et panneaux "émissifs" font un mur blanc.
	// On coupe le ciel, on noircit le brouillard et on remplace les matériaux non éclairés (fonds de décor) par du noir.
	bool bDarkOutside = true;
	if (GConfig) GConfig->GetBool(TEXT("Bodycam.Night"), TEXT("DarkOutside"), bDarkOutside, GGameIni);
	if (bDarkOutside)
	{
		UMaterialInterface* Black = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/MI_NightBlack.MI_NightBlack"));
		int32 NFog = 0, NSkyMesh = 0, NUnlit = 0;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* A = *It;
			if (!A || A->IsA<APawn>() || A->IsA<ABodycamWeapon>() || A->IsA<ABodycamAlarmLight>()) continue;
			const FString AName = A->GetName().ToLower();
			TArray<UActorComponent*> Comps;
			A->GetComponents(Comps);
			for (UActorComponent* C : Comps)
			{
				if (UExponentialHeightFogComponent* Fog = Cast<UExponentialHeightFogComponent>(C))
				{
					Fog->SetFogInscatteringColor(FLinearColor(0.002f, 0.002f, 0.003f));
					Fog->SetDirectionalInscatteringColor(FLinearColor::Black);
					++NFog;
				}
				else if (USkyAtmosphereComponent* Atmo = Cast<USkyAtmosphereComponent>(C))
				{
					Atmo->SetVisibility(false);
					++NSkyMesh;
				}
				else if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(C))
				{
					if (AName.Contains(TEXT("sky")))
					{
						SMC->SetVisibility(false);
						++NSkyMesh;
						continue;
					}
					for (int32 i = 0; i < SMC->GetNumMaterials(); ++i)
					{
						UMaterialInterface* M = SMC->GetMaterial(i);
						if (!M) continue;
						const FString MName = M->GetName().ToLower();
						const bool bUnlit = M->GetShadingModels().HasShadingModel(MSM_Unlit);
						if ((bUnlit || MName.Contains(TEXT("backdrop")) || MName.Contains(TEXT("sky")) || MName.Contains(TEXT("emissive_window")))
							&& M->GetBlendMode() != BLEND_Additive)
						{
							if (Black) SMC->SetMaterial(i, Black);
							else SMC->SetVisibility(false);
							UE_LOG(LogTemp, Log, TEXT("Nuit : fond lumineux noirci %s / %s"), *A->GetName(), *M->GetName());
							++NUnlit;
						}
					}
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("Nuit : %d brouillard(s), %d ciel(s) coupes, %d materiau(x) lumineux noircis"), NFog, NSkyMesh, NUnlit);
	}

	// Gyrophares d'alarme rouges au plafond des couloirs : DefaultGame.ini [Bodycam.Alarm] Beacon=(X=..,Y=..,Z=..)
	TArray<FString> Beacons;
	float Beam = 2500.f, Speed = 220.f;
	if (GConfig)
	{
		GConfig->GetArray(TEXT("Bodycam.Alarm"), TEXT("Beacon"), Beacons, GGameIni);
		GConfig->GetFloat(TEXT("Bodycam.Alarm"), TEXT("BeamIntensity"), Beam, GGameIni);
		GConfig->GetFloat(TEXT("Bodycam.Alarm"), TEXT("RotationSpeed"), Speed, GGameIni);
	}
	for (TActorIterator<ABodycamAlarmLight> It(GetWorld()); It; ++It)
	{
		It->Destroy();   // relance de niveau : pas de doublons
	}
	int32 NAlarm = 0;
	for (const FString& S : Beacons)
	{
		FVector P;
		if (!P.InitFromString(S)) continue;
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ABodycamAlarmLight* A = GetWorld()->SpawnActor<ABodycamAlarmLight>(ABodycamAlarmLight::StaticClass(), P, FRotator::ZeroRotator, SP))
		{
			A->BeamIntensity = Beam;
			A->RotationSpeed = Speed;
			A->PlaceUnderCeiling(P);
			// Sirène : sur 1 gyrophare sur 4 (sinon cacophonie), décalée dans le temps.
			if (NAlarm % 4 == 0)
			{
				FBodycamSounds::PlayAttached(FBodycamSounds::Get(TEXT("Siren_Loop")), A->GetRootComponent(), 0.55f, 1.f, 700.f, 5000.f, NAlarm * 9.f);
			}
			++NAlarm;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Alarme : %d gyrophare(s)"), NAlarm);
}

void ABodycamCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ABodycamCharacter, bIsSprinting, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ABodycamCharacter, LeanTarget, COND_SkipOwner);
}

void ABodycamCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	ABodycamPlayerController* PC = Cast<ABodycamPlayerController>(GetController());
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!PC || !Input)
	{
		UE_LOG(LogTemp, Error, TEXT("BodycamCharacter: il faut BodycamPlayerController + Enhanced Input (voir BodycamGameMode)."));
		return;
	}

	const UBodycamInputConfig* Cfg = PC->GetInputConfig();

	Input->BindAction(Cfg->Move, ETriggerEvent::Triggered, this, &ABodycamCharacter::OnMove);
	Input->BindAction(Cfg->Move, ETriggerEvent::Completed, this, &ABodycamCharacter::OnMoveCompleted);
	Input->BindAction(Cfg->Look, ETriggerEvent::Triggered, this, &ABodycamCharacter::OnLook);
	Input->BindAction(Cfg->LookGamepad, ETriggerEvent::Triggered, this, &ABodycamCharacter::OnLookGamepad);

	Input->BindAction(Cfg->Sprint, ETriggerEvent::Started, this, &ABodycamCharacter::OnSprintPressed);
	Input->BindAction(Cfg->Sprint, ETriggerEvent::Completed, this, &ABodycamCharacter::OnSprintReleased);
	Input->BindAction(Cfg->LeanLeft, ETriggerEvent::Started, this, &ABodycamCharacter::OnLeanLeftPressed);
	Input->BindAction(Cfg->LeanLeft, ETriggerEvent::Completed, this, &ABodycamCharacter::OnLeanLeftReleased);
	Input->BindAction(Cfg->LeanRight, ETriggerEvent::Started, this, &ABodycamCharacter::OnLeanRightPressed);
	Input->BindAction(Cfg->LeanRight, ETriggerEvent::Completed, this, &ABodycamCharacter::OnLeanRightReleased);
	Input->BindAction(Cfg->Crouch, ETriggerEvent::Started, this, &ABodycamCharacter::OnCrouchPressed);
	Input->BindAction(Cfg->Crouch, ETriggerEvent::Completed, this, &ABodycamCharacter::OnCrouchReleased);
	Input->BindAction(Cfg->Jump, ETriggerEvent::Started, this, &ABodycamCharacter::OnJumpPressed);
	Input->BindAction(Cfg->Jump, ETriggerEvent::Completed, this, &ABodycamCharacter::OnJumpReleased);
	Input->BindAction(Cfg->NightVision, ETriggerEvent::Started, this, &ABodycamCharacter::OnNightVisionPressed);
	Input->BindAction(Cfg->Fire, ETriggerEvent::Started, this, &ABodycamCharacter::OnFirePressed);
	Input->BindAction(Cfg->Fire, ETriggerEvent::Completed, this, &ABodycamCharacter::OnFireReleased);
	Input->BindAction(Cfg->Reload, ETriggerEvent::Started, this, &ABodycamCharacter::OnReloadPressed);
	Input->BindAction(Cfg->Interact, ETriggerEvent::Started, this, &ABodycamCharacter::OnInteractPressed);
	Input->BindAction(Cfg->Phone, ETriggerEvent::Started, this, &ABodycamCharacter::OnPhonePressed);
	Input->BindAction(Cfg->Flashlight, ETriggerEvent::Started, this, &ABodycamCharacter::OnFlashlightPressed);
	Input->BindAction(Cfg->Aim, ETriggerEvent::Started, this, &ABodycamCharacter::OnAimPressed);
	Input->BindAction(Cfg->Melee, ETriggerEvent::Started, this, &ABodycamCharacter::OnMeleePressed);
	Input->BindAction(Cfg->Aim, ETriggerEvent::Completed, this, &ABodycamCharacter::OnAimReleased);
}

void ABodycamCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead)
	{
		UpdateDeath(DeltaSeconds);
		BodycamCamera->SetInfection(Infection, HurtFlash * 0.5f);
		HurtFlash = FMath::Max(0.f, HurtFlash - DeltaSeconds * 0.5f);
		return;
	}

	if (IsLocallyControlled())
	{
		UpdateSprintAndLean();
	}
	UpdateStamina(DeltaSeconds);
	UpdateCameraHeight(DeltaSeconds);
	UpdateNVG(DeltaSeconds);
	UpdateGrab(DeltaSeconds);
	UpdateStruggle(DeltaSeconds);
	UpdateInfection(DeltaSeconds);
	UpdateDronePiloting();
	UpdateDroneDeploy(DeltaSeconds);

	float Speed = bIsSprinting ? SprintSpeed : WalkSpeed;
	if (GrabMode == EZombieAttack::Ankle) Speed = 85.f;
	else if (GrabMode != EZombieAttack::None) Speed = 0.f;
	// Visée : clic droit maintenu, impossible en courant / agrippé / téléphone.
	if (Weapon)
	{
		const bool bCanAim = bAimHeld && CanAct() && GrabMode == EZombieAttack::None && !bPhoneOpen && !bPilotingDrone;
		Weapon->SetAiming(bCanAim);
		const float Aim = Weapon->GetAimAlpha();
		BodycamCamera->SetFieldOfView(FMath::Lerp(100.f, Weapon->GetAimFOV(), Aim));
		if (Aim > 0.3f) Speed = FMath::Min(Speed, WalkSpeed * 0.55f);   // on avance lentement en visant
	}
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	BodycamCamera->SetLeanTarget(LeanTarget);

	if (Weapon)
	{
		const bool bBusyHands = bPhoneOpen || bPilotingDrone || DeployTimer >= 0.f || GrabMode == EZombieAttack::Front || GrabMode == EZombieAttack::Back
			|| NVGState == ENVGState::Lowering || NVGState == ENVGState::Raising;
		Weapon->SetLowered(bBusyHands);
	}

	if (GetCharacterMovement()->IsFalling())
	{
		LastFallSpeed = FMath::Max(0.f, -GetVelocity().Z);
	}
}

bool ABodycamCharacter::CanAct() const
{
	return !bDead && !bPilotingDrone && !bPhoneOpen && DeployTimer < 0.f;
}

// ---------------------------------------------------------------- Entrées

void ABodycamCharacter::OnMove(const FInputActionValue& Value)
{
	if (bPilotingDrone)
	{
		if (Drone) Drone->SetMoveInput(Value.Get<FVector2D>());
		MoveInput = FVector2D::ZeroVector;
		return;
	}
	if (bDead || DeployTimer >= 0.f || GrabMode == EZombieAttack::Front || GrabMode == EZombieAttack::Back)
	{
		MoveInput = FVector2D::ZeroVector;
		return;
	}
	MoveInput = Value.Get<FVector2D>();
	const float Forward = MoveInput.Y >= 0.f ? MoveInput.Y : MoveInput.Y * BackwardSpeedScale;
	AddMovementInput(GetActorForwardVector(), Forward);
	AddMovementInput(GetActorRightVector(), MoveInput.X * StrafeSpeedScale);
}

void ABodycamCharacter::OnMoveCompleted()
{
	MoveInput = FVector2D::ZeroVector;
	if (Drone) Drone->SetMoveInput(FVector2D::ZeroVector);
}

void ABodycamCharacter::OnLook(const FInputActionValue& Value)
{
	const ABodycamPlayerController* PC = Cast<ABodycamPlayerController>(GetController());
	const float Sensitivity = PC ? PC->MouseSensitivity : 1.f;
	FVector2D Delta = Value.Get<FVector2D>() * Sensitivity;

	if (bPilotingDrone)
	{
		if (Drone) Drone->AddLookInput(Delta * 0.8f);
		return;
	}
	if (bDead || (DeployTimer >= 0.f && bDeployNew)) return;
	if (GrabMode != EZombieAttack::None) Delta *= 0.35f;

	AddControllerYawInput(Delta.X);
	AddControllerPitchInput(-Delta.Y);
}

void ABodycamCharacter::OnLookGamepad(const FInputActionValue& Value)
{
	const ABodycamPlayerController* PC = Cast<ABodycamPlayerController>(GetController());
	const float Rate = (PC ? PC->GamepadLookRate : 60.f) * GetWorld()->GetDeltaSeconds();
	const FVector2D Stick = Value.Get<FVector2D>();
	if (bPilotingDrone)
	{
		if (Drone) Drone->AddLookInput(Stick * Rate);
		return;
	}
	if (bDead) return;
	AddControllerYawInput(Stick.X * Rate);
	AddControllerPitchInput(-Stick.Y * Rate);
}

bool ABodycamCharacter::IsCrouchToggle() const
{
	const ABodycamPlayerController* PC = Cast<ABodycamPlayerController>(GetController());
	return !PC || PC->bCrouchToggle;
}

void ABodycamCharacter::OnCrouchPressed()
{
	bCrouchHeld = true;
	if (bPilotingDrone || bDead) return;
	if (IsCrouchToggle() && bIsCrouched) UnCrouch();
	else Crouch();
}

void ABodycamCharacter::OnCrouchReleased()
{
	bCrouchHeld = false;
	if (bPilotingDrone || bDead) return;
	if (!IsCrouchToggle()) UnCrouch();
}

void ABodycamCharacter::OnJumpPressed()
{
	bJumpHeld = true;
	if (bPilotingDrone || bDead || GrabMode != EZombieAttack::None) return;
	if (bIsCrouched)
	{
		UnCrouch();
		return;
	}
	if (Stamina >= JumpStaminaCost && CanJump())
	{
		Stamina -= JumpStaminaCost;
		TimeSinceStaminaUse = 0.f;
		Jump();
	}
}

void ABodycamCharacter::OnJumpReleased()
{
	bJumpHeld = false;
	StopJumping();
}

void ABodycamCharacter::OnFirePressed()
{
	if (!Weapon || !CanAct()) return;
	if (GrabMode == EZombieAttack::Front || GrabMode == EZombieAttack::Back) return;
	Weapon->StartFire();
}

void ABodycamCharacter::OnFireReleased()
{
	if (Weapon) Weapon->StopFire();
}

void ABodycamCharacter::OnReloadPressed()
{
	if (bPilotingDrone)
	{
		if (Drone) Drone->StartAutoReturn();
		return;
	}
	if (Weapon && CanAct() && GrabMode == EZombieAttack::None)
	{
		Weapon->StartReload();
	}
}

void ABodycamCharacter::OnInteractPressed()
{
	if (bPilotingDrone)
	{
		if (Drone) Drone->TryGrab();
		return;
	}
	if (bDead) return;

	// Se dégager d'un zombie : marteler F (dos / face) ou donner des coups de pied (cheville).
	if (GrabMode == EZombieAttack::Front || GrabMode == EZombieAttack::Back)
	{
		EscapeProgress = FMath::Min(1.f, EscapeProgress + 0.11f);
		StruggleThrust = 1.f;   // les mains poussent d'un coup
		BodycamCamera->AddShake(0.35f);
		// Barre pleine : dégagé tout de suite (avant, la barre redescendait d'un poil dans la même image
		// et le test "== 100 %" n'était jamais vrai : le zombie ne lâchait jamais).
		if (EscapeProgress >= 0.999f)
		{
			EndGrab(true);
			if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>()) GM->PushEvent(TEXT("DEGAGE !"));
		}
		return;
	}
	if (GrabMode == EZombieAttack::Ankle)
	{
		if (KickAnim <= 0.f)
		{
			KickAnim = 0.45f;
			EscapeProgress = FMath::Min(1.f, EscapeProgress + 0.34f);
			BodycamCamera->AddShake(0.8f);
			if (EscapeProgress >= 0.999f)
			{
				EndGrab(true);
				if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>()) GM->PushEvent(TEXT("DEGAGE !"));
			}
		}
		return;
	}

}

void ABodycamCharacter::OnMeleePressed()
{
	if (Weapon && CanAct() && GrabMode == EZombieAttack::None && !bPhoneOpen) Weapon->StartMelee();
}

void ABodycamCharacter::OnFlashlightPressed()
{
	if (Weapon && !bDead) Weapon->ToggleFlashlight();
}

void ABodycamCharacter::OnPhonePressed()
{
	if (bDead || GrabMode != EZombieAttack::None) return;
	// T : retirer le casque / remettre le casque / sortir un nouveau drone.
	if (DeployTimer >= 0.f) return;
	if (bPilotingDrone)
	{
		StopPiloting();
		return;
	}
	bPhoneOpen = false;
	if (Weapon) Weapon->StopFire();
	SetSprinting(false);
	if (Drone)
	{
		bDeployNew = false;
		DeployTimer = 0.f;
		return;
	}
	ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>();
	if (GM && GM->RequestDroneLaunch())
	{
		bDeployNew = true;
		DeployTimer = 0.f;
		DeployStartPitch = FRotator::NormalizeAxis(GetControlRotation().Pitch);
	}
}

// ---------------------------------------------------------------- Arme

void ABodycamCharacter::EquipWeapon(EBodycamWeaponType Type, int32 ReserveMags)
{
	if (Weapon) Weapon->Destroy();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Weapon = GetWorld()->SpawnActor<ABodycamWeapon>(ABodycamWeapon::StaticClass(), GetActorTransform(), Params);
	if (!Weapon) return;
	Weapon->AttachToComponent(ViewModelRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Weapon->InitWeapon(Type);
	Weapon->SetReserveMagazines(ReserveMags);
}

// ---------------------------------------------------------------- Vie / attaques

bool ABodycamCharacter::ReceiveZombieAttack(ABodycamZombie* Zombie, EZombieAttack Mode)
{
	if (bDead || !Zombie) return false;
	if (GrabMode != EZombieAttack::None && Mode != EZombieAttack::Neck) return false;
	if (GrabImmunity > 0.f && Mode != EZombieAttack::Push) return false;
	if (bPilotingDrone || DeployTimer >= 0.f) return false;   // sous le casque : les zombies ne nous trouvent pas

	// Une attaque coupe le téléphone / le pilotage du drone.
	if (bPilotingDrone) StopPiloting();
	bPhoneOpen = false;
	if (DeployTimer >= 0.f && bDeployNew && !Drone) SpawnDroneOnFloor();   // le drone tombe par terre
	DeployTimer = -1.f;

	switch (Mode)
	{
	case EZombieAttack::Push:
		TakeHit(true, 0.6f);
		BodycamCamera->AddShake(1.4f);
		LaunchCharacter((GetActorLocation() - Zombie->GetActorLocation()).GetSafeNormal2D() * 350.f, true, false);
		return true;

	case EZombieAttack::Front:
	case EZombieAttack::Back:
	case EZombieAttack::Ankle:
		TakeHit(true, 0.35f);
		if (!bDead) StartGrab(Zombie, Mode);
		return true;

	case EZombieAttack::Neck:
		Zombie->PlayNeckBite();
		Die(true);
		return true;

	default:
		return false;
	}
}

void ABodycamCharacter::PlayHurtSound()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastHurtSound < 0.9f) return;
	LastHurtSound = Now;
	FBodycamSounds::Play2D(this, FBodycamSounds::GetRandom(TEXT("P_Hurt"), 2), 0.9f, FMath::FRandRange(0.96f, 1.04f));
}

void ABodycamCharacter::TakeHit(bool bCanInfect, float InfectChance)
{
	++Hits;
	PlayHurtSound();
	HurtFlash = 1.f;
	BodycamCamera->AddShake(1.f);
	if (bCanInfect && !bInfected && FMath::FRand() < InfectChance)
	{
		bInfected = true;
		Infection = 0.15f;
	}
	if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>())
	{
		GM->OnPlayerHit(Hits, MaxHits);
		if (bInfected && Infection <= 0.151f) GM->PushEvent(TEXT("CONTAMINATION DETECTEE"));
	}
	if (Hits >= MaxHits)
	{
		Die(false);
	}
}

void ABodycamCharacter::StartGrab(ABodycamZombie* Zombie, EZombieAttack Mode)
{
	Grabber = Zombie;
	GrabMode = Mode;
	EscapeProgress = 0.f;
	PlayHurtSound();
	GrabTimeLeft = Mode == EZombieAttack::Ankle ? 7.f : Mode == EZombieAttack::Back ? 4.5f : 5.f;
	SetSprinting(false);
	if (Weapon) Weapon->StopFire();
	BodycamCamera->AddShake(2.f);
}

void ABodycamCharacter::EndGrab(bool bEscaped)
{
	UE_LOG(LogTemp, Log, TEXT("Degagement : %s (zombie %s)"), bEscaped ? TEXT("reussi") : TEXT("echoue"), Grabber ? *Grabber->GetName() : TEXT("aucun"));
	if (Grabber) Grabber->ReleaseGrab(bEscaped);
	Grabber = nullptr;
	GrabMode = EZombieAttack::None;
	EscapeProgress = 0.f;
	// Après s'être dégagé : 3 s de répit pour reculer ou tirer.
	GrabImmunity = bEscaped ? 3.f : 1.5f;
	if (bEscaped)
	{
		ShoveTime = 0.45f;   // grande poussée finale des deux mains
		BodycamCamera->AddShake(0.8f);
	}
}

void ABodycamCharacter::OnGrabberKilled(ABodycamZombie* Zombie)
{
	if (Zombie && Zombie == Grabber)
	{
		Grabber = nullptr;
		GrabMode = EZombieAttack::None;
		EscapeProgress = 0.f;
		GrabImmunity = 1.f;
	}
}

void ABodycamCharacter::UpdateGrab(float DeltaSeconds)
{
	GrabImmunity = FMath::Max(0.f, GrabImmunity - DeltaSeconds);
	KickAnim = FMath::Max(0.f, KickAnim - DeltaSeconds);
	if (GrabMode == EZombieAttack::None) return;

	if (!Grabber || Grabber->IsDead())
	{
		OnGrabberKilled(Grabber);
		GrabMode = EZombieAttack::None;
		return;
	}

	GrabTimeLeft -= DeltaSeconds;
	if (GrabMode != EZombieAttack::Ankle)
	{
		EscapeProgress = FMath::Max(0.f, EscapeProgress - DeltaSeconds * 0.16f);
	}

	// Vue forcée : cheville = on regarde ses pieds ; face = visage du zombie ; dos = on se débat.
	if (AController* C = GetController())
	{
		FRotator Cur = C->GetControlRotation();
		FRotator Target = Cur;
		if (GrabMode == EZombieAttack::Ankle)
		{
			Target.Pitch = -62.f + KickAnim * 25.f;
			Target.Yaw = (Grabber->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
		}
		else if (GrabMode == EZombieAttack::Front)
		{
			const FVector Head = Grabber->GetActorLocation() + FVector(0, 0, 55.f);
			const FRotator Look = (Head - BodycamCamera->GetComponentLocation()).Rotation();
			Target.Pitch = Look.Pitch;
			Target.Yaw = Look.Yaw;
		}
		else
		{
			Target.Pitch = 8.f + FMath::Sin(GetWorld()->GetTimeSeconds() * 9.f) * 6.f;
		}
		Target.Pitch = FRotator::NormalizeAxis(Target.Pitch);
		Cur.Pitch = FRotator::NormalizeAxis(Cur.Pitch);
		const FRotator NewRot = FMath::RInterpTo(Cur, Target, DeltaSeconds, 5.f);
		C->SetControlRotation(NewRot);
	}
	BodycamCamera->AddShake(DeltaSeconds * (GrabMode == EZombieAttack::Back ? 3.f : 1.5f));

	if (EscapeProgress >= 0.999f)
	{
		EndGrab(true);
		if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>())
		{
			GM->PushEvent(TEXT("DEGAGE !"));
		}
		return;
	}

	if (GrabTimeLeft <= 0.f)
	{
		if (GrabMode == EZombieAttack::Ankle)
		{
			// Il mord encore : nouveau coup.
			TakeHit(true, 0.3f);
			GrabTimeLeft = 7.f;
		}
		else
		{
			// Pas réussi à se dégager : morsure au cou.
			ABodycamZombie* Z = Grabber;
			GrabMode = EZombieAttack::None;
			Grabber = nullptr;
			if (Z) Z->PlayNeckBite();
			Die(true);
		}
	}
}

void ABodycamCharacter::UpdateInfection(float DeltaSeconds)
{
	HurtFlash = FMath::Max(0.f, HurtFlash - DeltaSeconds * 1.2f);
	if (bInfected)
	{
		// La contamination progresse en ~5 min : vue trouble, double, essoufflement, toux.
		Infection = FMath::Min(1.f, Infection + DeltaSeconds / 300.f);
		CoughTimer -= DeltaSeconds;
		if (CoughTimer <= 0.f && Infection > 0.3f)
		{
			CoughTimer = FMath::FRandRange(12.f, 30.f) * (1.2f - Infection);
			BodycamCamera->AddShake(1.2f * Infection);
		}
	}
	BodycamCamera->SetInfection(bInfected ? Infection : 0.f, HurtFlash);
}

void ABodycamCharacter::Die(bool bNeckBite)
{
	if (bDead) return;
	bDead = true;
	FBodycamSounds::Play2D(this, FBodycamSounds::Get(TEXT("P_Death")), 1.f);
	bNeckBiteDeath = bNeckBite;
	DeathTime = 0.f;
	HurtFlash = 1.f;
	if (bPilotingDrone) StopPiloting();
	bPhoneOpen = false;
	DeployTimer = -1.f;
	// Façon de tomber : au hasard (morsure au cou = sur le côté, le zombie tire la tête).
	DeathStyle = bNeckBite ? (FMath::RandBool() ? 2 : 3) : FMath::RandRange(0, 3);
	DeathStartRot = GetControlRotation();
	DeathStartRot.Pitch = FRotator::NormalizeAxis(DeathStartRot.Pitch);
	bDeathImpact = false;
	if (Weapon)
	{
		Weapon->StopFire();
		Weapon->Drop(GetVelocity());   // les mains lâchent l'arme
	}
	BodycamCamera->SetFieldOfView(100.f);
	if (BareHands) BareHands->SetStruggle(0.f, 0.f);
	GetCharacterMovement()->DisableMovement();
	BodycamCamera->AddShake(0.5f);
	if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>())
	{
		GM->OnPlayerDied();
	}
}

void ABodycamCharacter::UpdateDeath(float DeltaSeconds)
{
	DeathTime += DeltaSeconds;

	// Chute en 3 temps : les jambes lâchent (0-0.35 s), chute accélérée (0.35-1.05 s), impact + léger rebond.
	struct FFall { float Pitch, Roll, Yaw, Fwd, Side, Height; };
	static const FFall Styles[4] = {
		{  62.f,  12.f,  10.f, -55.f,   0.f, -72.f },   // en arrière : on finit sur le dos, regard au plafond
		{ -68.f,  -6.f,  -5.f,  45.f,   0.f, -78.f },   // en avant : face contre le sol
		{ -12.f, -82.f, -18.f,   5.f, -35.f, -74.f },   // sur le côté gauche
		{ -12.f,  82.f,  18.f,   5.f,  35.f, -74.f },   // sur le côté droit
	};
	const FFall& F = Styles[FMath::Clamp(DeathStyle, 0, 3)];
	const float Knees = FMath::Clamp(DeathTime / 0.35f, 0.f, 1.f);
	const float FallT = FMath::Clamp((DeathTime - 0.35f) / 0.7f, 0.f, 1.f);
	const float Fall = FallT * FallT;                       // accélère comme une vraie chute
	const float Bounce = DeathTime > 1.05f ? FMath::Sin(FMath::Min((DeathTime - 1.05f) / 0.25f, 1.f) * PI) * 0.06f : 0.f;

	if (!bDeathImpact && FallT >= 1.f)
	{
		bDeathImpact = true;
		BodycamCamera->AddShake(1.4f);
		HurtFlash = FMath::Max(HurtFlash, 0.6f);
	}

	const float Height = FMath::Lerp(CameraStandHeight, CameraStandHeight - 25.f, FMath::InterpEaseOut(0.f, 1.f, Knees, 2.f));
	const float H = FMath::Lerp(Height, F.Height, Fall) + Bounce * 20.f;
	BodycamCamera->SetRelativeLocation(FVector(CameraForwardOffset + F.Fwd * Fall, F.Side * Fall, H));

	if (AController* C = GetController())
	{
		const FRotator Stagger(-6.f * Knees * (1.f - Fall), 4.f * FMath::Sin(DeathTime * 7.f) * (1.f - Fall), 6.f * Knees * (1.f - Fall));
		const FRotator Target(F.Pitch * Fall + Stagger.Pitch, F.Yaw * Fall + Stagger.Yaw, F.Roll * Fall + Stagger.Roll);
		FRotator R = DeathStartRot;
		R.Pitch = FMath::Clamp(FMath::Lerp(DeathStartRot.Pitch, 0.f, Fall) + Target.Pitch - Bounce * 30.f, -88.f, 88.f);
		R.Yaw += Target.Yaw;
		R.Roll = Target.Roll;
		C->SetControlRotation(R);
	}
}

// ---------------------------------------------------------------- Vision nocturne

void ABodycamCharacter::ToggleNightVision()
{
	if (bDead || bPilotingDrone) return;
	switch (NVGState)
	{
	case ENVGState::Up:
	case ENVGState::Raising:
		if (NVGBattery > 1.f) NVGState = ENVGState::Lowering;
		break;
	case ENVGState::Down:
	case ENVGState::Lowering:
		NVGState = ENVGState::Raising;
		BodycamCamera->SetNightVision(false);
		IRIlluminator->SetVisibility(false);
		break;
	}
}

void ABodycamCharacter::UpdateNVG(float DeltaSeconds)
{
	constexpr float FlipTime = 0.55f;

	if (NVGState == ENVGState::Lowering)
	{
		NVGAlpha += DeltaSeconds / FlipTime;
		if (NVGAlpha >= 1.f)
		{
			NVGAlpha = 1.f;
			NVGState = ENVGState::Down;
			NVGWarm = 0.f;
			BodycamCamera->AddShake(0.25f);
			BodycamCamera->SetNightVision(true);
			IRIlluminator->SetVisibility(true);
		}
	}
	else if (NVGState == ENVGState::Raising)
	{
		NVGAlpha -= DeltaSeconds / FlipTime;
		if (NVGAlpha <= 0.f)
		{
			NVGAlpha = 0.f;
			NVGState = ENVGState::Up;
		}
	}

	if (NVGState == ENVGState::Down)
	{
		NVGBattery = FMath::Max(0.f, NVGBattery - NVGDrainPerSecond * DeltaSeconds);
		// Les tubes mettent un instant à monter en lumière (avec quelques sautes).
		NVGWarm = FMath::Min(1.f, NVGWarm + DeltaSeconds / 0.7f);
		float Power = FMath::Clamp(NVGWarm * 1.15f, 0.f, 1.f) * (NVGWarm < 0.7f && FMath::FRand() < 0.15f ? 0.55f : 1.f);
		// Batterie faible : l'image faiblit et clignote.
		if (NVGBattery < 15.f)
		{
			Power = FMath::Lerp(0.45f, 1.f, NVGBattery / 15.f) * (FMath::FRand() < 0.06f ? 0.25f : 1.f);
		}
		// Coupure automatique (autogating) : le flash de bouche sature les tubes.
		if (Weapon && Weapon->GetTimeSinceShot() < 0.07f)
		{
			Power *= 0.4f;
		}
		BodycamCamera->SetNVGPower(Power);

		if (NVGBattery <= 0.f)
		{
			NVGState = ENVGState::Raising;
			BodycamCamera->SetNightVision(false);
			IRIlluminator->SetVisibility(false);
			if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>())
			{
				GM->PushEvent(TEXT("BATTERIE JVN VIDE"));
			}
		}
	}

	// Les jumelles basculent du front jusque devant les yeux (courbe adoucie).
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, NVGAlpha, 2.f);
	NVGPivot->SetRelativeRotation(FRotator(FMath::Lerp(105.f, 0.f, Eased), 0.f, 0.f));
	NVGMesh->SetVisibility(NVGAlpha > 0.02f && NVGAlpha < 0.8f);
}

// ---------------------------------------------------------------- Drone

void ABodycamCharacter::SpawnDroneOnFloor()
{
	if (Drone) return;
	// Posé par terre devant les pieds (le drone se cale lui-même sur le sol).
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector Fwd = FRotator(0.f, GetActorRotation().Yaw, 0.f).Vector();
	const FVector Loc = GetActorLocation() + Fwd * 110.f;
	Drone = GetWorld()->SpawnActor<ABodycamDrone>(ABodycamDrone::StaticClass(), Loc, FRotator(0.f, GetActorRotation().Yaw, 0.f), Params);
	if (Drone) GetCapsuleComponent()->IgnoreActorWhenMoving(Drone, true);
}

void ABodycamCharacter::UpdateDroneDeploy(float DeltaSeconds)
{
	if (DeployTimer < 0.f)
	{
		// Retrait du casque : l'écran noir se dissipe.
		if (!bPilotingDrone) HeadsetFade = FMath::Max(0.f, HeadsetFade - DeltaSeconds * 2.5f);
		else HeadsetFade = FMath::Max(0.f, HeadsetFade - DeltaSeconds * 1.6f);   // connexion du flux vidéo
		return;
	}
	DeployTimer += DeltaSeconds;
	MoveInput = FVector2D::ZeroVector;

	// Nouveau drone : on regarde vers le sol, on le pose (0,55 s), puis on enfile le casque.
	const float PlaceTime = bDeployNew ? 0.6f : 0.f;
	const float HeadsetStart = bDeployNew ? 2.8f : 0.05f;   // ~2 s pour voir le drone posé au sol
	const float HeadsetEnd = HeadsetStart + 0.45f;
	if (bDeployNew)
	{
		if (AController* C = GetController())
		{
			FRotator R = C->GetControlRotation();
			const float A = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(DeployTimer / 0.6f, 0.f, 1.f), 2.f);
			R.Pitch = FMath::Lerp(DeployStartPitch, -48.f, A);
			C->SetControlRotation(R);
		}
		if (DeployTimer >= PlaceTime && !Drone) SpawnDroneOnFloor();
	}
	HeadsetFade = FMath::Clamp((DeployTimer - HeadsetStart) / (HeadsetEnd - HeadsetStart), 0.f, 1.f);
	if (DeployTimer >= HeadsetEnd + 0.15f)
	{
		DeployTimer = -1.f;
		HeadsetFade = 1.f;
		if (!Drone) SpawnDroneOnFloor();
		LaunchDrone();
	}
}

void ABodycamCharacter::LaunchDrone()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !Drone) return;

	bPilotingDrone = true;
	bPhoneOpen = false;
	MoveInput = FVector2D::ZeroVector;
	SetSprinting(false);
	if (Weapon) Weapon->StopFire();
	if (NVGState == ENVGState::Down || NVGState == ENVGState::Lowering) ToggleNightVision();
	Drone->SetPiloted(true);
	PC->SetViewTargetWithBlend(Drone, 0.f);   // derrière l'écran noir du casque
}

void ABodycamCharacter::StopPiloting()
{
	if (!bPilotingDrone) return;
	bPilotingDrone = false;
	if (Drone) Drone->SetPiloted(false);
	HeadsetFade = 1.f;   // on retire le casque : noir puis retour au corps
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetViewTargetWithBlend(this, 0.f);
	}
}

void ABodycamCharacter::UpdateDronePiloting()
{
	if (!bPilotingDrone || !Drone) return;
	Drone->SetVerticalInput((bJumpHeld ? 1.f : 0.f) - (bCrouchHeld ? 1.f : 0.f));
}

void ABodycamCharacter::OnDroneFinished(bool bDestroyed)
{
	StopPiloting();
	Drone = nullptr;
}

// ---------------------------------------------------------------- États

void ABodycamCharacter::UpdateSprintAndLean()
{
	const bool bOnGround = GetCharacterMovement()->IsMovingOnGround();
	const bool bWantsSprint = bSprintHeld && MoveInput.Y > 0.5f && !bExhausted && bOnGround && GrabMode == EZombieAttack::None && !bPilotingDrone;

	if (bWantsSprint && bIsCrouched)
	{
		UnCrouch();
	}
	SetSprinting(bWantsSprint && !bIsCrouched);

	float NewLean = 0.f;
	if (!bIsSprinting && bOnGround && GrabMode == EZombieAttack::None && !bPilotingDrone)
	{
		NewLean = (bLeanRightHeld ? 1.f : 0.f) - (bLeanLeftHeld ? 1.f : 0.f);
	}
	SetLean(NewLean);
}

void ABodycamCharacter::SetSprinting(bool bNewSprinting)
{
	if (bIsSprinting == bNewSprinting) return;
	bIsSprinting = bNewSprinting;
	if (!HasAuthority())
	{
		ServerSetSprinting(bNewSprinting);
	}
}

void ABodycamCharacter::SetLean(float NewLean)
{
	if (FMath::IsNearlyEqual(LeanTarget, NewLean)) return;
	LeanTarget = NewLean;
	if (!HasAuthority())
	{
		ServerSetLean(NewLean);
	}
}

void ABodycamCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting && !bIsCrouched && !bExhausted;
}

void ABodycamCharacter::ServerSetLean_Implementation(float NewLean)
{
	LeanTarget = FMath::Clamp(NewLean, -1.f, 1.f);
}

void ABodycamCharacter::UpdateStamina(float DeltaSeconds)
{
	const bool bDraining = bIsSprinting && GetVelocity().Size2D() > 10.f;
	if (bDraining)
	{
		Stamina -= SprintDrainPerSecond * (1.f + Infection * 0.6f) * DeltaSeconds;
		TimeSinceStaminaUse = 0.f;
		if (Stamina <= 0.f)
		{
			Stamina = 0.f;
			bExhausted = true;
		}
	}
	else
	{
		TimeSinceStaminaUse += DeltaSeconds;
		if (TimeSinceStaminaUse >= RegenDelay)
		{
			Stamina = FMath::Min(MaxStamina, Stamina + RegenPerSecond * (1.f - Infection * 0.5f) * DeltaSeconds);
		}
	}

	if (bExhausted && Stamina >= ExhaustedRecoverThreshold)
	{
		bExhausted = false;
	}
}

// ---------------------------------------------------------------- Caméra

void ABodycamCharacter::UpdateCameraHeight(float DeltaSeconds, bool bInstant)
{
	if (bDead) return;
	const float Target = bIsCrouched ? CameraCrouchHeight : CameraStandHeight;
	if (!bInstant)
	{
		CurrentCameraHeight = FMath::FInterpTo(CurrentCameraHeight, Target, DeltaSeconds, CameraHeightInterpSpeed);
	}
	BodycamCamera->SetRelativeLocation(FVector(CameraForwardOffset, 0.f, CurrentCameraHeight));
}

void ABodycamCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CurrentCameraHeight += ScaledHalfHeightAdjust;
	UpdateCameraHeight(0.f, true);
}

void ABodycamCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CurrentCameraHeight -= ScaledHalfHeightAdjust;
	UpdateCameraHeight(0.f, true);
}

void ABodycamCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	const float Impact = FMath::Clamp((LastFallSpeed - 250.f) / 700.f, 0.f, 1.f);
	BodycamCamera->AddLandingImpact(Impact);
	LastFallSpeed = 0.f;
}
