#pragma once

#include "CoreMinimal.h"
#include "Core/BodycamTypes.h"
#include "GameFramework/Actor.h"
#include "BodycamWeapon.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UPointLightComponent;
class USpotLightComponent;
class UAnimSequence;
class USoundBase;
class UParticleSystem;
class UMaterialInstanceDynamic;

/**
 * Arme tenue en vue bodycam : tir instantané (hitscan), chargeur + réserve limitée,
 * recul progressif avec récupération, dispersion, rechargement, lampe torche.
 *
 * Pistolet et fusil d'assaut utilisent un "rig FPS" (bras gantés + arme + animations : tir, rechargement,
 * rechargement à vide, sortie d'arme...). Le fusil à pompe garde pour l'instant l'ancien modèle fixe.
 * Réglages : Config/DefaultGame.ini [Bodycam.Rig.Pistol] / [Bodycam.Rig.Rifle].
 */
UCLASS()
class BODYCAMUE_API ABodycamWeapon : public AActor
{
	GENERATED_BODY()

public:
	ABodycamWeapon();

	virtual void Tick(float DeltaSeconds) override;

	void InitWeapon(EBodycamWeaponType InType);

	void StartFire();
	void StopFire();
	bool StartReload();
	void CancelReload();

	/** Lampe torche fixée sous le canon. */
	void ToggleFlashlight();
	bool IsFlashlightOn() const { return bFlashlightOn; }

	/** Visée (clic droit) : aligne les organes de visée sur l'axe de la caméra. */
	void SetAiming(bool bInAiming) { bAiming = bInAiming; }

	/** Coup de crosse / d'arme au corps à corps (clic molette). */
	void StartMelee();
	bool IsMeleeing() const { return MeleeTime > 0.f; }
	float GetAimAlpha() const { return AimAlpha; }
	float GetAimFOV() const { return AimFOV; }

	/** Bras seuls (sans arme) : servent à repousser un zombie qui nous agrippe. */
	void MakeBareHands();
	/** Alpha = bras sortis (0-1), Thrust = poussée en cours (1 à chaque appui sur F, plus à la réussite). */
	void SetStruggle(float InAlpha, float InThrust) { StruggleAlpha = InAlpha; StruggleThrust = InThrust; }

	/** Mort du joueur : les mains lâchent l'arme, qui tombe au sol. */
	void Drop(const FVector& InheritVelocity);
	bool IsDropped() const { return bDropped; }

	/** Abaisse l'arme (agrippé par un zombie, téléphone, drone...). */
	void SetLowered(bool bInLowered) { bLowered = bInLowered; }

	EBodycamWeaponType GetWeaponType() const { return Type; }
	const FString& GetDisplayName() const { return DisplayName; }
	int32 GetMagAmmo() const { return MagAmmo; }
	int32 GetMagSize() const { return MagSize; }
	int32 GetReserveAmmo() const { return ReserveAmmo; }
	bool IsReloading() const { return ReloadRemaining > 0.f; }
	float GetReloadProgress() const { return ReloadTime > 0.f ? 1.f - ReloadRemaining / ReloadTime : 1.f; }
	void AddReserveMagazines(int32 Mags) { ReserveAmmo += Mags * MagSize; }
	void SetReserveMagazines(int32 Mags) { ReserveAmmo = Mags * MagSize; }

	/** Temps depuis le dernier tir (flash de bouche / coupure JVN). */
	float GetTimeSinceShot() const { return TimeSinceShot; }

	/** 1 juste après un "clic" à vide, redescend à 0 : sert à faire clignoter le compteur de balles en rouge. */
	float GetDryFireFlash() const { return DryFireFlash; }

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> StaticMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> SkelMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> MuzzleLight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> Flashlight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FlashFront;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FlashSide;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FlashSide2;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Reticle;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> SightMesh;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FlashMID;

	UPROPERTY() TObjectPtr<USoundBase> FireSound;
	UPROPERTY() TObjectPtr<USoundBase> DryFireSound;
	UPROPERTY() TObjectPtr<UParticleSystem> ImpactFX;

	// Animations du rig FPS
	UPROPERTY() TObjectPtr<UAnimSequence> AnimFire;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimReload;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimReloadEmpty;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimReloadStart;   // fusil à pompe : ouverture
	UPROPERTY() TObjectPtr<UAnimSequence> AnimReloadEnd;     // fusil à pompe : fermeture / pompe
	UPROPERTY() TObjectPtr<UAnimSequence> AnimDraw;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimHolster;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimIdle;

private:
	void FireOnce();
	void DryFire();
	void ApplyRecoil();
	void UpdateViewModel(float DeltaSeconds);
	void LoadTuning(const TCHAR* Section);
	void ShowMuzzleFlash();
	float FlashTime = 0.f;
	int32 FlashFrames = 0;          // le flash reste affiché au moins 2 images, même à faible FPS
	int32 ShotCounter = 0;
	int32 TracerEvery = 1;
	FVector EjectOffset = FVector(20.f, 6.f, -2.f);   // fenêtre d'éjection (repère caméra)
	FVector2D ShellSize = FVector2D(0.95f, 1.9f);      // diamètre, longueur (cm)
	float PendingShellTime = -1.f;                    // fusil à pompe : douille éjectée au coup de pompe
	void EjectShell();
	void UpdateDrop(float DeltaSeconds);
	void MeleeHit();

	// Bras nus
	bool bBareHands = false;
	float StruggleAlpha = 0.f;
	float StruggleThrust = 0.f;
	float StruggleTime = 0.f;

