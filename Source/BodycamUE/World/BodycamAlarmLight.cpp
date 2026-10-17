#include "World/BodycamAlarmLight.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor AlarmRed(1.f, 0.04f, 0.02f);

	USpotLightComponent* MakeBeam(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, float Yaw)
	{
		USpotLightComponent* L = Owner->CreateDefaultSubobject<USpotLightComponent>(Name);
		L->SetupAttachment(Parent);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetLightColor(AlarmRed);
		L->SetAttenuationRadius(1400.f);
		L->SetOuterConeAngle(28.f);
		L->SetInnerConeAngle(10.f);
		L->SetCastShadows(false);
		// Faisceau presque horizontal, légèrement vers le bas : balaie les murs du couloir.
		L->SetRelativeRotation(FRotator(-25.f, Yaw, 0.f));
		return L;
	}
}

ABodycamAlarmLight::ABodycamAlarmLight()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);

	Spinner = CreateDefaultSubobject<USceneComponent>(TEXT("Spinner"));
	Spinner->SetupAttachment(RootComponent);
	BeamA = MakeBeam(this, Spinner, TEXT("BeamA"), 0.f);
	BeamB = MakeBeam(this, Spinner, TEXT("BeamB"), 180.f);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetLightColor(AlarmRed);
	Glow->SetAttenuationRadius(450.f);
	Glow->SetCastShadows(false);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, -15.f));

	Housing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Housing"));
	Housing->SetupAttachment(RootComponent);
	Housing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Housing->CastShadow = false;
	Housing->SetRelativeLocation(FVector(0.f, 0.f, -6.f));
	Housing->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.12f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded()) Housing->SetStaticMesh(Cyl.Object);
}

void ABodycamAlarmLight::BeginPlay()
{
	Super::BeginPlay();
	BeamA->SetIntensity(BeamIntensity);
	BeamB->SetIntensity(BeamIntensity);
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/M_FX_Additive.M_FX_Additive")))
	{
		HousingMID = UMaterialInstanceDynamic::Create(Mat, this);
		HousingMID->SetVectorParameterValue(TEXT("Color"), AlarmRed);
		Housing->SetMaterial(0, HousingMID);
	}
	Time = FMath::FRand() * 3.f;   // les gyrophares ne sont pas synchronisés
}

void ABodycamAlarmLight::PlaceUnderCeiling(const FVector& Around)
{
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(AlarmCeiling), false, this);
	FVector Loc = Around;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Around, Around + FVector(0.f, 0.f, 600.f), ECC_Visibility, P))
	{
		Loc = Hit.ImpactPoint - FVector(0.f, 0.f, 12.f);
	}
	SetActorLocation(Loc);
}

void ABodycamAlarmLight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	Spinner->SetRelativeRotation(FRotator(0.f, FMath::Fmod(Time * RotationSpeed, 360.f), 0.f));
	// Halo : pulse au rythme de la rotation (deux passages de faisceau par tour).
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * FMath::DegreesToRadians(RotationSpeed) * 2.f);
	Glow->SetIntensity(GlowIntensity * (0.3f + 0.7f * Pulse));
	if (HousingMID) HousingMID->SetScalarParameterValue(TEXT("Brightness"), 2.f + 6.f * Pulse);
}
