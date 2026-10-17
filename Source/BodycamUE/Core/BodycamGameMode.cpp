#include "Core/BodycamGameMode.h"
#include "World/BodycamExterior.h"
#include "Gameplay/BodycamSounds.h"
#include "Misc/ConfigCacheIni.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/Light.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/BodycamPickup.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Player/BodycamCharacter.h"
#include "Player/BodycamPlayerController.h"
#include "Sound/SoundBase.h"
#include "UI/BodycamHUD.h"
#include "Weapons/BodycamWeapon.h"
#include "Zombies/BodycamZombie.h"

namespace
{
	const TCHAR* GameMapPath = TEXT("/Game/DerelictCorridor/Maps/DerelictCorridor");

	FString FormatTime(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
	}
}

ABodycamGameMode::ABodycamGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ABodycamCharacter::StaticClass();
	PlayerControllerClass = ABodycamPlayerController::StaticClass();
	HUDClass = ABodycamHUD::StaticClass();
}

void ABodycamGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	LevelIndex = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Lvl"), 1), 1, FBodycamLevelConfig::NumLevels);
	Money = FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("Money"), 0));
	MoneyAtStart = Money;
	Level = FBodycamLevelConfig::Get(LevelIndex);
	TimeLeft = Level.TimeLimit;
}

APawn* ABodycamGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	if (!Level.PlayerStart.IsNearlyZero())
	{
		FActorSpawnParameters Params;
		Params.Instigator = GetInstigator();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FTransform T(FRotator(0.f, Level.PlayerYaw, 0.f), Level.PlayerStart + FVector(0, 0, 95.f));
		if (APawn* Pawn = GetWorld()->SpawnActor<APawn>(GetDefaultPawnClassForController(NewPlayer), T, Params))
		{
			return Pawn;
		}
	}
	return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
}

void ABodycamGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer) return;
	if (!Level.PlayerStart.IsNearlyZero())
	{
		// Départ défini dans DefaultGame.ini [Bodycam.LevelN] (la map n'a pas de PlayerStart).
		// On cherche le sol sous le point configuré (les Z de l'ini n'ont pas besoin d'être exacts).
		FVector Start = Level.PlayerStart;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PlayerFloor), false);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start + FVector(0, 0, 150.f), Start - FVector(0, 0, 800.f), ECC_Visibility, Params))
		{
			Start = Hit.ImpactPoint;
		}
		RestartPlayerAtTransform(NewPlayer, FTransform(FRotator(0.f, Level.PlayerYaw, 0.f), Start + FVector(0, 0, 100.f)));
	}
	else
	{
		Super::RestartPlayer(NewPlayer);
		if (!NewPlayer->GetPawn())
		{
			UE_LOG(LogTemp, Warning, TEXT("Bodycam: aucun PlayerStart et pas de [Bodycam.Level%d] PlayerStart : depart a l'origine."), LevelIndex);
			RestartPlayerAtTransform(NewPlayer, FTransform(FRotator::ZeroRotator, FVector(0, 0, 200.f)));
		}
	}
	if (ABodycamCharacter* Player = NewPlayer ? Cast<ABodycamCharacter>(NewPlayer->GetPawn()) : nullptr)
	{
		Player->EquipWeapon(Level.Weapon, Level.StartReserveMags);
		Player->SetNVGDrainRate(Level.BatteryDrainPerSec);
		if (!Level.PlayerStart.IsNearlyZero())
		{
			NewPlayer->SetControlRotation(FRotator(0.f, Level.PlayerYaw, 0.f));
		}
	}
}

void ABodycamGameMode::BeginPlay()
{
	Super::BeginPlay();

	PrepareWorld();
	MatchState = EBodycamMatchState::Intro;
	StateTimer = 6.f;
	ShowMessage(FString::Printf(TEXT("%s  -  %s"), *Level.Title, *Level.Sector),
		FString::Printf(TEXT("ELIMINEZ %d INFECTES EN %s"), Level.KillQuota, *FormatTime(Level.TimeLimit)), 6.f);

	// Ambiance : vent dans les couloirs.
	if (USoundBase* Wind = LoadObject<USoundBase>(nullptr, TEXT("/Game/StarterContent/Audio/Starter_Wind06.Starter_Wind06")))
	{
		UGameplayStatics::SpawnSound2D(this, Wind, 0.35f, 0.8f);
	}
}

