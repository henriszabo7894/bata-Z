#pragma once

#include "CoreMinimal.h"
#include "Core/BodycamTypes.h"
#include "GameFramework/Character.h"
#include "BodycamZombie.generated.h"

class UAnimSequence;
class USkeletalMesh;
class ABodycamCharacter;

/**
 * Zombie : poursuit le joueur (navigation, ou ligne droite si pas de navmesh),
 * l'attaque selon 5 modes (cheville, dos, face, cou, poussée), meurt en ragdoll.
 * Animations Mixamo jouées directement (pas d'Animation Blueprint).
 */
UCLASS()
class BODYCAMUE_API ABodycamZombie : public ACharacter
{
	GENERATED_BODY()

public:
	ABodycamZombie();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	void Setup(EZombieVariant InVariant, float SpeedMul, float HealthMul);

	bool IsDead() const { return State == EState::Dead; }
	bool IsGrabbingPlayer() const { return State == EState::Grabbing; }
	EZombieVariant GetVariant() const { return Variant; }

	/** Le joueur s'est libéré (ou le zombie est tué) : le zombie lâche prise. */
	void ReleaseGrab(bool bPlayerEscaped);
	/** Le joueur n'a pas réussi à se libérer : morsure au cou. */
	void PlayNeckBite();
	/** Mode horde (batterie à récupérer) : plus rapide. */
	void SetFrenzy(bool bOn);

protected:
	UPROPERTY() TArray<TObjectPtr<USkeletalMesh>> MeshPool;
	UPROPERTY() TArray<float> MeshScales;

	UPROPERTY() TObjectPtr<UAnimSequence> AnimIdle;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimWalk;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimRun;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimCrawl;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimRunCrawl;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimAttack;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimBite;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimBite2;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimNeckBite;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimJumpAttack;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimScream;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimHit;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimDeath;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimDying;

private:
	enum class EState : uint8 { Spawning, Chasing, Attacking, Grabbing, Stagger, Feeding, Dead };

	void SetState(EState NewState);
	void PlayAnim(UAnimSequence* Anim, bool bLoop, float Rate = 1.f);
	/** Recentre le modèle sur la capsule : les animations de marche/course/rampement Mixamo ne sont pas "In Place". */
	void CancelRootDrift();
	void UpdateChase(float DeltaSeconds);
	void UpdateAttack(float DeltaSeconds);
	void UpdateGrab(float DeltaSeconds);
	void BeginAttack();
	void Die(const FVector& ShotDir, bool bHeadshot);
	ABodycamCharacter* GetTarget() const;
	float BaseSpeed() const;

	EState State = EState::Spawning;
	EZombieVariant Variant = EZombieVariant::Walker;
	EZombieAttack PendingAttack = EZombieAttack::None;
	EZombieAttack ActiveGrab = EZombieAttack::None;

	float Health = 100.f;
	float MaxHealth = 100.f;
	float SpeedMultiplier = 1.f;
	float StateTime = 0.f;
	float StateDuration = 0.f;
	float AttackCooldown = 0.f;
	float RepathTimer = 0.f;
	float AttackHitTime = 0.6f;
	bool bAttackResolved = false;
	bool bDirectChase = false;
	bool bFrenzy = false;
	bool bScreamed = false;
	UAnimSequence* CurrentAnim = nullptr;
	float LocoLockTime = 0.f;   // durée minimale avant de changer d'animation de déplacement
	float LowSpeedTime = 0.f;   // temps passé quasiment immobile (pour passer en idle)
	FName HipsBone = NAME_None;
	FVector HipsRefPos = FVector::ZeroVector;
	FVector LastPos = FVector::ZeroVector;
	float StuckTime = 0.f;
	float GroanTimer = 0.f;   // prochain grognement
	float GroanPitch() const;
};
