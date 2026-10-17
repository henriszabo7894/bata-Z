#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodycamAlarmLight.generated.h"

class USpotLightComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Gyrophare d'alarme rouge fixé au plafond : deux faisceaux qui tournent + halo qui pulse.
 * Aucun asset externe. Placés au lancement depuis DefaultGame.ini [Bodycam.Alarm].
 */
UCLASS()
class BODYCAMUE_API ABodycamAlarmLight : public AActor
{
	GENERATED_BODY()

public:
	ABodycamAlarmLight();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Cherche le plafond au-dessus du point donné et s'y accroche. */
	void PlaceUnderCeiling(const FVector& Around);

	float RotationSpeed = 220.f;   // degrés / seconde
	float BeamIntensity = 2500.f;  // candelas
	float GlowIntensity = 120.f;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Spinner;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> BeamA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> BeamB;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Housing;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HousingMID;

private:
	float Time = 0.f;
};
