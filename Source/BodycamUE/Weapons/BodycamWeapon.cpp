#include "Weapons/BodycamWeapon.h"

#include "Animation/AnimSequence.h"
#include "Camera/BodycamCameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Gameplay/BodycamImpactFX.h"
#include "World/BodycamExterior.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Particles/ParticleSystem.h"
#include "Player/BodycamCharacter.h"
#include "ReferenceSkeleton.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogBodycamWeapon, Log, All);

namespace
{
	// Canal de trace "Weapon" (DefaultEngine.ini).
	constexpr ECollisionChannel WeaponChannel = ECC_GameTraceChannel2;
}

ABodycamWeapon::ABodycamWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	RootComponent = Pivot;

	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMesh->SetupAttachment(Pivot);
	StaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMesh->CastShadow = false;

	SkelMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkelMesh"));
	SkelMesh->SetupAttachment(Pivot);
	SkelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkelMesh->CastShadow = false;

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(Pivot);
	MuzzleLight->SetIntensityUnits(ELightUnits::Candelas);
	MuzzleLight->SetIntensity(0.f);
	MuzzleLight->SetAttenuationRadius(900.f);
	MuzzleLight->SetLightColor(FLinearColor(1.f, 0.72f, 0.4f));
	MuzzleLight->SetCastShadows(false);

	// Lampe torche (touche L) : projecteur étroit, sans ombres (économie sur carte graphique intégrée).
	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(Pivot);
	Flashlight->SetMobility(EComponentMobility::Movable);
	Flashlight->SetIntensityUnits(ELightUnits::Candelas);
	Flashlight->SetIntensity(TorchIntensity);
	Flashlight->SetAttenuationRadius(3500.f);
	Flashlight->SetOuterConeAngle(26.f);
	Flashlight->SetInnerConeAngle(7.f);
	Flashlight->SetLightColor(FLinearColor(1.f, 0.93f, 0.8f));
	Flashlight->SetCastShadows(false);
	Flashlight->SetVisibility(false);

	// Flash de bouche : une étoile face à la caméra + une flamme le long du canon.
	FlashFront = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashFront"));
	FlashFront->SetupAttachment(MuzzleLight);
	FlashFront->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlashFront->CastShadow = false;
	FlashFront->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	FlashFront->SetRelativeScale3D(FVector(0.16f));
	FlashFront->SetVisibility(false);

	FlashSide = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashSide"));
	FlashSide->SetupAttachment(MuzzleLight);
	FlashSide->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlashSide->CastShadow = false;
	FlashSide->SetRelativeLocationAndRotation(FVector(10.f, 0.f, 0.f), FRotator(0.f, 0.f, 90.f));
	FlashSide->SetRelativeScale3D(FVector(0.22f, 0.08f, 1.f));
	FlashSide->SetVisibility(false);

	// Deuxième flamme, à plat : avec la verticale elle forme une croix visible sous tous les angles.
	FlashSide2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashSide2"));
	FlashSide2->SetupAttachment(MuzzleLight);
	FlashSide2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlashSide2->CastShadow = false;
	FlashSide2->SetVisibility(false);

	SightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SightMesh"));
	SightMesh->SetupAttachment(Pivot);
	SightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SightMesh->CastShadow = false;
	SightMesh->SetVisibility(false);

	// Point rouge du viseur (visible en visée seulement).
	Reticle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Reticle"));
	Reticle->SetupAttachment(Pivot);
	Reticle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Reticle->CastShadow = false;
	Reticle->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		FlashFront->SetStaticMesh(PlaneMesh.Object);
		FlashSide->SetStaticMesh(PlaneMesh.Object);
		FlashSide2->SetStaticMesh(PlaneMesh.Object);
		Reticle->SetStaticMesh(PlaneMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> FireSnd(TEXT("/Game/FPWeapon/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"));
	if (FireSnd.Succeeded()) FireSound = FireSnd.Object;
	static ConstructorHelpers::FObjectFinder<UParticleSystem> Sparks(TEXT("/Game/StarterContent/Particles/P_Sparks.P_Sparks"));
	if (Sparks.Succeeded()) ImpactFX = Sparks.Object;
}

void ABodycamWeapon::LoadTuning(const TCHAR* Section)
{
	if (!GConfig) return;
	GConfig->GetVector(Section, TEXT("HoldOffset"), HoldOffset, GGameIni);
	GConfig->GetRotator(Section, TEXT("MeshRotation"), MeshRotation, GGameIni);
	GConfig->GetVector(Section, TEXT("MeshOffset"), MeshOffset, GGameIni);
	GConfig->GetFloat(Section, TEXT("MeshScale"), MeshScale, GGameIni);
	GConfig->GetVector(Section, TEXT("MuzzleOffset"), MuzzleOffset, GGameIni);
	GConfig->GetFloat(Section, TEXT("Damage"), Damage, GGameIni);
	GConfig->GetFloat(Section, TEXT("SpreadDeg"), SpreadDeg, GGameIni);
	GConfig->GetFloat(Section, TEXT("RecoilPitch"), RecoilPitch, GGameIni);
}

// ------------------------------------------------------------------ Rig FPS (bras + arme + animations)

