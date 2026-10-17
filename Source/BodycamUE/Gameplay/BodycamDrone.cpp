#include "Gameplay/BodycamDrone.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/BodycamGameMode.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/BodycamPickup.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"
#include "Particles/ParticleSystem.h"
#include "Player/BodycamCharacter.h"
#include "Sound/SoundBase.h"
#include "World/BodycamExterior.h"
#include "Gameplay/BodycamSounds.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogBodycamDrone, Log, All);

namespace
{
	const TCHAR* DroneSec = TEXT("Bodycam.Drone");
}

ABodycamDrone::ABodycamDrone()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(26.f);
	Collision->SetCollisionProfileName(TEXT("Pawn"));
	Collision->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	CarriedCrate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarriedCrate"));
	CarriedCrate->SetupAttachment(Collision);
	CarriedCrate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CarriedCrate->SetVisibility(false);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Collision);
	Camera->SetRelativeLocation(FVector(30.f, 0.f, -6.f));
	Camera->FieldOfView = 100.f;

	Lamp = CreateDefaultSubobject<USpotLightComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(Camera);
	Lamp->SetIntensityUnits(ELightUnits::Candelas);
	Lamp->SetIntensity(120.f);
	Lamp->SetAttenuationRadius(3000.f);
	Lamp->SetOuterConeAngle(45.f);
	Lamp->SetInnerConeAngle(20.f);
	Lamp->SetCastShadows(false);
	Lamp->SetVisibility(true);   // allumé dès qu'il est posé : on le voit par terre

	// Petite LED de statut (verte) pour repérer le drone dans le noir.
	StatusLed = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLed"));
	StatusLed->SetupAttachment(Collision);
	StatusLed->SetRelativeLocation(FVector(0.f, 0.f, 12.f));
	StatusLed->SetIntensityUnits(ELightUnits::Candelas);
	StatusLed->SetIntensity(6.f);
	StatusLed->SetAttenuationRadius(140.f);
	StatusLed->SetLightColor(FLinearColor(0.3f, 1.f, 0.4f));
	StatusLed->SetCastShadows(false);

	AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