ABodycamCharacter* ABodycamGameMode::GetPlayer() const
{
	return Cast<ABodycamCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
}

void ABodycamGameMode::ShowMessage(const FString& Title, const FString& Sub, float Duration, const FLinearColor& Color)
{
	FBodycamMessage M;
	M.Title = Title;
	M.Sub = Sub;
	M.Duration = Duration;
	M.TimeLeft = Duration;
	M.Color = Color;
	Messages.Reset();
	Messages.Add(M);
}

void ABodycamGameMode::PushEvent(const FString& Text)
{
	LastEvent = Text;
	LastEventAge = 0.f;
}

// ------------------------------------------------------------------ Monde

void ABodycamGameMode::PrepareWorld()
{
	// Plus le niveau est élevé, plus il fait noir.
	const float NightFactor = LevelIndex == 1 ? 0.3f : LevelIndex == 2 ? 0.16f : 0.08f;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (!A || Prepared.Contains(A)) continue;

		const FString ClassName = A->GetClass()->GetName();
		if (ClassName.Contains(TEXT("LevelSequenceActor")) || ClassName.Contains(TEXT("CineCameraActor")))
		{
			Prepared.Add(A);
			A->Destroy();
			continue;
		}

		if (APostProcessVolume* PPV = Cast<APostProcessVolume>(A))
		{
			PPV->bEnabled = false;
			Prepared.Add(A);
			continue;
		}

		if (ASkyLight* Sky = Cast<ASkyLight>(A))
		{
			if (USkyLightComponent* C = Sky->GetLightComponent())
			{
				C->SetIntensity(C->Intensity * 0.04f);
			}
			Prepared.Add(A);
			continue;
		}

		if (ALight* LightActor = Cast<ALight>(A))
		{
			Prepared.Add(A);
			ULightComponent* L = LightActor->GetLightComponent();
			if (!L) continue;
			L->SetCastShadows(false);
			const float Roll = FMath::FRand();
			if (Roll < 0.35f)
			{
				L->SetVisibility(false);  // lampe grillée
			}
			else
			{
				L->SetIntensity(L->Intensity * NightFactor);
				if (Roll < 0.65f)
				{
					FFlicker F;
					F.Light = L;
					F.Base = L->Intensity;
					F.bFaulty = Roll < 0.5f;
					Flickers.Add(F);
				}
			}
		}
	}
}

void ABodycamGameMode::UpdateLights(float DeltaSeconds)
{
	for (FFlicker& F : Flickers)
	{
		ULightComponent* L = F.Light.Get();
		if (!L) continue;
		F.Timer -= DeltaSeconds;
		if (F.Timer > 0.f) continue;
		if (F.bFaulty)
		{
			// Néon défectueux : coupures brèves et irrégulières.
			const bool bOff = FMath::FRand() < 0.35f;
			L->SetIntensity(bOff ? 0.f : F.Base * FMath::FRandRange(0.6f, 1.1f));
			F.Timer = bOff ? FMath::FRandRange(0.04f, 0.25f) : FMath::FRandRange(0.05f, 1.8f);
		}
		else
		{
			L->SetIntensity(F.Base * FMath::FRandRange(0.85f, 1.f));
			F.Timer = FMath::FRandRange(0.03f, 0.12f);
		}
	}
}

// ------------------------------------------------------------------ Boucle

void ABodycamGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (int32 i = Messages.Num() - 1; i >= 0; --i)
	{
		Messages[i].TimeLeft -= DeltaSeconds;
		if (Messages[i].TimeLeft <= 0.f) Messages.RemoveAt(i);
	}
	LastEventAge += DeltaSeconds;

	// La carte se charge par morceaux (World Partition) : on traite les nouveaux éléments.
	PrepareTimer -= DeltaSeconds;
	if (PrepareTimer <= 0.f)
	{
		PrepareTimer = 2.f;
		PrepareWorld();
	}
	UpdateLights(DeltaSeconds);

	Zombies.RemoveAll([](const TObjectPtr<ABodycamZombie>& Z) { return !IsValid(Z) || Z->IsDead(); });

	ABodycamCharacter* Player = GetPlayer();
	BreadcrumbTimer -= DeltaSeconds;
	if (Player && !Player->IsDead() && BreadcrumbTimer <= 0.f)
	{
		BreadcrumbTimer = 0.6f;
		const FVector P = Player->GetActorLocation();
		if (Breadcrumbs.Num() == 0 || FVector::DistSquared(Breadcrumbs.Last(), P) > FMath::Square(250.f))
		{
			Breadcrumbs.Add(P);
			if (Breadcrumbs.Num() > 400) Breadcrumbs.RemoveAt(0);
		}
	}

	DroneCooldown = FMath::Max(0.f, DroneCooldown - DeltaSeconds);
	StateAge += DeltaSeconds;

	// Ambiance : de temps en temps, un grincement de métal / des débris quelque part dans le bâtiment.
	MetalTimer -= DeltaSeconds;
	if (MetalTimer <= 0.f)
	{
		MetalTimer = FMath::FRandRange(30.f, 70.f);
		if (Player)
		{
			const FVector Dir = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
			FBodycamSounds::PlayAt(this, FBodycamSounds::Get(TEXT("Metal_Groan")), Player->GetActorLocation() + Dir * FMath::FRandRange(1200.f, 2500.f) + FVector(0, 0, 200.f),
				0.7f, FMath::FRandRange(0.8f, 1.1f), 500.f, 4000.f);
		}
	}

	switch (MatchState)
	{
	case EBodycamMatchState::Intro:
		StateTimer -= DeltaSeconds;
		if (StateTimer <= 0.f)
		{
			MatchState = EBodycamMatchState::Playing;
			StateAge = 0.f;
			SpawnTimer = 3.f;
		}
		break;

	case EBodycamMatchState::Playing:
		TimeLeft -= DeltaSeconds;
		Elapsed += DeltaSeconds;
		if (TimeLeft <= 0.f)
		{
			TimeLeft = 0.f;
			EndLevel(false);
			break;
		}
		UpdateSpawning(DeltaSeconds);
		UpdateBattery(DeltaSeconds);
		break;

	case EBodycamMatchState::Won:
	case EBodycamMatchState::Lost:
		StateTimer -= DeltaSeconds;
		if (StateTimer <= 0.f)
		{
			if (MatchState == EBodycamMatchState::Won)
			{
				if (LevelIndex < FBodycamLevelConfig::NumLevels) TravelTo(LevelIndex + 1, Money);
				else TravelTo(1, 0);
			}
			else
			{
				TravelTo(LevelIndex, MoneyAtStart);
			}
			StateTimer = 1000.f;
		}
		break;
	}
}

// ------------------------------------------------------------------ Zombies

bool ABodycamGameMode::IsVisibleToPlayer(const FVector& Point) const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager) return false;
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Dir = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector Target = Point + FVector(0, 0, 60.f);
	if (FVector::DotProduct(Dir, (Target - Cam).GetSafeNormal()) < 0.45f) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpawnVis), false, PC->GetPawn());
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Cam, Target, ECC_Visibility, Params);
}

bool ABodycamGameMode::FindSpawnPoint(FVector& Out, float MinDist, float MaxDist, bool bMustBeHidden) const
{
	const ABodycamCharacter* Player = GetPlayer();
	if (!Player) return false;
	const FVector P = Player->GetActorLocation();

	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		for (int32 Try = 0; Try < 14; ++Try)
		{
			FNavLocation Loc;
			if (Nav->GetRandomReachablePointInRadius(P, MaxDist, Loc))
			{
				const float D = FVector::Dist(Loc.Location, P);
				if (D >= MinDist && (!bMustBeHidden || !IsVisibleToPlayer(Loc.Location)))
				{
					Out = Loc.Location + FVector(0, 0, 95.f);
					return true;
				}
			}
		}
	}

	// Sans navmesh : un endroit où le joueur est déjà passé (forcément praticable).
	TArray<int32> Order;
	for (int32 i = 0; i < Breadcrumbs.Num(); ++i) Order.Add(i);
	for (int32 i = Order.Num() - 1; i > 0; --i) Order.Swap(i, FMath::RandRange(0, i));
	for (int32 i : Order)
	{
		const float D = FVector::Dist(Breadcrumbs[i], P);
		if (D >= MinDist && D <= MaxDist * 1.6f && (!bMustBeHidden || !IsVisibleToPlayer(Breadcrumbs[i])))
		{
			Out = Breadcrumbs[i] + FVector(0, 0, 10.f);
			return true;
		}
	}
	return false;
}