bool ABodycamWeapon::SetupRig(const TCHAR* MeshPath, const TCHAR* AnimDir, const TCHAR* AnimPrefix,
	const TCHAR* ReloadEmptyName, const TCHAR* RefBone, const TCHAR* TuningSection, const TCHAR* CamBone, float ExpectedDist, bool bTubeRig)
{
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, MeshPath);
	if (!Mesh)
	{
		UE_LOG(LogBodycamWeapon, Warning, TEXT("Rig FPS introuvable (%s) : ancien modele utilise. Lance Importer_Armes_FPS.bat."), MeshPath);
		return false;
	}

	auto LoadAnim = [&](const TCHAR* Name) -> UAnimSequence*
	{
		const FString P = FString::Printf(TEXT("%s/%s_Anim_%s.%s_Anim_%s"), AnimDir, AnimPrefix, Name, AnimPrefix, Name);
		UAnimSequence* A = LoadObject<UAnimSequence>(nullptr, *P);
		if (!A) UE_LOG(LogBodycamWeapon, Warning, TEXT("Animation introuvable : %s"), *P);
		return A;
	};
	bTube = bTubeRig;
	AnimFire = LoadAnim(TEXT("Fire"));
	AnimDraw = LoadAnim(TEXT("Draw"));
	AnimHolster = LoadAnim(TEXT("Holster"));
	if (bTube)
	{
		// Pompe : ouverture, une animation par cartouche, fermeture. Pas d'animation de repos (on garde la dernière pose).
		AnimReloadStart = LoadAnim(TEXT("ReloadStart"));
		AnimReload = LoadAnim(TEXT("ReloadLoop"));
		AnimReloadEnd = LoadAnim(TEXT("ReloadEnd"));
	}
	else
	{
		AnimReload = LoadAnim(TEXT("Reload"));
		AnimReloadEmpty = LoadAnim(ReloadEmptyName);
		AnimIdle = LoadAnim(TEXT("Idle"));
	}

	// Réglages (DefaultGame.ini) : décalage / rotation / échelle du rig, bouche du canon, lampe.
	if (GConfig)
	{
		GConfig->GetVector(TuningSection, TEXT("RigOffset"), RigOffset, GGameIni);
		GConfig->GetRotator(TuningSection, TEXT("RigRotation"), RigRotation, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("RigScale"), RigScale, GGameIni);
		GConfig->GetVector(TuningSection, TEXT("MuzzleOffset"), MuzzleOffset, GGameIni);
		GConfig->GetVector(TuningSection, TEXT("TorchOffset"), TorchOffset, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("TorchIntensity"), TorchIntensity, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("ReloadAnimRate"), ReloadAnimRate, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("DrawAnimRate"), DrawAnimRate, GGameIni);
		GConfig->GetVector(TuningSection, TEXT("EjectOffset"), EjectOffset, GGameIni);
		GConfig->GetVector(TuningSection, TEXT("AimOffset"), AimOffset, GGameIni);
		GConfig->GetRotator(TuningSection, TEXT("AimRotation"), AimRotation, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("AimFOV"), AimFOV, GGameIni);
		GConfig->GetBool(TuningSection, TEXT("RedDot"), bRedDot, GGameIni);
		GConfig->GetBool(TuningSection, TEXT("Sight"), bSight, GGameIni);
		GConfig->GetVector(TuningSection, TEXT("SightOffset"), SightOffset, GGameIni);
		GConfig->GetRotator(TuningSection, TEXT("SightRotation"), SightRotation, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("SightScale"), SightScale, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("ReticleDist"), ReticleDist, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("KickPitch"), KickPitchDeg, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("Damage"), Damage, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("SpreadDeg"), SpreadDeg, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("RecoilPitch"), RecoilPitch, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("RecoilYaw"), RecoilYaw, GGameIni);
		GConfig->GetFloat(TuningSection, TEXT("FireInterval"), FireInterval, GGameIni);
	}

	SkelMesh->SetSkeletalMesh(Mesh);
	SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SkelMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	SkelMesh->SetCastShadow(false);
	SkelMesh->SetVisibility(true);
	StaticMesh->SetVisibility(false);

	WeaponBone = FName(RefBone);
	AlignRig(FName(CamBone), FName(RefBone), ExpectedDist);
	if (bSight) MountSight();
	return true;
}

void ABodycamWeapon::AlignRig(const FName& CamBone, const FName& RefBone, float ExpectedDist)
{
	// Le rig est modélisé "à l'œil" : l'os FPS_Camera_j est l'œil du joueur. On pose cet os sur la caméra du jeu
	// et on tourne le rig par pas de 90° pour que l'arme (os de référence) soit devant la caméra, quel que soit
	// le sens des axes choisi par l'import.
	const USkeletalMesh* SM = SkelMesh->GetSkeletalMeshAsset();
	if (!SM) return;
	const FReferenceSkeleton& Ref = SM->GetRefSkeleton();
	const TArray<FTransform>& Pose = Ref.GetRefBonePose();

	auto CompPos = [&](const FName& Name, FVector& Out) -> bool
	{
		const int32 I = Ref.FindBoneIndex(Name);
		if (I == INDEX_NONE) return false;
		FTransform T = Pose[I];
		for (int32 P = Ref.GetParentIndex(I); P != INDEX_NONE; P = Ref.GetParentIndex(P))
		{
			T = T * Pose[P];
		}
		Out = T.GetLocation();
		return true;
	};

	FVector Cam = FVector::ZeroVector, Weap = FVector::ZeroVector;
	float AlignYaw = 0.f;
	float AutoScale = 1.f;
	if (CompPos(CamBone, Cam) && CompPos(RefBone, Weap))
	{
		const FVector D = Weap - Cam;
		// Garde-fou d'échelle : distance œil -> arme attendue (cm). Si l'import a changé l'échelle, on la corrige.
		if (ExpectedDist > 0.f && D.Size() > KINDA_SMALL_NUMBER)
		{
			const float Ratio = ExpectedDist / D.Size();
			if (Ratio < 0.8f || Ratio > 1.25f)
			{
				AutoScale = Ratio;
				UE_LOG(LogBodycamWeapon, Warning, TEXT("Rig %s : echelle corrigee x%.3f (distance mesuree %.1f cm, attendue %.1f cm)"), *SM->GetName(), AutoScale, D.Size(), ExpectedDist);
			}
		}
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
		AlignYaw = -FMath::RoundToFloat(Yaw / 90.f) * 90.f;
		UE_LOG(LogBodycamWeapon, Log, TEXT("Rig %s : camera=%s arme=%s ecart=%s -> rotation %.0f deg"),
			*SM->GetName(), *Cam.ToString(), *Weap.ToString(), *D.ToString(), AlignYaw);
		if (FMath::Abs(D.Z) > FMath::Max(FMath::Abs(D.X), FMath::Abs(D.Y)))
		{
			UE_LOG(LogBodycamWeapon, Warning, TEXT("Rig %s : l'arme est devant la camera sur l'axe vertical, orientation a regler avec RigRotation."), *SM->GetName());
		}
	}
	else
	{
		UE_LOG(LogBodycamWeapon, Warning, TEXT("Rig %s : os %s ou %s introuvable."), *SM->GetName(), *CamBone.ToString(), *RefBone.ToString());
	}

	const FRotator Rot(RigRotation.Pitch, AlignYaw + RigRotation.Yaw, RigRotation.Roll);
	const float Scale = RigScale * AutoScale;
	const FVector Loc = -Rot.RotateVector(Cam * Scale) + RigOffset;
	SkelMesh->SetRelativeLocationAndRotation(Loc, Rot);
	SkelMesh->SetRelativeScale3D(FVector(Scale));
}

bool ABodycamWeapon::RefBoneTransform(const FName& Bone, FTransform& Out) const
{
	const USkeletalMesh* SM = SkelMesh->GetSkeletalMeshAsset();
	if (!SM) return false;
	const FReferenceSkeleton& Ref = SM->GetRefSkeleton();
	const int32 I = Ref.FindBoneIndex(Bone);
	if (I == INDEX_NONE) return false;
	const TArray<FTransform>& Pose = Ref.GetRefBonePose();
	FTransform T = Pose[I];
	for (int32 P = Ref.GetParentIndex(I); P != INDEX_NONE; P = Ref.GetParentIndex(P))
	{
		T = T * Pose[P];
	}
	Out = T;
	return true;
}

void ABodycamWeapon::MountSight()
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Bodycam/Weapons/RedDot/SM_RedDot.SM_RedDot"));
	if (!Mesh)
	{
		UE_LOG(LogBodycamWeapon, Warning, TEXT("Viseur SM_RedDot introuvable : lance Corriger_Armes.bat"));
		bSight = false;
		return;
	}
	SightMesh->SetStaticMesh(Mesh);
	SightMesh->SetVisibility(true);

	// Position voulue dans le repère caméra (arme au repos), puis exprimée par rapport à l'os du corps de l'arme
	// pour que le viseur suive l'arme pendant les animations (rechargement...).
	const FTransform Desired(SightRotation, SightOffset, FVector(SightScale));
	FTransform Bone;
	if (RefBoneTransform(TEXT("Main_j"), Bone))
	{
		const FTransform MeshRel = SkelMesh->GetRelativeTransform();
		const FTransform Rel = Desired * MeshRel.Inverse() * Bone.Inverse();
		SightMesh->AttachToComponent(SkelMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Main_j"));
		SightMesh->SetRelativeTransform(Rel);
	}
	else
	{
		SightMesh->SetRelativeTransform(Desired);
	}
	// En visée, le centre optique du viseur vient exactement dans l'axe de la caméra.
	AimOffset = FVector(AimOffset.X, -SightOffset.Y, -SightOffset.Z);
}

