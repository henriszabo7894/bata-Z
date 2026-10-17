#include "World/BodycamExterior.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "World/BodycamRain.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Gameplay/BodycamImpactFX.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY_STATIC(LogBodycamExterior, Log, All);

namespace
{
	const TCHAR* Sec = TEXT("Bodycam.Outdoor");

}

ABodycamExterior::ABodycamExterior()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

bool ABodycamExterior::GetCrateSite(FVector& Out)
{
	if (!GConfig) return false;
	bool bEnabled = true;
	GConfig->GetBool(Sec, TEXT("Enabled"), bEnabled, GGameIni);
	return bEnabled && GConfig->GetVector(Sec, TEXT("CrateSite"), Out, GGameIni);
}

UStaticMeshComponent* ABodycamExterior::AddMesh(UStaticMesh* Mesh, const FTransform& T, bool bCollide)
{
	if (!Mesh) return nullptr;
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(RootComponent);
	C->SetWorldTransform(T);
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	C->RegisterComponent();
	Spawned.Add(C);
	return C;
}

void ABodycamExterior::BeginPlay()
{
	Super::BeginPlay();
	bool bEnabled = true;
	FVector Min(4500.f, -420.f, 0.f), Max(11300.f, 4200.f, 0.f);
	float GroundZ = 0.f;
	if (GConfig)
	{
		GConfig->GetBool(Sec, TEXT("Enabled"), bEnabled, GGameIni);
		GConfig->GetVector(Sec, TEXT("GroundMin"), Min, GGameIni);
		GConfig->GetVector(Sec, TEXT("GroundMax"), Max, GGameIni);
		GConfig->GetFloat(Sec, TEXT("GroundZ"), GroundZ, GGameIni);
	}
	if (!bEnabled) return;

	ClearBackdrops();
	SetupWindows();
	BuildGround(Min, Max, GroundZ);
	BuildBuilding(GroundZ);
	BuildProps(GroundZ);
	BuildCity(Min, Max, GroundZ);
	BuildLights(GroundZ);
	BuildBounds(Min, Max, GroundZ);
	SetupAtmosphere();
	UE_LOG(LogBodycamExterior, Log, TEXT("Exterieur construit : %d elements"), Spawned.Num());
}

void ABodycamExterior::ClearBackdrops()
{
	// Derrière les fenêtres du couloir il y avait des panneaux de décor (fond lumineux / noir) : on les retire
	// pour voir la vraie zone extérieure et laisser passer le drone.
	float FromY = -760.f;
	if (GConfig) GConfig->GetFloat(Sec, TEXT("OutsideFromY"), FromY, GGameIni);
	int32 N = 0;
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		FVector O, E;
		It->GetActorBounds(false, O, E);
		if (O.Y - E.Y > FromY)
		{
			It->SetActorHiddenInGame(true);
			It->SetActorEnableCollision(false);
			UE_LOG(LogBodycamExterior, Log, TEXT("Decor de fond retire : %s"), *It->GetName());
			++N;
		}
	}
	UE_LOG(LogBodycamExterior, Log, TEXT("%d decor(s) de fond retire(s)"), N);
}

void ABodycamExterior::SetupWindows()
{
	// Fenêtres du mur nord : les vitres restent en place et bloquent le drone. Une balle les fait éclater ;
	// le drone peut alors traverser l'ouverture (les murs gardent leurs collisions, le joueur ne passe pas).
	FVector Win(6656.f, -768.f, 178.f);
	if (GConfig) GConfig->GetVector(Sec, TEXT("ExitWindow"), Win, GGameIni);
	WallY = Win.Y;

	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		const UStaticMeshComponent* C = It->GetStaticMeshComponent();
		const UStaticMesh* M = C ? C->GetStaticMesh() : nullptr;
		if (!M) continue;
		const FString Name = M->GetName();
		if (!Name.Contains(TEXT("Glass")) || Name.Contains(TEXT("Door"))) continue;
		FVector Ctr, Ext;
		It->GetActorBounds(false, Ctr, Ext);
		if (FMath::Abs(Ctr.Y - WallY) > 25.f || Ext.Y > 15.f || Ext.X < 40.f) continue;

		FPane P;
		P.Glass = *It;
		P.Box = FBox(FVector(Ctr.X - Ext.X, WallY - 90.f, Ctr.Z - Ext.Z), FVector(Ctr.X + Ext.X, WallY + 90.f, Ctr.Z + Ext.Z));
		// Vitre "solide" (le modèle de vitre n'a pas de collision fiable).
		UBoxComponent* B = NewObject<UBoxComponent>(this);
		B->SetMobility(EComponentMobility::Movable);
		B->SetupAttachment(RootComponent);
		B->SetBoxExtent(FVector(Ext.X, 3.f, Ext.Z));
		B->SetWorldLocation(FVector(Ctr.X, WallY, Ctr.Z));
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetHiddenInGame(true);
		B->RegisterComponent();
		Spawned.Add(B);
		P.Blocker = B;
		Panes.Add(P);
	}
	Panes.Sort([](const FPane& A, const FPane& B) { return A.Box.Min.X < B.Box.Min.X; });
	UE_LOG(LogBodycamExterior, Log, TEXT("%d vitre(s) cassable(s) sur le mur nord"), Panes.Num());
}

bool ABodycamExterior::IsGlassBlocker(const UPrimitiveComponent* C) const
{
	if (!C) return false;
	for (const FPane& P : Panes) if (P.Blocker == C) return true;
	return false;
}

bool ABodycamExterior::BreakGlassAlong(const FVector& Start, const FVector& End, const FVector& Dir)
{
	const FVector E = End + Dir * 10.f;   // l'impact est pile sur la vitre : on prolonge un peu
	const float S0 = Start.Y - WallY, S1 = E.Y - WallY;
	if (S0 * S1 > 0.f || FMath::IsNearlyEqual(S0, S1)) return false;
	const FVector P = Start + (E - Start) * (S0 / (S0 - S1));
	for (int32 i = 0; i < Panes.Num(); ++i)
	{
		const FPane& Pane = Panes[i];
		if (Pane.bBroken) continue;
		if (P.X >= Pane.Box.Min.X && P.X <= Pane.Box.Max.X && P.Z >= Pane.Box.Min.Z && P.Z <= Pane.Box.Max.Z)
		{
			BreakPane(i, Dir);
			return true;
		}
	}
	return false;
}

void ABodycamExterior::BreakPane(int32 Index, const FVector& Dir)
{
	FPane& P = Panes[Index];
	P.bBroken = true;
	if (AActor* G = P.Glass.Get())
	{
		G->SetActorHiddenInGame(true);
		G->SetActorEnableCollision(false);
	}
	if (P.Blocker)
	{
		P.Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	const FVector C = P.Box.GetCenter();
	const FVector2D Size(P.Box.Max.X - P.Box.Min.X, P.Box.Max.Z - P.Box.Min.Z);
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABodycamImpactFX* FX = GetWorld()->SpawnActor<ABodycamImpactFX>(ABodycamImpactFX::StaticClass(), FVector(C.X, WallY, C.Z), FRotator::ZeroRotator, SP))
	{
		FX->InitGlass(Dir, Size, C.Z - 178.f);
	}

	// Son de verre : [Bodycam.Outdoor] GlassSound=/Game/... (sinon un fracas du Starter Content, plus aigu).
	FString SoundPath = TEXT("/Game/StarterContent/Audio/Collapse01.Collapse01");
	float Pitch = 2.2f;
	if (GConfig && GConfig->GetString(Sec, TEXT("GlassSound"), SoundPath, GGameIni)) Pitch = 1.f;
	if (USoundBase* Snd = LoadObject<USoundBase>(nullptr, *SoundPath))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Snd, FVector(C.X, WallY, C.Z), 0.7f, Pitch);
	}
	UE_LOG(LogBodycamExterior, Log, TEXT("Vitre cassee a X=%.0f"), C.X);
}

bool ABodycamExterior::IsInOpening(const FVector& P, float Margin) const
{
	// Vitres cassées voisines (séparées par un simple montant) = une seule grande ouverture.
	for (int32 i = 0; i < Panes.Num(); ++i)
	{
		if (!Panes[i].bBroken) continue;
		FBox O = Panes[i].Box;
		while (i + 1 < Panes.Num() && Panes[i + 1].bBroken && Panes[i + 1].Box.Min.X - O.Max.X < 40.f
			&& FMath::Abs(Panes[i + 1].Box.GetCenter().Z - O.GetCenter().Z) < 30.f)
		{
			O += Panes[++i].Box;
		}
		if (P.X > O.Min.X + Margin && P.X < O.Max.X - Margin && P.Z > O.Min.Z + Margin && P.Z < O.Max.Z - Margin
			&& P.Y > O.Min.Y && P.Y < O.Max.Y)
		{
			return true;
		}
	}
	return false;
}

bool ABodycamExterior::FindNearestOpening(const FVector& From, FVector& Out, bool& bOutBroken) const
{
	float Best = TNumericLimits<float>::Max();
	for (const FPane& P : Panes)
	{
		const float D = FVector::DistSquared(P.Box.GetCenter(), From);
		if (D < Best) { Best = D; Out = P.Box.GetCenter(); bOutBroken = P.bBroken; }
	}
	return Panes.Num() > 0;
}

