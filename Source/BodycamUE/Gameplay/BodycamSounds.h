#pragma once

#include "CoreMinimal.h"

class USoundBase;
class USoundAttenuation;
class UAudioComponent;
class USceneComponent;

/**
 * Sons du jeu (importés dans /Game/Bodycam/Audio par Importer_Sons.bat).
 * Si un son manque, rien n'est joué (pas de plantage).
 */
struct BODYCAMUE_API FBodycamSounds
{
	static USoundBase* Get(const TCHAR* Name);
	/** Un son au hasard parmi Prefix_01..Prefix_Count. */
	static USoundBase* GetRandom(const TCHAR* Prefix, int32 Count);
	/** Atténuation avec la distance : plein volume jusqu'à Inner, silence à Inner + Falloff (cm). */
	static USoundAttenuation* Attenuation(float Inner, float Falloff);

	static void PlayAt(const UObject* Context, USoundBase* Sound, const FVector& Location, float Volume = 1.f, float Pitch = 1.f, float Inner = 400.f, float Falloff = 2500.f);
	static void Play2D(const UObject* Context, USoundBase* Sound, float Volume = 1.f, float Pitch = 1.f);
	static UAudioComponent* PlayAttached(USoundBase* Sound, USceneComponent* AttachTo, float Volume = 1.f, float Pitch = 1.f, float Inner = 400.f, float Falloff = 2500.f, float StartTime = 0.f);
};