void ABodycamWeapon::PlayRigAnim(UAnimSequence* Anim, float Rate, bool bLoop)
{
	if (!bRig || !Anim) return;
	SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SkelMesh->PlayAnimation(Anim, bLoop);
	SkelMesh->SetPlayRate(Rate);
	SkelMesh->SetPosition(0.f, false);
	AnimTimeLeft = bLoop ? 0.f : Anim->GetPlayLength() / FMath::Max(Rate, 0.01f);
	bIdlePlaying = bLoop;
}

// ------------------------------------------------------------------ Init

void ABodycamWeapon::InitWeapon(EBodycamWeaponType InType)
{
	Type = InType;
	const TCHAR* MeshPath = nullptr;
	const TCHAR* Section = nullptr;
	bool bSkeletal = false;
	bRig = false;

	switch (Type)
	{
	case EBodycamWeaponType::Pistol:
		DisplayName = TEXT("GLOCK 17  9MM");
		Damage = 34.f; Pellets = 1; SpreadDeg = 0.7f; FireInterval = 0.14f; bAutomatic = false;
		MagSize = 17; ReloadTime = 1.6f; SoundVolume = 0.8f;
		// Recul réel d'un 9 mm : fort relèvement, peu de dérive, le tireur ramène le canon entre deux coups.
		RecoilPitch = 3.0f; RecoilYaw = 0.7f; RecoilRamp = 0.f; RecoveryFraction = 0.65f; RecoveryRate = 12.f;
		KickBack = 4.5f; KickPitchDeg = 10.f; ShakeOnFire = 0.15f;
		HoldOffset = FVector(36.f, 13.f, -17.f); MuzzleOffset = FVector(38.f, 6.f, -4.f);
		EjectOffset = FVector(26.f, 7.f, -3.f); ShellSize = FVector2D(0.95f, 1.9f);
		AimOffset = FVector(-3.f, -6.1f, 1.5f); AimFOV = 80.f;
		MeshPath = TEXT("/Game/Bodycam/Weapons/Pistol/SM_Pistol.SM_Pistol");
		Section = TEXT("Bodycam.Weapon.Pistol");
		bRig = SetupRig(TEXT("/Game/Bodycam/Weapons/Glock17/SK_Glock17.SK_Glock17"), TEXT("/Game/Bodycam/Weapons/Glock17"),
			TEXT("glock17"), TEXT("ReloadEmpty"), TEXT("Slide_j"), TEXT("Bodycam.Rig.Pistol"), TEXT("FPS_Camera_j"), 29.7f);
		break;
	case EBodycamWeaponType::Shotgun:
		DisplayName = TEXT("FUSIL A POMPE M12");
		Damage = 17.f; Pellets = 9; SpreadDeg = 4.5f; FireInterval = 0.85f; bAutomatic = false;
		MagSize = 6; ReloadTime = 3.f; SoundVolume = 1.2f;
		RecoilPitch = 7.f; RecoilYaw = 1.6f; RecoilRamp = 0.f; RecoveryFraction = 0.6f; RecoveryRate = 10.f;
		KickBack = 9.f; KickPitchDeg = 15.f; ShakeOnFire = 0.45f;
		HoldOffset = FVector(30.f, 15.f, -20.f); MuzzleOffset = FVector(75.f, 0.f, 4.f);
		EjectOffset = FVector(34.f, 7.f, -7.f); ShellSize = FVector2D(2.f, 6.5f);
		AimOffset = FVector(-4.f, -2.2f, 1.5f); AimFOV = 78.f;
		MeshPath = TEXT("/Game/Bodycam/Weapons/Shotgun/SK_Shotgun.SK_Shotgun");
		Section = TEXT("Bodycam.Weapon.Shotgun");
		bSkeletal = true;
		bRig = SetupRig(TEXT("/Game/Bodycam/Weapons/Shotgun12/SK_Shotgun12.SK_Shotgun12"), TEXT("/Game/Bodycam/Weapons/Shotgun12"),
			TEXT("SG"), TEXT(""), TEXT("Pump"), TEXT("Bodycam.Rig.Shotgun"), TEXT("camera"), 46.f, true);
		break;
	default:
		DisplayName = TEXT("M4A1  5.56");
		Damage = 29.f; Pellets = 1; SpreadDeg = 1.0f; FireInterval = 0.085f; bAutomatic = true;
		MagSize = 30; ReloadTime = 2.4f; SoundVolume = 1.f;
		// Recul réel d'un fusil 5.56 : poussée régulière vers le haut qui s'accentue en rafale, dérive latérale.
		RecoilPitch = 0.95f; RecoilYaw = 0.55f; RecoilRamp = 0.05f; RecoveryFraction = 0.5f; RecoveryRate = 6.f;
		KickBack = 3.f; KickPitchDeg = 3.5f; ShakeOnFire = 0.03f;
		HoldOffset = FVector(28.f, 15.f, -19.f); MuzzleOffset = FVector(72.f, 7.f, -9.f);
		EjectOffset = FVector(28.f, 7.f, -7.f); ShellSize = FVector2D(0.9f, 4.5f);
		AimOffset = FVector(-3.f, -6.8f, 0.8f); AimFOV = 70.f;
		MeshPath = TEXT("/Game/Bodycam/Weapons/AK47/SM_AK47.SM_AK47");
		Section = TEXT("Bodycam.Weapon.Rifle");
		bRig = SetupRig(TEXT("/Game/Bodycam/Weapons/M4/SK_M4.SK_M4"), TEXT("/Game/Bodycam/Weapons/M4"),
			TEXT("M4"), TEXT("Reload_Empty"), TEXT("Bolt_j"), TEXT("Bodycam.Rig.Rifle"), TEXT("FPS_Camera_j"), 29.4f);
		break;
	}

	MagAmmo = MagSize;

	if (bRig)
	{
		Pivot->SetRelativeLocation(FVector::ZeroVector);
		AnimTimeLeft = 0.f;
		if (AnimDraw)
		{
			DrawRemaining = AnimDraw->GetPlayLength() / FMath::Max(DrawAnimRate, 0.01f);
			PlayRigAnim(AnimDraw, DrawAnimRate, false);
		}
		else
		{
			PlayRigAnim(AnimIdle, 1.f, true);
		}
	}
	else
	{
		LoadTuning(Section);
		if (bSkeletal)
		{
			SkelMesh->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr, MeshPath));
			SkelMesh->SetVisibility(true);
			StaticMesh->SetVisibility(false);
		}
		else
		{
			StaticMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
			StaticMesh->SetVisibility(true);
			SkelMesh->SetVisibility(false);
		}

		USceneComponent* MeshComp = bSkeletal ? static_cast<USceneComponent*>(SkelMesh.Get()) : static_cast<USceneComponent*>(StaticMesh.Get());
		MeshComp->SetRelativeLocationAndRotation(MeshOffset, MeshRotation);
		MeshComp->SetRelativeScale3D(FVector(MeshScale));
		Pivot->SetRelativeLocation(HoldOffset);
	}

	MuzzleLight->SetRelativeLocation(MuzzleOffset);
	Flashlight->SetRelativeLocation(bRig ? TorchOffset : MuzzleOffset - FVector(10.f, 0.f, 4.f));
	Flashlight->SetIntensity(TorchIntensity);
	Flashlight->SetVisibility(bFlashlightOn);

	// Flash de bouche : textures réelles (pack gun flash) si importées, sinon l'ancien matériau procédural.
	UMaterialInterface* FrontMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/MI_GunFlash_Front.MI_GunFlash_Front"));
	UMaterialInterface* SideMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/MI_GunFlash_Side.MI_GunFlash_Side"));
	if (!FrontMat || !SideMat)
	{
		if (UMaterialInterface* FlashMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/M_MuzzleFlash.M_MuzzleFlash")))
		{
			FlashMID = UMaterialInstanceDynamic::Create(FlashMat, this);
			FrontMat = SideMat = FlashMID;
		}
	}
	FlashFront->SetMaterial(0, FrontMat);
	FlashSide->SetMaterial(0, SideMat);
	FlashSide2->SetMaterial(0, SideMat);

	if (GConfig)
	{
		GConfig->GetFloat(TEXT("Bodycam.Recoil"), TEXT("CameraScale"), CameraRecoilScale, GGameIni);
		GConfig->GetFloat(TEXT("Bodycam.Recoil"), TEXT("VisualScale"), VisualRecoilScale, GGameIni);
		const TCHAR* FX = TEXT("Bodycam.MuzzleFlash");
		GConfig->GetFloat(FX, TEXT("FrontSize"), FlashFrontSize, GGameIni);
		GConfig->GetFloat(FX, TEXT("FlameLength"), FlameLength, GGameIni);
		GConfig->GetFloat(FX, TEXT("FlameWidth"), FlameWidth, GGameIni);
		GConfig->GetFloat(FX, TEXT("FlameAngle"), FlameAngle, GGameIni);
		GConfig->GetInt(FX, TEXT("TracerEvery"), TracerEvery, GGameIni);
	}
	const float FlashScale = Type == EBodycamWeaponType::Shotgun ? 1.6f : Type == EBodycamWeaponType::Rifle ? 1.25f : 1.f;
	FlashFrontSize *= FlashScale;
	FlameLength *= FlashScale;
	FlameWidth *= FlashScale;
	const FQuat InPlane(FVector::UpVector, FMath::DegreesToRadians(FlameAngle));
	const FVector FlameCenter(FlameLength * 50.f, 0.f, 0.f);   // la flamme part de la bouche vers l'avant
	FlashFront->SetRelativeScale3D(FVector(FlashFrontSize));
	FlashSide->SetRelativeLocationAndRotation(FlameCenter, FQuat(FRotator(0.f, 0.f, 90.f)) * InPlane);
	FlashSide->SetRelativeScale3D(FVector(FlameWidth, FlameLength, 1.f));
	FlashSide2->SetRelativeLocationAndRotation(FlameCenter, InPlane);

	// Point rouge : placé sur l'axe de visée (pleine visée), il bouge ensuite avec l'arme comme un vrai viseur.
	if (UMaterialInterface* Dot = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/MI_RedDot.MI_RedDot")))
	{
		Reticle->SetMaterial(0, Dot);
	}
	else
	{
		bRedDot = false;
	}
	if (bSight && SightMesh->GetStaticMesh())
	{
		// Le point est projeté au centre de la vitre du viseur.
		Reticle->AttachToComponent(SightMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Reticle->SetRelativeLocationAndRotation(FVector(0.5f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f));
		Reticle->SetWorldScale3D(FVector(0.0035f));
		bRedDot = true;
	}
	else
	{
		Reticle->SetRelativeLocationAndRotation(FVector(ReticleDist, 0.f, 0.f) - AimOffset, FRotator(90.f, 0.f, 0.f));
		Reticle->SetRelativeScale3D(FVector(0.0045f));
	}
	FlashSide2->SetRelativeScale3D(FVector(FlameWidth, FlameLength, 1.f));
}

void ABodycamWeapon::ShowMuzzleFlash()
{
	FlashTime = 0.04f;
	FlashFrames = 2;
	if (FlashMID) FlashMID->SetScalarParameterValue(TEXT("Seed"), FMath::FRand());
	FlashFront->SetRelativeRotation(FQuat(FRotator(90.f, 0.f, 0.f)) * FQuat(FVector::UpVector, FMath::FRandRange(0.f, 2.f * PI)));
	// Chaque flash est différent : taille et longueur de flamme varient d'un tir à l'autre.
	const float J = FMath::FRandRange(0.8f, 1.2f);
	FlashFront->SetRelativeScale3D(FVector(FlashFrontSize * J));
	const FVector FlameScale(FlameWidth * FMath::FRandRange(0.8f, 1.2f), FlameLength * FMath::FRandRange(0.75f, 1.25f), 1.f);
	FlashSide->SetRelativeScale3D(FlameScale);
	FlashSide2->SetRelativeScale3D(FlameScale);
	FlashFront->SetVisibility(true);
	FlashSide->SetVisibility(true);
	FlashSide2->SetVisibility(true);
}

void ABodycamWeapon::ToggleFlashlight()
{
	bFlashlightOn = !bFlashlightOn;
	Flashlight->SetVisibility(bFlashlightOn);
}

void ABodycamWeapon::StartFire()
{
	bTriggerHeld = true;
	if (Cooldown <= 0.f)
	{
		FireOnce();
	}
}

void ABodycamWeapon::StopFire()
{
	bTriggerHeld = false;
}

bool ABodycamWeapon::StartReload()
{
	if (IsReloading() || DrawRemaining > 0.f || MagAmmo >= MagSize || ReserveAmmo <= 0) return false;

	if (bRig && bTube)
	{
		// Fusil à pompe : ouverture, une cartouche à la fois, puis pompe.
		ShellsToLoad = FMath::Min(MagSize - MagAmmo, ReserveAmmo);
		const float Rate = ReloadAnimRate;
		const float StartLen = AnimReloadStart ? AnimReloadStart->GetPlayLength() / Rate : 0.f;
		const float LoopLen = AnimReload ? AnimReload->GetPlayLength() / Rate : 0.4f;
		const float EndLen = AnimReloadEnd ? AnimReloadEnd->GetPlayLength() / Rate : 0.f;
		ReloadTime = StartLen + ShellsToLoad * LoopLen + EndLen;
		TubeStage = 1;
		TubeTimer = StartLen;
		PlayRigAnim(AnimReloadStart, Rate, false);
		ReloadRemaining = ReloadTime;
		return true;
	}

	if (bRig)
	{
		// Chargeur vide : l'animation finit par le lâcher de culasse. Sinon rechargement tactique.
		UAnimSequence* A = (MagAmmo <= 0 && AnimReloadEmpty) ? AnimReloadEmpty : AnimReload;
		if (A)
		{
			ReloadTime = A->GetPlayLength() / FMath::Max(ReloadAnimRate, 0.01f);
			PlayRigAnim(A, ReloadAnimRate, false);
		}
	}
	ReloadRemaining = ReloadTime;
	return true;
}

void ABodycamWeapon::CancelReload()
{
	ReloadRemaining = 0.f;
	TubeStage = 0;
	if (bRig) PlayRigAnim(AnimIdle, 1.f, true);
}

void ABodycamWeapon::DryFire()
{
	// Gâchette pressée chargeur vide : simple "clic", le compteur de balles clignote en rouge.
	Cooldown = 0.25f;
	bTriggerHeld = false;
	DryFireFlash = 1.f;
	KickOffset += FVector(-0.7f, 0.f, 0.f);
	if (DryFireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DryFireSound, GetActorLocation(), 0.7f);
	}
}

void ABodycamWeapon::FireOnce()
{
	// Pompe : on peut interrompre le rechargement pour tirer (s'il reste au moins une cartouche).
	if (bTube && (TubeStage == 1 || TubeStage == 2) && MagAmmo > 0)
	{
		CancelReload();
	}

	if (bDropped || bLowered || IsReloading() || DrawRemaining > 0.f || MeleeTime > 0.f) return;

	if (MagAmmo <= 0)
	{
		DryFire();
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* Controller = OwnerPawn ? OwnerPawn->GetController() : nullptr;
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!OwnerPawn || !PC || !PC->PlayerCameraManager) return;

	--MagAmmo;
	++ShotCounter;
	Cooldown = FireInterval;
	TimeSinceShot = 0.f;

	const FVector Start = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Forward = PC->PlayerCameraManager->GetCameraRotation().Vector();

	// Viser en marchant / courant / en rafale = moins précis.
	const float MoveSpread = FMath::Clamp(OwnerPawn->GetVelocity().Size2D() / 400.f, 0.f, 1.f) * 1.2f;
	// Épauler : beaucoup plus précis (la dispersion vient surtout du tir sans viser).
	const float ConeDeg = (SpreadDeg * (1.f + Bloom) + MoveSpread) * FMath::Lerp(1.f, 0.35f, AimAlpha);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BodycamShot), true, OwnerPawn);
	Params.AddIgnoredActor(this);
	Params.bReturnPhysicalMaterial = false;

	// Vitres des fenêtres (zone extérieure) : une balle les fait éclater.
	ABodycamExterior* Exterior = nullptr;
	for (TActorIterator<ABodycamExterior> It(GetWorld()); It; ++It) { Exterior = *It; break; }

	for (int32 i = 0; i < Pellets; ++i)
	{
		const FVector Dir = FMath::VRandCone(Forward, FMath::DegreesToRadians(ConeDeg * (Pellets > 1 ? 1.f : FMath::FRand())));
		const FVector End = Start + Dir * Range;

		FHitResult Hit;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, WeaponChannel, Params);
		bool bHitGlass = false;
		if (Exterior)
		{
			bHitGlass = Exterior->IsGlassBlocker(Hit.GetComponent());
			if (Exterior->BreakGlassAlong(Start, bHit ? Hit.ImpactPoint : End, Dir)) bHitGlass = true;
		}

		// Traçante depuis la bouche du canon (fusil à pompe : 3 plombs sur 9 seulement).
		if ((ShotCounter % FMath::Max(TracerEvery, 1)) == 0 && i < 3)
		{
			FActorSpawnParameters TP;
			TP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector From = MuzzleLight->GetComponentLocation();
			if (ABodycamTracer* Tracer = GetWorld()->SpawnActor<ABodycamTracer>(ABodycamTracer::StaticClass(), From, FRotator::ZeroRotator, TP))
			{
				Tracer->Init(From, bHit ? Hit.ImpactPoint : End);
			}
		}

		if (bHit && !bHitGlass)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && HitActor->CanBeDamaged() && HitActor != OwnerPawn)
			{
				UGameplayStatics::ApplyPointDamage(HitActor, Damage, Dir, Hit, Controller, this, UDamageType::StaticClass());

				// Retour visuel : gerbe de sang + tache sur le mur/sol derrière la cible.
				const bool bHead = Hit.BoneName.ToString().Contains(TEXT("Head"));
				FActorSpawnParameters FXParams;
				FXParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (ABodycamImpactFX* FX = GetWorld()->SpawnActor<ABodycamImpactFX>(ABodycamImpactFX::StaticClass(), Hit.ImpactPoint, FRotator::ZeroRotator, FXParams))
				{
					FX->Init(Dir, bHead ? 1.f : 0.5f);
				}
				// Plaie visible sur le corps du zombie.
				if (const ACharacter* HitChar = Cast<ACharacter>(HitActor))
				{
					ABodycamImpactFX::AttachWound(HitChar->GetMesh(), Hit.ImpactPoint, Dir, bHead ? 0.1f : 0.08f);
				}
				FCollisionQueryParams Behind(SCENE_QUERY_STAT(BloodBehind), false, OwnerPawn);
				Behind.AddIgnoredActor(HitActor);
				FHitResult Wall;
				if (GetWorld()->LineTraceSingleByChannel(Wall, Hit.ImpactPoint, Hit.ImpactPoint + Dir * 280.f + FVector(0, 0, -60.f), ECC_Visibility, Behind))
				{
					ABodycamImpactFX::SpawnDecal(this, true, Wall.ImpactPoint, Wall.ImpactNormal, FMath::FRandRange(18.f, bHead ? 45.f : 30.f));
				}
			}
			else
			{
				// Mur / sol : éclats et poussière grise + trou de balle (plus d'étincelles bleues).
				FActorSpawnParameters DustParams;
				DustParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (ABodycamImpactFX* Dust = GetWorld()->SpawnActor<ABodycamImpactFX>(ABodycamImpactFX::StaticClass(), Hit.ImpactPoint + Hit.ImpactNormal * 2.f, FRotator::ZeroRotator, DustParams))
				{
					Dust->Init(Hit.ImpactNormal, 0.5f, true);
				}
				ABodycamImpactFX::SpawnDecal(this, false, Hit.ImpactPoint, Hit.ImpactNormal, 3.5f);
			}
		}
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation(), SoundVolume, FMath::FRandRange(0.94f, 1.04f));
	}

	Bloom = FMath::Min(Bloom + (bAutomatic ? 0.35f : 0.6f), 3.f);
	// Douille : tout de suite (culasse), ou au coup de pompe pour le fusil à pompe.
	if (Type == EBodycamWeaponType::Shotgun) PendingShellTime = 0.45f; else EjectShell();
	ApplyRecoil();
	ShowMuzzleFlash();
	MuzzleLight->SetIntensity(Type == EBodycamWeaponType::Shotgun ? 900.f : 500.f);

	if (bRig && AnimFire)
	{
		// Animation de tir accélérée pour tenir dans la cadence (la culasse / le chien doivent avoir le temps de bouger).
		const float Rate = bAutomatic
			? FMath::Clamp(AnimFire->GetPlayLength() / FMath::Max(FireInterval * 3.f, 0.28f), 1.f, 3.5f)
			: FMath::Clamp(AnimFire->GetPlayLength() / FMath::Max(FireInterval * 0.95f, 0.1f), 1.f, 3.5f);
		PlayRigAnim(AnimFire, Rate, false);
	}

	if (!bAutomatic)
	{
		bTriggerHeld = false;
	}
}