void ABodycamExterior::BuildGround(const FVector& Min, const FVector& Max, float Z)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) return;
	Ground = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Ground"));
	Ground->SetMobility(EComponentMobility::Movable);
	Ground->SetStaticMesh(Cube);
	Ground->SetupAttachment(RootComponent);
	Ground->SetCollisionProfileName(TEXT("BlockAll"));
	Ground->SetCastShadow(false);
	Ground->RegisterComponent();
	FString MatPath = TEXT("/Game/DerelictCorridor/Assets/Fab/Megascans/Surfaces/Ind_Unf_Floor_Concrete_Dirty_02/MI_Ind_Unf_Floor_Concrete_Dirty_02.MI_Ind_Unf_Floor_Concrete_Dirty_02");
	if (GConfig) GConfig->GetString(Sec, TEXT("GroundMaterial"), MatPath, GGameIni);
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *MatPath)) Ground->SetMaterial(0, Mat);

	// Dalles de 4 m (une seule passe de rendu). Le sol continue au-delà de la zone jouable jusqu'aux immeubles du fond.
	const float Tile = 500.f, Pad = 9000.f;
	for (float X = Min.X - Pad; X < Max.X + Pad; X += Tile)
	{
		for (float Y = Min.Y; Y < Max.Y + Pad; Y += Tile)
		{
			const float Yaw = 90.f * FMath::RandRange(0, 3);   // casse la répétition de la texture
			Ground->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(X + Tile * 0.5f, Y + Tile * 0.5f, Z - 10.f), FVector(Tile / 100.f, Tile / 100.f, 0.2f)), true);
		}
	}
	Spawned.Add(Ground);
}

void ABodycamExterior::BuildBuilding(float GroundZ)
{
	UStaticMesh* B = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Bodycam/Exterior/SM_PA_Building12.SM_PA_Building12"));
	if (!B)
	{
		UE_LOG(LogBodycamExterior, Warning, TEXT("Batiment SM_PA_Building12 introuvable (lance Importer_Exterieur.bat)"));
		return;
	}
	float Scale = 0.0254f;
	if (GConfig) GConfig->GetFloat(Sec, TEXT("BuildingScale"), Scale, GGameIni);
	const FBoxSphereBounds Bd = B->GetBounds();

	// Immeuble en ruine au milieu (on peut voler autour) + immeubles en fond qui ferment la cour.
	struct FB { float X, Y, Yaw, S; bool bCollide; };
	const FB List[] = {
		{ 9100.f, 2600.f, 15.f, 1.f, true },
	};
	for (const FB& E : List)
	{
		const float S = Scale * E.S;
		const FVector Center = FRotator(0.f, E.Yaw, 0.f).RotateVector(Bd.Origin * S);
		const FVector Loc(E.X - Center.X, E.Y - Center.Y, GroundZ - (Bd.Origin.Z - Bd.BoxExtent.Z) * S);
		if (UStaticMeshComponent* C = AddMesh(B, FTransform(FRotator(0.f, E.Yaw, 0.f), Loc, FVector(S)), E.bCollide))
		{
			C->SetCastShadow(false);
		}
	}
	UE_LOG(LogBodycamExterior, Log, TEXT("Batiments : %d (%.0f x %.0f x %.0f cm)"), (int32)UE_ARRAY_COUNT(List), Bd.BoxExtent.X * 2 * Scale, Bd.BoxExtent.Y * 2 * Scale, Bd.BoxExtent.Z * 2 * Scale);
}

UInstancedStaticMeshComponent* ABodycamExterior::GetISM(UStaticMesh* Mesh, bool bCollide)
{
	const FString Key = FString::Printf(TEXT("%s_%d"), *Mesh->GetPathName(), bCollide ? 1 : 0);
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = ISMs.Find(Key)) return *Found;
	UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this);
	I->SetMobility(EComponentMobility::Movable);
	I->SetStaticMesh(Mesh);
	I->SetupAttachment(RootComponent);
	I->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	I->SetCastShadow(false);
	I->RegisterComponent();
	ISMs.Add(Key, I);
	Spawned.Add(I);
	return I;
}

bool ABodycamExterior::Place(const TCHAR* Folder, const TCHAR* MeshName, float X, float Y, float Yaw, float Scale, bool bCollide, float ZOffset, const FRotator& Tilt)
{
	const FString Path = FString::Printf(TEXT("/Game/DerelictCorridor/Assets/Fab/Megascans/3D/%s/%s.%s"), Folder, MeshName, MeshName);
	UStaticMesh* M = nullptr;
	if (TObjectPtr<UStaticMesh>* Cached = MeshCache.Find(Path)) M = *Cached;
	else
	{
		M = LoadObject<UStaticMesh>(nullptr, *Path);
		MeshCache.Add(Path, M);
		if (!M) UE_LOG(LogBodycamExterior, Warning, TEXT("Objet introuvable : %s"), *Path);
	}
	if (!M) return false;
	// Posé sur le sol (le bas du modèle à la hauteur du sol).
	const FBoxSphereBounds Bd = M->GetBounds();
	const FVector Loc(X, Y, PlaceGroundZ - (Bd.Origin.Z - Bd.BoxExtent.Z) * Scale + ZOffset);
	GetISM(M, bCollide)->AddInstance(FTransform(FRotator(Tilt.Pitch, Yaw, Tilt.Roll), Loc, FVector(Scale)), true);
	++NumProps;
	return true;
}

float ABodycamExterior::MeshHeight(const TCHAR* Folder, const TCHAR* MeshName) const
{
	const FString Path = FString::Printf(TEXT("/Game/DerelictCorridor/Assets/Fab/Megascans/3D/%s/%s.%s"), Folder, MeshName, MeshName);
	const TObjectPtr<UStaticMesh>* Cached = MeshCache.Find(Path);
	const UStaticMesh* M = Cached ? Cached->Get() : LoadObject<UStaticMesh>(nullptr, *Path);
	return M ? M->GetBounds().BoxExtent.Z * 2.f : 0.f;
}

