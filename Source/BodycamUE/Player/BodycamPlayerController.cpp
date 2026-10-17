#include "Player/BodycamPlayerController.h"

#include "Camera/BodycamCameraComponent.h"
#include "Core/BodycamGameMode.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "UnrealClient.h"
#include "Framework/Application/SlateApplication.h"
#include "InputMappingContext.h"
#include "Player/BodycamCharacter.h"
#include "UI/BodycamHUD.h"

UBodycamInputConfig* ABodycamPlayerController::GetInputConfig()
{
	if (!InputConfig)
	{
		InputConfig = NewObject<UBodycamInputConfig>(this);
		InputConfig->Build(KeyboardLayout);
	}
	return InputConfig;
}

void ABodycamPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		ApplyMappingContext();
		ApplyGameInputMode();
	}
}

void ABodycamPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		const UBodycamInputConfig* Cfg = GetInputConfig();
		Input->BindAction(Cfg->Pause, ETriggerEvent::Started, this, &ABodycamPlayerController::OnPausePressed);
		Input->BindAction(Cfg->Confirm, ETriggerEvent::Started, this, &ABodycamPlayerController::OnConfirmPressed);
	}
}

void ABodycamPlayerController::ApplyGameInputMode()
{
	// Jeu : souris cachée, capturée et bloquée dans la fenêtre (la vue ne part plus n'importe où).
	FInputModeGameOnly Mode;
	Mode.SetConsumeCaptureMouseDown(true);
	SetInputMode(Mode);
	bShowMouseCursor = false;
	SetMouseLocked(true);
}

void ABodycamPlayerController::SetMouseLocked(bool bLocked)
{
	UGameViewportClient* VC = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!VC) return;
	VC->SetMouseLockMode(bLocked ? EMouseLockMode::LockAlways : EMouseLockMode::DoNotLock);
	VC->SetMouseCaptureMode(bLocked ? EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown : EMouseCaptureMode::NoCapture);
	VC->SetHideCursorDuringCapture(bLocked);
	if (FViewport* V = VC->Viewport)
	{
		if (!bLocked || V->HasFocus())
		{
			V->CaptureMouse(bLocked);
			V->LockMouseToViewport(bLocked);
			V->ShowCursor(!bLocked);
		}
	}
}

void ABodycamPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	// La souris peut se libérer (clic hors fenêtre, Alt+Tab, changement de niveau) : on la rebloque en continu.
	if (!IsLocalController() || IsPaused()) return;
	MouseLockTimer -= DeltaTime;
	if (MouseLockTimer > 0.f) return;
	MouseLockTimer = 0.25f;
	bShowMouseCursor = false;
	SetMouseLocked(true);
	// Curseur Windows caché de force (il restait visible par-dessus le jeu).
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
		if (TSharedPtr<ICursor> Cursor = FSlateApplication::Get().GetPlatformCursor())
		{
			Cursor->Show(false);
		}
	}
}

void ABodycamPlayerController::OnPausePressed()
{
	const bool bPause = !IsPaused();
	SetPause(bPause);
	if (bPause)
	{
		// Pause : on rend la souris (curseur visible, libre de sortir de la fenêtre).
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		bShowMouseCursor = true;
		SetMouseLocked(false);
	}
	else
	{
		ApplyGameInputMode();
	}
}

void ABodycamPlayerController::OnConfirmPressed()
{
	if (IsPaused())
	{
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
		return;
	}
	// Écran de mort : ENTRÉE = recommencer tout de suite.
	if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>())
	{
		GM->RetryNow();
	}
}

void ABodycamPlayerController::BodycamLevel(int32 Index)
{
	if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>()) GM->DebugLevel(Index);
}

void ABodycamPlayerController::BodycamMoney(int32 Amount)
{
	if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>()) GM->DebugMoney(Amount);
}

void ABodycamPlayerController::BodycamKills(int32 Amount)
{
	if (ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>()) GM->DebugKills(Amount);
}

void ABodycamPlayerController::BodycamBattery(float Percent)
{
	if (ABodycamCharacter* C = Cast<ABodycamCharacter>(GetPawn())) C->DebugSetBattery(Percent);
}

void ABodycamPlayerController::ApplyMappingContext()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer) return;

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		UInputMappingContext* Context = GetInputConfig()->Context;
		Subsystem->RemoveMappingContext(Context);
		Subsystem->AddMappingContext(Context, 0);
	}
}

void ABodycamPlayerController::BodycamDebug()
{
	if (ABodycamHUD* BodycamHUD = Cast<ABodycamHUD>(GetHUD()))
	{
		BodycamHUD->bShowDebug = !BodycamHUD->bShowDebug;
	}
}

void ABodycamPlayerController::BodycamOverlay()
{
	if (ABodycamHUD* BodycamHUD = Cast<ABodycamHUD>(GetHUD()))
	{
		BodycamHUD->bShowOverlay = !BodycamHUD->bShowOverlay;
	}
}

void ABodycamPlayerController::BodycamFX()
{
	if (const ABodycamCharacter* BodycamCharacter = Cast<ABodycamCharacter>(GetPawn()))
	{
		UBodycamCameraComponent* Camera = BodycamCharacter->GetBodycamCamera();
		Camera->SetEffectsEnabled(!Camera->AreEffectsEnabled());
	}
}

void ABodycamPlayerController::BodycamHeadBob(float Scale)
{
	if (const ABodycamCharacter* BodycamCharacter = Cast<ABodycamCharacter>(GetPawn()))
	{
		BodycamCharacter->GetBodycamCamera()->HeadBobScale = FMath::Clamp(Scale, 0.f, 2.f);
	}
}

void ABodycamPlayerController::BodycamSensitivity(float Value)
{
	MouseSensitivity = FMath::Clamp(Value, 0.05f, 5.f);
}

void ABodycamPlayerController::BodycamLayout(const FString& Layout)
{
	if (Layout.Equals(TEXT("azerty"), ESearchCase::IgnoreCase))      KeyboardLayout = EBodycamKeyboardLayout::Azerty;
	else if (Layout.Equals(TEXT("qwerty"), ESearchCase::IgnoreCase)) KeyboardLayout = EBodycamKeyboardLayout::Qwerty;
	else                                                              KeyboardLayout = EBodycamKeyboardLayout::Auto;

	GetInputConfig()->Build(KeyboardLayout);
	ApplyMappingContext();
}