void ABodycamWeapon::EjectShell()
{
	const FTransform Frame = Pivot->GetComponentTransform();
	const FVector From = Frame.TransformPosition(EjectOffset);
	const FVector Right = Frame.GetUnitAxis(EAxis::Y);
	const FVector Up = Frame.GetUnitAxis(EAxis::Z);
	const FVector Fwd = Frame.GetUnitAxis(EAxis::X);
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABodycamShell* Shell = GetWorld()->SpawnActor<ABodycamShell>(ABodycamShell::StaticClass(), From, Frame.Rotator(), SP))
	{
		const FVector OwnerVel = GetOwner() ? GetOwner()->GetVelocity() : FVector::ZeroVector;
		const FVector V = Right * FMath::FRandRange(180.f, 260.f) + Up * FMath::FRandRange(120.f, 220.f) - Fwd * FMath::FRandRange(0.f, 60.f);
		Shell->Init(V + OwnerVel, ShellSize, Type == EBodycamWeaponType::Shotgun);
	}
}

void ABodycamWeapon::ApplyRecoil()
{
	// Nouvelle rafale : nouvelle direction de dérive (le recul d'un fusil tire plutôt d'un côté pendant la rafale).
	if (ShotsInBurst == 0)
	{
		BurstYawBias = FMath::FRandRange(-1.f, 1.f);
	}

	const float Ramp = 1.f + FMath::Min(ShotsInBurst, 12) * RecoilRamp;
	const float Pitch = RecoilPitch * Ramp * FMath::FRandRange(0.85f, 1.15f);
	const float Yaw = RecoilYaw * (BurstYawBias * 0.6f + FMath::FRandRange(-0.7f, 0.7f));

	RecoilToApply += Pitch * CameraRecoilScale;
	RecoilYawToApply += Yaw * CameraRecoilScale;
	RecoilTotalPitch += Pitch * CameraRecoilScale;
	++ShotsInBurst;
	BurstTimer = 0.f;
	bRecoverPending = true;

	// Recul visible de l'arme dans les mains (le rig anime déjà une partie du mouvement : on en met moins).
	// Recul bien visible : l'arme recule et le canon monte (un peu moins quand on épaule).
	const float Visual = FMath::Lerp(1.f, 0.7f, AimAlpha);
	const float V = Visual * VisualRecoilScale;
	KickOffset += FVector(-KickBack * FMath::Sqrt(VisualRecoilScale), FMath::FRandRange(-0.6f, 0.6f), 1.2f) * Visual;
	KickRot += FRotator(KickPitchDeg, FMath::FRandRange(-1.5f, 1.5f), FMath::FRandRange(-3.f, 3.f)) * V;
	// Garde-fous : l'arme monte fort mais ne se retourne pas.
	KickRot.Pitch = FMath::Min(KickRot.Pitch, 40.f);
	KickOffset.X = FMath::Max(KickOffset.X, -18.f);

	if (const ABodycamCharacter* Char = Cast<ABodycamCharacter>(GetOwner()))
	{
		if (UBodycamCameraComponent* Cam = Char->GetBodycamCamera())
		{
			Cam->AddShake(ShakeOnFire);
		}
	}
}

