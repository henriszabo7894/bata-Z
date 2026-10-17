#include "Zombies/BodycamZombie.h"

#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/BodycamGameMode.h"
#include "Gameplay/BodycamSounds.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "Player/BodycamCharacter.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr ECollisionChannel WeaponChannel = ECC_GameTraceChannel2;

	UAnimSequence* FindAnim(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("/Game/Bodycam/Characters/Zombie/Anims/%s.%s"), Name, Name);
		return LoadObject<UAnimSequence>(nullptr, *Path);
	}
}

ABodycamZombie::ABodycamZombie()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(32.f, 88.f);
	// Les balles touchent la capsule (fiable même si le modèle n'a pas de corps physiques) ;
	// la zone touchée (tête / corps / jambes) est déduite de la hauteur de l'impact.
	GetCapsuleComponent()->SetCollisionResponseToChannel(WeaponChannel, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	USkeletalMeshComponent* M = GetMesh();
	M->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -88.f), FRotator(0.f, -90.f, 0.f));
	M->SetCollisionProfileName(TEXT("CharacterMesh"));
	M->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	M->SetCollisionResponseToChannel(WeaponChannel, ECR_Block);
	M->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	M->SetGenerateOverlapEvents(false);
	M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	M->bCastDynamicShadow = false;
	M->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 280.f, 0.f);
	Move->MaxWalkSpeed = 140.f;
	Move->bUseRVOAvoidance = true;
	Move->AvoidanceConsiderationRadius = 250.f;
	Move->MaxAcceleration = 900.f;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	SetCanBeDamaged(true);

	// Modèles Mixamo (même squelette). Échelle : ramener chacun à ~1,80 m.
	struct FMeshEntry { const TCHAR* Path; float Scale; };
	static const FMeshEntry Entries[] = {
		{ TEXT("/Game/Bodycam/Characters/Zombie/SK_Zombie_Ch10.SK_Zombie_Ch10"), 1.0f },
		{ TEXT("/Game/Bodycam/Characters/Zombie/Parasite/SK_Zombie_Parasite.SK_Zombie_Parasite"), 0.88f },
		{ TEXT("/Game/Bodycam/Characters/Zombie/Pumpkinhulk/SK_Zombie_Pumpkinhulk.SK_Zombie_Pumpkinhulk"), 0.95f },
		{ TEXT("/Game/Bodycam/Characters/Zombie/Skeletonzombie/SK_Zombie_Skeletonzombie.SK_Zombie_Skeletonzombie"), 0.87f },
		{ TEXT("/Game/Bodycam/Characters/Zombie/Warzombie/SK_Zombie_Warzombie.SK_Zombie_Warzombie"), 0.86f },
		{ TEXT("/Game/Bodycam/Characters/Zombie/Zombiegirl/SK_Zombie_Zombiegirl.SK_Zombie_Zombiegirl"), 0.84f },
	};
	for (const FMeshEntry& E : Entries)
	{
		ConstructorHelpers::FObjectFinder<USkeletalMesh> Finder(E.Path);
		if (Finder.Succeeded())
		{
			MeshPool.Add(Finder.Object);
			MeshScales.Add(E.Scale);
		}
	}
}

