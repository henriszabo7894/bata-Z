#pragma once

#include "CoreMinimal.h"
#include "Core/BodycamInputConfig.h"
#include "GameFramework/PlayerController.h"
#include "BodycamPlayerController.generated.h"

/**
 * Contrôleur du joueur : possède la config d'entrées, les réglages joueur
 * et les commandes console de test (taper dans la console : BodycamDebug, BodycamFX...).
 */
UCLASS(config=Game)
class BODYCAMUE_API ABodycamPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Créée à la première demande, puis réutilisée. */
	UBodycamInputConfig* GetInputConfig();

	UPROPERTY(EditAnywhere, Config, Category = "Bodycam|Input")
	EBodycamKeyboardLayout KeyboardLayout = EBodycamKeyboardLayout::Auto;

	UPROPERTY(EditAnywhere, Config, Category = "Bodycam|Input", meta = (ClampMin = "0.05", ClampMax = "5"))
	float MouseSensitivity = 1.f;

	/** Vitesse de rotation au stick (multipliée par l'échelle legacy 2.5 d'Unreal => ~150°/s). */
	UPROPERTY(EditAnywhere, Config, Category = "Bodycam|Input")
	float GamepadLookRate = 60.f;

	/** true = Ctrl/C bascule accroupi, false = maintenir. */
	UPROPERTY(EditAnywhere, Config, Category = "Bodycam|Input")
	bool bCrouchToggle = true;

	// --- Commandes console (touche ² ou champ "Cmd" en bas de l'éditeur) ---
	UFUNCTION(Exec) void BodycamDebug();
	UFUNCTION(Exec) void BodycamOverlay();
	UFUNCTION(Exec) void BodycamFX();
	UFUNCTION(Exec) void BodycamHeadBob(float Scale);
	UFUNCTION(Exec) void BodycamSensitivity(float Value);
	UFUNCTION(Exec) void BodycamLayout(const FString& Layout);
	UFUNCTION(Exec) void BodycamLevel(int32 Index);
	UFUNCTION(Exec) void BodycamMoney(int32 Amount);
	UFUNCTION(Exec) void BodycamKills(int32 Amount);
	UFUNCTION(Exec) void BodycamBattery(float Percent);

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

private:
	UPROPERTY()
	TObjectPtr<UBodycamInputConfig> InputConfig;

	void ApplyMappingContext();
	void OnPausePressed();
	void ApplyGameInputMode();
	void SetMouseLocked(bool bLocked);
	float MouseLockTimer = 0.f;
	void OnConfirmPressed();
};
