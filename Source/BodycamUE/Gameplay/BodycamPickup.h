#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodycamPickup.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class USphereComponent;
class UStaticMesh;

UENUM()
enum class EBodycamPickupType : uint8
{
	Battery,   // pile pour la vision nocturne
	AmmoCrate  // caisse de munitions commandée (récupérée par le drone)
};

/** Objet à récupérer, signalé à l'écran par un marqueur. */
UCLASS()
class BODYCAMUE_API ABodycamPickup : public AActor
{
	GENERATED_BODY()

public:
	ABodycamPickup();

	virtual void Tick(float DeltaSeconds) override;

	void InitPickup(EBodycamPickupType InType);
	EBodycamPickupType GetPickupType() const { return Type; }

	/** Caisse de munitions (modèle importé, sinon caisse Megascans) et son échelle — partagé avec le drone. */
	static UStaticMesh* LoadAmmoCrateMesh(float& OutScale);

	/** Distance de ramassage (touche F ou passage dessus pour la pile). */
	float GetPickupRadius() const { return Type == EBodycamPickupType::Battery ? 170.f : 260.f; }

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Beacon;

private:
	EBodycamPickupType Type = EBodycamPickupType::Battery;
	float Time = 0.f;
};