	// Corps à corps
	float MeleeTime = 0.f;          // temps restant de l'animation de coup
	float MeleeCooldown = 0.f;
	bool bMeleeHitDone = false;
	float MeleeDamage = 45.f;

	// Viseur point rouge (point lumineux qui reste dans l'axe en visée)
	bool bRedDot = false;
	float ReticleDist = 30.f;
	bool bSight = false;                       // viseur point rouge monté sur l'arme
	FVector SightOffset = FVector(30.f, 6.8f, -1.7f);   // centre optique du viseur (repère caméra, arme au repos)
	FRotator SightRotation = FRotator::ZeroRotator;
	float SightScale = 1.f;
	void MountSight();
	bool RefBoneTransform(const FName& Bone, FTransform& Out) const;
	float CameraRecoilScale = 2.f;
	float VisualRecoilScale = 5.f;

	// Visée
	bool bAiming = false;
	float AimAlpha = 0.f;
	float AimFOV = 75.f;
	FVector AimOffset = FVector::ZeroVector;
	FRotator AimRotation = FRotator::ZeroRotator;

	// Arme lâchée
	bool bDropped = false;
	bool bDropRest = false;
	FName WeaponBone;
	FVector DropVel = FVector::ZeroVector;
	FVector DropSpin = FVector::ZeroVector;
	float FlashFrontSize = 0.16f;   // taille du flash face caméra (x 100 cm)
	float FlameLength = 0.28f;
	float FlameWidth = 0.09f;
	float FlameAngle = 90.f;        // orientation de la texture de flamme dans son plan

	// --- Rig FPS ---
	bool SetupRig(const TCHAR* MeshPath, const TCHAR* AnimDir, const TCHAR* AnimPrefix, const TCHAR* ReloadEmptyName, const TCHAR* RefBone, const TCHAR* TuningSection,
		const TCHAR* CamBone = TEXT("FPS_Camera_j"), float ExpectedDist = 0.f, bool bTubeRig = false);
	void AlignRig(const FName& CamBone, const FName& RefBone, float ExpectedDist);
	void UpdateTubeReload(float DeltaSeconds);
	void PlayRigAnim(UAnimSequence* Anim, float Rate, bool bLoop);

	bool bRig = false;
	bool bTube = false;          // rechargement cartouche par cartouche (fusil à pompe)
	int32 TubeStage = 0;         // 0 aucun, 1 ouverture, 2 cartouches, 3 fermeture
	int32 ShellsToLoad = 0;
	float TubeTimer = 0.f;
	bool bIdlePlaying = false;
	float AnimTimeLeft = 0.f;
	float DrawRemaining = 0.f;
	float FireAnimRate = 3.f;
	float ReloadAnimRate = 1.15f;
	float DrawAnimRate = 1.2f;
	FVector RigOffset = FVector::ZeroVector;
	FRotator RigRotation = FRotator::ZeroRotator;
	float RigScale = 1.f;
	FVector TorchOffset = FVector(30.f, 8.f, -6.f);
	float TorchIntensity = 4000.f;

	EBodycamWeaponType Type = EBodycamWeaponType::Pistol;
	FString DisplayName;

	float Damage = 34.f;
	int32 Pellets = 1;
	float SpreadDeg = 0.6f;
	float FireInterval = 0.16f;
	bool bAutomatic = false;
	int32 MagSize = 12;
	float ReloadTime = 1.7f;
	float RecoilPitch = 1.8f;
	float RecoilYaw = 0.5f;
	float RecoilRamp = 0.f;          // gain de recul par tir d'une même rafale
	float RecoveryFraction = 0.5f;   // part du recul que le joueur "ramène" une fois le tir arrêté
	float RecoveryRate = 8.f;        // degrés / seconde
	float KickBack = 3.5f;           // recul visuel de l'arme (cm)
	float KickPitchDeg = 4.f;        // relèvement visuel de l'arme (degrés)
	float ShakeOnFire = 0.15f;
	float Range = 8000.f;
	float SoundVolume = 1.f;

	// Placement à l'écran (repère caméra : X avant, Y droite, Z haut) pour les modèles non riggés.
	FVector HoldOffset = FVector(38.f, 14.f, -18.f);
	FRotator MeshRotation = FRotator::ZeroRotator;
	FVector MeshOffset = FVector::ZeroVector;
	float MeshScale = 1.f;
	FVector MuzzleOffset = FVector(40.f, 0.f, 4.f);

	int32 MagAmmo = 12;
	int32 ReserveAmmo = 24;
	bool bTriggerHeld = false;
	bool bLowered = false;
	bool bFlashlightOn = false;
	float Cooldown = 0.f;
	float ReloadRemaining = 0.f;
	float TimeSinceShot = 10.f;
	float Bloom = 0.f;
	float DryFireFlash = 0.f;

	// Recul
	int32 ShotsInBurst = 0;
	float BurstTimer = 10.f;
	float BurstYawBias = 0.f;
	float RecoilTotalPitch = 0.f;
	float RecoveryPool = 0.f;
	bool bRecoverPending = false;

	// Animation procédurale
	FVector KickOffset = FVector::ZeroVector;
	FRotator KickRot = FRotator::ZeroRotator;
	FRotator SwayRot = FRotator::ZeroRotator;
	FRotator LastControlRot = FRotator::ZeroRotator;
	float LowerAlpha = 0.f;
	float SprintAlpha = 0.f;
	float BobTime = 0.f;
	float RecoilToApply = 0.f;
	float RecoilYawToApply = 0.f;
};
