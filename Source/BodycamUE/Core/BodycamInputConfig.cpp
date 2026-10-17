#include "Core/BodycamInputConfig.h"

#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

bool UBodycamInputConfig::DetectAzerty()
{
#if PLATFORM_WINDOWS
	const HKL Layout = ::GetKeyboardLayout(0);
	const WORD LangId = LOWORD(reinterpret_cast<UINT_PTR>(Layout));
	const WORD Sub = SUBLANGID(LangId);
	// Le français canadien est en QWERTY, le suisse en QWERTZ (ZQSD inutile dans ces deux cas).
	return PRIMARYLANGID(LangId) == LANG_FRENCH && Sub != SUBLANG_FRENCH_CANADIAN && Sub != SUBLANG_FRENCH_SWISS;
#else
	return false;
#endif
}

UInputAction* UBodycamInputConfig::MakeAction(const TCHAR* Name, EInputActionValueType Type)
{
	UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
	Action->ValueType = Type;
	return Action;
}

void UBodycamInputConfig::MapKeys(UInputAction* Action, const TArray<FKey>& Keys)
{
	for (const FKey& Key : Keys)
	{
		Context->MapKey(Action, Key);
	}
}

void UBodycamInputConfig::MapMoveKey(const FKey& Key, bool bYAxis, bool bNegate)
{
	// Une touche vaut (1,0) : on la tourne vers l'axe Y (avant/arrière) et/ou on l'inverse.
	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Move, Key);
	if (bYAxis)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}
	if (bNegate)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}
}

void UBodycamInputConfig::Build(EBodycamKeyboardLayout Layout)
{
	bAzerty = Layout == EBodycamKeyboardLayout::Azerty || (Layout == EBodycamKeyboardLayout::Auto && DetectAzerty());

	if (!Context)
	{
		Context = NewObject<UInputMappingContext>(this, TEXT("IMC_Bodycam"));

		Move        = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
		Look        = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
		LookGamepad = MakeAction(TEXT("IA_LookGamepad"), EInputActionValueType::Axis2D);
		Fire        = MakeAction(TEXT("IA_Fire"), EInputActionValueType::Boolean);
		Aim         = MakeAction(TEXT("IA_Aim"), EInputActionValueType::Boolean);
		Reload      = MakeAction(TEXT("IA_Reload"), EInputActionValueType::Boolean);
		Sprint      = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
		Crouch      = MakeAction(TEXT("IA_Crouch"), EInputActionValueType::Boolean);
		LeanLeft    = MakeAction(TEXT("IA_LeanLeft"), EInputActionValueType::Boolean);
		LeanRight   = MakeAction(TEXT("IA_LeanRight"), EInputActionValueType::Boolean);
		Jump        = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
		Interact    = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
		FireMode    = MakeAction(TEXT("IA_FireMode"), EInputActionValueType::Boolean);
		Weapon1     = MakeAction(TEXT("IA_Weapon1"), EInputActionValueType::Boolean);
		Weapon2     = MakeAction(TEXT("IA_Weapon2"), EInputActionValueType::Boolean);
		Heal        = MakeAction(TEXT("IA_Heal"), EInputActionValueType::Boolean);
		Objectives  = MakeAction(TEXT("IA_Objectives"), EInputActionValueType::Boolean);
		Pause       = MakeAction(TEXT("IA_Pause"), EInputActionValueType::Boolean);
		NightVision = MakeAction(TEXT("IA_NightVision"), EInputActionValueType::Boolean);
		Phone       = MakeAction(TEXT("IA_Phone"), EInputActionValueType::Boolean);
		Flashlight  = MakeAction(TEXT("IA_Flashlight"), EInputActionValueType::Boolean);
		Melee       = MakeAction(TEXT("IA_Melee"), EInputActionValueType::Boolean);
		Confirm     = MakeAction(TEXT("IA_Confirm"), EInputActionValueType::Boolean);
		// Pause / quitter doivent marcher même jeu en pause.
		Pause->bTriggerWhenPaused = true;
		Confirm->bTriggerWhenPaused = true;
	}
	else
	{
		Context->UnmapAll();
	}

	// AZERTY : ZQSD + A/E pour pencher. QWERTY : WASD + Q/E.
	const FKey Forward  = bAzerty ? EKeys::Z : EKeys::W;
	const FKey Left     = bAzerty ? EKeys::Q : EKeys::A;
	const FKey LeanLKey = bAzerty ? EKeys::A : EKeys::Q;

	MapMoveKey(Forward, true, false);
	MapMoveKey(EKeys::S, true, true);
	MapMoveKey(Left, false, true);
	MapMoveKey(EKeys::D, false, false);
	Context->MapKey(Move, EKeys::Gamepad_Left2D);

	Context->MapKey(Look, EKeys::Mouse2D);
	Context->MapKey(LookGamepad, EKeys::Gamepad_Right2D);

	MapKeys(Fire,       { EKeys::LeftMouseButton, EKeys::Gamepad_RightTrigger });
	MapKeys(Aim,        { EKeys::RightMouseButton, EKeys::Gamepad_LeftTrigger });
	MapKeys(Reload,     { EKeys::R, EKeys::Gamepad_FaceButton_Left });
	MapKeys(Sprint,     { EKeys::LeftShift, EKeys::Gamepad_LeftThumbstick });
	MapKeys(Crouch,     { EKeys::LeftControl, EKeys::C, EKeys::Gamepad_FaceButton_Right });
	MapKeys(LeanLeft,   { LeanLKey, EKeys::Gamepad_LeftShoulder });
	MapKeys(LeanRight,  { EKeys::E, EKeys::Gamepad_RightShoulder });
	MapKeys(Jump,       { EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom });
	MapKeys(Interact,   { EKeys::F, EKeys::Gamepad_FaceButton_Top });
	MapKeys(FireMode,   { EKeys::V, EKeys::Gamepad_DPad_Down });
	// Sur AZERTY, la touche "1" sans Maj est lue comme "&" et "2" comme "é" : on mappe les deux.
	MapKeys(Weapon1,    { EKeys::One, EKeys::Ampersand, EKeys::Gamepad_DPad_Left });
	MapKeys(Weapon2,    { EKeys::Two, EKeys::E_AccentAigu, EKeys::Gamepad_DPad_Right });
	MapKeys(Heal,       { EKeys::H, EKeys::Gamepad_DPad_Up });
	MapKeys(Objectives, { EKeys::Tab, EKeys::Gamepad_Special_Left });
	MapKeys(Pause,      { EKeys::Escape, EKeys::P, EKeys::Gamepad_Special_Right });
	MapKeys(NightVision, { EKeys::N, EKeys::Gamepad_RightThumbstick });
	MapKeys(Phone,      { EKeys::T });
	MapKeys(Flashlight, { EKeys::L });
	MapKeys(Melee,      { EKeys::MiddleMouseButton });
	MapKeys(Confirm,    { EKeys::Enter });
}
