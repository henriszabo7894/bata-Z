#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodycamImpactFX.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;

/**
 * Gerbe de sang quand une balle touche un zombie : petites gouttes projetées qui retombent
 * (sans système de particules), + tache de sang sur le mur/sol derrière.
 */
UCLASS()
class BODYCAMUE_API ABodycamImpactFX : public AActor
{
	GENERATED_BODY()

public:
	ABodycamImpactFX();

	virtual void Tick(float DeltaSeconds) override;

	/** bDust = impact sur un mur : éclats et poussière grise au lieu du sang (ShotDir = normale du mur). */
	void Init(const FVector& ShotDir, float Strength, bool bDust = false);

	/** Vitre qui éclate : éclats de verre plats qui tombent et restent un moment au sol. */
	void InitGlass(const FVector& ShotDir, const FVector2D& PaneSize, float FloorZ);

	/** Plaie de sang collée sur le corps (suit l'os le plus proche de l'impact). */
	static void AttachWound(USkeletalMeshComponent* Mesh, const FVector& Point, const FVector& ShotDir, float Size);

	/** Tache de sang ou trou de balle (décalcomanie) sur une surface. */
	static void SpawnDecal(UObject* WorldContext, bool bBlood, const FVector& Location, const FVector& Normal, float Size);

private:
	struct FDrop
	{
		TObjectPtr<UStaticMeshComponent> Comp;
		FVector Velocity;
		float Scale;
		FVector Scale3 = FVector::ZeroVector;   // éclats de verre : échelle non uniforme
		float FloorZ = -1.e9f;
		FRotator Spin = FRotator::ZeroRotator;
	};
	TArray<FDrop> Drops;

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> DropComps;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BloodMID;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mist;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MistMID;
	float MistSize = 0.3f;
	float Age = 0.f;
	float Life = 0.7f;
	float ShrinkTime = 0.7f;
};

/** Douille éjectée par l'arme : vole, rebondit sur le sol puis reste quelques secondes. */
UCLASS()
class BODYCAMUE_API ABodycamShell : public AActor
{
	GENERATED_BODY()

public:
	ABodycamShell();
	virtual void Tick(float DeltaSeconds) override;
	/** Size = (diamètre, longueur) en cm. bShotgun = cartouche rouge. */
	void Init(const FVector& Velocity, const FVector2D& Size, bool bShotgun);

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;
	FVector Vel = FVector::ZeroVector;
	FRotator Spin = FRotator::ZeroRotator;
	bool bResting = false;
	int32 Bounces = 0;
};

/** Traçante : trait lumineux qui part de la bouche du canon jusqu'au point d'impact. */
UCLASS()
class BODYCAMUE_API ABodycamTracer : public AActor
{
	GENERATED_BODY()

public:
	ABodycamTracer();
	virtual void Tick(float DeltaSeconds) override;
	void Init(const FVector& From, const FVector& To);

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
	FVector Start = FVector::ZeroVector;
	FVector Dir = FVector::ForwardVector;
	float Dist = 0.f;
	float Traveled = 0.f;
	float Speed = 18000.f;   // cm/s (ralenti par rapport au réel pour être visible)
	float Length = 140.f;
};