void ABodycamExterior::BuildProps(float GroundZ)
{
	PlaceGroundZ = GroundZ;
	FRandomStream R(1337);   // même décor à chaque partie

	// Raccourcis : dossier Megascans + nom du modèle.
#define MS(F, N) TEXT(F), TEXT(N)
	const FVector Crate(6900.f, 1500.f, 0.f);

	// --- 1. Dépôt de ravitaillement autour de la caisse --------------------------------------------
	Place(MS("Ind_Ware_Storage_Pallet_Wood_Worn_01", "SM_Ind_Ware_Storage_Pallet_Wood_Worn_01"), Crate.X, Crate.Y, 10.f, 1.f, true);
	const float PalletH = MeshHeight(MS("Ind_Ware_Storage_Pallet_Wood_Worn_01", "SM_Ind_Ware_Storage_Pallet_Wood_Worn_01"));
	for (int32 i = 0; i < 4; ++i)   // pile de palettes
		Place(MS("Ind_Ware_Storage_Pallet_Wood_Worn_01", "SM_Ind_Ware_Storage_Pallet_Wood_Worn_01"), 7250.f, 1300.f, 80.f + R.FRandRange(-6.f, 6.f), 1.f, true, PalletH * i);
	for (int32 i = 0; i < 3; ++i)
		Place(MS("Ind_Ware_Storage_Pallet_Wood_Worn_01", "SM_Ind_Ware_Storage_Pallet_Wood_Worn_01"), 6450.f, 1900.f, -15.f + R.FRandRange(-8.f, 8.f), 1.f, true, PalletH * i);
	const float CrateH = MeshHeight(MS("Ind_Ware_Storage_Crate_Plastic_White_01", "SM_Ind_Ware_Storage_Crate_Plastic_White_01"));
	for (int32 i = 0; i < 3; ++i)
		Place(MS("Ind_Ware_Storage_Crate_Plastic_White_01", "SM_Ind_Ware_Storage_Crate_Plastic_White_01"), 6600.f, 1250.f, R.FRandRange(-20.f, 20.f), 1.f, true, CrateH * i);
	for (int32 i = 0; i < 2; ++i)
		Place(MS("Ind_Ware_Storage_Crate_Plastic_White_01", "SM_Ind_Ware_Storage_Crate_Plastic_White_01"), 6680.f, 1700.f, 35.f, 1.f, true, CrateH * i);
	Place(MS("Ind_Ware_Storage_Crate_Trap_Covered_03", "SM_Ind_Ware_Storage_Crate_Trap_Covered_03"), 7200.f, 1750.f, 20.f, 1.f, true);
	Place(MS("Ind_Ware_Storage_Crate_Trap_Covered_03", "SM_Ind_Ware_Storage_Crate_Trap_Covered_03"), 7450.f, 1650.f, 70.f, 1.f, true);
	const TCHAR* Oil[] = { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E"), TEXT("F") };
	for (int32 i = 0; i < 6; ++i)
	{
		const FString N = FString::Printf(TEXT("SM_Ind_Container_Oil_Plastic_Pack_01_%s"), Oil[i]);
		Place(TEXT("Ind_Container_Oil_Plastic_Pack_01"), *N, 7050.f + (i % 3) * 70.f, 1950.f + (i / 3) * 70.f, R.FRandRange(0.f, 360.f), 1.f, true);
	}
	Place(MS("Ind_AbaFact_Tank_Gas_Metal_Worn_02", "SM_Ind_AbaFact_Tank_Gas_Metal_Worn_02"), 7550.f, 1150.f, 40.f, 1.f, true);
	Place(MS("Ind_AbaFact_Tarp_Wrinkled_01", "SM_Ind_AbaFact_Tarp_Wrinkled_01"), 7350.f, 1900.f, 30.f, 1.f, false);
	Place(MS("Ind_OW_Equipment_Toolbox_Metal_Green_01", "SM_Ind_OW_Equipment_Toolbox_Metal_Green_01"), 7000.f, 1230.f, 65.f, 1.f, false);
	Place(MS("Ind_Ware_Equipment_Handtruck_Metal_Worn_01", "SM_Ind_Ware_Equipment_Handtruck_Metal_Worn_01_A"), 6750.f, 1980.f, 120.f, 1.f, true);
	Place(MS("Ind_Ware_Equipment_Cable_Rubber_Black_01", "SM_Ind_Ware_Equipment_Cable_Rubber_Black_01"), 6820.f, 1180.f, 0.f, 1.f, false);

	// --- 2. Poste de contrôle abandonné devant les fenêtres + barricade ----------------------------
	Place(MS("Ind_Ware_Furn_Table_Wood_Worn_01", "SM_Ind_Ware_Furn_Table_Wood_Worn_01_A"), 7700.f, -250.f, 5.f, 1.f, true);
	Place(MS("Res_Furn_Chair_Metal_Armless_01", "SM_Res_Furn_Chair_Metal_Armless_01"), 7600.f, -340.f, 200.f, 1.f, true);
	Place(MS("Res_Furn_Chair_Metal_Armless_01", "SM_Res_Furn_Chair_Metal_Armless_01"), 7850.f, -150.f, 60.f, 1.f, true, 0.f, FRotator(0.f, 0.f, 88.f));
	Place(MS("Res_Furn_Stool_Metal_Worn_01", "SM_Res_Furn_Stool_Metal_Worn_01"), 7560.f, -120.f, 10.f, 1.f, true);
	Place(MS("Ind_Ware_Furn_Table_Wood_Worn_01", "SM_Ind_Ware_Furn_Table_Wood_Worn_01_C"), 5000.f, -200.f, 95.f, 1.f, true);
	BuildBarriers(GroundZ, R);

	// --- 3. Coffrets électriques et tuyaux le long du mur du bâtiment ------------------------------
	struct FWallItem { const TCHAR* F; const TCHAR* N; float X; };
	const FWallItem WallItems[] = {
		{ TEXT("Urb_Street_ElectricalBox_Metal_Worn_01"), TEXT("SM_Urb_Street_ElectricalBox_Metal_Worn_01"), 4800.f },
		{ TEXT("Urb_Street_ElectricalBox_Metal_Worn_02"), TEXT("SM_Urb_Street_ElectricalBox_Metal_Worn_02"), 5050.f },
		{ TEXT("Ind_Ware_Cabinet_Electric_Metal_Dirty_01"), TEXT("SM_Ind_Ware_Cabinet_Electric_Metal_Dirty_01"), 6050.f },
		{ TEXT("Urb_Road_Cabinet_Electric_Metal_01"), TEXT("SM_Urb_Road_Cabinet_Electric_Metal_01"), 8000.f },
		{ TEXT("Ind_Aba_Box_Electric_Metal_Worn_01"), TEXT("SM_Ind_Aba_Box_Electric_Metal_Worn_01"), 8400.f },
		{ TEXT("Ind_Aba_Box_Electric_Metal_Worn_02"), TEXT("SM_Ind_Aba_Box_Electric_Metal_Worn_02"), 8550.f },
		{ TEXT("Ind_Fact_Panel_ElectricalBox_Metal_Worn_01"), TEXT("SM_Ind_Fact_Panel_ElectricalBox_Metal_Worn_01"), 9800.f },
		{ TEXT("Urb_Office_Heater_White_Old_01"), TEXT("SM_Urb_Office_Heater_White_Old_01"), 10150.f },
		{ TEXT("Ind_Ware_Storage_Cabinet_Metal_Rusty_01"), TEXT("SM_Ind_Ware_Storage_Cabinet_Metal_Rusty_01"), 10500.f },
		{ TEXT("Ind_Aba_Box_Electric_Metal_Worn_03"), TEXT("SM_Ind_Aba_Box_Electric_Metal_Worn_03"), 10900.f },
	};
	for (const FWallItem& W : WallItems) Place(W.F, W.N, W.X, -680.f, 180.f + R.FRandRange(-4.f, 4.f), 1.f, true);
	Place(MS("Ind_Con_Set_Pipe_PVC_Mod_01", "SM_Ind_Con_Set_Pipe_PVC_Mod_01_A"), 5500.f, -560.f, 0.f, 1.f, false);
	Place(MS("Ind_Con_Set_Pipe_PVC_Mod_01", "SM_Ind_Con_Set_Pipe_PVC_Mod_01_C"), 9500.f, -580.f, 5.f, 1.f, false);
	Place(MS("Ind_Ware_Equipment_Ladder_Metal_Worn_01", "SM_Ind_Ware_Equipment_Ladder_Metal_Worn_01"), 10700.f, -600.f, 180.f, 1.f, true, 0.f, FRotator(-18.f, 0.f, 0.f));

	// --- 4. Fûts et bidons métalliques (côté ouest) ------------------------------------------------
	const TCHAR* Drum[] = { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E") };
	for (int32 i = 0; i < 16; ++i)
	{
		const FString N = FString::Printf(TEXT("SM_Ind_Container_Metal_Set_01_%s"), Drum[i % 5]);
		const bool bFallen = R.FRand() < 0.25f;
		Place(TEXT("Ind_Container_Metal_Set_01"), *N, 5000.f + R.FRandRange(0.f, 700.f), 2300.f + R.FRandRange(0.f, 1200.f), R.FRandRange(0.f, 360.f), 1.f, true,
			0.f, bFallen ? FRotator(0.f, 0.f, 90.f) : FRotator::ZeroRotator);
	}
	Place(MS("Ind_AbaFact_Tarp_Wrinkled_02", "SM_Ind_AbaFact_Tarp_Wrinkled_02"), 5400.f, 3000.f, 140.f, 1.f, false);
	Place(MS("Ind_Ware_Storage_Shelf_Metal_Rusted_01", "SM_Ind_Ware_Storage_Shelf_Metal_Rusted_01"), 4900.f, 3800.f, 90.f, 1.f, true);
	Place(MS("Ind_Ware_Storage_Shelf_Metal_Rusted_01", "SM_Ind_Ware_Storage_Shelf_Metal_Rusted_01"), 5150.f, 3850.f, 85.f, 1.f, true, 0.f, FRotator(0.f, 0.f, -70.f));

	// --- 5. Chantier de tuyaux et poutres (côté est) -----------------------------------------------
	const TCHAR* Pipe[] = { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"), TEXT("E"), TEXT("F"), TEXT("G") };
	for (int32 i = 0; i < 7; ++i)
	{
		const FString N = FString::Printf(TEXT("SM_Ind_Set_Pipe_Metal_Mod_01_%s"), Pipe[i]);
		Place(TEXT("Ind_Set_Pipe_Metal_Mod_01"), *N, 10300.f + (i % 4) * 160.f, 500.f + (i / 4) * 300.f, 90.f + R.FRandRange(-10.f, 10.f), 1.f, true);
	}
	const TCHAR* Beam[] = { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D") };
	for (int32 i = 0; i < 8; ++i)
	{
		const FString N = FString::Printf(TEXT("SM_Ind_Con_Supplies_Beam_Metal_Rusty_02_%s"), Beam[i % 4]);
		Place(TEXT("Ind_Con_Supplies_Beam_Metal_Rusty_02"), *N, 10200.f + R.FRandRange(0.f, 800.f), 1400.f + R.FRandRange(0.f, 500.f), R.FRandRange(0.f, 180.f), 1.f, true);
	}
	for (int32 i = 0; i < 6; ++i)
	{
		Place(MS("Ind_Con_Supplies_Plank_Wood_Long_01", "SM_Ind_Con_Supplies_Plank_Wood_Long_01"), 8300.f + R.FRandRange(0.f, 500.f), 200.f + R.FRandRange(0.f, 400.f), R.FRandRange(0.f, 180.f), 1.f, false);
		Place(MS("Ind_Con_Supplies_Plank_Wood_Long_02", "SM_Ind_Con_Supplies_Plank_Wood_Long_02"), 5600.f + R.FRandRange(0.f, 600.f), 600.f + R.FRandRange(0.f, 500.f), R.FRandRange(0.f, 180.f), 1.f, false);
		Place(MS("Ind_Mine_Beam_Square_Wood_Worn_01", "SM_Ind_Mine_Beam_Square_Wood_Worn_01"), 9000.f + R.FRandRange(0.f, 1800.f), 3800.f + R.FRandRange(0.f, 300.f), R.FRandRange(0.f, 180.f), 1.f, true);
	}
	Place(MS("Ind_Ware_Equipment_Ladder_Metal_01", "SM_Ind_Ware_Equipment_Ladder_Metal_01"), 8000.f, 1250.f, 30.f, 1.f, true, 0.f, FRotator(0.f, 0.f, 90.f));

	// --- 6. Gros tas de gravats (bloquent le drone) -------------------------------------------------
	struct FRub { const TCHAR* F; const TCHAR* N; };
	const FRub Rubble[] = {
		{ TEXT("Ind_Rubble_Pile_Concrete_M_01"), TEXT("SM_Ind_Rubble_Pile_Concrete_M_01") },
		{ TEXT("Ind_Rubble_Pile_Concrete_M_02"), TEXT("SM_Ind_Rubble_Pile_Concrete_M_02") },
		{ TEXT("Ind_Rubble_Pile_Debris_Concrete_M_01"), TEXT("SM_Ind_Rubble_Pile_Debris_Concrete_M_01") },
		{ TEXT("Ind_Rubble_Pile_Debris_Concrete_M_05"), TEXT("SM_Ind_Rubble_Pile_Debris_Concrete_M_05") },
		{ TEXT("Ind_Rubble_Debris_Pile_Gravel_M_01"), TEXT("SM_Ind_Rubble_Debris_Pile_Gravel_M_01") },
	};
	int32 Piles = 0;
	for (int32 Try = 0; Try < 200 && Piles < 18; ++Try)
	{
		const FVector P(R.FRandRange(4700.f, 11100.f), R.FRandRange(400.f, 4000.f), 0.f);
		if (!IsFreeSpot(P, Crate, 500.f)) continue;
		const FRub& Rb = Rubble[Piles % (int32)UE_ARRAY_COUNT(Rubble)];
		Place(Rb.F, Rb.N, P.X, P.Y, R.FRandRange(0.f, 360.f), R.FRandRange(0.8f, 1.4f), true);
		++Piles;
	}

	// --- 7. Débris au sol partout (pas de collision : le drone passe au-dessus) -------------------
	struct FDeb { const TCHAR* F; const TCHAR* N; float SMin, SMax; };
	const FDeb Debris[] = {
		{ TEXT("Ind_Rubble_Debris_Concrete_Pack_01"), TEXT("SM_Ind_Rubble_Debris_Concrete_Pack_01_A"), 0.8f, 1.5f },
		{ TEXT("Ind_Rubble_Debris_Concrete_Pack_01"), TEXT("SM_Ind_Rubble_Debris_Concrete_Pack_01_B"), 0.8f, 1.5f },
		{ TEXT("Ind_Rubble_Debris_Concrete_Pack_01"), TEXT("SM_Ind_Rubble_Debris_Concrete_Pack_01_C"), 0.8f, 1.5f },
		{ TEXT("Ind_Rubble_Debris_Concrete_Pack_01"), TEXT("SM_Ind_Rubble_Debris_Concrete_Pack_01_D"), 0.8f, 1.5f },
		{ TEXT("Ind_Rubble_Debris_Concrete_Pack_01"), TEXT("SM_Ind_Rubble_Debris_Concrete_Pack_01_E"), 0.8f, 1.5f },
		{ TEXT("Ind_Rubble_Debris_Concrete_Pack_01"), TEXT("SM_Ind_Rubble_Debris_Concrete_Pack_01_F"), 0.8f, 1.5f },
		{ TEXT("Ind_Rubble_Debris_Plank_Wood_Broken_01"), TEXT("SM_Ind_Rubble_Debris_Plank_Wood_Broken_01"), 0.9f, 1.2f },
		{ TEXT("His_Med_Debris_Piece_Wood_Damaged_02"), TEXT("SM_His_Med_Debris_Piece_Wood_Damaged_02"), 0.9f, 1.2f },
		{ TEXT("Forest_Scatter_Rock_Set_01"), TEXT("SM_Forest_Scatter_Rock_Set_01_A"), 0.8f, 1.6f },
		{ TEXT("Forest_Scatter_Rock_Set_01"), TEXT("SM_Forest_Scatter_Rock_Set_01_B"), 0.8f, 1.6f },
		{ TEXT("Forest_Scatter_Rock_Set_01"), TEXT("SM_Forest_Scatter_Rock_Set_01_C"), 0.8f, 1.6f },
		{ TEXT("Forest_Scatter_Rock_Set_01"), TEXT("SM_Forest_Scatter_Rock_Set_01_D"), 0.8f, 1.6f },
		{ TEXT("Forest_Scatter_Rock_Set_01"), TEXT("SM_Forest_Scatter_Rock_Set_01_E"), 0.8f, 1.6f },
		{ TEXT("Forest_Scatter_Branch_Debris_04"), TEXT("SM_Forest_Scatter_Branch_Debris_04"), 0.8f, 1.3f },
		{ TEXT("Urb_Road_Sheet_Metal_Rusty_03"), TEXT("SM_Urb_Road_Sheet_Metal_Rusty_03"), 0.9f, 1.2f },
		{ TEXT("Ind_Ware_Storage_Sheet_Cardboard_Flat_06"), TEXT("SM_Ind_Ware_Storage_Sheet_Cardboard_Flat_06_A"), 0.9f, 1.2f },
		{ TEXT("Ind_Ware_Storage_Sheet_Cardboard_Flat_06"), TEXT("SM_Ind_Ware_Storage_Sheet_Cardboard_Flat_06_B"), 0.9f, 1.2f },
		{ TEXT("Ind_Junk_Cover_Drain_Metal_Rusty_01"), TEXT("SM_Ind_Junk_Cover_Drain_Metal_Rusty_01"), 1.f, 1.f },
		{ TEXT("Urb_Street_Hatch_Metal_Worn_01"), TEXT("SM_Urb_Street_Hatch_Metal_Worn_01"), 1.f, 1.f },
	};
	int32 Deb = 0;
	for (int32 Try = 0; Try < 600 && Deb < 170; ++Try)
	{
		const FVector P(R.FRandRange(4600.f, 11200.f), R.FRandRange(-600.f, 4100.f), 0.f);
		if (!IsFreeSpot(P, Crate, 160.f)) continue;
		const FDeb& D = Debris[R.RandRange(0, (int32)UE_ARRAY_COUNT(Debris) - 1)];
		Place(D.F, D.N, P.X, P.Y, R.FRandRange(0.f, 360.f), R.FRandRange(D.SMin, D.SMax), false);
		++Deb;
	}
	// Plaques de gravier (casse l'aspect "sol uniforme").
	for (int32 i = 0; i < 14; ++i)
	{
		const FVector P(R.FRandRange(4800.f, 11000.f), R.FRandRange(-300.f, 4000.f), 0.f);
		if (!IsFreeSpot(P, Crate, 200.f)) continue;
		Place(MS("Ind_Con_Pile_Rubble_Gravel_Patch_01", "SM_Ind_Con_Pile_Rubble_Gravel_Patch_01"), P.X, P.Y, R.FRandRange(0.f, 360.f), R.FRandRange(1.f, 2.f), false);
	}
#undef MS
	UE_LOG(LogBodycamExterior, Log, TEXT("%d objet(s) exterieur(s) (%d types)"), NumProps, ISMs.Num());
}

bool ABodycamExterior::IsFreeSpot(const FVector& P, const FVector& Crate, float CrateClear) const
{
	if (FVector::Dist2D(P, Crate) < CrateClear) return false;
	// Pas dans l'immeuble central (empreinte approximative) ni collé aux fenêtres de sortie.
	if (P.X > 7800.f && P.X < 10400.f && P.Y > 1500.f && P.Y < 3700.f) return false;
	if (P.Y < -350.f && P.X > 5200.f && P.X < 9500.f) return false;
	return true;
}

void ABodycamExterior::BuildBarriers(float GroundZ, FRandomStream& R)
{
	// Barricade de blocs de béton entre le bâtiment et la cour, avec deux passages pour le drone.
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) return;
	UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this);
	I->SetMobility(EComponentMobility::Movable);
	I->SetStaticMesh(Cube);
	I->SetupAttachment(RootComponent);
	I->SetCollisionProfileName(TEXT("BlockAll"));
	I->SetCastShadow(false);
	I->RegisterComponent();
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/DerelictCorridor/Assets/Fab/Megascans/Surfaces/Ind_Unf_Floor_Concrete_Dirty_02/MI_Ind_Unf_Floor_Concrete_Dirty_02.MI_Ind_Unf_Floor_Concrete_Dirty_02")))
	{
		I->SetMaterial(0, Mat);
	}
	Spawned.Add(I);
	int32 N = 0;
	auto Block = [&](float X, float Y, float Yaw, bool bFallen)
	{
		const FRotator Rot(0.f, Yaw, bFallen ? 90.f : R.FRandRange(-3.f, 3.f));
		const float Z = GroundZ + (bFallen ? 25.f : 45.f);
		I->AddInstance(FTransform(Rot, FVector(X, Y, Z), FVector(1.9f, 0.5f, 0.9f)), true);
		++N;
	};
	for (float X = 4800.f; X < 11000.f; X += 205.f)
	{
		if ((X > 6300.f && X < 6900.f) || (X > 8600.f && X < 9100.f)) continue;   // passages
		if (R.FRand() < 0.12f) continue;                                           // bloc manquant
		Block(X + R.FRandRange(-15.f, 15.f), 380.f + R.FRandRange(-25.f, 25.f), R.FRandRange(-6.f, 6.f), R.FRand() < 0.15f);
	}
	// Quelques blocs éparpillés dans la cour.
	for (int32 i = 0; i < 14; ++i)
	{
		const FVector P(R.FRandRange(4800.f, 11000.f), R.FRandRange(900.f, 4000.f), 0.f);
		if (!IsFreeSpot(P, FVector(6900.f, 1500.f, 0.f), 450.f)) continue;
		Block(P.X, P.Y, R.FRandRange(0.f, 180.f), R.FRand() < 0.3f);
	}
	UE_LOG(LogBodycamExterior, Log, TEXT("%d bloc(s) de beton"), N);
}

// ------------------------------------------------------------------ Ville en ruine autour de la cour

UHierarchicalInstancedStaticMeshComponent* ABodycamExterior::GetHISM(UStaticMesh* Mesh, UMaterialInterface* Mat, const FString& Key)
{
	if (TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* Found = CityHISMs.Find(Key)) return *Found;
	UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
	H->SetMobility(EComponentMobility::Movable);
	H->SetStaticMesh(Mesh);
	if (Mat) H->SetMaterial(0, Mat);
	H->SetupAttachment(RootComponent);
	H->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // la cour est fermée par des murs invisibles posés sur les façades
	H->SetCastShadow(false);
	H->RegisterComponent();
	CityHISMs.Add(Key, H);
	Spawned.Add(H);
	return H;
}

UMaterialInterface* ABodycamExterior::CityMat(const TCHAR* Name)
{
	if (TObjectPtr<UMaterialInterface>* Found = CityMats.Find(Name)) return *Found;
	const FString Path = FString::Printf(TEXT("/Game/Bodycam/Materials/MI_%s.MI_%s"), Name, Name);
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *Path);
	if (!M) M = ConcreteMat;
	CityMats.Add(Name, M);
	return M;
}

void ABodycamExterior::CityBox(const FVector& Center, const FVector& Size, UMaterialInterface* Mat, float Yaw, float Roll, bool bCylinder)
{
	UStaticMesh* Mesh = bCylinder ? CylinderMesh : CubeMesh;
	if (!Mesh) return;
	const FString Key = FString::Printf(TEXT("%s_%s"), bCylinder ? TEXT("Cyl") : TEXT("Box"), Mat ? *Mat->GetName() : TEXT("None"));
	GetHISM(Mesh, Mat, Key)->AddInstance(FTransform(FRotator(0.f, Yaw, Roll), Center, Size / 100.f), true);
	++NumBoxes;
}

void ABodycamExterior::BuildBlock(float X0, float X1, float Y0, float Y1, float Height, float GroundZ, FRandomStream& R, bool bDetailed)
{
	// Immeuble en "boîtes" + matériaux alignés sur le monde (pas d'étirement de texture), fenêtres, toit.
	const TCHAR* Walls[] = { TEXT("BrickRed"), TEXT("BrickBeige"), TEXT("ConcreteDirty"), TEXT("ConcretePanels"), TEXT("Plaster") };
	UMaterialInterface* Wall = CityMat(Walls[R.RandRange(0, 4)]);
	const bool bRuined = R.FRand() < 0.45f;
	const float Main = bRuined ? Height * R.FRandRange(0.5f, 0.8f) : Height;
	const FVector C((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, GroundZ);
	const FVector Sz(X1 - X0, Y1 - Y0, Main);
	CityBox(C + FVector(0, 0, Main * 0.5f), Sz, Wall);

	if (bRuined)
	{
		// Haut effondré : morceaux d'étages qui restent debout, de travers.
		const int32 N = R.RandRange(1, 3);
		for (int32 i = 0; i < N; ++i)
		{
			const float FX = R.FRandRange(0.3f, 0.7f) * Sz.X, FY = R.FRandRange(0.3f, 0.7f) * Sz.Y, FH = R.FRandRange(0.15f, 0.45f) * Height;
			const float PX = (R.FRand() < 0.5f) ? X0 + FX * 0.5f : X1 - FX * 0.5f, PY = (R.FRand() < 0.5f) ? Y0 + FY * 0.5f : Y1 - FY * 0.5f;
			CityBox(FVector(PX, PY, GroundZ + Main + FH * 0.5f), FVector(FX, FY, FH), Wall, 0.f, R.FRandRange(-2.f, 2.f));
		}
		if (bDetailed) CityBox(C + FVector(R.FRandRange(-0.2f, 0.2f) * Sz.X, R.FRandRange(-0.2f, 0.2f) * Sz.Y, Main + 4.f), FVector(Sz.X * 0.5f, Sz.Y * 0.4f, 20.f), CityMat(TEXT("Charred")));
	}
	else if (bDetailed)
	{
		CityBox(C + FVector(0, 0, Main + 40.f), FVector(Sz.X + 30.f, Sz.Y + 30.f, 30.f), CityMat(TEXT("ConcreteLight")));   // corniche
		if (R.FRand() < 0.6f) CityBox(C + FVector(R.FRandRange(-0.3f, 0.3f) * Sz.X, R.FRandRange(-0.3f, 0.3f) * Sz.Y, Main + 200.f), FVector(220.f, 220.f, 300.f), CityMat(TEXT("MetalRust")), 0.f, 0.f, true);   // réservoir
		for (int32 i = 0; i < 3; ++i) CityBox(C + FVector(R.FRandRange(-0.4f, 0.4f) * Sz.X, R.FRandRange(-0.4f, 0.4f) * Sz.Y, Main + 60.f), FVector(120.f, 90.f, 80.f), CityMat(TEXT("SteelDark")));   // clims
	}

	// Fenêtres : surtout noires ou cassées (ville abandonnée), quelques-unes éclairées par des survivants.
	const float FloorH = 320.f, Spacing = 360.f, WW = 140.f, WH = 170.f;
	UMaterialInterface* Dark = CityMat(TEXT("Charred"));
	UMaterialInterface* Glass = CityMat(TEXT("Glass"));
	UMaterialInterface* Warm = CityMat(TEXT("Emissive_WindowWarm"));
	UMaterialInterface* Cold = CityMat(TEXT("Emissive_WindowCold"));
	auto Face = [&](bool bAlongX, float A0, float A1, float Fixed, float Out)
	{
		const int32 Count = FMath::Max(0, (int32)((A1 - A0 - 120.f) / Spacing));
		const float Start = A0 + ((A1 - A0) - (Count - 1) * Spacing) * 0.5f;
		for (float Z = GroundZ + FloorH; Z + FloorH <= GroundZ + Main; Z += FloorH)
		{
			for (int32 i = 0; i < Count; ++i)
			{
				UMaterialInterface* M = nullptr;
				const float P = R.FRand();
				if (bDetailed) M = P < 0.05f ? Warm : P < 0.07f ? Cold : P < 0.55f ? Dark : P < 0.92f ? Glass : nullptr;
				else M = P < 0.035f ? Warm : P < 0.045f ? Cold : nullptr;   // au loin : seulement les fenêtres allumées
				if (!M) continue;
				const float T = Start + i * Spacing;
				const FVector WC = bAlongX ? FVector(T, Fixed + Out * 4.f, Z + 90.f + WH * 0.5f) : FVector(Fixed + Out * 4.f, T, Z + 90.f + WH * 0.5f);
				CityBox(WC, bAlongX ? FVector(WW, 10.f, WH) : FVector(10.f, WW, WH), M);
			}
		}
		// Rez-de-chaussée : rideaux de fer des commerces.
		if (bDetailed)
		{
			for (float T = A0 + 250.f; T < A1 - 250.f; T += R.FRandRange(550.f, 800.f))
			{
				const FVector SC = bAlongX ? FVector(T, Fixed + Out * 5.f, GroundZ + 150.f) : FVector(Fixed + Out * 5.f, T, GroundZ + 150.f);
				CityBox(SC, bAlongX ? FVector(300.f, 10.f, 300.f) : FVector(10.f, 300.f, 300.f), CityMat(R.FRand() < 0.5f ? TEXT("MetalRust") : TEXT("SteelDark")));
			}
		}
	};
	Face(true, X0, X1, Y0, -1.f);
	Face(true, X0, X1, Y1, 1.f);
	Face(false, Y0, Y1, X0, -1.f);
	Face(false, Y0, Y1, X1, 1.f);
	++NumTowers;
}

void ABodycamExterior::BuildRow(bool bAlongX, float From, float To, float Face, float Dir, float GroundZ, FRandomStream& R,
	float MinH, float MaxH, float MinDepth, float MaxDepth, bool bAlleys, bool bDetailed, TArray<FVector4>& OutAlleys)
{
	// Une rangée d'immeubles collés les uns aux autres, avec parfois une ruelle entre deux.
	float P = From;
	while (P < To)
	{
		const float Len = R.FRandRange(1200.f, 2600.f), Depth = R.FRandRange(MinDepth, MaxDepth);
		const float A0 = P, A1 = P + Len;
		const float B0 = Dir > 0.f ? Face : Face - Depth, B1 = Dir > 0.f ? Face + Depth : Face;
		const float H = R.FRandRange(MinH, MaxH);
		if (bAlongX) BuildBlock(A0, A1, B0, B1, H, GroundZ, R, bDetailed);
		else BuildBlock(B0, B1, A0, A1, H, GroundZ, R, bDetailed);
		// Gravats au pied de certains immeubles.
		if (bDetailed && R.FRand() < 0.35f)
		{
			const float T = R.FRandRange(A0 + 200.f, A1 - 200.f);
			const float D = Face - Dir * R.FRandRange(150.f, 400.f);
			Place(TEXT("Ind_Rubble_Pile_Debris_Concrete_M_01"), TEXT("SM_Ind_Rubble_Pile_Debris_Concrete_M_01"), bAlongX ? T : D, bAlongX ? D : T, R.FRandRange(0.f, 360.f), 0.8f, false);
		}
		P = A1;
		if (bAlleys && R.FRand() < 0.4f && P < To - 600.f)
		{
			const float Gap = R.FRandRange(650.f, 950.f);
			OutAlleys.Add(FVector4(P, P + Gap, Face, Dir));   // ruelle : début, fin, façade, sens
			P += Gap;
		}
		else
		{
			P += R.FRandRange(0.f, 30.f);
		}
	}
}

void ABodycamExterior::BuildStreet(bool bAlongX, float From, float To, float Near, float Dir, float GroundZ, FRandomStream& R)
{
	// Rue (asphalte + trottoirs) parallèle à la cour, derrière la 1re rangée : voitures abandonnées, lampadaires.
	const float W = 1300.f, Side = 260.f;
	const float S0 = Near, S1 = Near + Dir * W;
	const float Lo = FMath::Min(S0, S1), Hi = FMath::Max(S0, S1), Mid = (Lo + Hi) * 0.5f, Len = To - From, LMid = (From + To) * 0.5f;
	auto At = [&](float Along, float Across, float Z) { return bAlongX ? FVector(Along, Across, Z) : FVector(Across, Along, Z); };
	auto Sz = [&](float Along, float Across, float Z) { return bAlongX ? FVector(Along, Across, Z) : FVector(Across, Along, Z); };
	CityBox(At(LMid, Mid, GroundZ + 0.5f), Sz(Len, W, 2.f), CityMat(TEXT("Asphalt")));
	CityBox(At(LMid, Lo - Side * 0.5f, GroundZ + 8.f), Sz(Len, Side, 16.f), CityMat(TEXT("Sidewalk")));
	CityBox(At(LMid, Hi + Side * 0.5f, GroundZ + 8.f), Sz(Len, Side, 16.f), CityMat(TEXT("Sidewalk")));
	// Marquage central en pointillés.
	for (float T = From + 200.f; T < To; T += 900.f) CityBox(At(T, Mid, GroundZ + 2.f), Sz(400.f, 15.f, 1.f), CityMat(TEXT("ConcreteLight")));
	// Voitures abandonnées / épaves brûlées.
	const TCHAR* Paints[] = { TEXT("CarRed"), TEXT("CarBlue"), TEXT("CarGrey"), TEXT("CarWhite") };
	for (float T = From + R.FRandRange(300.f, 1200.f); T < To - 300.f; T += R.FRandRange(900.f, 2200.f))
	{
		const bool bLane = (R.FRand() < 0.5f);
		const float Across = bLane ? Lo + 260.f : Hi - 260.f;
		const float Yaw = (bAlongX ? 0.f : 90.f) + (bLane ? 0.f : 180.f) + (R.FRand() < 0.3f ? R.FRandRange(-35.f, 35.f) : R.FRandRange(-4.f, 4.f));
		BuildCar(At(T, Across, GroundZ), Yaw, R.FRand() < 0.4f, Paints[R.RandRange(0, 3)], R);
	}
	// Lampadaires (la plupart éteints).
	for (float T = From + 600.f; T < To; T += 2400.f)
	{
		const bool bNearSide = (R.FRand() < 0.5f);
		const float Across = bNearSide ? Lo - 80.f : Hi + 80.f;
		const FVector Base = At(T, Across, GroundZ);
		CityBox(Base + FVector(0, 0, 310.f), FVector(14.f, 14.f, 620.f), CityMat(TEXT("SteelDark")), 0.f, 0.f, true);
		const bool bLit = R.FRand() < 0.3f;
		CityBox(Base + FVector(0, 0, 625.f), FVector(40.f, 40.f, 18.f), bLit ? CityMat(TEXT("Emissive_StreetLamp")) : CityMat(TEXT("SteelDark")));
		if (bLit && StreetLights < 10)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetMobility(EComponentMobility::Movable);
			L->SetupAttachment(RootComponent);
			L->SetWorldLocation(Base + FVector(0, 0, 580.f));
			L->SetIntensityUnits(ELightUnits::Candelas);
			L->SetIntensity(300.f);
			L->SetAttenuationRadius(1500.f);
			L->SetLightColor(FLinearColor(1.f, 0.6f, 0.28f));
			L->SetCastShadows(false);
			L->RegisterComponent();
			Spawned.Add(L);
			++StreetLights;
		}
	}
	// Déchets et gravats sur la chaussée.
	for (int32 i = 0; i < 12; ++i)
	{
		const FVector P = At(R.FRandRange(From, To), R.FRandRange(Lo, Hi), GroundZ);
		const int32 K = R.RandRange(0, 3);
		if (K == 0) Place(TEXT("Ind_Rubble_Debris_Concrete_Pack_01"), TEXT("SM_Ind_Rubble_Debris_Concrete_Pack_01_D"), P.X, P.Y, R.FRandRange(0.f, 360.f), 1.3f, false);
		else if (K == 1) Place(TEXT("Ind_Container_Metal_Set_01"), TEXT("SM_Ind_Container_Metal_Set_01_C"), P.X, P.Y, R.FRandRange(0.f, 360.f), 1.f, false, 0.f, FRotator(0.f, 0.f, 90.f));
		else if (K == 2) Place(TEXT("Ind_Ware_Storage_Sheet_Cardboard_Flat_06"), TEXT("SM_Ind_Ware_Storage_Sheet_Cardboard_Flat_06_B"), P.X, P.Y, R.FRandRange(0.f, 360.f), 1.f, false);
		else Place(TEXT("Forest_Scatter_Branch_Debris_04"), TEXT("SM_Forest_Scatter_Branch_Debris_04"), P.X, P.Y, R.FRandRange(0.f, 360.f), 1.f, false);
	}
}

void ABodycamExterior::BuildCar(const FVector& Base, float Yaw, bool bBurnt, const TCHAR* Paint, FRandomStream& R)
{
	const FRotator Rot(0.f, Yaw, 0.f);
	auto At = [&](float DX, float DY, float Z) { return Base + Rot.RotateVector(FVector(DX, DY, 0.f)) + FVector(0, 0, Z); };
	UMaterialInterface* Body = bBurnt ? CityMat(TEXT("Charred")) : CityMat(Paint);
	const float Roll = bBurnt ? R.FRandRange(-5.f, 5.f) : 0.f;
	CityBox(At(0.f, 0.f, 62.f), FVector(440.f, 178.f, 62.f), Body, Yaw, Roll);
	CityBox(At(-25.f, 0.f, 122.f), FVector(230.f, 160.f, 58.f), bBurnt ? CityMat(TEXT("Charred")) : CityMat(TEXT("Glass")), Yaw, Roll);
	CityBox(At(-30.f, 0.f, 153.f), FVector(200.f, 152.f, 6.f), Body, Yaw, Roll);
	if (!bBurnt || R.FRand() < 0.5f)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const float DX = i < 2 ? -140.f : 140.f, DY = (i % 2) ? -82.f : 82.f;
			// Cylindre couché (axe le long de Y de la voiture).
			UStaticMesh* Mesh = CylinderMesh;
			if (!Mesh) break;
			const FString Key = TEXT("Wheel");
			GetHISM(Mesh, CityMat(TEXT("Rubber")), Key)->AddInstance(FTransform(FRotator(0.f, Yaw, 90.f), At(DX, DY, 31.f), FVector(0.62f, 0.62f, 0.22f)), true);
		}
	}
	else
	{
		CityBox(At(0.f, 0.f, 18.f), FVector(400.f, 160.f, 20.f), CityMat(TEXT("Charred")), Yaw);   // posée sur les jantes
	}
}