void ABodycamGameMode::UpdateSpawning(float DeltaSeconds)
{
	const bool bHorde = IsHordeActive();
	const int32 MaxAlive = Level.MaxAlive + (bHorde ? 6 : 0);
	const float Interval = Level.SpawnInterval * (bHorde ? 0.45f : 1.f);

	SpawnTimer -= DeltaSeconds;
	if (SpawnTimer > 0.f || Zombies.Num() >= MaxAlive) return;
	SpawnTimer = Interval * FMath::FRandRange(0.7f, 1.3f);
	SpawnZombie();
}

void ABodycamGameMode::SpawnZombie()
{
	FVector Loc;
	if (!FindSpawnPoint(Loc, 1100.f, 2800.f, true)) return;

	EZombieVariant Variant = EZombieVariant::Walker;
	const float R = FMath::FRand();
	if (R < Level.CrawlerChance) Variant = EZombieVariant::Crawler;
	else if (R < Level.CrawlerChance + Level.RunnerChance) Variant = EZombieVariant::Runner;

	const ABodycamCharacter* Player = GetPlayer();
	const FRotator Face = Player ? (Player->GetActorLocation() - Loc).GetSafeNormal2D().Rotation() : FRotator::ZeroRotator;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	ABodycamZombie* Z = GetWorld()->SpawnActor<ABodycamZombie>(ABodycamZombie::StaticClass(), Loc, FRotator(0.f, Face.Yaw, 0.f), Params);
	if (!Z) return;
	Z->Setup(Variant, Level.ZombieSpeedMul, Level.ZombieHealthMul);
	Z->SetFrenzy(IsHordeActive());
	Zombies.Add(Z);
}

void ABodycamGameMode::OnZombieKilled(ABodycamZombie* Zombie, bool bHeadshot)
{
	Zombies.Remove(Zombie);
	if (MatchState != EBodycamMatchState::Playing) return;

	++Kills;
	const int32 Reward = Level.KillReward + (bHeadshot ? 10 : 0);
	Money += Reward;
	PushEvent(bHeadshot ? FString::Printf(TEXT("TIR A LA TETE  +%d $"), Reward) : FString::Printf(TEXT("INFECTE ELIMINE  +%d $"), Reward));

	if (Kills >= Level.KillQuota)
	{
		EndLevel(true);
	}
}

// ------------------------------------------------------------------ Piles JVN

void ABodycamGameMode::UpdateBattery(float DeltaSeconds)
{
	ABodycamCharacter* Player = GetPlayer();
	if (!Player) return;

	if (BatteryObjective)
	{
		const float D = FVector::Dist(BatteryObjective->GetActorLocation(), Player->GetActorLocation());
		if (D < BatteryObjective->GetPickupRadius())
		{
			OnBatteryCollected(BatteryObjective);
		}
		return;
	}

	BatteryCheckTimer -= DeltaSeconds;
	if (BatteryCheckTimer <= 0.f)
	{
		BatteryCheckTimer = 2.f;
		if (Player->GetNVGBattery() < 35.f)
		{
			SpawnBattery();
		}
	}
}

