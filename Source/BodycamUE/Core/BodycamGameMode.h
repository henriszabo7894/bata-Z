#pragma once

#include "CoreMinimal.h"
#include "Core/BodycamTypes.h"
#include "GameFramework/GameModeBase.h"
#include "BodycamGameMode.generated.h"

class ABodycamZombie;
class ABodycamPickup;
class ABodycamCharacter;
class ULightComponent;

/** Message central affiché par le HUD (titre de niveau, objectif, alerte...). */
struct FBodycamMessage
{
	FString Title;
	FString Sub;
	float TimeLeft = 0.f;
	float Duration = 0.f;
	FLinearColor Color = FLinearColor::White;
};

/**
 * Règles d'une partie : 10 minutes pour tuer le quota de zombies, argent par zombie,
 * apparition des zombies hors de vue, piles de la vision nocturne à aller chercher,
 * commande de munitions livrées par drone, enchaînement des 3 niveaux.
 * Niveau et argent passent d'une carte à l'autre par les options d'URL (?Lvl=2?Money=340).
 */
UCLASS()
class BODYCAMUE_API ABodycamGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABodycamGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	// --- Événements ---
	void OnZombieKilled(ABodycamZombie* Zombie, bool bHeadshot);
	void OnPlayerDied();
	/** Écran de mort : ENTRÉE relance le niveau tout de suite. */
	void RetryNow();
	void OnBatteryCollected(ABodycamPickup* Battery);
	void OnCratePickedUp();
	void OnAmmoDelivered();
	void OnDroneDestroyed(bool bWasCarrying);
	void OnPlayerHit(int32 HitsTaken, int32 MaxHits);

	/** Téléphone : commander des munitions et/ou lancer le drone. Renvoie true si le drone doit décoller. */
	bool RequestDroneLaunch();

	// --- État pour le HUD ---
	EBodycamMatchState GetMatchState() const { return MatchState; }
	const FBodycamLevelConfig& GetLevel() const { return Level; }
	float GetTimeLeft() const { return TimeLeft; }
	float GetStateTimer() const { return StateTimer; }
	float GetStateAge() const { return StateAge; }
	float GetElapsed() const { return Elapsed; }
	int32 GetKills() const { return Kills; }
	int32 GetMoney() const { return Money; }
	int32 GetAliveZombies() const { return Zombies.Num(); }
	float GetDroneCooldown() const { return DroneCooldown; }
	bool HasPendingOrder() const { return bOrderPending; }
	ABodycamPickup* GetSupplyCrate() const { return SupplyCrate; }
	ABodycamPickup* GetBattery() const { return BatteryObjective; }
	bool IsHordeActive() const { return BatteryObjective != nullptr; }
	const TArray<FBodycamMessage>& GetMessages() const { return Messages; }
	const FString& GetLastEvent() const { return LastEvent; }
	float GetLastEventAge() const { return LastEventAge; }

	void ShowMessage(const FString& Title, const FString& Sub, float Duration, const FLinearColor& Color = FLinearColor(0.95f, 0.75f, 0.35f));
	void PushEvent(const FString& Text);

	// --- Triche / test (appelé par les commandes console du PlayerController) ---
	void DebugLevel(int32 Index) { TravelTo(Index, Money); }
	void DebugMoney(int32 Amount) { Money += Amount; }
	void DebugKills(int32 Amount) { Kills += Amount; }

private:
	void PrepareWorld();
	void UpdateLights(float DeltaSeconds);
	void UpdateSpawning(float DeltaSeconds);
	void UpdateBattery(float DeltaSeconds);
	bool FindSpawnPoint(FVector& Out, float MinDist, float MaxDist, bool bMustBeHidden) const;
	bool IsVisibleToPlayer(const FVector& Point) const;
	void SpawnZombie();
	void SpawnBattery();
	void SpawnSupplyCrate();
	void EndLevel(bool bWon);
	void TravelTo(int32 LevelIndex, int32 InMoney);
	ABodycamCharacter* GetPlayer() const;

	FBodycamLevelConfig Level;
	EBodycamMatchState MatchState = EBodycamMatchState::Intro;
	int32 LevelIndex = 1;
	int32 Money = 0;
	int32 MoneyAtStart = 0;
	int32 Kills = 0;
	float TimeLeft = 600.f;
	float StateTimer = 0.f;
	float StateAge = 0.f;
	float Elapsed = 0.f;
	float SpawnTimer = 2.f;
	float DroneCooldown = 0.f;
	float MetalTimer = 25.f;   // prochain grincement de métal au loin
	bool bOrderPending = false;
	float BatteryCheckTimer = 3.f;
	float PrepareTimer = 0.f;
	float BreadcrumbTimer = 0.f;

	UPROPERTY() TArray<TObjectPtr<ABodycamZombie>> Zombies;
	UPROPERTY() TObjectPtr<ABodycamPickup> SupplyCrate;
	UPROPERTY() TObjectPtr<ABodycamPickup> BatteryObjective;

	/** Endroits où le joueur est passé : points sûrs pour faire apparaître zombies et piles sans navmesh. */
	TArray<FVector> Breadcrumbs;

	struct FFlicker { TWeakObjectPtr<ULightComponent> Light; float Base = 0.f; float Timer = 0.f; bool bFaulty = false; };
	TArray<FFlicker> Flickers;
	TSet<TWeakObjectPtr<UObject>> Prepared;

	TArray<FBodycamMessage> Messages;
	FString LastEvent;
	float LastEventAge = 100.f;
};