void ABodycamExterior::BuildCity(const FVector& Min, const FVector& Max, float GroundZ)
{
	ConcreteMat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/DerelictCorridor/Assets/Fab/Megascans/Surfaces/Ind_Unf_Floor_Concrete_Dirty_02/MI_Ind_Unf_Floor_Concrete_Dirty_02.MI_Ind_Unf_Floor_Concrete_Dirty_02"));
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!CubeMesh) return;

	int32 Density = 2;   // 1 = léger, 2 = normal, 3 = dense
	if (GConfig) GConfig->GetInt(Sec, TEXT("CityDensity"), Density, GGameIni);
	FRandomStream R(4242);
	TArray<FVector4> Alleys, Unused;

	// 1re rangée : leurs façades ferment la cour (pile sur les murs invisibles), avec des ruelles.
	const float N1 = Max.Y + 100.f, W1 = Min.X - 100.f, E1 = Max.X + 100.f;
	const float D1 = 1800.f;          // profondeur max de la 1re rangée
	const float Street = 1300.f;
	BuildRow(true, Min.X - 1900.f, Max.X + 1900.f, N1, 1.f, GroundZ, R, 1000.f, 3200.f, 1100.f, D1, true, true, Alleys);
	BuildRow(false, Min.Y + 150.f, Max.Y + 100.f, W1, -1.f, GroundZ, R, 1000.f, 3000.f, 1100.f, D1, true, true, Alleys);
	BuildRow(false, Min.Y + 150.f, Max.Y + 100.f, E1, 1.f, GroundZ, R, 1000.f, 3000.f, 1100.f, D1, true, true, Alleys);

	// Rues derrière la 1re rangée (vues par les ruelles), puis 2e rangée plus haute.
	BuildStreet(true, Min.X - 6000.f, Max.X + 6000.f, N1 + D1 + 100.f, 1.f, GroundZ, R);
	BuildStreet(false, Min.Y - 600.f, Max.Y + D1 + 100.f, W1 - D1 - 100.f, -1.f, GroundZ, R);
	BuildStreet(false, Min.Y - 600.f, Max.Y + D1 + 100.f, E1 + D1 + 100.f, 1.f, GroundZ, R);
	const float N2 = N1 + D1 + 100.f + Street + 520.f, W2 = W1 - D1 - 100.f - Street - 520.f, E2 = E1 + D1 + 100.f + Street + 520.f;
	BuildRow(true, Min.X - 6500.f, Max.X + 6500.f, N2, 1.f, GroundZ, R, 2000.f, 5500.f, 1400.f, 2400.f, false, true, Unused);
	BuildRow(false, Min.Y - 700.f, N2, W2, -1.f, GroundZ, R, 2000.f, 5500.f, 1400.f, 2400.f, false, true, Unused);
	BuildRow(false, Min.Y - 700.f, N2, E2, 1.f, GroundZ, R, 2000.f, 5500.f, 1400.f, 2400.f, false, true, Unused);
	// Côté sud : visibles par-dessus le toit du couloir.
	BuildRow(true, Min.X - 4000.f, Max.X + 4000.f, -4700.f, -1.f, GroundZ, R, 2500.f, 6000.f, 1500.f, 2500.f, false, Density >= 2, Unused);
	BuildRow(false, -4700.f, Min.Y - 100.f, W1 - 300.f, -1.f, GroundZ, R, 1500.f, 4000.f, 1500.f, 2500.f, false, Density >= 2, Unused);
	BuildRow(false, -4700.f, Min.Y - 100.f, E1 + 300.f, 1.f, GroundZ, R, 1500.f, 4000.f, 1500.f, 2500.f, false, Density >= 2, Unused);

	// Ruelles de la 1re rangée : barricadées côté cour, encombrées, avec une lampe faible.
	UHierarchicalInstancedStaticMeshComponent* Blocks = GetHISM(CubeMesh, ConcreteMat, TEXT("AlleyBlock"));
	Blocks->SetCollisionProfileName(TEXT("BlockAll"));
	int32 AlleyLights = 0;
	for (const FVector4& A : Alleys)
	{
		const bool bAlongX = FMath::Abs((float)A.Z - N1) < 1.f;   // rangée nord : la ruelle part vers +Y
		const float A0 = (float)A.X, A1 = (float)A.Y, Face = (float)A.Z, Dir = (float)A.W;
		const float Mid = (A0 + A1) * 0.5f, Width = A1 - A0;
		auto At = [&](float Along, float Depth) { return bAlongX ? FVector(Along, Face + Dir * Depth, GroundZ) : FVector(Face + Dir * Depth, Along, GroundZ); };
		const float Yaw = bAlongX ? 0.f : 90.f;
		for (float T = A0 + 100.f; T < A1 - 50.f; T += 200.f)
		{
			for (int32 Lv = 0; Lv < 2; ++Lv)
			{
				Blocks->AddInstance(FTransform(FRotator(0.f, Yaw + R.FRandRange(-6.f, 6.f), 0.f), At(T, 60.f) + FVector(0, 0, 45.f + Lv * 90.f), FVector(1.9f, 0.5f, 0.9f)), true);
			}
		}
		const FVector Sheet = At(Mid, 120.f);
		Place(TEXT("Urb_Road_Sheet_Metal_Rusty_03"), TEXT("SM_Urb_Road_Sheet_Metal_Rusty_03"), Sheet.X, Sheet.Y, Yaw, 1.3f, false, 140.f, FRotator(0.f, 0.f, 80.f));
		for (int32 i = 0; i < 9; ++i)
		{
			const FVector P = At(A0 + R.FRandRange(60.f, Width - 60.f), R.FRandRange(300.f, D1));
			const int32 K = R.RandRange(0, 4);
			if (K == 0) Place(TEXT("Ind_Container_Metal_Set_01"), TEXT("SM_Ind_Container_Metal_Set_01_B"), P.X, P.Y, R.FRandRange(0.f, 360.f), 1.f, false);
			else if (K == 1) Place(TEXT("Ind_Rubble_Pile_Concrete_M_02"), TEXT("SM_Ind_Rubble_Pile_Concrete_M_02"), P.X, P.Y, R.FRandRange(0.f, 360.f), 0.6f, false);
			else if (K == 2) Place(TEXT("Ind_Ware_Storage_Pallet_Wood_Worn_01"), TEXT("SM_Ind_Ware_Storage_Pallet_Wood_Worn_01"), P.X, P.Y, R.FRandRange(0.f, 360.f), 1.f, false);
			else if (K == 3) Place(TEXT("Ind_Ware_Storage_Sheet_Cardboard_Flat_06"), TEXT("SM_Ind_Ware_Storage_Sheet_Cardboard_Flat_06_A"), P.X, P.Y, R.FRandRange(0.f, 360.f), 1.f, false);
			else Place(TEXT("Ind_Rubble_Debris_Concrete_Pack_01"), TEXT("SM_Ind_Rubble_Debris_Concrete_Pack_01_B"), P.X, P.Y, R.FRandRange(0.f, 360.f), 1.2f, false);
		}
		if (AlleyLights < 6)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetMobility(EComponentMobility::Movable);
			L->SetupAttachment(RootComponent);
			L->SetWorldLocation(At(Mid, R.FRandRange(700.f, 1500.f)) + FVector(0, 0, 320.f));
			L->SetIntensityUnits(ELightUnits::Candelas);
			L->SetIntensity(R.FRand() < 0.5f ? 350.f : 200.f);
			L->SetAttenuationRadius(1100.f);
			L->SetLightColor(R.FRand() < 0.5f ? FLinearColor(1.f, 0.55f, 0.25f) : FLinearColor(0.6f, 0.75f, 1.f));
			L->SetCastShadows(false);
			L->RegisterComponent();
			Spawned.Add(L);
			++AlleyLights;
		}
	}

	// Tours lointaines dans toutes les directions (avec quelques fenêtres allumées), qui se fondent dans le brouillard.
	int32 NFar = 0;
	const FVector Ctr((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, 0.f);
	const float Step = Density >= 3 ? 3000.f : 3600.f;
	for (float X = Ctr.X - 32000.f; X <= Ctr.X + 32000.f; X += Step)
	{
		for (float Y = Ctr.Y - 32000.f; Y <= Ctr.Y + 32000.f; Y += Step)
		{
			const FVector P(X + R.FRandRange(-700.f, 700.f), Y + R.FRandRange(-700.f, 700.f), 0.f);
			// Pas dans la ville détaillée.
			if (P.X > W2 - 4000.f && P.X < E2 + 4000.f && P.Y > -9000.f && P.Y < N2 + 4000.f) continue;
			const float Wd = R.FRandRange(1600.f, 2800.f), Dp = R.FRandRange(1600.f, 2800.f), Ht = R.FRandRange(3000.f, 9000.f);
			BuildBlock(P.X - Wd * 0.5f, P.X + Wd * 0.5f, P.Y - Dp * 0.5f, P.Y + Dp * 0.5f, Ht, GroundZ, R, false);
			++NFar;
		}
	}
	UE_LOG(LogBodycamExterior, Log, TEXT("Ville : %d immeuble(s), %d ruelle(s), %d tour(s) au loin, %d boite(s)"), NumTowers, Alleys.Num(), NFar, NumBoxes);
}