void ABodycamWeapon::UpdateTubeReload(float DeltaSeconds)
{
	ReloadRemaining = FMath::Max(ReloadRemaining - DeltaSeconds, 0.01f);
	TubeTimer -= DeltaSeconds;
	if (TubeTimer > 0.f) return;

	const float Rate = ReloadAnimRate;
	auto StartLoop = [&]()
	{
		TubeStage = 2;
		TubeTimer = AnimReload ? AnimReload->GetPlayLength() / Rate : 0.4f;
		PlayRigAnim(AnimReload, Rate, false);
	};
	auto StartEnd = [&]()
	{
		TubeStage = 3;
		TubeTimer = AnimReloadEnd ? AnimReloadEnd->GetPlayLength() / Rate : 0.f;
		PlayRigAnim(AnimReloadEnd, Rate, false);
	};

	switch (TubeStage)
	{
	case 1:  // ouverture terminée
		if (ShellsToLoad > 0) StartLoop(); else StartEnd();
		break;
	case 2:  // une cartouche insérée
		if (ReserveAmmo > 0 && MagAmmo < MagSize)
		{
			++MagAmmo;
			--ReserveAmmo;
		}
		--ShellsToLoad;
		if (ShellsToLoad > 0 && ReserveAmmo > 0) StartLoop(); else StartEnd();
		break;
	default: // fermeture terminée
		TubeStage = 0;
		ReloadRemaining = 0.f;
		break;
	}
}