void ABodycamDrone::BeginPlay()
{
	Super::BeginPlay();

	USkeletalMesh* SK = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Drone_Scavenger/SK/SK_Drone_Scavenger.SK_Drone_Scavenger"));
	if (!SK) SK = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Drone_Scavenger/SK/SKM_Drone_Scavenger.SKM_Drone_Scavenger"));
	Mesh->SetSkeletalMesh(SK);

	float DroneSize = 60.f;
	if (GConfig)
	{
		GConfig->GetRotator(DroneSec, TEXT("MeshRotation"), MeshBaseRotation, GGameIni);
		GConfig->GetVector(DroneSec, TEXT("MeshOffset"), MeshBaseOffset, GGameIni);
		GConfig->GetFloat(DroneSec, TEXT("MeshScale"), MeshScale, GGameIni);
		GConfig->GetFloat(DroneSec, TEXT("DroneSize"), DroneSize, GGameIni);
		GConfig->GetFloat(DroneSec, TEXT("CrashSpeed"), CrashSpeed, GGameIni);
		GConfig->GetFloat(DroneSec, TEXT("MaxSpeed"), MaxSpeed, GGameIni);
		GConfig->GetFloat(DroneSec, TEXT("ReturnSpeed"), ReturnSpeed, GGameIni);
		GConfig->GetFloat(DroneSec, TEXT("GrabRange"), GrabRange, GGameIni);
		GConfig->GetFloat(DroneSec, TEXT("GrabHeight"), GrabHeight, GGameIni);
	}

	// Taille réelle : MeshScale <= 0 = mise à l'échelle automatique (plus grande largeur = DroneSize cm).
	float Radius = 26.f;
	if (SK)
	{
		const FBoxSphereBounds B = SK->GetBounds();
		const float Width = FMath::Max(B.BoxExtent.X, B.BoxExtent.Y) * 2.f;
		if (MeshScale <= 0.f) MeshScale = Width > 1.f ? DroneSize / Width : 1.f;
		const FVector C = MeshBaseRotation.RotateVector(B.Origin * MeshScale);
		if (MeshBaseOffset.IsNearlyZero()) MeshBaseOffset = FVector(-C.X, -C.Y, -C.Z);
		Radius = FMath::Clamp(Width * MeshScale * 0.38f, 18.f, 60.f);
		const float Bottom = MeshBaseOffset.Z + C.Z - B.BoxExtent.Z * MeshScale;   // sous le centre de l'acteur
		LandHeight = FMath::Max(Radius, -Bottom);
		UE_LOG(LogBodycamDrone, Log, TEXT("Drone : modele %.0f x %.0f x %.0f, echelle %.3f, rayon %.0f, hauteur au sol %.0f"),
			B.BoxExtent.X * 2, B.BoxExtent.Y * 2, B.BoxExtent.Z * 2, MeshScale, Radius, LandHeight);
	}
	Collision->SetSphereRadius(Radius);
	Mesh->SetRelativeLocationAndRotation(MeshBaseOffset, MeshBaseRotation);
	Mesh->SetRelativeScale3D(FVector(MeshScale));
	Camera->SetRelativeLocation(FVector(Radius * 0.9f, 0.f, -4.f));

	AnimFlight = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Drone_Scavenger/Animations/Anim_Flight.Anim_Flight"));
	AnimOpen = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Drone_Scavenger/Animations/Anim_Opening.Anim_Opening"));
	AnimClose = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Drone_Scavenger/Animations/Anim_Closing.Anim_Closing"));
	if (AnimFlight)
	{
		Mesh->PlayAnimation(AnimFlight, true);
		Mesh->SetPlayRate(0.f);   // posé : hélices à l'arrêt
	}

	// Caisse accrochée sous la pince (même modèle que la caisse au sol).
	float CS = 1.f;
	if (UStaticMesh* CM = ABodycamPickup::LoadAmmoCrateMesh(CS))
	{
		CarriedCrate->SetStaticMesh(CM);
		CarriedCrate->SetRelativeScale3D(FVector(CS));
		const FBoxSphereBounds B = CM->GetBounds();
		CarriedCrate->SetRelativeLocation(FVector(-B.Origin.X * CS, -B.Origin.Y * CS, -LandHeight * 0.8f - (B.Origin.Z + B.BoxExtent.Z) * CS));
	}

	// Retour vidéo du casque FPV : monochrome + lignes + grain.
	if (UMaterialInterface* PP = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/PP/M_PP_Drone.M_PP_Drone")))
	{
		FeedMID = UMaterialInstanceDynamic::Create(PP, this);
		Camera->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, FeedMID));
	}
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->PostProcessSettings.AutoExposureBias = 2.5f;
	Camera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	Camera->PostProcessSettings.AutoExposureMinBrightness = -6.f;
	Camera->PostProcessSettings.bOverride_FilmGrainIntensity = true;
	Camera->PostProcessSettings.FilmGrainIntensity = 0.5f;
	Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
	Camera->PostProcessSettings.VignetteIntensity = 0.9f;

	if (AActor* O = GetOwner()) Collision->IgnoreActorWhenMoving(O, true);

	for (TActorIterator<ABodycamExterior> It(GetWorld()); It; ++It) { Exterior = *It; break; }

	Yaw = GetActorRotation().Yaw;
	SnapToFloor();
}

void ABodycamDrone::SnapToFloor()
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DroneLand), false, this);
	if (GetOwner()) Params.AddIgnoredActor(GetOwner());
	const FVector L = GetActorLocation();
	if (GetWorld()->LineTraceSingleByChannel(Hit, L + FVector(0, 0, 60.f), L - FVector(0, 0, 600.f), ECC_Visibility, Params))
	{
		SetActorLocation(FVector(L.X, L.Y, Hit.ImpactPoint.Z + LandHeight + 1.f));
	}
}

void ABodycamDrone::AddLookInput(const FVector2D& Delta)
{
	if (State == EDroneState::AutoReturn) { CamPitch = FMath::Clamp(CamPitch - Delta.Y, -70.f, 25.f); return; }
	Yaw += Delta.X;
	CamPitch = FMath::Clamp(CamPitch - Delta.Y, -70.f, 25.f);
}