void ABodycamExterior::SetupAtmosphere()
{
	// Brouillard nocturne bleu-gris : les immeubles lointains disparaissent dedans, plus de "bord du monde" visible.
	float Density = 0.02f, Falloff = 0.03f, Start = 1200.f;
	FLinearColor Color(0.012f, 0.014f, 0.02f);
	if (GConfig)
	{
		GConfig->GetFloat(Sec, TEXT("FogDensity"), Density, GGameIni);
		GConfig->GetFloat(Sec, TEXT("FogFalloff"), Falloff, GGameIni);
		GConfig->GetFloat(Sec, TEXT("FogStart"), Start, GGameIni);
		FVector C;
		if (GConfig->GetVector(Sec, TEXT("FogColor"), C, GGameIni)) Color = FLinearColor(C.X, C.Y, C.Z);
	}
	UExponentialHeightFogComponent* Fog = nullptr;
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It) { Fog = It->GetComponent(); break; }
	if (!Fog)
	{
		if (AExponentialHeightFog* NewFog = GetWorld()->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FTransform(FVector(7900.f, 2000.f, 0.f))))
		{
			Fog = NewFog->GetComponent();
		}
	}
	if (Fog)
	{
		Fog->SetFogDensity(Density);
		Fog->SetFogHeightFalloff(Falloff);
		Fog->SetStartDistance(Start);
		Fog->SetFogInscatteringColor(Color);
		Fog->SetFogMaxOpacity(1.f);
	}

	// Pluie (se coupe toute seule sous un toit : rien dans le couloir).
	bool bRain = true;
	if (GConfig) GConfig->GetBool(Sec, TEXT("Rain"), bRain, GGameIni);
	bool bHasRain = false;
	for (TActorIterator<ABodycamRain> It(GetWorld()); It; ++It) { bHasRain = true; break; }
	if (bRain && !bHasRain) GetWorld()->SpawnActor<ABodycamRain>(ABodycamRain::StaticClass(), FTransform::Identity);
	UE_LOG(LogBodycamExterior, Log, TEXT("Brouillard %.3f, pluie %s"), Density, bRain ? TEXT("oui") : TEXT("non"));
}