void ABodycamWeapon::MakeBareHands()
{
	bBareHands = true;
	DrawRemaining = 0.f;
	bFlashlightOn = false;
	Flashlight->SetVisibility(false);
	Reticle->SetVisibility(false);
	SightMesh->SetVisibility(false);
	if (bRig)
	{
		// On garde mains / gants / manches, l'arme et les balles deviennent invisibles.
		UMaterialInterface* Invisible = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/MI_Invisible.MI_Invisible"));
		const TArray<FName> Slots = SkelMesh->GetMaterialSlotNames();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			const FString N = Slots[i].ToString().ToLower();
			const bool bArm = N.Contains(TEXT("hand")) || N.Contains(TEXT("glove")) || N.Contains(TEXT("sleeve")) || N.Contains(TEXT("skin"));
			if (!bArm && Invisible) SkelMesh->SetMaterial(i, Invisible);
		}
		for (const TCHAR* B : { TEXT("Mag2_j"), TEXT("Mag1_j") })
		{
			if (SkelMesh->GetBoneIndex(B) != INDEX_NONE) SkelMesh->HideBoneByName(B, PBO_None);
		}
		PlayRigAnim(AnimIdle, 1.f, true);
	}
	SetActorHiddenInGame(true);
}

void ABodycamWeapon::StartMelee()
{
	if (bDropped || bLowered || MeleeCooldown > 0.f || DrawRemaining > 0.f) return;
	if (IsReloading()) CancelReload();
	bTriggerHeld = false;
	MeleeTime = 0.55f;
	MeleeCooldown = 0.8f;
	bMeleeHitDone = false;
}