void ABodycamZombie::BeginPlay()
{
	Super::BeginPlay();

	AnimIdle = FindAnim(TEXT("A_Z_ZombieIdle"));
	AnimWalk = FindAnim(TEXT("A_Z_ZombieWalk"));
	AnimRun = FindAnim(TEXT("A_Z_ZombieRun"));
	AnimCrawl = FindAnim(TEXT("A_Z_ZombieCrawl"));
	AnimRunCrawl = FindAnim(TEXT("A_Z_RunningCrawl"));
	AnimAttack = FindAnim(TEXT("A_Z_ZombieAttack"));
	AnimBite = FindAnim(TEXT("A_Z_ZombieBiting"));
	AnimBite2 = FindAnim(TEXT("A_Z_ZombieBiting2"));
	AnimNeckBite = FindAnim(TEXT("A_Z_ZombieNeckBite"));
	AnimJumpAttack = FindAnim(TEXT("A_Z_JumpAttack"));
	AnimScream = FindAnim(TEXT("A_Z_ZombieScream"));
	AnimHit = FindAnim(TEXT("A_Z_ZombieReactionHit"));
	AnimDeath = FindAnim(TEXT("A_Z_ZombieDeath"));
	AnimDying = FindAnim(TEXT("A_Z_ZombieDying"));

	if (MeshPool.Num() > 0 && !GetMesh()->GetSkeletalMeshAsset())
	{
		const int32 Pick = FMath::RandRange(0, MeshPool.Num() - 1);
		GetMesh()->SetSkeletalMesh(MeshPool[Pick]);
		GetMesh()->SetRelativeScale3D(FVector(MeshScales[Pick]));
	}

	// Position du bassin dans la pose de repos (espace composant) : référence pour annuler la dérive des animations.
	if (const USkeletalMesh* SM = GetMesh()->GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& Ref = SM->GetRefSkeleton();
		const TArray<FTransform>& Pose = Ref.GetRefBonePose();
		for (int32 i = 0; i < Ref.GetNum(); ++i)
		{
			if (Ref.GetBoneName(i).ToString().EndsWith(TEXT("Hips")))
			{
				HipsBone = Ref.GetBoneName(i);
				FTransform T = Pose[i];
				for (int32 P = Ref.GetParentIndex(i); P != INDEX_NONE; P = Ref.GetParentIndex(P))
				{
					T = T * Pose[P];
				}
				HipsRefPos = T.GetLocation();
				break;
			}
		}
	}
	// Notre Tick doit passer APRÈS l'évaluation de l'animation, sinon le recentrage a une image de retard.
	AddTickPrerequisiteComponent(GetMesh());

	LastPos = GetActorLocation();
	SetState(EState::Spawning);
}

void ABodycamZombie::CancelRootDrift()
{
	USkeletalMeshComponent* M = GetMesh();
	if (HipsBone.IsNone() || M->IsSimulatingPhysics()) return;

	const int32 Idx = M->GetBoneIndex(HipsBone);
	const TArray<FTransform>& CS = M->GetComponentSpaceTransforms();
	if (!CS.IsValidIndex(Idx)) return;

	// Dérive horizontale du bassin par rapport à la pose de repos, ramenée dans le repère de la capsule.
	const FVector Hips = CS[Idx].GetLocation();
	const FVector Drift(Hips.X - HipsRefPos.X, Hips.Y - HipsRefPos.Y, 0.f);
	const FVector Offset = M->GetRelativeRotation().RotateVector(Drift * M->GetRelativeScale3D());
	const float Z = M->GetRelativeLocation().Z;
	M->SetRelativeLocation(FVector(-Offset.X, -Offset.Y, Z));
}

float ABodycamZombie::GroanPitch() const
{
	// Voix différente selon le type : rampant plus grave, coureur plus aigu.
	const float Base = Variant == EZombieVariant::Crawler ? 0.82f : Variant == EZombieVariant::Runner ? 1.1f : 0.95f;
	return Base + (GetUniqueID() % 7) * 0.015f;
}

void ABodycamZombie::Setup(EZombieVariant InVariant, float SpeedMul, float HealthMul)
{
	Variant = InVariant;
	GroanTimer = FMath::FRandRange(0.5f, 6.f);
	SpeedMultiplier = SpeedMul;
	MaxHealth = (Variant == EZombieVariant::Runner ? 80.f : Variant == EZombieVariant::Crawler ? 70.f : 110.f) * HealthMul;
	Health = MaxHealth;
	GetCharacterMovement()->MaxWalkSpeed = BaseSpeed();
	if (Variant == EZombieVariant::Crawler)
	{
		// Un rampant est plus bas : la capsule rétrécit (on peut lui tirer dessus en regardant le sol).
		GetCapsuleComponent()->SetCapsuleHalfHeight(45.f);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -45.f));
	}
}

float ABodycamZombie::BaseSpeed() const
{
	float S = 140.f;
	switch (Variant)
	{
	case EZombieVariant::Runner:  S = FMath::FRandRange(360.f, 420.f); break;
	case EZombieVariant::Crawler: S = 95.f; break;
	default:                      S = FMath::FRandRange(115.f, 165.f); break;
	}
	return S * SpeedMultiplier * (bFrenzy ? 1.35f : 1.f);
}

void ABodycamZombie::SetFrenzy(bool bOn)
{
	if (bFrenzy == bOn || IsDead()) return;
	bFrenzy = bOn;
	GetCharacterMovement()->MaxWalkSpeed = BaseSpeed();
}

ABodycamCharacter* ABodycamZombie::GetTarget() const
{
	return Cast<ABodycamCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
}

