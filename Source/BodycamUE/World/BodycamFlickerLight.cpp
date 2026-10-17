#include "World/BodycamFlickerLight.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ABodycamFlickerLight::ABodycamFlickerLight()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Fixture = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fixture"));
	Fixture->SetupAttachment(Root);
	Fixture->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Fixture->SetCastShadow(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(LightIntensity);
	Light->SetAttenuationRadius(900.f);
	Light->SetCastShadows(false);
	Light->SetLightColor(FLinearColor(0.85f, 0.95f, 1.f));
}

void ABodycamFlickerLight::BeginPlay()
{
	Super::BeginPlay();

	// Graine différente par lampe pour qu'elles ne clignotent pas en même temps.
	Random.Initialize(GetUniqueID() ^ FMath::RoundToInt(GetActorLocation().X * 7.f + GetActorLocation().Y));
	NextEventTime = Random.FRandRange(0.5f, 3.f);

	if (Fixture->GetMaterial(0))
	{
		FixtureMaterial = Fixture->CreateDynamicMaterialInstance(0);
	}
	ApplyLevel(Pattern == EBodycamFlicker::Faulty ? 0.1f : 1.f);
}

void ABodycamFlickerLight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds * Speed;
	ApplyLevel(UpdateLevel(DeltaSeconds));
}

float ABodycamFlickerLight::UpdateLevel(float DeltaSeconds)
{
	switch (Pattern)
	{
	case EBodycamFlicker::Steady:
		return 1.f;

	case EBodycamFlicker::Buzz:
	{
		// Petite vibration rapide, comme un tube qui grésille.
		SmoothedLevel = FMath::FInterpTo(SmoothedLevel, Random.FRandRange(0.8f, 1.f), DeltaSeconds, 30.f);
		return SmoothedLevel;
	}

	case EBodycamFlicker::Flicker:
	case EBodycamFlicker::Faulty:
	default:
	{
		const bool bFaulty = Pattern == EBodycamFlicker::Faulty;
		const float RestLevel = bFaulty ? 0.05f : 1.f;

		// Début d'une salve de clignotements.
		if (Time >= NextEventTime && Time >= BurstEndTime)
		{
			BurstEndTime = Time + Random.FRandRange(0.15f, bFaulty ? 1.2f : 0.7f);
			NextEventTime = BurstEndTime + Random.FRandRange(bFaulty ? 0.8f : 1.5f, bFaulty ? 4.f : 7.f);
			NextToggleTime = Time;
		}

		if (Time < BurstEndTime)
		{
			if (Time >= NextToggleTime)
			{
				bBurstState = !bBurstState;
				NextToggleTime = Time + Random.FRandRange(0.02f, 0.12f);
			}
			return bBurstState ? Random.FRandRange(0.6f, 1.f) : Random.FRandRange(0.f, 0.08f);
		}
		return RestLevel;
	}
	}
}

void ABodycamFlickerLight::ApplyLevel(float Level)
{
	Light->SetIntensity(LightIntensity * Level);
	Light->SetVisibility(Level > 0.01f);
	if (FixtureMaterial)
	{
		FixtureMaterial->SetScalarParameterValue(TEXT("Intensity"), EmissiveIntensity * Level);
	}
}
