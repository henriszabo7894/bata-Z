#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodycamFlickerLight.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EBodycamFlicker : uint8
{
	/** Allumée en continu. */
	Steady,
	/** Néon fatigué : allumé, avec des salves de coupures aléatoires. */
	Flicker,
	/** Lampe en panne : presque éteinte, se rallume par à-coups. */
	Faulty,
	/** Bourdonnement : légère vibration rapide de l'intensité. */
	Buzz
};

/**
 * Lampe dont la lumière ET le tube/ampoule (matériau émissif) clignotent ensemble.
 * Lumière "mobile" sans ombre pour rester légère sur une puce graphique intégrée.
 * Le matériau de Fixture doit avoir un paramètre scalaire "Intensity" (MI_Emissive_*).
 */
UCLASS()
class BODYCAMUE_API ABodycamFlickerLight : public AActor
{
	GENERATED_BODY()

public:
	ABodycamFlickerLight();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bodycam")
	TObjectPtr<UStaticMeshComponent> Fixture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Flicker")
	EBodycamFlicker Pattern = EBodycamFlicker::Flicker;

	/** Intensité de la lumière allumée (candelas). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Flicker")
	float LightIntensity = 20.f;

	/** Intensité du matériau émissif allumé. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Flicker")
	float EmissiveIntensity = 15.f;

	/** Multiplie la fréquence des clignotements. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|Flicker")
	float Speed = 1.f;

protected:
	virtual void BeginPlay() override;

private:
	float UpdateLevel(float DeltaSeconds);
	void ApplyLevel(float Level);

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FixtureMaterial;

	FRandomStream Random;
	float Time = 0.f;
	float NextEventTime = 0.f;
	float BurstEndTime = 0.f;
	float NextToggleTime = 0.f;
	bool bBurstState = true;
	float SmoothedLevel = 1.f;
};
