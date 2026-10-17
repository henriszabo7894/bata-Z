#include "Gameplay/BodycamSounds.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

USoundBase* FBodycamSounds::Get(const TCHAR* Name)
{
	static TMap<FString, TWeakObjectPtr<USoundBase>> Cache;
	static TSet<FString> Missing;
	if (TWeakObjectPtr<USoundBase>* Found = Cache.Find(Name))
	{
		if (USoundBase* S = Found->Get()) return S;
	}
	if (Missing.Contains(Name)) return nullptr;
	const FString Path = FString::Printf(TEXT("/Game/Bodycam/Audio/%s.%s"), Name, Name);
	USoundBase* S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
	if (!S)
	{
		Missing.Add(Name);
		UE_LOG(LogTemp, Warning, TEXT("Son introuvable : %s (lance Importer_Sons.bat)"), *Path);
		return nullptr;
	}
	S->AddToRoot();   // reste chargé (pas de saccade au rechargement)
	Cache.Add(Name, S);
	return S;
}

USoundBase* FBodycamSounds::GetRandom(const TCHAR* Prefix, int32 Count)
{
	const FString Name = FString::Printf(TEXT("%s_%02d"), Prefix, FMath::RandRange(1, FMath::Max(1, Count)));
	return Get(*Name);
}

USoundAttenuation* FBodycamSounds::Attenuation(float Inner, float Falloff)
{
	static TMap<FIntPoint, USoundAttenuation*> Cache;
	const FIntPoint Key(FMath::RoundToInt(Inner), FMath::RoundToInt(Falloff));
	if (USoundAttenuation** Found = Cache.Find(Key)) return *Found;
	USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage());
	A->AddToRoot();
	A->Attenuation.bAttenuate = true;
	A->Attenuation.bSpatialize = true;
	A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	A->Attenuation.AttenuationShapeExtents = FVector(Inner, 0.f, 0.f);
	A->Attenuation.FalloffDistance = Falloff;
	A->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Cache.Add(Key, A);
	return A;
}

void FBodycamSounds::PlayAt(const UObject* Context, USoundBase* Sound, const FVector& Location, float Volume, float Pitch, float Inner, float Falloff)
{
	if (!Sound || !Context) return;
	UGameplayStatics::PlaySoundAtLocation(Context, Sound, Location, FRotator::ZeroRotator, Volume, Pitch, 0.f, Attenuation(Inner, Falloff));
}

void FBodycamSounds::Play2D(const UObject* Context, USoundBase* Sound, float Volume, float Pitch)
{
	if (!Sound || !Context) return;
	UGameplayStatics::PlaySound2D(Context, Sound, Volume, Pitch);
}

UAudioComponent* FBodycamSounds::PlayAttached(USoundBase* Sound, USceneComponent* AttachTo, float Volume, float Pitch, float Inner, float Falloff, float StartTime)
{
	if (!Sound || !AttachTo) return nullptr;
	return UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
		false, Volume, Pitch, StartTime, Attenuation(Inner, Falloff));
}
