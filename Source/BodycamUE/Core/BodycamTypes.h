#pragma once

#include "CoreMinimal.h"
#include "BodycamTypes.generated.h"

/** Arme donnée d'office au joueur selon le niveau (pas d'achat d'arme). */
UENUM(BlueprintType)
enum class EBodycamWeaponType : uint8
{
	Pistol,
	Shotgun,
	Rifle
};

/** Les 5 façons dont un zombie peut attaquer le joueur. */
UENUM(BlueprintType)
enum class EZombieAttack : uint8
{
	None,
	Ankle,   // mord la cheville : caméra vers le bas, zombie traîné, coups de pied (F)
	Back,    // saute dans le dos : marteler F
	Front,   // attrape de face : marteler F
	Neck,    // morsure au cou : mort immédiate
	Push     // pousse / griffe : 1 coup + risque de contamination
};

UENUM(BlueprintType)
enum class EZombieVariant : uint8
{
	Walker,
	Runner,
	Crawler
};

UENUM(BlueprintType)
enum class EBodycamMatchState : uint8
{
	Intro,
	Playing,
	Won,
	Lost
};

/** Réglages d'un niveau (difficulté croissante). */
struct FBodycamLevelConfig
{
	int32 Index = 1;
	FString Title;
	FString Sector;
	int32 KillQuota = 25;
	float TimeLimit = 600.f;
	int32 MaxAlive = 8;
	float SpawnInterval = 4.f;
	float ZombieSpeedMul = 1.f;
	float ZombieHealthMul = 1.f;
	float RunnerChance = 0.1f;
	float CrawlerChance = 0.1f;
	EBodycamWeaponType Weapon = EBodycamWeaponType::Pistol;
	int32 StartReserveMags = 2;
	float BatteryDrainPerSec = 0.75f;
	int32 KillReward = 20;
	int32 AmmoOrderCost = 100;
	int32 AmmoOrderMags = 3;
	/** Point de départ du joueur et site de largage des munitions (coordonnées de la map). */
	FVector PlayerStart = FVector::ZeroVector;
	float PlayerYaw = 0.f;
	FVector SupplySite = FVector::ZeroVector;

	static FBodycamLevelConfig Get(int32 Index);
	static constexpr int32 NumLevels = 3;
};