void ABodycamZombie::PlayAnim(UAnimSequence* Anim, bool bLoop, float Rate)
{
	if (!Anim || (IsDead() && Anim != AnimDeath && Anim != AnimDying)) return;
	if (Anim == CurrentAnim && bLoop)
	{
		GetMesh()->SetPlayRate(Rate);
		return;
	}
	CurrentAnim = Anim;
	GetMesh()->PlayAnimation(Anim, bLoop);
	GetMesh()->SetPlayRate(Rate);
}

void ABodycamZombie::SetState(EState NewState)
{
	State = NewState;
	StateTime = 0.f;
	bAttackResolved = false;
	LocoLockTime = 0.f;
	LowSpeedTime = 0.f;

	switch (State)
	{
	case EState::Spawning:
		StateDuration = FMath::FRandRange(0.3f, 0.9f);
		PlayAnim(Variant == EZombieVariant::Crawler ? AnimCrawl : AnimIdle, true, Variant == EZombieVariant::Crawler ? 0.3f : 1.f);
		break;
	case EState::Chasing:
		RepathTimer = 0.f;
		break;
	case EState::Stagger:
		GetCharacterMovement()->StopMovementImmediately();
		if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
		PlayAnim(Variant == EZombieVariant::Crawler ? AnimCrawl : AnimHit, Variant == EZombieVariant::Crawler, 1.2f);
		break;
	case EState::Feeding:
		if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
		PlayAnim(AnimBite, true, 1.f);
		break;
	default:
		break;
	}
}

void ABodycamZombie::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsDead()) return;

	CancelRootDrift();

	StateTime += DeltaSeconds;
	AttackCooldown -= DeltaSeconds;

	// Grognements réguliers (plus fréquents en frénésie).
	GroanTimer -= DeltaSeconds;
	if (GroanTimer <= 0.f)
	{
		GroanTimer = bFrenzy ? FMath::FRandRange(2.f, 5.f) : FMath::FRandRange(4.f, 10.f);
		if (State != EState::Grabbing && State != EState::Attacking)
		{
			FBodycamSounds::PlayAt(this, FBodycamSounds::GetRandom(TEXT("Z_Groan"), 6), GetActorLocation() + FVector(0, 0, 60.f), 0.85f, GroanPitch(), 300.f, 2200.f);
		}
	}

	switch (State)
	{
	case EState::Spawning:
		if (StateTime >= StateDuration)
		{
			// Certains coureurs hurlent avant de charger.
			if (Variant == EZombieVariant::Runner && !bScreamed && FMath::FRand() < 0.5f && AnimScream)
			{
				bScreamed = true;
				PlayAnim(AnimScream, false, 1.3f);
				FBodycamSounds::PlayAt(this, FBodycamSounds::GetRandom(TEXT("Z_Scream"), 4), GetActorLocation() + FVector(0, 0, 60.f), 1.f, FMath::FRandRange(0.95f, 1.08f), 600.f, 3500.f);
				StateDuration = AnimScream->GetPlayLength() / 1.3f;
				StateTime = 0.f;
			}
			else
			{
				SetState(EState::Chasing);
			}
		}
		break;
	case EState::Chasing:
		UpdateChase(DeltaSeconds);
		break;
	case EState::Attacking:
		UpdateAttack(DeltaSeconds);
		break;
	case EState::Grabbing:
		UpdateGrab(DeltaSeconds);
		break;
	case EState::Stagger:
		if (StateTime >= (Variant == EZombieVariant::Crawler ? 0.6f : 1.1f))
		{
			SetState(EState::Chasing);
		}
		break;
	case EState::Feeding:
		break;
	default:
		break;
	}
}