void ABodycamDrone::SetPiloted(bool bInPiloted)
{
	bPiloted = bInPiloted;
	if (!bPiloted)
	{
		ClearInput();
		return;
	}
	if (State == EDroneState::Landed)
	{
		State = EDroneState::TakingOff;
		TakeOffZ = GetActorLocation().Z + 110.f;
		Mesh->SetPlayRate(1.f);
		Lamp->SetVisibility(true);
		CamPitch = -5.f;
		// Bruit des hélices (en boucle, suit le drone).
		if (!RotorSound) RotorSound = FBodycamSounds::PlayAttached(FBodycamSounds::Get(TEXT("Drone_Loop")), Collision, 0.55f, 0.85f, 300.f, 2500.f);
		SetHint(TEXT("DÉCOLLAGE"));
	}
}

void ABodycamDrone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bExploded) return;
	HintTime = FMath::Max(0.f, HintTime - DeltaSeconds);

	switch (State)
	{
	case EDroneState::Landed: return;
	case EDroneState::TakingOff: UpdateTakeOff(DeltaSeconds); break;
	case EDroneState::Flying: UpdateManual(DeltaSeconds); break;
	case EDroneState::AutoReturn: UpdateAutoReturn(DeltaSeconds); break;
	}
	if (bExploded || IsActorBeingDestroyed()) return;
	UpdateGrab(DeltaSeconds);

	// Hauteur au-dessus du sol (affichée sur le retour vidéo).
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DroneAlt), false, this);
	Altitude = GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), GetActorLocation() - FVector(0, 0, 5000), ECC_Visibility, Params)
		? FMath::Max(0.f, Hit.Distance - LandHeight) : 5000.f;

	if (CloseAnimTimer >= 0.f)
	{
		CloseAnimTimer -= DeltaSeconds;
		if (CloseAnimTimer < 0.f && AnimFlight) Mesh->PlayAnimation(AnimFlight, true);
	}
}

