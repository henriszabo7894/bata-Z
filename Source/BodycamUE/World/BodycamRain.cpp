#include "World/BodycamRain.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ABodycamRain::ABodycamRain()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	Drops = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Drops"));
	SetRootComponent(Drops);
	Drops->SetMobility(EComponentMobility::Movable);
	Drops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Drops->SetCastShadow(false);
	Drops->bAffectDistanceFieldLighting = false;
	Drops->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RainMaterial(TEXT("/Game/Bodycam/Materials/M_Rain.M_Rain"));
	if (Cube.Succeeded()) Drops->SetStaticMesh(Cube.Object);
	if (RainMaterial.Succeeded()) Drops->SetMaterial(0, RainMaterial.Object);
}

void ABodycamRain::BeginPlay()
{
	Super::BeginPlay();

	// Les traits sont inclinés dans le sens du vent.
	const FVector Velocity = Wind + FVector(0.f, 0.f, -FallSpeed);
	DropRotation = FRotationMatrix::MakeFromZ(-Velocity).Rotator();

	const FVector Scale = DropSize / 100.f;
	Offsets.SetNum(DropCount);
	Transforms.SetNum(DropCount);
	Drops->ClearInstances();
	for (int32 i = 0; i < DropCount; ++i)
	{
		Offsets[i] = FVector(FMath::FRandRange(-Radius, Radius), FMath::FRandRange(-Radius, Radius), FMath::FRandRange(0.f, Height));
		Transforms[i] = FTransform(DropRotation, Offsets[i], Scale);
	}
	Drops->AddInstances(Transforms, false, true);
}

bool ABodycamRain::IsCameraSheltered(const FVector& CameraLocation) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BodycamRainShelter), false);
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Params.AddIgnoredActor(Pawn);
	}
	return GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, CameraLocation + FVector(0.f, 0.f, 3000.f),
		ECC_Visibility, Params);
}

void ABodycamRain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager || Transforms.Num() == 0) return;
	const FVector CameraLocation = CameraManager->GetCameraLocation();

	// Sous un toit : on coupe la pluie (vérifié 4 fois par seconde).
	ShelterCheckTimer -= DeltaSeconds;
	if (ShelterCheckTimer <= 0.f)
	{
		ShelterCheckTimer = 0.25f;
		bSheltered = IsCameraSheltered(CameraLocation);
		Drops->SetVisibility(!bSheltered);
	}
	if (bSheltered) return;

	// Chaque goutte vit dans une boîte qui suit la caméra ; elle "boucle" quand elle sort.
	const FVector Step = (Wind + FVector(0.f, 0.f, -FallSpeed)) * DeltaSeconds;
	const FVector Origin = CameraLocation - FVector(Radius, Radius, Height * 0.5f);
	const FVector Box(2.f * Radius, 2.f * Radius, Height);
	const FVector Scale = DropSize / 100.f;

	for (int32 i = 0; i < Offsets.Num(); ++i)
	{
		FVector& Offset = Offsets[i];
		Offset += Step;
		const FVector Local = Offset - Origin;
		const FVector Wrapped(FMath::Fmod(FMath::Fmod(Local.X, Box.X) + Box.X, Box.X),
		                      FMath::Fmod(FMath::Fmod(Local.Y, Box.Y) + Box.Y, Box.Y),
		                      FMath::Fmod(FMath::Fmod(Local.Z, Box.Z) + Box.Z, Box.Z));
		Offset = Origin + Wrapped;
		Transforms[i] = FTransform(DropRotation, Offset, Scale);
	}
	Drops->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
}
