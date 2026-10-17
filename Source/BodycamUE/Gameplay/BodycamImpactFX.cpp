#include "Gameplay/BodycamImpactFX.h"

#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RotationMatrix.h"
#include "UObject/ConstructorHelpers.h"

ABodycamImpactFX::ABodycamImpactFX()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABodycamImpactFX::Init(const FVector& ShotDir, float Strength, bool bDust)
{
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	// Sang non éclairé : reste visible dans l'obscurité (sinon les gouttes sont noires et invisibles).
	if (UMaterialInterface* Blood = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/MI_BloodFX.MI_BloodFX")))
	{
		BloodMID = UMaterialInstanceDynamic::Create(Blood, this);
		MistMID = UMaterialInstanceDynamic::Create(Blood, this);
		if (bDust)
		{
			BloodMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.07f, 0.065f, 0.058f));
			MistMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.09f, 0.085f, 0.078f));
		}
	}
	else if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		BloodMID = UMaterialInstanceDynamic::Create(Base, this);
		BloodMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.005f, 0.004f));
	}
	if (!Sphere) return;

	// Nuage de sang à l'impact (s'étend puis s'efface) : confirme visuellement que la balle a touché.
	if (MistMID)
	{
		Mist = NewObject<UStaticMeshComponent>(this);
		Mist->SetStaticMesh(Sphere);
		Mist->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mist->CastShadow = false;
		Mist->SetupAttachment(RootComponent);
		Mist->RegisterComponent();
		Mist->SetMaterial(0, MistMID);
		MistSize = bDust ? 0.22f : 0.3f + 0.3f * Strength;
		Mist->SetRelativeScale3D(FVector(0.05f));
	}

	// Les gouttes partent surtout dans le sens de la balle (sortie) et un peu vers le tireur.
	const int32 Count = bDust ? 7 : FMath::RoundToInt(16 + 16 * Strength);
	for (int32 i = 0; i < Count; ++i)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Sphere);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->CastShadow = false;
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		if (BloodMID) C->SetMaterial(0, BloodMID);
		const float S = bDust ? FMath::FRandRange(0.006f, 0.016f) : FMath::FRandRange(0.018f, 0.045f);
		C->SetRelativeScale3D(FVector(S));
		DropComps.Add(C);

		FDrop D;
		D.Comp = C;
		D.Scale = S;
		const FVector Base = (bDust || i % 3 != 0) ? ShotDir : -ShotDir;
		D.Velocity = FMath::VRandCone(Base, FMath::DegreesToRadians(35.f)) * FMath::FRandRange(150.f, 450.f) * (0.6f + Strength * 0.6f);
		Drops.Add(D);
	}
	SetLifeSpan(Life + 0.1f);
}

void ABodycamImpactFX::InitGlass(const FVector& ShotDir, const FVector2D& PaneSize, float FloorZ)
{
	Life = 7.f;
	ShrinkTime = 1.5f;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) return;
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/MI_BloodFX.MI_BloodFX")))
	{
		BloodMID = UMaterialInstanceDynamic::Create(Mat, this);
		BloodMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.16f, 0.19f, 0.2f));
	}
	// Le verre éclate dans le sens du tir ; le plan de la vitre = X (largeur) / Z (hauteur).
	const FVector Along = FVector(1.f, 0.f, 0.f);
	for (int32 i = 0; i < 46; ++i)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Cube);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->CastShadow = false;
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		if (BloodMID) C->SetMaterial(0, BloodMID);
		C->SetWorldLocation(GetActorLocation() + Along * FMath::FRandRange(-0.5f, 0.5f) * PaneSize.X + FVector(0.f, 0.f, FMath::FRandRange(-0.5f, 0.5f) * PaneSize.Y));
		C->SetWorldRotation(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f)));
		DropComps.Add(C);

		FDrop D;
		D.Comp = C;
		D.Scale = 1.f;
		D.Scale3 = FVector(FMath::FRandRange(0.03f, 0.11f), FMath::FRandRange(0.03f, 0.09f), 0.004f);
		D.FloorZ = FloorZ + 0.5f;
		D.Spin = FRotator(FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(-500.f, 500.f));
		D.Velocity = FMath::VRandCone(ShotDir, FMath::DegreesToRadians(40.f)) * FMath::FRandRange(80.f, 380.f);
		C->SetWorldScale3D(D.Scale3);
		Drops.Add(D);
	}
	SetLifeSpan(Life + 0.1f);
}

void ABodycamImpactFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Shrink = FMath::Clamp((Life - Age) / ShrinkTime, 0.f, 1.f);
	for (FDrop& D : Drops)
	{
		if (!D.Comp) continue;
		if (!D.Scale3.IsZero())
		{
			// Éclat de verre : tombe en tournoyant puis reste posé au sol.
			if (D.Comp->GetComponentLocation().Z > D.FloorZ)
			{
				D.Velocity += FVector(0.f, 0.f, -980.f) * DeltaSeconds;
				D.Velocity *= 1.f - 0.8f * DeltaSeconds;
				D.Comp->AddWorldOffset(D.Velocity * DeltaSeconds);
				D.Comp->AddLocalRotation(D.Spin * DeltaSeconds);
			}
			D.Comp->SetWorldScale3D(D.Scale3 * Shrink);
			continue;
		}
		D.Velocity += FVector(0.f, 0.f, -980.f) * DeltaSeconds;
		D.Velocity *= 1.f - 2.5f * DeltaSeconds;
		D.Comp->AddWorldOffset(D.Velocity * DeltaSeconds);
		D.Comp->SetWorldScale3D(FVector(D.Scale * Shrink));
	}
	if (Mist && MistMID)
	{
		const float T = FMath::Clamp(Age / 0.35f, 0.f, 1.f);
		Mist->SetRelativeScale3D(FVector(FMath::Lerp(0.05f, MistSize, FMath::Sqrt(T))));
		MistMID->SetScalarParameterValue(TEXT("Opacity"), 0.7f * (1.f - T));
		if (T >= 1.f) Mist->SetVisibility(false);
	}
}

// ------------------------------------------------------------------ Douille

ABodycamShell::ABodycamShell()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->CastShadow = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded()) Mesh->SetStaticMesh(Cyl.Object);
}

void ABodycamShell::Init(const FVector& Velocity, const FVector2D& Size, bool bShotgun)
{
	Vel = Velocity;
	Spin = FRotator(FMath::FRandRange(-900.f, 900.f), FMath::FRandRange(-600.f, 600.f), FMath::FRandRange(-900.f, 900.f));
	Mesh->SetWorldScale3D(FVector(Size.X / 100.f, Size.X / 100.f, Size.Y / 100.f));
	// Laiton (ou cartouche rouge) non éclairé un peu sombre : visible même dans l'obscurité.
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/M_FX_Unlit.M_FX_Unlit"));
	if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Base)
	{
		MID = UMaterialInstanceDynamic::Create(Base, this);
		const FLinearColor C = bShotgun ? FLinearColor(0.3f, 0.02f, 0.02f) : FLinearColor(0.32f, 0.22f, 0.06f);
		MID->SetVectorParameterValue(TEXT("Color"), C);
		MID->SetScalarParameterValue(TEXT("Brightness"), 1.f);
		Mesh->SetMaterial(0, MID);
	}
	SetLifeSpan(10.f);
}

void ABodycamShell::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bResting) return;

	Vel += FVector(0.f, 0.f, -980.f) * DeltaSeconds;
	const FVector From = GetActorLocation();
	const FVector To = From + Vel * DeltaSeconds;
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(ShellBounce), false, this);
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0)) P.AddIgnoredActor(Pawn);
	if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, P))
	{
		// Rebond amorti ; au 3e rebond ou trop lente, la douille se couche au sol.
		Vel = FMath::GetReflectionVector(Vel, Hit.ImpactNormal) * 0.35f;
		SetActorLocation(Hit.ImpactPoint + Hit.ImpactNormal * 0.6f);
		Spin *= 0.5f;
		if (++Bounces >= 3 || Vel.Size() < 40.f)
		{
			bResting = true;
			SetActorRotation(FRotator(90.f, FMath::FRandRange(0.f, 360.f), 0.f));
		}
		return;
	}
	SetActorLocation(To);
	AddActorWorldRotation(Spin * DeltaSeconds);
}

// ------------------------------------------------------------------ Traçante

ABodycamTracer::ABodycamTracer()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->CastShadow = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded()) Mesh->SetStaticMesh(Cyl.Object);
}

void ABodycamTracer::Init(const FVector& From, const FVector& To)
{
	Start = From;
	Dist = FVector::Dist(From, To);
	Dir = (To - From).GetSafeNormal();
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/MI_Tracer.MI_Tracer")))
	{
		Mesh->SetMaterial(0, Mat);
	}
	SetActorLocationAndRotation(From, FRotationMatrix::MakeFromZ(Dir).Rotator());
	Mesh->SetWorldScale3D(FVector(0.006f, 0.006f, 0.01f));
	SetLifeSpan(2.f);
}