void ABodycamDrone::Move(float DeltaSeconds, const FVector& Desired, bool bCanCrash)
{
	// Inertie : le drone accélère et freine progressivement.
	Velocity = FMath::VInterpTo(Velocity, Desired, DeltaSeconds, 2.4f);

	// Dans l'ouverture d'une fenêtre : on passe sans toucher le cadre.
	const FVector Delta = Velocity * DeltaSeconds;
	const FVector L = GetActorLocation();
	const bool bInOpening = Exterior && (Exterior->IsInOpening(L + Delta, 8.f) || Exterior->IsInOpening(L, 8.f));
	FHitResult Hit;
	if (bInOpening) SetActorLocation(L + Delta, false);
	else AddActorWorldOffset(Delta, true, &Hit);
	if (Hit.bBlockingHit)
	{
		const float Impact = -FVector::DotProduct(Velocity, Hit.ImpactNormal);
		// Sol : on peut s'y poser doucement. Mur / objet : au-delà de CrashSpeed, il explose.
		const float Limit = Hit.ImpactNormal.Z > 0.7f ? CrashSpeed * 2.2f : CrashSpeed;
		if (bCanCrash && Impact > Limit)
		{
			const UPrimitiveComponent* HC = Hit.GetComponent();
			const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(HC);
			UE_LOG(LogBodycamDrone, Log, TEXT("Choc %.0f cm/s contre %s (%s) a (%.0f,%.0f,%.0f) : explosion"), Impact,
				Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("?"),
				SMC && SMC->GetStaticMesh() ? *SMC->GetStaticMesh()->GetName() : (HC ? *HC->GetName() : TEXT("?")),
				Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
			Explode();
			return;
		}
		Velocity = FVector::VectorPlaneProject(Velocity, Hit.ImpactNormal) * 0.6f + Hit.ImpactNormal * FMath::Max(0.f, Impact) * 0.25f;
	}

	const FRotator YawRot(0.f, Yaw, 0.f);
	SetActorRotation(YawRot);
	// Inclinaison visuelle selon la vitesse (le corps penche dans le sens du vol).
	const FVector LocalVel = YawRot.UnrotateVector(Velocity);
	const FRotator Tilt(-LocalVel.X / MaxSpeed * 18.f, 0.f, LocalVel.Y / MaxSpeed * 18.f);
	Mesh->SetRelativeRotation(MeshBaseRotation + Tilt);
	Camera->SetRelativeRotation(FRotator(CamPitch, 0.f, Tilt.Roll * 0.3f));
	Mesh->SetPlayRate(GrabTimer >= 0.f || CloseAnimTimer >= 0.f ? 1.f : 1.f + Velocity.Size() / MaxSpeed);
	if (RotorSound) RotorSound->SetPitchMultiplier(0.85f + 0.35f * FMath::Clamp(Velocity.Size() / MaxSpeed, 0.f, 1.f) + FMath::Abs(VerticalInput) * 0.08f);
}

void ABodycamDrone::UpdateTakeOff(float DeltaSeconds)
{
	const float Dz = TakeOffZ - GetActorLocation().Z;
	Move(DeltaSeconds, FVector(0, 0, FMath::Clamp(Dz * 3.f, 0.f, 220.f)), false);
	if (Dz < 8.f || (Velocity.Z < 5.f && Dz < 40.f))
	{
		State = EDroneState::Flying;
		Path.Reset();
		Path.Add(GetActorLocation());
	}
}

void ABodycamDrone::UpdateManual(float DeltaSeconds)
{
	const FRotator YawRot(0.f, Yaw, 0.f);
	const FVector Fwd = YawRot.Vector();
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	const FVector Desired = bPiloted
		? (Fwd * MoveInput.Y + Right * MoveInput.X).GetClampedToMaxSize(1.f) * MaxSpeed + FVector::UpVector * VerticalInput * 420.f
		: FVector::ZeroVector;   // casque retiré : vol stationnaire
	Move(DeltaSeconds, Desired, true);
	if (!bExploded) RecordPath();
}

void ABodycamDrone::RecordPath()
{
	const FVector L = GetActorLocation();
	if (Path.Num() == 0 || FVector::Dist(Path.Last(), L) > 120.f) Path.Add(L);
}

float ABodycamDrone::GetReturnDistance() const
{
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	return P ? FVector::Dist(P->GetActorLocation(), GetActorLocation()) : 0.f;
}

void ABodycamDrone::StartAutoReturn()
{
	if (State != EDroneState::Flying) return;
	if (!bCarrying)
	{
		SetHint(TEXT("ATTRAPEZ D'ABORD LA CAISSE  [F]"));
		return;
	}
	State = EDroneState::AutoReturn;
	RecordPath();
	ReturnIndex = Path.Num() - 1;
	SetHint(TEXT("RETOUR AUTOMATIQUE ENGAGÉ"));
}

void ABodycamDrone::UpdateAutoReturn(float DeltaSeconds)
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector L = GetActorLocation();

	FVector Target;
	const bool bFinal = ReturnIndex < 0;
	if (bFinal)
	{
		if (!Player) { Deliver(); return; }
		Target = Player->GetActorLocation() + Player->GetActorForwardVector() * 110.f + FVector(0, 0, 40.f);
	}
	else
	{
		// Raccourci : on vise le point le plus ancien encore visible en ligne droite (évite de refaire les boucles).
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DroneReturn), false, this);
		if (Player) Params.AddIgnoredActor(Player);
		const FCollisionShape Sphere = FCollisionShape::MakeSphere(Collision->GetScaledSphereRadius() * 0.9f);
		for (int32 i = FMath::Max(0, ReturnIndex - 10); i < ReturnIndex; ++i)
		{
			if (!GetWorld()->SweepTestByChannel(L, Path[i], FQuat::Identity, ECC_Pawn, Sphere, Params))
			{
				ReturnIndex = i;
				break;
			}
		}
		Target = Path[ReturnIndex];
	}

	const FVector To = Target - L;
	const float Dist = To.Size();
	if (bFinal && Dist < 140.f)
	{
		Deliver();
		return;
	}
	if (!bFinal && Dist < 70.f)
	{
		--ReturnIndex;
		StuckTime = 0.f;
		return;
	}
	if (To.Size2D() > 30.f) Yaw = FMath::FixedTurn(Yaw, To.Rotation().Yaw, 220.f * DeltaSeconds);
	Move(DeltaSeconds, To.GetSafeNormal() * FMath::Min(ReturnSpeed, Dist * 2.5f + 80.f), false);

	// Bloqué (joueur parti ailleurs, zombie sur le chemin...) : on passe au point suivant, puis on livre quand même.
	StuckTime = Velocity.Size() < 40.f ? StuckTime + DeltaSeconds : 0.f;
	if (StuckTime > 2.5f)
	{
		StuckTime = 0.f;
		if (bFinal) Deliver();
		else --ReturnIndex;
	}
}