void ABodycamWeapon::MeleeHit()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PC = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (!PC || !PC->PlayerCameraManager) return;
	const FVector Start = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Dir = PC->PlayerCameraManager->GetCameraRotation().Vector();

	// Balayage court devant soi (portée d'un bras + arme).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BodycamMelee), false, OwnerPawn);
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Start, Start + Dir * 140.f, FQuat::Identity, WeaponChannel, FCollisionShape::MakeSphere(28.f), Params))
	{
		AActor* HitActor = Hit.GetActor();
		if (const ABodycamCharacter* Me = Cast<ABodycamCharacter>(OwnerPawn))
		{
			if (UBodycamCameraComponent* Cam = Me->GetBodycamCamera()) Cam->AddShake(0.5f);
		}
		if (HitActor && HitActor->CanBeDamaged())
		{
			UGameplayStatics::ApplyPointDamage(HitActor, MeleeDamage, Dir, Hit, PC, this, UDamageType::StaticClass());
			FActorSpawnParameters FXParams;
			FXParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ABodycamImpactFX* FX = GetWorld()->SpawnActor<ABodycamImpactFX>(ABodycamImpactFX::StaticClass(), Hit.ImpactPoint, FRotator::ZeroRotator, FXParams))
			{
				FX->Init(Dir, 0.4f);
			}
			if (ACharacter* Victim = Cast<ACharacter>(HitActor))
			{
				// Le coup fait reculer franchement.
				Victim->LaunchCharacter(Dir.GetSafeNormal2D() * 520.f + FVector(0.f, 0.f, 60.f), true, true);
			}
		}
		else
		{
			FActorSpawnParameters DustParams;
			DustParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ABodycamImpactFX* Dust = GetWorld()->SpawnActor<ABodycamImpactFX>(ABodycamImpactFX::StaticClass(), Hit.ImpactPoint, FRotator::ZeroRotator, DustParams))
			{
				Dust->Init(Hit.ImpactNormal, 0.5f, true);
			}
		}
	}
}

void ABodycamWeapon::Drop(const FVector& InheritVelocity)
{
	if (bDropped) return;
	bDropped = true;
	bTriggerHeld = false;
	ReloadRemaining = 0.f;
	TubeStage = 0;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	FlashFront->SetVisibility(false);
	FlashSide->SetVisibility(false);
	FlashSide2->SetVisibility(false);
	MuzzleLight->SetIntensity(0.f);

	if (bRig)
	{
		// On ne garde que l'arme : bras / gants / manches deviennent invisibles, les pièces de rechange sont cachées.
		UMaterialInterface* Invisible = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/MI_Invisible.MI_Invisible"));
		const TArray<FName> Slots = SkelMesh->GetMaterialSlotNames();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			const FString N = Slots[i].ToString().ToLower();
			if (N.Contains(TEXT("hand")) || N.Contains(TEXT("glove")) || N.Contains(TEXT("sleeve")) || N.Contains(TEXT("skin")))
			{
				if (Invisible) SkelMesh->SetMaterial(i, Invisible);
			}
		}
		for (const TCHAR* B : { TEXT("Mag2_j"), TEXT("Shell") })
		{
			if (SkelMesh->GetBoneIndex(B) != INDEX_NONE) SkelMesh->HideBoneByName(B, PBO_None);
		}
		SkelMesh->bPauseAnims = true;
	}
	const FVector Fwd = GetActorForwardVector();
	DropVel = InheritVelocity + Fwd * FMath::FRandRange(60.f, 140.f) + FVector(0.f, 0.f, FMath::FRandRange(40.f, 120.f));
	DropSpin = FVector(FMath::FRandRange(-260.f, 260.f), FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-260.f, 260.f));
}

void ABodycamWeapon::UpdateDrop(float DeltaSeconds)
{
	if (bDropRest) return;
	// Point de référence = l'arme elle-même (le pivot de l'acteur est à l'œil du joueur).
	const FVector Gun = (bRig && !WeaponBone.IsNone()) ? SkelMesh->GetBoneLocation(WeaponBone) : StaticMesh->GetComponentLocation();
	DropVel += FVector(0.f, 0.f, -980.f) * DeltaSeconds;
	FVector Move = DropVel * DeltaSeconds;

	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(WeaponDrop), false, this);
	if (GetOwner()) P.AddIgnoredActor(GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(Hit, Gun, Gun + Move + FVector(0.f, 0.f, -4.f), ECC_Visibility, P))
	{
		Move = Hit.ImpactPoint + FVector(0.f, 0.f, 4.f) - Gun;
		DropVel = FMath::GetReflectionVector(DropVel, Hit.ImpactNormal) * 0.25f;
		DropSpin *= 0.4f;
		if (DropVel.Size() < 60.f) bDropRest = true;
	}
	AddActorWorldOffset(Move);

	// Rotation autour de l'arme (et non autour de l'œil).
	const FQuat Q = FQuat::MakeFromEuler(DropSpin * DeltaSeconds);
	const FVector NewGun = Gun + Move;
	const FVector Off = GetActorLocation() - NewGun;
	SetActorLocationAndRotation(NewGun + Q.RotateVector(Off), Q * GetActorQuat());
}

void ABodycamWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDropped)
	{
		UpdateDrop(DeltaSeconds);
		return;
	}

	Cooldown -= DeltaSeconds;
	TimeSinceShot += DeltaSeconds;
	BurstTimer += DeltaSeconds;
	DryFireFlash = FMath::Max(0.f, DryFireFlash - DeltaSeconds * 2.2f);
	Bloom = FMath::FInterpTo(Bloom, 0.f, DeltaSeconds, 3.f);

	if (DrawRemaining > 0.f) DrawRemaining = FMath::Max(0.f, DrawRemaining - DeltaSeconds);
	MeleeCooldown -= DeltaSeconds;
	if (MeleeTime > 0.f)
	{
		MeleeTime -= DeltaSeconds;
		if (!bMeleeHitDone && MeleeTime < 0.38f)   // l'impact au moment où l'arme est la plus en avant
		{
			bMeleeHitDone = true;
			MeleeHit();
		}
	}
	if (PendingShellTime >= 0.f)
	{
		PendingShellTime -= DeltaSeconds;
		if (PendingShellTime < 0.f) EjectShell();
	}

	if (bTriggerHeld && bAutomatic && Cooldown <= 0.f)
	{
		FireOnce();
	}

	if (bTube && TubeStage > 0)
	{
		UpdateTubeReload(DeltaSeconds);
	}
	else if (ReloadRemaining > 0.f)
	{
		ReloadRemaining -= DeltaSeconds;
		if (ReloadRemaining <= 0.f)
		{
			ReloadRemaining = 0.f;
			const int32 Needed = MagSize - MagAmmo;
			const int32 Taken = FMath::Min(Needed, ReserveAmmo);
			MagAmmo += Taken;
			ReserveAmmo -= Taken;
		}
	}

	// Rig : retour à la pose de repos une fois l'animation (tir, rechargement, sortie d'arme) terminée.
	if (bRig)
	{
		if (AnimTimeLeft > 0.f) AnimTimeLeft -= DeltaSeconds;
		if (AnimTimeLeft <= 0.f && !bIdlePlaying && !IsReloading() && DrawRemaining <= 0.f)
		{
			PlayRigAnim(AnimIdle, 1.f, true);
		}
	}

	// Flash de bouche : 40 ms ET au moins 2 images (à 20-30 FPS une durée seule le cachait avant l'affichage).
	if (FlashTime > 0.f || FlashFrames > 0)
	{
		FlashTime -= DeltaSeconds;
		--FlashFrames;
		if (FlashTime <= 0.f && FlashFrames <= 0)
		{
			FlashFront->SetVisibility(false);
			FlashSide->SetVisibility(false);
			FlashSide2->SetVisibility(false);
			MuzzleLight->SetIntensity(0.f);
		}
	}

	// Fin de rafale : le tireur ramène le canon vers sa visée d'origine.
	if (ShotsInBurst > 0 && BurstTimer > 0.3f) ShotsInBurst = 0;
	if (bRecoverPending && BurstTimer > 0.18f)
	{
		RecoveryPool = RecoilTotalPitch * RecoveryFraction;
		RecoilTotalPitch = 0.f;
		bRecoverPending = false;
	}

	// Recul appliqué à la visée sur quelques images (plus naturel qu'un à-coup).
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			const float Step = FMath::Min(1.f, DeltaSeconds * 18.f);
			FRotator R = PC->GetControlRotation();
			float PitchDelta = RecoilToApply * Step;
			if (RecoveryPool > 0.f && !bTriggerHeld)
			{
				const float Back = FMath::Min(RecoveryPool, RecoveryRate * DeltaSeconds);
				RecoveryPool -= Back;
				PitchDelta -= Back;
			}
			R.Pitch = FMath::ClampAngle(FRotator::NormalizeAxis(R.Pitch + PitchDelta), -88.f, 88.f);
			R.Yaw += RecoilYawToApply * Step;
			PC->SetControlRotation(R);
			RecoilToApply -= RecoilToApply * Step;
			RecoilYawToApply -= RecoilYawToApply * Step;
		}
	}

	UpdateViewModel(DeltaSeconds);
}