void ABodycamZombie::UpdateChase(float DeltaSeconds)
{
	ABodycamCharacter* Target = GetTarget();
	if (!Target) return;
	if (Target->IsDead())
	{
		SetState(EState::Feeding);
		return;
	}
	// Joueur immobile sous le casque du drone : les zombies perdent sa trace et attendent sur place.
	if (Target->IsPilotingDrone() || Target->IsDeployingDrone())
	{
		if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
		RepathTimer = 0.f;
		LowSpeedTime = 1.f;
		if (CurrentAnim != AnimIdle && Variant != EZombieVariant::Crawler) PlayAnim(AnimIdle, true, 1.f);
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const float Dist2D = ToTarget.Size2D();

	// Animation de déplacement : choisie selon la vitesse VOULUE (stable), pas la vitesse instantanée qui oscille
	// (les zombies changeaient d'animation à chaque image). Une animation est gardée au moins 0,5 s.
	const float Speed = GetVelocity().Size2D();
	LowSpeedTime = Speed < 15.f ? LowSpeedTime + DeltaSeconds : 0.f;
	const float Wanted = GetCharacterMovement()->MaxWalkSpeed;
	UAnimSequence* Desired = AnimWalk;
	float Rate = 1.f;
	if (Variant == EZombieVariant::Crawler)
	{
		Desired = Wanted > 140.f ? AnimRunCrawl : AnimCrawl;
		Rate = FMath::Clamp(Wanted / 90.f, 0.4f, 1.6f);
	}
	else if (Wanted > 260.f)
	{
		Desired = AnimRun;
		Rate = FMath::Clamp(Wanted / 400.f, 0.7f, 1.4f);
	}
	else
	{
		Desired = AnimWalk;
		Rate = FMath::Clamp(Wanted / 130.f, 0.6f, 1.5f);
	}
	if (LowSpeedTime > 0.8f && Variant != EZombieVariant::Crawler)
	{
		Desired = AnimIdle;
		Rate = 1.f;
	}

	LocoLockTime -= DeltaSeconds;
	if (Desired == CurrentAnim || LocoLockTime <= 0.f)
	{
		if (Desired != CurrentAnim) LocoLockTime = 0.5f;
		PlayAnim(Desired, true, Rate);
	}

	// Attaque
	const float Range = Variant == EZombieVariant::Crawler ? 115.f : 135.f;
	if (Dist2D < Range && FMath::Abs(ToTarget.Z) < 130.f && AttackCooldown <= 0.f)
	{
		BeginAttack();
		return;
	}

	// Navigation (recalculée régulièrement). Sans navmesh : ligne droite vers le joueur.
	RepathTimer -= DeltaSeconds;
	if (RepathTimer <= 0.f)
	{
		RepathTimer = Dist2D > 1500.f ? 1.f : 0.35f;
		bDirectChase = true;
		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			const EPathFollowingRequestResult::Type Res = AI->MoveToActor(Target, 40.f, true, true, false, nullptr, true);
			bDirectChase = Res == EPathFollowingRequestResult::Failed;
		}
	}
	if (bDirectChase)
	{
		AddMovementInput(ToTarget.GetSafeNormal2D(), 1.f);
	}

	// Bloqué contre un obstacle depuis longtemps : petit saut de côté.
	if (FVector::DistSquared2D(LastPos, GetActorLocation()) < FMath::Square(5.f))
	{
		StuckTime += DeltaSeconds;
		if (StuckTime > 2.f)
		{
			StuckTime = 0.f;
			LaunchCharacter(FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal() * 250.f + FVector(0, 0, 150.f), true, true);
		}
	}
	else
	{
		StuckTime = 0.f;
	}
	LastPos = GetActorLocation();
}