void ABodycamGameMode::SpawnBattery()
{
	FVector Loc;
	if (!FindSpawnPoint(Loc, 1600.f, 3800.f, false) && !FindSpawnPoint(Loc, 900.f, 5000.f, false)) return;

	// On pose la pile au sol.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BatteryFloor), false);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Loc + FVector(0, 0, 50), Loc - FVector(0, 0, 400), ECC_Visibility, Params))
	{
		Loc = Hit.ImpactPoint;
	}

	BatteryObjective = GetWorld()->SpawnActor<ABodycamPickup>(ABodycamPickup::StaticClass(), Loc, FRotator::ZeroRotator);
	if (!BatteryObjective) return;
	BatteryObjective->InitPickup(EBodycamPickupType::Battery);

	for (ABodycamZombie* Z : Zombies) if (Z) Z->SetFrenzy(true);
	ShowMessage(TEXT("BATTERIE JVN FAIBLE"), TEXT("UNE PILE EST SIGNALEE  -  LA HORDE ARRIVE"), 4.f, FLinearColor(1.f, 0.3f, 0.2f));
}

void ABodycamGameMode::OnBatteryCollected(ABodycamPickup* Battery)
{
	if (ABodycamCharacter* Player = GetPlayer())
	{
		Player->RechargeNVG();
	}
	if (Battery) Battery->Destroy();
	BatteryObjective = nullptr;
	BatteryCheckTimer = 6.f;
	for (ABodycamZombie* Z : Zombies) if (Z) Z->SetFrenzy(false);
	PushEvent(TEXT("PILE RECUPEREE  -  JVN 100%"));
}

// ------------------------------------------------------------------ Munitions / drone

bool ABodycamGameMode::RequestDroneLaunch()
{
	if (MatchState != EBodycamMatchState::Playing && MatchState != EBodycamMatchState::Intro) return false;

	if (DroneCooldown > 0.f)
	{
		PushEvent(FString::Printf(TEXT("NOUVEAU DRONE DANS %s"), *FormatTime(DroneCooldown)));
		return false;
	}
	bool bFree = true;
	if (GConfig) GConfig->GetBool(TEXT("Bodycam.Drone"), TEXT("FreeOrders"), bFree, GGameIni);
	if (!bOrderPending && bFree)
	{
		bOrderPending = true;
		SpawnSupplyCrate();
		PushEvent(FString::Printf(TEXT("COMMANDE : %d CHARGEURS  -  CAISSE DEHORS"), Level.AmmoOrderMags));
	}
	else if (!bOrderPending)
	{
		if (Money < Level.AmmoOrderCost)
		{
			PushEvent(FString::Printf(TEXT("FONDS INSUFFISANTS  (%d $ requis)"), Level.AmmoOrderCost));
			return false;
		}
		Money -= Level.AmmoOrderCost;
		bOrderPending = true;
		SpawnSupplyCrate();
		PushEvent(FString::Printf(TEXT("COMMANDE : %d CHARGEURS  -%d $"), Level.AmmoOrderMags, Level.AmmoOrderCost));
	}
	return true;
}

void ABodycamGameMode::SpawnSupplyCrate()
{
	if (SupplyCrate) return;

	FVector Loc = Level.SupplySite;
	FVector Outside;
	if (ABodycamExterior::GetCrateSite(Outside)) Loc = Outside;   // caisse posée dans la zone extérieure
	if (Loc.IsNearlyZero())
	{
		// Sans site configuré : le point connu le plus éloigné du joueur.
		const ABodycamCharacter* Player = GetPlayer();
		float Best = -1.f;
		for (const FVector& B : Breadcrumbs)
		{
			const float D = Player ? FVector::Dist(B, Player->GetActorLocation()) : 0.f;
			if (D > Best) { Best = D; Loc = B; }
		}
		if (Best < 1500.f)
		{
			FVector Alt;
			if (FindSpawnPoint(Alt, 2000.f, 5000.f, false)) Loc = Alt;
		}
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrateFloor), false);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Loc + FVector(0, 0, 150), Loc - FVector(0, 0, 800), ECC_Visibility, Params))
	{
		Loc = Hit.ImpactPoint;
	}

	SupplyCrate = GetWorld()->SpawnActor<ABodycamPickup>(ABodycamPickup::StaticClass(), Loc, FRotator::ZeroRotator);
	if (SupplyCrate) SupplyCrate->InitPickup(EBodycamPickupType::AmmoCrate);
}

void ABodycamGameMode::OnCratePickedUp()
{
	if (SupplyCrate) SupplyCrate->Destroy();
	SupplyCrate = nullptr;
	PushEvent(TEXT("CAISSE RECUPEREE  -  RETOUR AU JOUEUR"));
}

