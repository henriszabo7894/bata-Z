#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/HUD.h"
#include "BodycamHUD.generated.h"

class ABodycamCharacter;
class ABodycamGameMode;

UENUM()
enum class EHudFont : uint8
{
	Display, // Barlow Condensed SemiBold : titres, chrono, munitions
	Label,   // Barlow Condensed Medium
	Mono     // Share Tech Mono : horodatage, valeurs, libellés
};

/**
 * Interface minimale "caméra corporelle" dessinée au Canvas avec de vraies polices (TTF dans
 * Content/Bodycam/Fonts) : horodatage, chrono, quota, état, JVN, munitions, argent, alertes,
 * marqueurs, téléphone, retour drone, écrans d'intro / mort / victoire, pause.
 */
UCLASS()
class BODYCAMUE_API ABodycamHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UPROPERTY(EditAnywhere, Category = "Bodycam") bool bShowOverlay = true;
	/** Infos de test (vitesse, zombies, position, FPS). Commande console : BodycamDebug. */
	UPROPERTY(EditAnywhere, Category = "Bodycam") bool bShowDebug = false;
	UPROPERTY(EditAnywhere, Category = "Bodycam") FString CameraLabel = TEXT("BODYCAM 3 · X6039F2A");

private:
	// Primitives
	FSlateFontInfo Font(EHudFont Kind, float Px, int32 Spacing = 0) const;
	FVector2D Measure(const FString& Str, const FSlateFontInfo& F) const;
	void Text(const FString& Str, float X, float Y, const FLinearColor& C, EHudFont Kind, float Px, int32 Spacing = 0, int32 Align = 0);
	void Rect(float X, float Y, float W, float H, const FLinearColor& C);
	void Frame(float X, float Y, float W, float H, const FLinearColor& C, float T = 1.f);
	void Segments(float X, float Y, float W, float H, int32 Count, float Fill, const FLinearColor& On, const FLinearColor& Off);

	// Blocs
	void DrawStamp(const ABodycamCharacter* P);
	void DrawMission(const ABodycamGameMode* GM);
	void DrawVitals(const ABodycamCharacter* P);
	void DrawAmmo(const ABodycamCharacter* P, const ABodycamGameMode* GM);
	void DrawMarkers(const ABodycamCharacter* P, const ABodycamGameMode* GM);
	void DrawMarker(const FVector& World, const FString& Label, const FLinearColor& C, const FVector& From);
	void DrawAlert(const ABodycamGameMode* GM);
	void DrawToast(const ABodycamGameMode* GM);
	void DrawGrab(const ABodycamCharacter* P);
	void DrawNVGTransition(const ABodycamCharacter* P);
	void DrawPhone(const ABodycamCharacter* P, const ABodycamGameMode* GM);
	void DrawDrone(const ABodycamCharacter* P, const ABodycamGameMode* GM);
	void DrawHeadset(const ABodycamCharacter* P);
	void DrawIntro(const ABodycamGameMode* GM);
	void DrawControls();
	bool bShowControls = true;
	void DrawDeath(const ABodycamCharacter* P, const ABodycamGameMode* GM);
	void DrawEnd(const ABodycamCharacter* P, const ABodycamGameMode* GM);
	void DrawPause();
	void DrawDebug(const ABodycamCharacter* P);

	float S = 1.f;   // échelle (1080p = 1)
	float Now = 0.f; // temps réel
};