void ABodycamExterior::BuildLights(float GroundZ)
{
	// Projecteurs de chantier orangés : X, Y, hauteur, intensité (cd).
	const FVector4 Lamps[] = {
		FVector4(6950.f, 1250.f, 520.f, 2600.f), FVector4(6400.f, -350.f, 420.f, 1200.f), FVector4(8700.f, 900.f, 600.f, 2000.f),
		FVector4(5500.f, 1900.f, 500.f, 1500.f), FVector4(10500.f, 700.f, 500.f, 1500.f), FVector4(5200.f, 3700.f, 520.f, 1300.f),
		FVector4(10700.f, 3600.f, 520.f, 1300.f), FVector4(8200.f, -300.f, 450.f, 1000.f),
	};
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInstanceDynamic* Dark = BaseMat ? UMaterialInstanceDynamic::Create(BaseMat, this) : nullptr;
	if (Dark) Dark->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.035f, 0.033f, 0.03f));
	for (const FVector4& L : Lamps)
	{
		const float H = L.Z, I = L.W;
		const FVector Base(L.X, L.Y, GroundZ);
		USpotLightComponent* SL = NewObject<USpotLightComponent>(this);
		SL->SetMobility(EComponentMobility::Movable);
		SL->SetupAttachment(RootComponent);
		SL->SetWorldLocationAndRotation(Base + FVector(0, 0, H), FRotator(-90.f, 0.f, 0.f));
		SL->SetIntensityUnits(ELightUnits::Candelas);
		SL->SetIntensity(I);
		SL->SetLightColor(FLinearColor(1.f, 0.62f, 0.32f));
		SL->SetAttenuationRadius(1600.f);
		SL->SetOuterConeAngle(55.f);
		SL->SetInnerConeAngle(25.f);
		SL->SetCastShadows(false);
		SL->RegisterComponent();
		Spawned.Add(SL);

		if (UStaticMeshComponent* Pole = AddMesh(Cyl, FTransform(FRotator::ZeroRotator, Base + FVector(0, 0, H * 0.5f), FVector(0.1f, 0.1f, H / 100.f)), true))
		{
			if (Dark) Pole->SetMaterial(0, Dark);
		}
		if (UStaticMeshComponent* Head = AddMesh(Cube, FTransform(FRotator::ZeroRotator, Base + FVector(0, 0, H + 6.f), FVector(0.45f, 0.3f, 0.12f)), false))
		{
			if (Dark) Head->SetMaterial(0, Dark);
		}
		UPointLightComponent* Glow = NewObject<UPointLightComponent>(this);
		Glow->SetMobility(EComponentMobility::Movable);
		Glow->SetupAttachment(RootComponent);
		Glow->SetWorldLocation(Base + FVector(0, 0, H - 12.f));
		Glow->SetIntensityUnits(ELightUnits::Candelas);
		Glow->SetIntensity(I * 0.03f);
		Glow->SetAttenuationRadius(120.f);
		Glow->SetLightColor(FLinearColor(1.f, 0.62f, 0.32f));
		Glow->SetCastShadows(false);
		Glow->RegisterComponent();
		Spawned.Add(Glow);
	}

	// Feux de fûts (braseros) : lumière orange qui vacille.
	const FVector2D FirePos[] = { FVector2D(5650.f, 1450.f), FVector2D(8200.f, 650.f), FVector2D(10600.f, 2700.f), FVector2D(6200.f, 3300.f) };
	for (const FVector2D& F : FirePos)
	{
		Place(TEXT("Ind_Container_Metal_Set_01"), TEXT("SM_Ind_Container_Metal_Set_01_A"), F.X, F.Y, 0.f, 1.f, true);
		UPointLightComponent* Fire = NewObject<UPointLightComponent>(this);
		Fire->SetMobility(EComponentMobility::Movable);
		Fire->SetupAttachment(RootComponent);
		Fire->SetWorldLocation(FVector(F.X, F.Y, GroundZ + 130.f));
		Fire->SetIntensityUnits(ELightUnits::Candelas);
		Fire->SetIntensity(500.f);
		Fire->SetAttenuationRadius(900.f);
		Fire->SetLightColor(FLinearColor(1.f, 0.45f, 0.15f));
		Fire->SetCastShadows(false);
		Fire->RegisterComponent();
		Spawned.Add(Fire);
		Fires.Add(Fire);
	}
	UE_LOG(LogBodycamExterior, Log, TEXT("%d projecteur(s), %d feu(x)"), (int32)UE_ARRAY_COUNT(Lamps), Fires.Num());

	float Moon = 0.08f;
	if (GConfig) GConfig->GetFloat(Sec, TEXT("MoonLight"), Moon, GGameIni);
	if (Moon > 0.f)
	{
		UPointLightComponent* M = NewObject<UPointLightComponent>(this);
		M->SetMobility(EComponentMobility::Movable);
		M->SetupAttachment(RootComponent);
		M->SetWorldLocation(FVector(7900.f, 2000.f, GroundZ + 3000.f));
		M->SetIntensityUnits(ELightUnits::Candelas);
		M->SetIntensity(Moon * 100000.f);
		M->SetAttenuationRadius(9000.f);
		M->SetLightColor(FLinearColor(0.55f, 0.65f, 1.f));
		M->SetCastShadows(false);
		M->RegisterComponent();
		Spawned.Add(M);
	}
}

