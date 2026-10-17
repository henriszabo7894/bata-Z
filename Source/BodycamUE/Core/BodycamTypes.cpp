#include "Core/BodycamTypes.h"

#include "Misc/ConfigCacheIni.h"

FBodycamLevelConfig FBodycamLevelConfig::Get(int32 Index)
{
	FBodycamLevelConfig C;
	C.Index = FMath::Clamp(Index, 1, NumLevels);

	switch (C.Index)
	{
	case 1:
		C.Title = TEXT("NIVEAU 1"); C.Sector = TEXT("SECTEUR BLEU");
		C.KillQuota = 25; C.MaxAlive = 7; C.SpawnInterval = 4.5f;
		C.ZombieSpeedMul = 1.f; C.ZombieHealthMul = 1.f; C.RunnerChance = 0.1f; C.CrawlerChance = 0.12f;
		C.Weapon = EBodycamWeaponType::Pistol; C.StartReserveMags = 3;
		C.BatteryDrainPerSec = 0.7f; C.KillReward = 20; C.AmmoOrderCost = 100; C.AmmoOrderMags = 3;
		break;
	case 2:
		C.Title = TEXT("NIVEAU 2"); C.Sector = TEXT("SECTEUR VERT");
		C.KillQuota = 40; C.MaxAlive = 11; C.SpawnInterval = 3.2f;
		C.ZombieSpeedMul = 1.15f; C.ZombieHealthMul = 1.35f; C.RunnerChance = 0.25f; C.CrawlerChance = 0.15f;
		C.Weapon = EBodycamWeaponType::Shotgun; C.StartReserveMags = 3;
		C.BatteryDrainPerSec = 0.9f; C.KillReward = 25; C.AmmoOrderCost = 150; C.AmmoOrderMags = 3;
		break;
	default:
		C.Title = TEXT("NIVEAU 3"); C.Sector = TEXT("SECTEUR JAUNE");
		C.KillQuota = 60; C.MaxAlive = 15; C.SpawnInterval = 2.3f;
		C.ZombieSpeedMul = 1.3f; C.ZombieHealthMul = 1.7f; C.RunnerChance = 0.4f; C.CrawlerChance = 0.18f;
		C.Weapon = EBodycamWeaponType::Rifle; C.StartReserveMags = 2;
		C.BatteryDrainPerSec = 1.1f; C.KillReward = 30; C.AmmoOrderCost = 200; C.AmmoOrderMags = 3;
		break;
	}

	// Positions et réglages modifiables sans recompiler : Config/DefaultGame.ini, section [Bodycam.LevelN].
	const FString Section = FString::Printf(TEXT("Bodycam.Level%d"), C.Index);
	if (GConfig)
	{
		GConfig->GetVector(*Section, TEXT("PlayerStart"), C.PlayerStart, GGameIni);
		GConfig->GetFloat(*Section, TEXT("PlayerYaw"), C.PlayerYaw, GGameIni);
		GConfig->GetVector(*Section, TEXT("SupplySite"), C.SupplySite, GGameIni);
		GConfig->GetInt(*Section, TEXT("KillQuota"), C.KillQuota, GGameIni);
		GConfig->GetInt(*Section, TEXT("MaxAlive"), C.MaxAlive, GGameIni);
		GConfig->GetFloat(*Section, TEXT("SpawnInterval"), C.SpawnInterval, GGameIni);
		GConfig->GetFloat(*Section, TEXT("TimeLimit"), C.TimeLimit, GGameIni);
	}
	return C;
}