void ABodycamZombie::BeginAttack()
{
	ABodycamCharacter* Target = GetTarget();
	if (!Target) return;

	if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
	GetCharacterMovement()->StopMovementImmediately();
	FBodycamSounds::PlayAt(this, FBodycamSounds::Get(TEXT("Z_Attack")), GetActorLocation() + FVector(0, 0, 60.f), 1.f, GroanPitch() * FMath::FRandRange(0.95f, 1.05f), 300.f, 2000.f);

	// Choix du mode d'attaque selon la position par rapport au joueur.
	const FVector FromPlayer = (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	const float FrontDot = FVector::DotProduct(Target->GetActorForwardVector().GetSafeNormal2D(), FromPlayer);

	if (Variant == EZombieVariant::Crawler)
	{
		PendingAttack = EZombieAttack::Ankle;
	}
	else if (FrontDot < -0.3f)
	{
		PendingAttack = FMath::FRand() < 0.6f ? EZombieAttack::Back : EZombieAttack::Push;
	}
	else
	{
		PendingAttack = FMath::FRand() < 0.55f ? EZombieAttack::Push : EZombieAttack::Front;
	}

	UAnimSequence* Anim = AnimAttack;
	float Rate = 1.3f;
	AttackHitTime = 0.75f;
	switch (PendingAttack)
	{
	case EZombieAttack::Back:  Anim = AnimJumpAttack ? AnimJumpAttack : AnimAttack; Rate = 1.4f; AttackHitTime = 0.9f; break;
	case EZombieAttack::Front: Anim = AnimBite2 ? AnimBite2 : AnimAttack; Rate = 1.2f; AttackHitTime = 0.55f; break;
	case EZombieAttack::Ankle: Anim = AnimCrawl; Rate = 1.6f; AttackHitTime = 0.6f; break;
	default: break;
	}

	// Face au joueur
	FRotator Face = (-FromPlayer).Rotation();
	SetActorRotation(FRotator(0.f, Face.Yaw, 0.f));
	PlayAnim(Anim, PendingAttack == EZombieAttack::Ankle, Rate);
	SetState(EState::Attacking);
}

void ABodycamZombie::UpdateAttack(float DeltaSeconds)
{
	ABodycamCharacter* Target = GetTarget();
	if (!Target)
	{
		SetState(EState::Chasing);
		return;
	}

	if (!bAttackResolved && StateTime >= AttackHitTime)
	{
		bAttackResolved = true;
		const float Dist2D = FVector::Dist2D(Target->GetActorLocation(), GetActorLocation());
		const float Reach = PendingAttack == EZombieAttack::Back ? 220.f : 170.f;
		bool bLanded = false;
		if (Dist2D < Reach)
		{
			bLanded = Target->ReceiveZombieAttack(this, PendingAttack);
		}

		if (bLanded && (PendingAttack == EZombieAttack::Front || PendingAttack == EZombieAttack::Back || PendingAttack == EZombieAttack::Ankle))
		{
			ActiveGrab = PendingAttack;
			State = EState::Grabbing;
			StateTime = 0.f;
			GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			GetCharacterMovement()->SetMovementMode(MOVE_None);
			if (ActiveGrab == EZombieAttack::Ankle) PlayAnim(AnimCrawl, true, 0.35f);
			else PlayAnim(ActiveGrab == EZombieAttack::Front ? AnimBite : AnimBite2, true, 1.f);
			return;
		}
		AttackCooldown = bLanded ? 2.2f : 1.f;
	}

	const float Dur = CurrentAnim ? CurrentAnim->GetPlayLength() / FMath::Max(GetMesh()->GetPlayRate(), 0.1f) : 1.5f;
	if (StateTime >= FMath::Min(Dur, 2.f))
	{
		SetState(EState::Chasing);
	}
}

void ABodycamZombie::UpdateGrab(float DeltaSeconds)
{
	ABodycamCharacter* Target = GetTarget();
	if (!Target)
	{
		ReleaseGrab(false);
		return;
	}

	// Le zombie reste collé au joueur (traîné s'il agrippe la cheville).
	const FVector PFwd = Target->GetActorForwardVector().GetSafeNormal2D();
	const FVector PLoc = Target->GetActorLocation();
	FVector Desired = PLoc;
	float Yaw = 0.f;
	switch (ActiveGrab)
	{
	case EZombieAttack::Front: Desired = PLoc + PFwd * 72.f; Yaw = (-PFwd).Rotation().Yaw; break;   // assez loin pour voir les mains qui le repoussent
	case EZombieAttack::Back:  Desired = PLoc - PFwd * 42.f; Yaw = PFwd.Rotation().Yaw; break;
	case EZombieAttack::Ankle: Desired = PLoc + PFwd * 70.f + FVector(0, 0, -40.f); Yaw = (-PFwd).Rotation().Yaw; break;
	default: break;
	}
	const FVector NewLoc = FMath::VInterpTo(GetActorLocation(), Desired, DeltaSeconds, 12.f);
	SetActorLocationAndRotation(NewLoc, FRotator(0.f, Yaw, 0.f));
}

void ABodycamZombie::ReleaseGrab(bool bPlayerEscaped)
{
	if (State != EState::Grabbing) return;
	ActiveGrab = EZombieAttack::None;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	AttackCooldown = bPlayerEscaped ? 4.f : 3.f;
	SetState(EState::Stagger);
	if (bPlayerEscaped)
	{
		if (ABodycamCharacter* Target = GetTarget())
		{
			// Repoussé : on le décolle d'abord du joueur (sinon les capsules restent imbriquées), puis on le projette.
			FVector Away = (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			if (Away.IsNearlyZero()) Away = Target->GetActorForwardVector().GetSafeNormal2D();
			SetActorLocation(Target->GetActorLocation() + Away * 95.f + FVector(0.f, 0.f, 5.f), true);
			LaunchCharacter(Away * 650.f + FVector(0, 0, 160.f), true, true);
			UE_LOG(LogTemp, Log, TEXT("Zombie repousse : %s"), *GetName());
		}
	}
}

void ABodycamZombie::PlayNeckBite()
{
	State = EState::Grabbing;
	ActiveGrab = EZombieAttack::Front;
	PlayAnim(AnimNeckBite ? AnimNeckBite : AnimBite, false, 1.f);
}

float ABodycamZombie::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (IsDead()) return 0.f;

	float Mult = 1.f;
	bool bHeadshot = false;
	FVector ShotDir = FVector::ZeroVector;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& Point = static_cast<const FPointDamageEvent&>(DamageEvent);
		ShotDir = Point.ShotDirection;
		const FString Bone = Point.HitInfo.BoneName.ToString();
		// Hauteur relative de l'impact dans la capsule : 0 = pieds, 1 = sommet du crâne.
		const UCapsuleComponent* Cap = GetCapsuleComponent();
		const float Half = Cap->GetScaledCapsuleHalfHeight();
		const float Rel = (Point.HitInfo.ImpactPoint.Z - (Cap->GetComponentLocation().Z - Half)) / (2.f * Half);
		const bool bCrawler = Variant == EZombieVariant::Crawler;
		if (Bone.Contains(TEXT("Head")) || Bone.Contains(TEXT("Neck")) || (!bCrawler && Rel > 0.83f) || (bCrawler && Rel > 0.55f))
		{
			Mult = 3.f;
			bHeadshot = true;
		}
		else if (Bone.Contains(TEXT("Leg")) || Bone.Contains(TEXT("Foot")) || Bone.Contains(TEXT("Arm")) || Bone.Contains(TEXT("Hand")) || (!bCrawler && Rel < 0.45f))
		{
			Mult = 0.6f;
		}
		UE_LOG(LogTemp, Log, TEXT("Zombie touche : os=%s hauteur=%.2f x%.1f vie=%.0f"), *Bone, Rel, Mult, Health - DamageAmount * Mult);
	}

	// Impact visible : la balle repousse le zombie (plus fort à la tête).
	if (!ShotDir.IsNearlyZero() && State != EState::Grabbing)
	{
		const FVector Push = ShotDir.GetSafeNormal2D() * (bHeadshot ? 320.f : 220.f) * FMath::Clamp(DamageAmount / 30.f, 0.4f, 1.3f);
		LaunchCharacter(Push, true, false);
	}

	const float Applied = Super::TakeDamage(DamageAmount * Mult, DamageEvent, EventInstigator, DamageCauser);
	Health -= Applied;

	if (Health <= 0.f)
	{
		Die(ShotDir, bHeadshot);
	}
	else if (State == EState::Chasing && (bHeadshot || FMath::FRand() < 0.6f))
	{
		SetState(EState::Stagger);
	}
	return Applied;
}