void ABodycamWeapon::UpdateViewModel(float DeltaSeconds)
{
	if (bBareHands)
	{
		// Les deux mains montent devant soi et poussent le zombie : à-coup vers l'avant à chaque appui,
		// tremblement de l'effort, mains qui glissent d'un côté à l'autre.
		SetActorHiddenInGame(StruggleAlpha < 0.02f);
		StruggleTime += DeltaSeconds;
		const float T = StruggleTime;
		const float Thrust = FMath::Clamp(StruggleThrust, 0.f, 2.f);
		const FVector Hidden(-15.f, 0.f, -45.f);
		const FVector Pushing = FVector(-9.f + Thrust * 12.f, FMath::Sin(T * 3.1f) * 3.f, 3.f + Thrust * 3.f)
			+ FVector(0.f, FMath::PerlinNoise1D(T * 9.f) * 1.5f, FMath::PerlinNoise1D(T * 11.f + 4.f) * 1.5f);
		const FRotator Rot = FRotator(-6.f - Thrust * 10.f, FMath::Sin(T * 2.3f) * 6.f, FMath::Sin(T * 3.1f) * 8.f) * StruggleAlpha
			+ FRotator(-35.f, 0.f, 0.f) * (1.f - StruggleAlpha);
		Pivot->SetRelativeLocationAndRotation(FMath::Lerp(Hidden, Pushing, StruggleAlpha), Rot);
		return;
	}
	const ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	FRotator ControlRot = FRotator::ZeroRotator;
	float Speed = 0.f;
	bool bSprinting = false;
	if (OwnerChar)
	{
		ControlRot = OwnerChar->GetControlRotation();
		Speed = OwnerChar->GetVelocity().Size2D();
		bSprinting = Speed > 380.f && OwnerChar->GetCharacterMovement()->IsMovingOnGround();
	}

	AimAlpha = FMath::FInterpTo(AimAlpha, (bAiming && !bLowered && MeleeTime <= 0.f) ? 1.f : 0.f, DeltaSeconds, 10.f);
	Reticle->SetVisibility(bRedDot && AimAlpha > (bSight ? 0.5f : 0.85f));

	// Coup de crosse : l'arme part sur le côté (armer), frappe vers l'avant en tournant, puis revient.
	FVector MeleeOffset = FVector::ZeroVector;
	FRotator MeleeRot = FRotator::ZeroRotator;
	if (MeleeTime > 0.f)
	{
		const float T = 1.f - MeleeTime / 0.55f;
		const float Wind = FMath::Clamp(T / 0.3f, 0.f, 1.f) * (1.f - FMath::Clamp((T - 0.3f) / 0.15f, 0.f, 1.f));
		const float Strike = FMath::Clamp((T - 0.3f) / 0.15f, 0.f, 1.f) * (1.f - FMath::Clamp((T - 0.55f) / 0.45f, 0.f, 1.f));
		MeleeOffset = FVector(-6.f, 10.f, -4.f) * Wind + FVector(22.f, -12.f, 6.f) * Strike;
		MeleeRot = FRotator(-10.f, 25.f, 20.f) * Wind + FRotator(15.f, -40.f, -55.f) * Strike;
	}

	// Balancement : l'arme traîne légèrement derrière les mouvements de souris (presque rien en visée).
	const float SwayScale = (bRig ? 0.6f : 1.f) * FMath::Lerp(1.f, 0.25f, AimAlpha);
	const FRotator Delta = (ControlRot - LastControlRot).GetNormalized();
	LastControlRot = ControlRot;
	const FRotator SwayTarget(FMath::Clamp(-Delta.Pitch * 0.8f, -4.f, 4.f) * SwayScale, FMath::Clamp(-Delta.Yaw * 0.8f, -5.f, 5.f) * SwayScale, FMath::Clamp(Delta.Yaw * 1.2f, -6.f, 6.f) * SwayScale);
	SwayRot = FMath::RInterpTo(SwayRot, SwayTarget, DeltaSeconds, 8.f);

	KickOffset = FMath::VInterpTo(KickOffset, FVector::ZeroVector, DeltaSeconds, 14.f);
	KickRot = FMath::RInterpTo(KickRot, FRotator::ZeroRotator, DeltaSeconds, 12.f);

	LowerAlpha = FMath::FInterpTo(LowerAlpha, bLowered ? 1.f : 0.f, DeltaSeconds, 6.f);
	SprintAlpha = FMath::FInterpTo(SprintAlpha, bSprinting ? 1.f : 0.f, DeltaSeconds, 7.f);

	BobTime += DeltaSeconds * FMath::Lerp(1.f, 1.5f, SprintAlpha) * FMath::Clamp(Speed / 260.f, 0.f, 1.6f) * 2.f * PI * 0.95f;
	const float BobAmp = FMath::Clamp(Speed / 260.f, 0.f, 1.5f);
	const FVector Bob = FVector(0.f, FMath::Sin(BobTime * 0.5f) * 0.6f * BobAmp, FMath::Sin(BobTime) * 0.4f * BobAmp) * FMath::Lerp(1.f, 0.15f, AimAlpha);

	// Rechargement des anciens modèles : l'arme descend et pivote, puis revient (le rig a sa propre animation).
	float ReloadDip = 0.f;
	if (IsReloading() && !bRig)
	{
		const float P = GetReloadProgress();
		ReloadDip = FMath::Sin(P * PI);
	}

	const FVector Offset = (bRig ? FVector::ZeroVector : HoldOffset) + KickOffset + Bob
		+ FVector(-4.f, -4.f, -10.f) * SprintAlpha
		+ FVector(-6.f, 2.f, -9.f) * ReloadDip
		+ FVector(-10.f, 6.f, -35.f) * LowerAlpha
		+ AimOffset * AimAlpha
		+ MeleeOffset;

	const FRotator Rot = SwayRot + KickRot
		+ FRotator(-12.f, 18.f, -10.f) * SprintAlpha
		+ FRotator(-18.f, 8.f, 35.f) * ReloadDip
		+ FRotator(-30.f, 10.f, 20.f) * LowerAlpha
		+ AimRotation * AimAlpha
		+ MeleeRot;

	Pivot->SetRelativeLocationAndRotation(Offset, Rot);
}
