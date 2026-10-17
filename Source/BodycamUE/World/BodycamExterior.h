#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BodycamExterior.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UStaticMesh;
class UPrimitiveComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;

/**
 * Zone extérieure collée contre l'immeuble (côté fenêtres du couloir bleu) : sol de béton, bâtiment en ruine,
 * objets Megascans, éclairage de nuit, murs invisibles. Le drone sort par une fenêtre "brisée".
 * Tout est construit au lancement depuis DefaultGame.ini [Bodycam.Outdoor].
 */
UCLASS()
class BODYCAMUE_API ABodycamExterior : public AActor
{
	GENERATED_BODY()

public:
	ABodycamExterior();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Où poser la caisse de munitions (Z = sol). */
	static bool GetCrateSite(FVector& Out);

	/** Le point est-il dans une fenêtre CASSÉE du mur nord ? (le drone peut y passer sans choc) */
	bool IsInOpening(const FVector& P, float Margin) const;
	/** Centre de la fenêtre la plus proche (marqueur "SORTIE") ; bOutBroken = vitre déjà cassée. */
	bool FindNearestOpening(const FVector& From, FVector& Out, bool& bOutBroken) const;
	/** Tir : casse la vitre traversée par le segment Start -> End. Renvoie true si une vitre a éclaté. */
	bool BreakGlassAlong(const FVector& Start, const FVector& End, const FVector& Dir);
	/** Le composant touché par une balle est-il une vitre (pas d'impact de mur à dessiner) ? */
	bool IsGlassBlocker(const UPrimitiveComponent* C) const;
	/** Le point est-il encore à l'intérieur du bâtiment (côté couloir) ? */
	bool IsInside(const FVector& P) const { return P.Y < WallY; }

private:
	void BuildGround(const FVector& Min, const FVector& Max, float Z);
	void BuildBuilding(float GroundZ);
	void BuildProps(float GroundZ);
	void BuildLights(float GroundZ);
	void BuildBounds(const FVector& Min, const FVector& Max, float Z);
	void SetupWindows();
	void BreakPane(int32 Index, const FVector& Dir);
	void ClearBackdrops();
	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, const FTransform& T, bool bCollide);
	UInstancedStaticMeshComponent* GetISM(UStaticMesh* Mesh, bool bCollide);
	bool Place(const TCHAR* Folder, const TCHAR* MeshName, float X, float Y, float Yaw, float Scale, bool bCollide,
		float ZOffset = 0.f, const FRotator& Tilt = FRotator::ZeroRotator);
	float MeshHeight(const TCHAR* Folder, const TCHAR* MeshName) const;
	bool IsFreeSpot(const FVector& P, const FVector& Crate, float CrateClear) const;
	void BuildBarriers(float GroundZ, FRandomStream& R);
	void BuildCity(const FVector& Min, const FVector& Max, float GroundZ);
	void BuildRow(bool bAlongX, float From, float To, float Face, float Dir, float GroundZ, FRandomStream& R,
		float MinH, float MaxH, float MinDepth, float MaxDepth, bool bAlleys, bool bDetailed, TArray<FVector4>& OutAlleys);
	void BuildBlock(float X0, float X1, float Y0, float Y1, float Height, float GroundZ, FRandomStream& R, bool bDetailed);
	void BuildStreet(bool bAlongX, float From, float To, float Near, float Dir, float GroundZ, FRandomStream& R);
	void BuildCar(const FVector& Base, float Yaw, bool bBurnt, const TCHAR* Paint, FRandomStream& R);
	void CityBox(const FVector& Center, const FVector& Size, UMaterialInterface* Mat, float Yaw = 0.f, float Roll = 0.f, bool bCylinder = false);
	UMaterialInterface* CityMat(const TCHAR* Name);
	UHierarchicalInstancedStaticMeshComponent* GetHISM(UStaticMesh* Mesh, UMaterialInterface* Mat, const FString& Key);
	void SetupAtmosphere();

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Ground;
	UPROPERTY() TArray<TObjectPtr<UActorComponent>> Spawned;
	struct FPane
	{
		TWeakObjectPtr<AActor> Glass;
		UBoxComponent* Blocker = nullptr;   // gardé en vie par Spawned
		FBox Box;
		bool bBroken = false;
	};
	TArray<FPane> Panes;
	UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> ISMs;
	UPROPERTY() TMap<FString, TObjectPtr<UStaticMesh>> MeshCache;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Fires;
	float PlaceGroundZ = 0.f;

	UPROPERTY() TMap<FString, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> CityHISMs;
	UPROPERTY() TMap<FString, TObjectPtr<UMaterialInterface>> CityMats;
	UPROPERTY() TObjectPtr<UMaterialInterface> ConcreteMat;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	int32 NumTowers = 0, NumBoxes = 0, StreetLights = 0;
	int32 NumProps = 0;
	float FireTime = 0.f;
	float WallY = -768.f;
};