void ABodycamZombie::Die(const FVector& ShotDir, bool bHeadshot)
{
	const bool bWasGrabbing = State == EState::Grabbing;
	State = EState::Dead;
	FBodycamSounds::PlayAt(this, FBodycamSounds::GetRandom(TEXT("Z_Groan"), 6), GetActorLocation() + FVector(0, 0, 40.f), 0.7f, GroanPitch() * 0.75f, 300.f, 1800.f);

	if (bWasGrabbing)
	{
		if (ABodycamCharacter* Target = GetTarget())
		{
			Target->OnGrabberKilled(this);
		}
	}

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
		AI->UnPossess();
		AI->Destroy();
	}
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Ragdoll s'il y a un Physics Asset, sinon animation de mort.
	USkeletalMeshComponent* M = GetMesh();
	if (M->GetPhysicsAsset())
	{
		M->SetCollisionProfileName(TEXT("Ragdoll"));
		M->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		M->SetCollisionResponseToChannel(WeaponChannel, ECR_Ignore);
		M->SetAllBodiesSimulatePhysics(true);
		M->SetSimulatePhysics(true);
		M->WakeAllRigidBodies();
		if (!ShotDir.IsNearlyZero())
		{
			M->AddImpulse(ShotDir * (bHeadshot ? 6000.f : 4000.f), NAME_None, true);
		}
	}
	else
	{
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PlayAnim(FMath::RandBool() ? AnimDeath : AnimDying, false, 1.f);
	}
	SetLifeSpan(20.f);

	if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>())
	{
		GM->OnZombieKilled(this, bHeadshot);
	}
}