void ABodycamDrone::Deliver()
{
	if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>()) GM->OnAmmoDelivered();
	if (ABodycamCharacter* Player = Cast<ABodycamCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		Player->OnDroneFinished(false);
	}
	Destroy();
}

bool ABodycamDrone::GetExitMarker(FVector& Out, bool& bBroken) const
{
	return Exterior && Exterior->IsInside(GetActorLocation()) && Exterior->FindNearestOpening(GetActorLocation(), Out, bBroken);
}

bool ABodycamDrone::CanGrabNow() const
{
	if (bCarrying || State == EDroneState::Landed) return false;
	const ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>();
	const ABodycamPickup* Crate = GM ? GM->GetSupplyCrate() : nullptr;
	if (!Crate) return false;
	const FVector D = GetActorLocation() - Crate->GetActorLocation();
	return D.Size2D() < GrabRange && D.Z > 0.f && D.Z < GrabHeight;
}

void ABodycamDrone::TryGrab()
{
	if (State == EDroneState::Landed || State == EDroneState::TakingOff || GrabTimer >= 0.f) return;
	if (bCarrying)
	{
		SetHint(TEXT("CAISSE DÉJÀ ACCROCHÉE  ·  [R] RETOUR AUTOMATIQUE"));
		return;
	}
	GrabTimer = 0.f;
	bGrabCaught = CanGrabNow();
	CloseAnimTimer = -1.f;
	if (AnimOpen) Mesh->PlayAnimation(AnimOpen, false);
	SetHint(TEXT("PINCE"));
}

void ABodycamDrone::UpdateGrab(float DeltaSeconds)
{
	if (GrabTimer < 0.f) return;
	GrabTimer += DeltaSeconds;
	const float OpenTime = AnimOpen ? FMath::Clamp(AnimOpen->GetPlayLength(), 0.3f, 1.2f) : 0.6f;
	if (GrabTimer < OpenTime) return;

	GrabTimer = -1.f;
	if (bGrabCaught && CanGrabNow())
	{
		bCarrying = true;
		CarriedCrate->SetVisibility(true);
		if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>()) GM->OnCratePickedUp();
		SetHint(TEXT("CAISSE ATTRAPÉE  ·  [R] RETOUR AUTOMATIQUE"));
	}
	else
	{
		SetHint(TEXT("RIEN SOUS LA PINCE  ·  PLACEZ-VOUS AU-DESSUS DE LA CAISSE"));
	}
	if (AnimClose)
	{
		Mesh->PlayAnimation(AnimClose, false);
		CloseAnimTimer = AnimClose->GetPlayLength();
	}
	else if (AnimFlight)
	{
		Mesh->PlayAnimation(AnimFlight, true);
	}
}

void ABodycamDrone::Explode()
{
	if (bExploded) return;
	bExploded = true;

	if (UParticleSystem* FX = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/StarterContent/Particles/P_Explosion.P_Explosion")))
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FX, GetActorLocation(), FRotator::ZeroRotator, FVector(0.5f));
	}
	if (RotorSound) RotorSound->Stop();
	if (USoundBase* Boom = FBodycamSounds::Get(TEXT("Drone_Explosion")))
	{
		// Explosion + débris métalliques qui retombent. En 2D : on l'entend dans le casque.
		FBodycamSounds::Play2D(this, Boom, 1.f);
		FBodycamSounds::Play2D(this, FBodycamSounds::Get(TEXT("Metal_Groan")), 0.6f, 1.3f);
	}
	else if (USoundBase* Snd = LoadObject<USoundBase>(nullptr, TEXT("/Game/StarterContent/Audio/Explosion01.Explosion01")))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Snd, GetActorLocation(), 0.8f);
	}

	if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>())
	{
		GM->OnDroneDestroyed(bCarrying);
	}
	if (ABodycamCharacter* Player = Cast<ABodycamCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		Player->OnDroneFinished(true);
	}
	Destroy();
}