void ABodycamExterior::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FireTime += DeltaSeconds;
	for (int32 i = 0; i < Fires.Num(); ++i)
	{
		if (!Fires[i]) continue;
		const float T = FireTime * 9.f + i * 3.7f;
		const float Flicker = 0.75f + 0.15f * FMath::Sin(T) + 0.1f * FMath::Sin(T * 2.3f + 1.f) + FMath::FRandRange(-0.05f, 0.05f);
		Fires[i]->SetIntensity(500.f * Flicker);
	}
}

void ABodycamExterior::BuildBounds(const FVector& Min, const FVector& Max, float Z)
{
	// Murs invisibles autour de la zone (et un plafond) : le drone ne part pas dans le vide.
	float Ceiling = 2500.f;
	if (GConfig) GConfig->GetFloat(Sec, TEXT("Ceiling"), Ceiling, GGameIni);
	const FVector C((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, Z + Ceiling * 0.5f);
	const FVector Half((Max.X - Min.X) * 0.5f, (Max.Y - Min.Y) * 0.5f, Ceiling * 0.5f);
	auto Wall = [&](const FVector& Loc, const FVector& Ext)
	{
		UBoxComponent* B = NewObject<UBoxComponent>(this);
		B->SetMobility(EComponentMobility::Movable);
		B->SetupAttachment(RootComponent);
		B->SetBoxExtent(Ext);
		B->SetWorldLocation(Loc);
		B->SetCollisionProfileName(TEXT("BlockAll"));
		B->SetHiddenInGame(true);
		B->RegisterComponent();
		Spawned.Add(B);
	};
	Wall(FVector(Min.X - 50.f, C.Y, C.Z), FVector(50.f, Half.Y, Half.Z));
	Wall(FVector(Max.X + 50.f, C.Y, C.Z), FVector(50.f, Half.Y, Half.Z));
	Wall(FVector(C.X, Max.Y + 50.f, C.Z), FVector(Half.X, 50.f, Half.Z));
	Wall(FVector(C.X, C.Y, Z + Ceiling + 50.f), FVector(Half.X, Half.Y, 50.f));
}
