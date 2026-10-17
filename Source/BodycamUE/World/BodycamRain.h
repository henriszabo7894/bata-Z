#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodycamRain.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Pluie légère pour puce graphique intégrée : quelques milliers de traits translucides
 * (un seul draw call instancié) qui tombent dans une boîte autour de la caméra.
 * Coupée automatiquement quand la caméra est sous un toit.
 */
UCLASS()
class BODYCAMUE_API ABodycamRain : public AActor
{
	GENERATED_BODY()

public:
	ABodycamRain();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<UInstancedStaticMeshComponent> Drops;

	UPROPERTY(EditAnywhere, Category = "Bodycam|Rain", meta = (ClampMin = "100", ClampMax = "6000"))
	int32 DropCount = 2500;

	/** Demi-largeur de la zone de pluie autour de la caméra (cm). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Rain") float Radius = 1400.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Rain") float Height = 1100.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Rain") float FallSpeed = 1000.f;
	UPROPERTY(EditAnywhere, Category = "Bodycam|Rain") FVector Wind = FVector(120.f, 60.f, 0.f);
	/** Taille d'un trait de pluie (cm). */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Rain") FVector DropSize = FVector(0.8f, 0.8f, 55.f);

protected:
	virtual void BeginPlay() override;

private:
	bool IsCameraSheltered(const FVector& CameraLocation) const;

	TArray<FVector> Offsets;
	TArray<FTransform> Transforms;
	FRotator DropRotation = FRotator::ZeroRotator;
	float ShelterCheckTimer = 0.f;
	bool bSheltered = false;
};