void ABodycamTracer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Traveled += Speed * DeltaSeconds;
	const float Head = FMath::Min(Traveled, Dist);
	const float Tail = FMath::Clamp(Traveled - Length, 0.f, Dist);
	if (Tail >= Dist - 1.f)
	{
		Destroy();
		return;
	}
	// Le cylindre de base fait 100 cm de haut, centré : on le place au milieu du segment visible.
	SetActorLocation(Start + Dir * ((Head + Tail) * 0.5f));
	Mesh->SetWorldScale3D(FVector(0.006f, 0.006f, FMath::Max(Head - Tail, 1.f) / 100.f));
}

void ABodycamImpactFX::AttachWound(USkeletalMeshComponent* Mesh, const FVector& Point, const FVector& ShotDir, float Size)
{
	if (!Mesh || !Mesh->GetOwner()) return;
	AActor* Owner = Mesh->GetOwner();
	TArray<UActorComponent*> Existing = Owner->GetComponentsByTag(UStaticMeshComponent::StaticClass(), TEXT("Wound"));
	if (Existing.Num() >= 14) return;

	const FName Bone = Mesh->FindClosestBone(Point);
	if (Bone == NAME_None) return;
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Blood = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Bodycam/Materials/FX/MI_BloodFX.MI_BloodFX"));
	if (!Sphere || !Blood) return;

	// La balle touche la capsule (un peu à l'extérieur du corps) : on ramène la plaie sur la surface,
	// du côté du tireur, à quelques cm de l'os le plus proche.
	const FVector B = Mesh->GetBoneLocation(Bone);
	const FVector Toward = (-ShotDir).GetSafeNormal2D();
	// Mieux vaut une plaie légèrement en surface qu'enfoncée (invisible) dans le corps.
	const float R = FMath::Clamp(FVector::Dist2D(Point, B), 9.f, 18.f);
	const FVector Pos = B + Toward * R + FVector(0.f, 0.f, (Point.Z - B.Z) * 0.5f);

	UStaticMeshComponent* W = NewObject<UStaticMeshComponent>(Owner);
	W->ComponentTags.Add(TEXT("Wound"));
	W->SetStaticMesh(Sphere);
	W->SetMaterial(0, Blood);
	W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	W->CastShadow = false;
	W->RegisterComponent();
	W->AttachToComponent(Mesh, FAttachmentTransformRules::KeepWorldTransform, Bone);
	W->SetWorldLocationAndRotation(Pos, FRotationMatrix::MakeFromZ(Toward).Rotator());
	const float S = Size * FMath::FRandRange(0.8f, 1.3f);
	W->SetWorldScale3D(FVector(S, S * FMath::FRandRange(0.7f, 1.2f), S * 0.35f));

	// Coulure sous la plaie.
	UStaticMeshComponent* Drip = NewObject<UStaticMeshComponent>(Owner);
	Drip->ComponentTags.Add(TEXT("Wound"));
	Drip->SetStaticMesh(Sphere);
	Drip->SetMaterial(0, Blood);
	Drip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Drip->CastShadow = false;
	Drip->RegisterComponent();
	Drip->AttachToComponent(Mesh, FAttachmentTransformRules::KeepWorldTransform, Bone);
	Drip->SetWorldLocationAndRotation(Pos - FVector(0.f, 0.f, S * 60.f), FRotationMatrix::MakeFromZ(Toward).Rotator());
	Drip->SetWorldScale3D(FVector(S * 1.6f, S * 0.35f, S * 0.25f));
}

void ABodycamImpactFX::SpawnDecal(UObject* WorldContext, bool bBlood, const FVector& Location, const FVector& Normal, float Size)
{
	const TCHAR* Path = bBlood ? TEXT("/Game/Bodycam/Materials/FX/M_Decal_Blood.M_Decal_Blood")
							   : TEXT("/Game/Bodycam/Materials/FX/M_Decal_BulletHole.M_Decal_BulletHole");
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, Path);
	if (!Mat || !WorldContext) return;

	UMaterialInterface* Use = Mat;
	if (bBlood)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, WorldContext);
		MID->SetScalarParameterValue(TEXT("Seed"), FMath::FRand());
		Use = MID;
	}
	// Une décalcomanie projette le long de son axe X : on l'aligne sur la normale de la surface.
	FRotator Rot = (-Normal).Rotation();
	Rot.Roll = FMath::FRandRange(-180.f, 180.f);
	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(WorldContext, Use, FVector(6.f, Size, Size), Location, Rot, bBlood ? 40.f : 25.f);
	if (Decal)
	{
		Decal->SetFadeScreenSize(0.002f);
	}
}
