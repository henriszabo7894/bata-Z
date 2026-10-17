#include "Gameplay/BodycamPickup.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"

ABodycamPickup::ABodycamPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Beacon = CreateDefaultSubobject<UPointLightComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(Root);
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	Beacon->SetIntensityUnits(ELightUnits::Candelas);
	Beacon->SetAttenuationRadius(450.f);
	Beacon->SetCastShadows(false);
}

void ABodycamPickup::InitPickup(EBodycamPickupType InType)
{
	Type = InType;
	if (Type == EBodycamPickupType::Battery)
	{
		// Pile militaire : petit cylindre vert sur une caisse, balise qui clignote.
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Mesh->SetRelativeScale3D(FVector(0.09f, 0.09f, 0.22f));
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, 11.f));
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.08f, 0.25f, 0.08f));
			Mesh->SetMaterial(0, MID);
		}
		Beacon->SetLightColor(FLinearColor(0.3f, 1.f, 0.35f));
		Beacon->SetIntensity(25.f);
	}
	else
	{
		float S = 1.f;
		UStaticMesh* M = LoadAmmoCrateMesh(S);
		Mesh->SetStaticMesh(M);
		Mesh->SetRelativeScale3D(FVector(S));
		if (M)
		{
			// Posée sur le sol quel que soit le pivot du modèle.
			const FBoxSphereBounds B = M->GetBounds();
			Mesh->SetRelativeLocation(FVector(-B.Origin.X * S, -B.Origin.Y * S, -(B.Origin.Z - B.BoxExtent.Z) * S));
		}
		Beacon->SetLightColor(FLinearColor(1.f, 0.35f, 0.1f));
		Beacon->SetIntensity(40.f);
		Beacon->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	}
}

void ABodycamPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	// Balise : clignote une fois par seconde.
	const float Base = Type == EBodycamPickupType::Battery ? 25.f : 40.f;
	Beacon->SetIntensity(FMath::Fmod(Time, 1.f) < 0.15f ? Base * 4.f : Base * 0.3f);
	if (Type == EBodycamPickupType::Battery)
	{
		Mesh->AddLocalRotation(FRotator(0.f, 60.f * DeltaSeconds, 0.f));
	}
}

UStaticMesh* ABodycamPickup::LoadAmmoCrateMesh(float& OutScale)
{
	OutScale = 2.f;
	if (GConfig) GConfig->GetFloat(TEXT("Bodycam.Drone"), TEXT("CrateScale"), OutScale, GGameIni);
	if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Bodycam/Props/AmmoCrate/SM_AmmoCrate.SM_AmmoCrate")))
	{
		return M;
	}
	OutScale = 0.6f;
	return LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/DerelictCorridor/Assets/Fab/Megascans/3D/Ind_Ware_Storage_Crate_Trap_Covered_03/SM_Ind_Ware_Storage_Crate_Trap_Covered_03.SM_Ind_Ware_Storage_Crate_Trap_Covered_03"));
}
