#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "UObject/Object.h"
#include "BodycamInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

UENUM()
enum class EBodycamKeyboardLayout : uint8
{
	Auto,
	Azerty,
	Qwerty
};

/**
 * Crée à l'exécution toutes les Input Actions et le Mapping Context du jeu.
 * Aucun asset à créer dans l'éditeur : tout le schéma de touches est défini ici.
 * Unreal lit les lettres selon la disposition du clavier (Z sur AZERTY = Z),
 * d'où la détection AZERTY/QWERTY.
 */
UCLASS()
class BODYCAMUE_API UBodycamInputConfig : public UObject
{
	GENERATED_BODY()

public:
	/** Crée les actions (une seule fois) puis (re)mappe toutes les touches. */
	void Build(EBodycamKeyboardLayout Layout);

	bool IsAzerty() const { return bAzerty; }

	/** true si la disposition clavier Windows active est AZERTY (français France/Belgique). */
	static bool DetectAzerty();

	UPROPERTY() TObjectPtr<UInputMappingContext> Context;

	UPROPERTY() TObjectPtr<UInputAction> Move;
	UPROPERTY() TObjectPtr<UInputAction> Look;
	UPROPERTY() TObjectPtr<UInputAction> LookGamepad;
	UPROPERTY() TObjectPtr<UInputAction> Fire;
	UPROPERTY() TObjectPtr<UInputAction> Aim;
	UPROPERTY() TObjectPtr<UInputAction> Reload;
	UPROPERTY() TObjectPtr<UInputAction> Sprint;
	UPROPERTY() TObjectPtr<UInputAction> Crouch;
	UPROPERTY() TObjectPtr<UInputAction> LeanLeft;
	UPROPERTY() TObjectPtr<UInputAction> LeanRight;
	UPROPERTY() TObjectPtr<UInputAction> Jump;
	UPROPERTY() TObjectPtr<UInputAction> Interact;
	UPROPERTY() TObjectPtr<UInputAction> FireMode;
	UPROPERTY() TObjectPtr<UInputAction> Weapon1;
	UPROPERTY() TObjectPtr<UInputAction> Weapon2;
	UPROPERTY() TObjectPtr<UInputAction> Heal;
	UPROPERTY() TObjectPtr<UInputAction> Objectives;
	UPROPERTY() TObjectPtr<UInputAction> Pause;
	UPROPERTY() TObjectPtr<UInputAction> NightVision;
	UPROPERTY() TObjectPtr<UInputAction> Phone;
	UPROPERTY() TObjectPtr<UInputAction> Flashlight;
	UPROPERTY() TObjectPtr<UInputAction> Melee;
	UPROPERTY() TObjectPtr<UInputAction> Confirm;

private:
	bool bAzerty = false;

	UInputAction* MakeAction(const TCHAR* Name, EInputActionValueType Type);
	void MapKeys(UInputAction* Action, const TArray<FKey>& Keys);
	void MapMoveKey(const FKey& Key, bool bYAxis, bool bNegate);
};