void ABodycamGameMode::OnAmmoDelivered()
{
	bOrderPending = false;
	if (ABodycamCharacter* Player = GetPlayer())
	{
		if (ABodycamWeapon* W = Player->GetWeapon())
		{
			W->AddReserveMagazines(Level.AmmoOrderMags);
		}
	}
	ShowMessage(TEXT("MUNITIONS LIVREES"), FString::Printf(TEXT("+%d CHARGEURS"), Level.AmmoOrderMags), 2.5f, FLinearColor(0.5f, 1.f, 0.5f));
}

void ABodycamGameMode::OnDroneDestroyed(bool bWasCarrying)
{
	DroneCooldown = 120.f;
	if (GConfig) GConfig->GetFloat(TEXT("Bodycam.Drone"), TEXT("Cooldown"), DroneCooldown, GGameIni);
	const FString Wait = FString::Printf(TEXT("NOUVEAU DRONE DANS %s"), *FormatTime(DroneCooldown));
	if (bWasCarrying)
	{
		// La caisse retombe : on la remet à son emplacement pour le prochain drone.
		ShowMessage(TEXT("DRONE DETRUIT"), TEXT("CAISSE PERDUE  -  ") + Wait, 4.f, FLinearColor(1.f, 0.3f, 0.2f));
		SpawnSupplyCrate();
	}
	else
	{
		ShowMessage(TEXT("DRONE DETRUIT"), TEXT("LA CAISSE ATTEND  -  ") + Wait, 4.f, FLinearColor(1.f, 0.3f, 0.2f));
	}
}

// ------------------------------------------------------------------ Joueur / fin

void ABodycamGameMode::OnPlayerHit(int32 HitsTaken, int32 MaxHits)
{
	PushEvent(FString::Printf(TEXT("BLESSURE  %d / %d"), HitsTaken, MaxHits));
}

void ABodycamGameMode::OnPlayerDied()
{
	if (MatchState == EBodycamMatchState::Lost) return;
	MatchState = EBodycamMatchState::Lost;
	// Laisse le temps de voir la chute, puis l'écran de mort ; relance auto au bout de 20 s.
	StateTimer = 20.f;
	StateAge = 0.f;
}

void ABodycamGameMode::RetryNow()
{
	if (MatchState == EBodycamMatchState::Lost && StateAge > 3.f)
	{
		StateTimer = 1000.f;
		TravelTo(LevelIndex, MoneyAtStart);
	}
}

void ABodycamGameMode::EndLevel(bool bWon)
{
	if (MatchState == EBodycamMatchState::Won || MatchState == EBodycamMatchState::Lost) return;
	MatchState = bWon ? EBodycamMatchState::Won : EBodycamMatchState::Lost;
	StateTimer = 7.f;
	StateAge = 0.f;

	if (bWon)
	{
		const bool bLast = LevelIndex >= FBodycamLevelConfig::NumLevels;
		if (bLast) StateTimer = 12.f;
		ShowMessage(bLast ? TEXT("DEMO TERMINEE") : TEXT("SECTEUR NETTOYE"),
			bLast ? FString::Printf(TEXT("3 SECTEURS NETTOYES  -  %d $"), Money)
				  : FString::Printf(TEXT("TEMPS RESTANT %s  -  PASSAGE AU NIVEAU %d"), *FormatTime(TimeLeft), LevelIndex + 1),
			StateTimer, FLinearColor(0.55f, 1.f, 0.55f));
	}
	else
	{
		ShowMessage(TEXT("TEMPS ECOULE"), FString::Printf(TEXT("%d / %d INFECTES  -  NOUVELLE TENTATIVE"), Kills, Level.KillQuota), StateTimer, FLinearColor(0.9f, 0.15f, 0.1f));
	}
}

void ABodycamGameMode::TravelTo(int32 InLevelIndex, int32 InMoney)
{
	UGameplayStatics::OpenLevel(this, FName(GameMapPath), true, FString::Printf(TEXT("Lvl=%d?Money=%d"), InLevelIndex, InMoney));
}
