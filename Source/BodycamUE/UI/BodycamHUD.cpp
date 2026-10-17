#include "UI/BodycamHUD.h"

#include "CanvasItem.h"
#include "Core/BodycamGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Gameplay/BodycamDrone.h"
#include "Gameplay/BodycamPickup.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Player/BodycamCharacter.h"
#include "Player/BodycamPlayerController.h"
#include "Rendering/SlateRenderer.h"
#include "Weapons/BodycamWeapon.h"

namespace
{
	// Palette : papier vieilli, ambre, rouge sang, vert toxique.
	const FLinearColor Paper(0.91f, 0.886f, 0.83f, 1.f);
	const FLinearColor Dim(0.91f, 0.886f, 0.83f, 0.55f);
	const FLinearColor Faint(0.91f, 0.886f, 0.83f, 0.18f);
	const FLinearColor Amber(0.88f, 0.66f, 0.31f, 1.f);
	const FLinearColor Blood(0.78f, 0.2f, 0.16f, 1.f);
	const FLinearColor Toxic(0.72f, 0.79f, 0.35f, 1.f);

	FLinearColor A(FLinearColor C, float Alpha) { C.A *= Alpha; return C; }

	FString Clock(float Seconds)
	{
		const int32 T = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), T / 60, T % 60);
	}

	FString Stamp(const FDateTime& T)
	{
		return FString::Printf(TEXT("%02d:%02d:%02d"), T.GetHour(), T.GetMinute(), T.GetSecond());
	}
}

// ------------------------------------------------------------------ Primitives

FSlateFontInfo ABodycamHUD::Font(EHudFont Kind, float Px, int32 Spacing) const
{
	static FString Files[3];
	if (Files[0].IsEmpty())
	{
		const FString Dir = FPaths::ProjectContentDir() / TEXT("Bodycam/Fonts/");
		const FString Fallback = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf");
		const TCHAR* Names[3] = { TEXT("BarlowCondensed-SemiBold.ttf"), TEXT("BarlowCondensed-Medium.ttf"), TEXT("ShareTechMono-Regular.ttf") };
		for (int32 i = 0; i < 3; ++i)
		{
			const FString Path = Dir + Names[i];
			Files[i] = IFileManager::Get().FileExists(*Path) ? Path : Fallback;
		}
	}
	// Taille Slate en points (96 dpi) : 1 pt = 1,333 px.
	const float Size = FMath::Max(1.f, Px * S * 0.75f);
	FSlateFontInfo F(Files[static_cast<int32>(Kind)], Size);

	// Si la police .ttf ne se charge pas (texte vide à l'écran), on passe aux polices du moteur.
	// Les .ttf chargées depuis un fichier se mesurent mais ne s'affichent pas dans le Canvas : polices du moteur
	// par défaut. DefaultGame.ini [Bodycam.HUD] CustomFonts=True pour réessayer les .ttf.
	static int32 bTtfOk = -1;
	if (bTtfOk < 0)
	{
		bool bCustom = false;
		if (GConfig) GConfig->GetBool(TEXT("Bodycam.HUD"), TEXT("CustomFonts"), bCustom, GGameIni);
		bTtfOk = bCustom ? 1 : 0;
	}
	if (bTtfOk == 0)
	{
		static UObject* EngineFont = LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		static UObject* EngineMono = LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono"));
		UObject* Obj = (Kind == EHudFont::Mono && EngineMono) ? EngineMono : EngineFont;
		if (Obj)
		{
			F = FSlateFontInfo(Obj, FMath::RoundToInt(Size), Kind == EHudFont::Mono ? FName(NAME_None) : (Kind == EHudFont::Display ? FName(TEXT("Bold")) : FName(TEXT("Regular"))));
		}
	}
	F.LetterSpacing = Spacing;
	return F;
}

FVector2D ABodycamHUD::Measure(const FString& Str, const FSlateFontInfo& F) const
{
	if (!FSlateApplication::IsInitialized()) return FVector2D(Str.Len() * F.Size, F.Size);
	return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Str, F);
}

void ABodycamHUD::Text(const FString& Str, float X, float Y, const FLinearColor& C, EHudFont Kind, float Px, int32 Spacing, int32 Align)
{
	if (Str.IsEmpty() || C.A <= 0.01f) return;
	const FSlateFontInfo F = Font(Kind, Px, Spacing);
	if (Align != 0)
	{
		const FVector2D M = Measure(Str, F);
		X -= Align == 1 ? M.X : M.X * 0.5f;
	}
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Str), F, C);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f * C.A), FVector2D(1.f, 1.5f) * FMath::Max(S, 0.7f));
	Canvas->DrawItem(Item);
}

void ABodycamHUD::Rect(float X, float Y, float W, float H, const FLinearColor& C)
{
	FCanvasTileItem Tile(FVector2D(X, Y), FVector2D(W, H), C);
	Tile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Tile);
}

void ABodycamHUD::Frame(float X, float Y, float W, float H, const FLinearColor& C, float T)
{
	T *= FMath::Max(S, 0.7f);
	Rect(X, Y, W, T, C);
	Rect(X, Y + H - T, W, T, C);
	Rect(X, Y, T, H, C);
	Rect(X + W - T, Y, T, H, C);
}

void ABodycamHUD::Segments(float X, float Y, float W, float H, int32 Count, float Fill, const FLinearColor& On, const FLinearColor& Off)
{
	const float Gap = 4.f * S;
	const float SegW = (W - Gap * (Count - 1)) / Count;
	const int32 Lit = FMath::CeilToInt(FMath::Clamp(Fill, 0.f, 1.f) * Count - 0.01f);
	for (int32 i = 0; i < Count; ++i)
	{
		Rect(X + i * (SegW + Gap), Y, SegW, H, i < Lit ? On : Off);
	}
}

// ------------------------------------------------------------------ Dessin

void ABodycamHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	S = Canvas->ClipY / 1080.f;
	Now = GetWorld()->GetRealTimeSeconds();

	const ABodycamCharacter* P = Cast<ABodycamCharacter>(GetOwningPawn());
	const ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>();
	APlayerController* PC = GetOwningPlayerController();

	const EBodycamMatchState State = GM ? GM->GetMatchState() : EBodycamMatchState::Playing;
	const bool bEnd = State == EBodycamMatchState::Won || State == EBodycamMatchState::Lost;

	if (P && P->IsPilotingDrone())
	{
		DrawDrone(P, GM);
	}
	else if (P && !bEnd)
	{
		DrawNVGTransition(P);
		if (bShowOverlay) DrawStamp(P);
		if (GM && State == EBodycamMatchState::Playing)
		{
			DrawMarkers(P, GM);
			DrawMission(GM);
			DrawVitals(P);
			DrawAmmo(P, GM);
			DrawAlert(GM);
			DrawToast(GM);
			DrawControls();
		}
		DrawGrab(P);
		if (P->IsPhoneOpen()) DrawPhone(P, GM);
	}

	if (P && !bEnd) DrawHeadset(P);
	if (GM && State == EBodycamMatchState::Intro) DrawIntro(GM);
	if (GM && bEnd) DrawEnd(P, GM);
	if (bShowDebug && P) DrawDebug(P);
	if (PC && PC->IsPaused()) DrawPause();
}

void ABodycamHUD::DrawStamp(const ABodycamCharacter* P)
{
	const float X = 48.f * S, Y = 40.f * S, L = 29.f * S;
	const FDateTime T = FDateTime::Now();

	if (P->IsNightVisionOn())
	{
		Text(FString::Printf(TEXT("JVN  ·  TUBE %d %%"), FMath::RoundToInt(P->GetNVGBattery())), X, Y, P->GetNVGBattery() < 20.f ? Blood : A(Paper, 0.8f), EHudFont::Mono, 20.f, 60);
		return;
	}

	// REC qui clignote, comme sur une vraie caméra corporelle.
	if (FMath::Fmod(Now, 1.f) < 0.55f)
	{
		FCanvasNGonItem Dot(FVector2D(X + 6.f * S, Y + 13.f * S), FVector2D(6.f * S, 6.f * S), 16, Blood);
		Canvas->DrawItem(Dot);
	}
	Text(FString::Printf(TEXT("REC   %04d-%02d-%02d   %s"), T.GetYear(), T.GetMonth(), T.GetDay(), *Stamp(T)), X + 22.f * S, Y, Paper, EHudFont::Mono, 20.f, 60);
	Text(CameraLabel, X, Y + L, Paper, EHudFont::Mono, 20.f, 60);
}

void ABodycamHUD::DrawMission(const ABodycamGameMode* GM)
{
	const FBodycamLevelConfig& Lv = GM->GetLevel();
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = 28.f * S;

	const float TL = GM->GetTimeLeft();
	const bool bUrgent = TL < 60.f;
	const FLinearColor TimeColor = bUrgent && FMath::Fmod(Now, 1.f) < 0.5f ? Blood : Paper;
	Text(Clock(TL), CX, Y, TimeColor, EHudFont::Display, 54.f, 80, 2);
	Text(FString::Printf(TEXT("INFECTÉS  %d / %d"), FMath::Min(GM->GetKills(), Lv.KillQuota), Lv.KillQuota), CX, Y + 64.f * S, Dim, EHudFont::Mono, 17.f, 200, 2);

	const float W = 190.f * S;
	Rect(CX - W * 0.5f, Y + 94.f * S, W, 2.f * S, Faint);
	Rect(CX - W * 0.5f, Y + 94.f * S, W * FMath::Clamp(float(GM->GetKills()) / FMath::Max(1, Lv.KillQuota), 0.f, 1.f), 2.f * S, Amber);
}

void ABodycamHUD::DrawVitals(const ABodycamCharacter* P)
{
	const float X = 48.f * S;
	float Y = Canvas->ClipY - 170.f * S;

	// État : 3 blocs = 3 coups avant la mort.
	Text(TEXT("ÉTAT"), X, Y, Dim, EHudFont::Mono, 15.f, 250);
	Y += 24.f * S;
	const int32 Left = P->GetMaxHits() - P->GetHits();
	for (int32 i = 0; i < P->GetMaxHits(); ++i)
	{
		const float BX = X + i * 44.f * S;
		if (i < Left) Rect(BX, Y, 36.f * S, 8.f * S, Left == 1 ? Blood : Paper);
		else Frame(BX, Y, 36.f * S, 8.f * S, A(Blood, 0.8f));
	}
	Y += 26.f * S;

	if (P->IsInfected())
	{
		const float Pulse = 0.65f + 0.35f * FMath::Sin(Now * 3.f);
		Text(FString::Printf(TEXT("CONTAMINÉ  ·  %d %%"), FMath::RoundToInt(P->GetInfection() * 100.f)), X, Y, A(Toxic, Pulse), EHudFont::Mono, 15.f, 250);
		Y += 30.f * S;
	}

	const float Bat = P->GetNVGBattery();
	const bool bLow = Bat < 25.f;
	Text(FString::Printf(TEXT("JVN  ·  %d %%"), FMath::RoundToInt(Bat)), X, Y, bLow && FMath::Fmod(Now, 0.8f) < 0.4f ? Blood : Dim, EHudFont::Mono, 15.f, 250);
	Y += 24.f * S;
	Segments(X, Y, 200.f * S, 7.f * S, 10, Bat / 100.f, bLow ? Blood : Paper, Faint);
}

void ABodycamHUD::DrawAmmo(const ABodycamCharacter* P, const ABodycamGameMode* GM)
{
	const float R = Canvas->ClipX - 50.f * S;
	float Y = Canvas->ClipY - 176.f * S;

	Text(FString::Printf(TEXT("$ %d"), GM->GetMoney()), R, Y, Amber, EHudFont::Mono, 19.f, 120, 1);
	Y += 30.f * S;

	const ABodycamWeapon* W = P->GetWeapon();
	if (!W) return;
	Text(W->GetDisplayName(), R, Y, Dim, EHudFont::Mono, 15.f, 250, 1);
	Y += 20.f * S;

	const FString Res = FString::Printf(TEXT(" / %d"), W->GetReserveAmmo());
	const FSlateFontInfo ResFont = Font(EHudFont::Mono, 26.f, 60);
	const float ResW = Measure(Res, ResFont).X;
	Text(Res, R, Y + 46.f * S, W->GetReserveAmmo() == 0 ? Blood : Dim, EHudFont::Mono, 26.f, 60, 1);
	Text(FString::Printf(TEXT("%d"), W->GetMagAmmo()), R - ResW, Y, (W->GetMagAmmo() == 0 || W->GetDryFireFlash() > 0.f) ? Blood : Paper, EHudFont::Display, 78.f, 40, 1);
	Y += 100.f * S;

	if (W->IsReloading())
	{
		Rect(R - 140.f * S, Y, 140.f * S, 2.f * S, Faint);
		Rect(R - 140.f * S, Y, 140.f * S * W->GetReloadProgress(), 2.f * S, Amber);
	}
	else if (W->GetMagAmmo() == 0 && FMath::Fmod(Now, 0.9f) < 0.55f)
	{
		Text(W->GetReserveAmmo() > 0 ? TEXT("[R]  RECHARGER") : TEXT("[T]  COMMANDER DES MUNITIONS"), R, Y - 6.f * S, Blood, EHudFont::Mono, 15.f, 200, 1);
	}
}

// ------------------------------------------------------------------ Marqueurs / alertes

void ABodycamHUD::DrawMarker(const FVector& World, const FString& Label, const FLinearColor& C, const FVector& From)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;
	FVector CamLoc; FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector Dir = (World - CamLoc).GetSafeNormal();

	const float Margin = 80.f * S;
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	FVector2D Pos;
	bool bOnScreen = false;
	if (FVector::DotProduct(CamRot.Vector(), Dir) > 0.15f)
	{
		const FVector Proj = Canvas->Project(World);
		Pos = FVector2D(Proj.X, Proj.Y);
		bOnScreen = Pos.X > Margin && Pos.X < Canvas->ClipX - Margin && Pos.Y > Margin && Pos.Y < Canvas->ClipY - Margin;
	}
	if (!bOnScreen)
	{
		const FVector Local = CamRot.UnrotateVector(Dir);
		FVector2D D(Local.Y, -Local.Z);
		if (D.IsNearlyZero()) D = FVector2D(0.f, 1.f);
		D.Normalize();
		const float K = FMath::Min((Canvas->ClipX * 0.5f - Margin) / FMath::Max(FMath::Abs(D.X), 0.001f), (Canvas->ClipY * 0.5f - Margin) / FMath::Max(FMath::Abs(D.Y), 0.001f));
		Pos = Center + D * K;
	}

	const float Sz = 7.f * S;
	const FVector2D Pts[4] = { Pos + FVector2D(0, -Sz), Pos + FVector2D(Sz, 0), Pos + FVector2D(0, Sz), Pos + FVector2D(-Sz, 0) };
	for (int32 i = 0; i < 4; ++i)
	{
		FCanvasLineItem Line(Pts[i], Pts[(i + 1) % 4]);
		Line.SetColor(C);
		Line.LineThickness = 2.f * S;
		Canvas->DrawItem(Line);
	}
	Text(FString::Printf(TEXT("%s  ·  %d M"), *Label, FMath::RoundToInt(FVector::Dist(World, From) / 100.f)), Pos.X, Pos.Y + 14.f * S, C, EHudFont::Mono, 14.f, 150, 2);
}

void ABodycamHUD::DrawMarkers(const ABodycamCharacter* P, const ABodycamGameMode* GM)
{
	if (const ABodycamPickup* Bat = GM->GetBattery())
	{
		DrawMarker(Bat->GetActorLocation() + FVector(0, 0, 25.f), TEXT("PILE"), Toxic, P->GetActorLocation());
	}
	if (const ABodycamDrone* D = P->GetDrone())
	{
		DrawMarker(D->GetActorLocation(), TEXT("DRONE"), Dim, P->GetActorLocation());
	}
	if (const ABodycamPickup* Crate = GM->GetSupplyCrate())
	{
		DrawMarker(Crate->GetActorLocation() + FVector(0, 0, 60.f), TEXT("CAISSE"), Amber, P->GetActorLocation());
	}
}

void ABodycamHUD::DrawAlert(const ABodycamGameMode* GM)
{
	const TArray<FBodycamMessage>& Msgs = GM->GetMessages();
	if (Msgs.Num() == 0) return;
	const FBodycamMessage& M = Msgs.Last();
	const float Elapsed = M.Duration - M.TimeLeft;
	const float Alpha = FMath::Clamp(FMath::Min(Elapsed / 0.25f, M.TimeLeft / 0.6f), 0.f, 1.f);
	const float CX = Canvas->ClipX * 0.5f;
	const float Y = 150.f * S;
	Text(M.Title, CX, Y, A(M.Color, Alpha), EHudFont::Mono, 18.f, 300, 2);
	Text(M.Sub, CX, Y + 28.f * S, A(Dim, Alpha), EHudFont::Mono, 15.f, 200, 2);
}

void ABodycamHUD::DrawToast(const ABodycamGameMode* GM)
{
	const float Age = GM->GetLastEventAge();
	if (Age > 2.6f || GM->GetLastEvent().IsEmpty()) return;
	const float Alpha = FMath::Clamp((2.6f - Age) / 0.6f, 0.f, 1.f);
	Text(GM->GetLastEvent(), Canvas->ClipX * 0.5f, Canvas->ClipY - 200.f * S, A(Amber, Alpha), EHudFont::Mono, 19.f, 300, 2);
}

// ------------------------------------------------------------------ Situations

void ABodycamHUD::DrawNVGTransition(const ABodycamCharacter* P)
{
	// Les jumelles passent devant les yeux : noir, puis les tubes "chauffent".
	float Black = 0.f;
	if (P->GetNVGState() == ENVGState::Lowering) Black = FMath::SmoothStep(0.6f, 1.f, P->GetNVGAlpha());
	else if (P->GetNVGState() == ENVGState::Down) Black = 1.f - P->GetNVGWarm();
	else if (P->GetNVGState() == ENVGState::Raising) Black = FMath::SmoothStep(0.5f, 1.f, P->GetNVGAlpha()) * 0.8f;
	if (Black > 0.01f) Rect(0.f, 0.f, Canvas->ClipX, Canvas->ClipY, FLinearColor(0.f, 0.f, 0.f, Black));
}

void ABodycamHUD::DrawGrab(const ABodycamCharacter* P)
{
	const EZombieAttack Mode = P->GetGrabMode();
	if (Mode == EZombieAttack::None || P->IsDead()) return;

	const float CX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.64f;
	const bool bAnkle = Mode == EZombieAttack::Ankle;
	const FString Title = bAnkle ? TEXT("CHEVILLE AGRIPPÉE") : Mode == EZombieAttack::Back ? TEXT("ATTAQUE DANS LE DOS") : TEXT("AGRIPPÉ");

	Text(Title, CX, Y, Blood, EHudFont::Display, 44.f, 120, 2);

	// Touche F dans un cadre, qui pulse.
	const float Pulse = FMath::Fmod(Now, 0.25f) < 0.12f ? 1.f : 0.6f;
	const float K = 46.f * S;
	Frame(CX - K * 0.5f, Y + 64.f * S, K, K, A(Paper, Pulse), 2.f);
	Text(TEXT("F"), CX, Y + 70.f * S, A(Paper, Pulse), EHudFont::Display, 34.f, 0, 2);
	Text(bAnkle ? TEXT("COUPS DE PIED  ·  OU TIREZ") : TEXT("MARTELEZ"), CX, Y + 122.f * S, Dim, EHudFont::Mono, 15.f, 300, 2);

	const float W = 320.f * S;
	Rect(CX - W * 0.5f, Y + 152.f * S, W, 3.f * S, Faint);
	Rect(CX - W * 0.5f, Y + 152.f * S, W * P->GetEscapeProgress(), 3.f * S, Paper);
	Text(FString::Printf(TEXT("%.1f"), FMath::Max(0.f, P->GetGrabTimeLeft())), CX + W * 0.5f + 14.f * S, Y + 142.f * S, Blood, EHudFont::Mono, 15.f, 0, 0);
}

void ABodycamHUD::DrawPhone(const ABodycamCharacter* P, const ABodycamGameMode* GM)
{
	if (!GM) return;
	const float W = 420.f * S, H = 300.f * S;
	const float X = Canvas->ClipX * 0.5f + 140.f * S;
	const float Y = Canvas->ClipY * 0.5f - H * 0.5f;
	const FBodycamLevelConfig& Lv = GM->GetLevel();

	Rect(X, Y, W, H, FLinearColor(0.012f, 0.012f, 0.01f, 0.88f));
	Frame(X, Y, W, H, Faint);
	const float LX = X + 28.f * S;
	float LY = Y + 24.f * S;
	Text(TEXT("TERMINAL LOGISTIQUE"), LX, LY, Dim, EHudFont::Mono, 15.f, 300); LY += 44.f * S;
	Text(FString::Printf(TEXT("SOLDE        $ %d"), GM->GetMoney()), LX, LY, Paper, EHudFont::Mono, 19.f, 60); LY += 30.f * S;
	Text(FString::Printf(TEXT("COLIS        %d CHARGEURS"), Lv.AmmoOrderMags), LX, LY, Paper, EHudFont::Mono, 19.f, 60); LY += 30.f * S;
	Text(FString::Printf(TEXT("PRIX         $ %d"), Lv.AmmoOrderCost), LX, LY, Paper, EHudFont::Mono, 19.f, 60); LY += 50.f * S;

	FString Action;
	FLinearColor AC = Amber;
	if (P->GetDrone()) Action = TEXT("[F]  REPRENDRE LE DRONE");
	else if (GM->GetDroneCooldown() > 0.f) { Action = FString::Printf(TEXT("NOUVEAU DRONE  %s"), *Clock(GM->GetDroneCooldown())); AC = Blood; }
	else if (GM->HasPendingOrder()) Action = TEXT("[F]  ENVOYER LE DRONE");
	else if (GM->GetMoney() < Lv.AmmoOrderCost) { Action = TEXT("FONDS INSUFFISANTS"); AC = Blood; }
	else Action = TEXT("[F]  COMMANDER ET DÉCOLLER");
	Text(Action, LX, LY, AC, EHudFont::Display, 30.f, 60); LY += 52.f * S;
	Text(TEXT("[T] FERMER     DRONE : ZQSD · SOURIS · ESPACE / CTRL"), LX, LY, Dim, EHudFont::Mono, 13.f, 60);
}

void ABodycamHUD::DrawDrone(const ABodycamCharacter* P, const ABodycamGameMode* GM)
{
	const ABodycamDrone* D = P->GetDrone();
	if (!D) return;
	const float W = Canvas->ClipX, H = Canvas->ClipY;

	// Bords du casque FPV : coins sombres arrondis + fines bordures.
	const float EW = W * 0.07f, EH = H * 0.07f;
	for (int32 i = 0; i < 6; ++i)
	{
		const FLinearColor C(0.f, 0.f, 0.f, 0.16f);
		const float K = (i + 1) / 6.f;
		Rect(0.f, 0.f, EW * K, H, C);
		Rect(W - EW * K, 0.f, EW * K, H, C);
		Rect(0.f, 0.f, W, EH * K, C);
		Rect(0.f, H - EH * K, W, EH * K, C);
	}
	Frame(EW * 0.6f, EH * 0.6f, W - EW * 1.2f, H - EH * 1.2f, A(Dim, 0.35f), 1.f);

	const float X0 = EW + 20.f * S, X1 = W - EW - 20.f * S, Y0 = EH + 16.f * S;
	Text(TEXT("CASQUE FPV  ·  SCAVENGER-01"), X0, Y0, Paper, EHudFont::Mono, 20.f, 60);
	if (FMath::Fmod(Now, 1.f) < 0.55f) Text(TEXT("LIVE"), X1, Y0, Blood, EHudFont::Mono, 20.f, 200, 1);

	// Réticule fin + horizon
	const float CX = W * 0.5f, CY = H * 0.5f, T = 2.f * S;
	Rect(CX - 34.f * S, CY - T * 0.5f, 22.f * S, T, Dim); Rect(CX + 12.f * S, CY - T * 0.5f, 22.f * S, T, Dim);
	Rect(CX - T * 0.5f, CY - 34.f * S, T, 22.f * S, Dim); Rect(CX - T * 0.5f, CY + 12.f * S, T, 22.f * S, Dim);

	float Y = H - EH - 130.f * S;
	Text(FString::Printf(TEXT("ALT  %5.1f M"), D->GetAltitude() / 100.f), X0, Y, Paper, EHudFont::Mono, 19.f, 60);
	Text(FString::Printf(TEXT("VIT  %5.1f M/S"), D->GetSpeed() / 100.f), X0, Y + 28.f * S, D->GetSpeed() > 400.f ? Amber : Paper, EHudFont::Mono, 19.f, 60);
	Text(FString::Printf(TEXT("OPÉRATEUR  %d M"), FMath::RoundToInt(D->GetReturnDistance() / 100.f)), X0, Y + 56.f * S, Dim, EHudFont::Mono, 16.f, 60);
	Text(TEXT("ZQSD · SOURIS · ESPACE MONTER · CTRL DESCENDRE"), X0, Y + 84.f * S, A(Dim, 0.8f), EHudFont::Mono, 13.f, 60);

	FString Status;
	FLinearColor SC = Amber;
	switch (D->GetState())
	{
	case EDroneState::Landed:
	case EDroneState::TakingOff: Status = TEXT("DÉCOLLAGE..."); break;
	case EDroneState::AutoReturn: Status = FString::Printf(TEXT("RETOUR AUTOMATIQUE  ·  %d M"), FMath::RoundToInt(D->GetReturnDistance() / 100.f)); SC = Toxic; break;
	default:
		if (D->IsGrabbing()) Status = TEXT("PINCE...");
		else if (D->IsCarrying()) { Status = TEXT("CAISSE ACCROCHÉE  ·  [R] RETOUR AUTOMATIQUE"); SC = Toxic; }
		else if (D->CanGrabNow()) { Status = TEXT("CAISSE SOUS LE DRONE  ·  [F] PINCE"); SC = Toxic; }
		else Status = TEXT("SORTEZ PAR UNE FENÊTRE CASSÉE  ·  PUIS LA CAISSE");
		break;
	}
	Text(Status, X1, Y, SC, EHudFont::Mono, 18.f, 150, 1);
	Text(TEXT("NE TOUCHEZ RIEN : UN CHOC DÉTRUIT LE DRONE"), X1, Y + 30.f * S, A(Blood, 0.85f), EHudFont::Mono, 14.f, 150, 1);
	Text(TEXT("[F] PINCE   [R] RETOUR AUTO   [T] RETIRER LE CASQUE"), X1, Y + 56.f * S, Dim, EHudFont::Mono, 14.f, 150, 1);

	if (D->GetHintTime() > 0.f && !D->GetHint().IsEmpty())
	{
		Text(D->GetHint(), CX, CY + 70.f * S, A(Paper, FMath::Clamp(D->GetHintTime(), 0.f, 1.f)), EHudFont::Mono, 20.f, 200, 2);
	}

	FVector Exit;
	bool bExitOpen = false;
	if (D->GetExitMarker(Exit, bExitOpen))
	{
		DrawMarker(Exit, bExitOpen ? TEXT("SORTIE") : TEXT("VITRE INTACTE"), bExitOpen ? Toxic : Blood, D->GetActorLocation());
	}
	if (GM)
	{
		if (const ABodycamPickup* Crate = GM->GetSupplyCrate())
		{
			DrawMarker(Crate->GetActorLocation() + FVector(0, 0, 60.f), TEXT("CAISSE"), Amber, D->GetActorLocation());
		}
	}
	if (D->IsCarrying())
	{
		DrawMarker(P->GetActorLocation() + FVector(0, 0, 60.f), TEXT("OPÉRATEUR"), Toxic, D->GetActorLocation());
	}
	if (P->GetGrabMode() != EZombieAttack::None || P->GetHurtFlash() > 0.5f)
	{
		Text(TEXT("L'OPÉRATEUR EST ATTAQUÉ"), CX, Y0 + 70.f * S, Blood, EHudFont::Mono, 20.f, 300, 2);
	}
}

void ABodycamHUD::DrawHeadset(const ABodycamCharacter* P)
{
	const float F = P->GetHeadsetFade();
	if (F <= 0.f) return;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	Rect(0.f, 0.f, W, H, FLinearColor(0.f, 0.f, 0.f, F));
	if (F < 0.5f) return;
	const FString Msg = P->IsPilotingDrone() || P->IsDeployingDrone() ? TEXT("CASQUE FPV  ·  CONNEXION AU DRONE") : TEXT("RETRAIT DU CASQUE");
	const float TA = (F - 0.5f) * 2.f;
	Text(Msg, W * 0.5f, H * 0.5f - 12.f * S, A(Dim, TA), EHudFont::Mono, 20.f, 300, 2);
	if (P->IsPilotingDrone() || P->IsDeployingDrone())
	{
		const float Bar = 260.f * S;
		Rect(W * 0.5f - Bar * 0.5f, H * 0.5f + 22.f * S, Bar, 2.f * S, A(Faint, TA));
		Rect(W * 0.5f - Bar * 0.5f, H * 0.5f + 22.f * S, Bar * FMath::Fmod(Now * 1.7f, 1.f), 2.f * S, A(Amber, TA));
	}
}

// ------------------------------------------------------------------ Écrans

void ABodycamHUD::DrawIntro(const ABodycamGameMode* GM)
{
	const float T = GM->GetStateTimer(); // 6 -> 0
	const float BlackA = FMath::Clamp((T - 2.f) / 2.5f, 0.f, 1.f);
	const float TextA = FMath::Clamp(T / 1.f, 0.f, 1.f) * FMath::Clamp((6.f - T) / 0.8f, 0.f, 1.f);
	Rect(0.f, 0.f, Canvas->ClipX, Canvas->ClipY, FLinearColor(0.f, 0.f, 0.f, 0.96f * BlackA));

	const FBodycamLevelConfig& Lv = GM->GetLevel();
	const float X = 140.f * S;
	const float Y = Canvas->ClipY * 0.58f;
	const FDateTime D = FDateTime::Now();
	Text(FString::Printf(TEXT("%s  ·  NIVEAU %d  ·  %s"), *CameraLabel, Lv.Index, *Stamp(D)), X, Y, A(Dim, TextA), EHudFont::Mono, 16.f, 300);
	Text(Lv.Sector, X - 4.f * S, Y + 24.f * S, A(Paper, TextA), EHudFont::Display, 96.f, 120);
	Text(FString::Printf(TEXT("ÉLIMINER %d INFECTÉS  ·  %s"), Lv.KillQuota, *Clock(Lv.TimeLimit)), X, Y + 142.f * S, A(Amber, TextA), EHudFont::Mono, 19.f, 300);

	const float BY = Canvas->ClipY - 110.f * S;
	Text(TEXT("ZQSD  DÉPLACER   SHIFT  COURIR   CTRL  ACCROUPI   A / E  PENCHER   CLIC  TIRER   R  RECHARGER"), X, BY, A(Dim, TextA), EHudFont::Mono, 14.f, 150);
	Text(TEXT("N  VISION NOCTURNE   T  DRONE   F  SE DÉGAGER   ÉCHAP  PAUSE"), X, BY + 24.f * S, A(Dim, TextA), EHudFont::Mono, 14.f, 150);
}

void ABodycamHUD::DrawControls()
{
	// F1 : afficher / masquer la liste des touches.
	APlayerController* PC = GetOwningPlayerController();
	if (PC && PC->WasInputKeyJustPressed(EKeys::F1)) bShowControls = !bShowControls;
	const float X = 48.f * S;
	float Y = Canvas->ClipY * 0.42f;
	if (!bShowControls)
	{
		Text(TEXT("F1  TOUCHES"), X, Canvas->ClipY - 40.f * S, A(Dim, 0.6f), EHudFont::Mono, 13.f, 150);
		return;
	}
	ABodycamPlayerController* BPC = Cast<ABodycamPlayerController>(PC);
	const bool bAz = BPC && BPC->GetInputConfig()->IsAzerty();
	const TCHAR* Keys[][2] = {
		{ bAz ? TEXT("ZQSD") : TEXT("WASD"), TEXT("SE DÉPLACER") },
		{ TEXT("SHIFT"), TEXT("COURIR") },
		{ TEXT("CTRL / C"), TEXT("S'ACCROUPIR") },
		{ TEXT("ESPACE"), TEXT("SAUTER") },
		{ bAz ? TEXT("A / E") : TEXT("Q / E"), TEXT("SE PENCHER") },
		{ TEXT("CLIC GAUCHE"), TEXT("TIRER") },
		{ TEXT("CLIC DROIT"), TEXT("VISER") },
		{ TEXT("CLIC MOLETTE"), TEXT("COUP D'ARME") },
		{ TEXT("R"), TEXT("RECHARGER") },
		{ TEXT("L"), TEXT("LAMPE TORCHE") },
		{ TEXT("N"), TEXT("VISION NOCTURNE") },
		{ TEXT("F"), TEXT("SE DÉGAGER / PINCE DRONE") },
		{ TEXT("T"), TEXT("DRONE + CASQUE FPV") },
		{ TEXT("R (DRONE)"), TEXT("RETOUR AUTOMATIQUE") },
		{ TEXT("ÉCHAP"), TEXT("PAUSE") },
		{ TEXT("F1"), TEXT("MASQUER CETTE AIDE") },
	};
	Rect(X - 14.f * S, Y - 14.f * S, 330.f * S, (UE_ARRAY_COUNT(Keys) * 21.f + 22.f) * S, FLinearColor(0.f, 0.f, 0.f, 0.35f));
	for (const auto& K : Keys)
	{
		Text(K[0], X, Y, A(Amber, 0.9f), EHudFont::Mono, 13.f, 100);
		Text(K[1], X + 130.f * S, Y, A(Paper, 0.75f), EHudFont::Mono, 13.f, 100);
		Y += 21.f * S;
	}
}

void ABodycamHUD::DrawDeath(const ABodycamCharacter* P, const ABodycamGameMode* GM)
{
	const float Since = GM->GetStateAge();
	const float W = Canvas->ClipX, H = Canvas->ClipY;

	// 0-2.6 s : on voit la chute. Bords rouges qui pulsent comme un cœur qui ralentit.
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Since * FMath::Lerp(7.f, 2.5f, FMath::Clamp(Since / 2.6f, 0.f, 1.f)));
	const float Edge = FMath::Clamp(1.f - Since / 4.f, 0.25f, 1.f) * (0.35f + 0.25f * Pulse);
	const float EW = W * 0.16f, EH = H * 0.16f;
	for (int32 i = 0; i < 8; ++i)
	{
		const float K = (8 - i) / 8.f;
		const FLinearColor C(0.35f, 0.f, 0.f, Edge * K * 0.18f);
		Rect(0.f, 0.f, EW * (i + 1) / 8.f, H, C);
		Rect(W - EW * (i + 1) / 8.f, 0.f, EW * (i + 1) / 8.f, H, C);
		Rect(0.f, 0.f, W, EH * (i + 1) / 8.f, C);
		Rect(0.f, H - EH * (i + 1) / 8.f, W, EH * (i + 1) / 8.f, C);
	}

	// Puis fondu au noir lent.
	const float Fade = FMath::Clamp((Since - 2.4f) / 1.6f, 0.f, 1.f);
	Rect(0.f, 0.f, W, H, FLinearColor(0.f, 0.f, 0.f, Fade * 0.9f));
	const float TA = FMath::Clamp((Since - 3.2f) / 0.8f, 0.f, 1.f);
	if (TA <= 0.f) return;

	const FBodycamLevelConfig& Lv = GM->GetLevel();
	const float X = 140.f * S;
	float Y = H * 0.30f;
	Text(FString::Printf(TEXT("%s  ·  NIVEAU %d  ·  %s"), *CameraLabel, Lv.Index, *Lv.Title.ToUpper()), X, Y, A(Dim, TA), EHudFont::Mono, 16.f, 300);
	Y += 28.f * S;
	Text(TEXT("VOUS ÊTES MORT"), X - 4.f * S, Y, A(Paper, TA), EHudFont::Display, 104.f, 120);
	Y += 132.f * S;
	Rect(X, Y, 520.f * S, 2.f * S, A(Blood, TA));
	Y += 22.f * S;
	Text(P && P->WasNeckBite() ? TEXT("MORSURE AU COU") : TEXT("BLESSURES MORTELLES"), X, Y, A(Blood, TA), EHudFont::Mono, 20.f, 250);
	Y += 56.f * S;

	// Bilan
	auto Stat = [&](float SX, const FString& Value, const TCHAR* Label)
	{
		Text(Value, SX, Y, A(Paper, TA), EHudFont::Display, 54.f, 40);
		Text(Label, SX, Y + 66.f * S, A(Dim, TA), EHudFont::Mono, 14.f, 250);
	};
	Stat(X, FString::Printf(TEXT("%d"), GM->GetKills()), TEXT("INFECTÉS ÉLIMINÉS"));
	Stat(X + 300.f * S, Clock(GM->GetElapsed()), TEXT("TEMPS DE SURVIE"));
	Stat(X + 600.f * S, FString::Printf(TEXT("%d / %d"), GM->GetKills(), Lv.KillQuota), TEXT("OBJECTIF"));

	// Choix
	const float BY = H - 150.f * S;
	const float Blink = FMath::Fmod(Now, 1.2f) < 0.8f ? 1.f : 0.55f;
	Frame(X, BY - 10.f * S, 330.f * S, 52.f * S, A(Paper, TA * Blink), 2.f * S);
	Text(TEXT("[ENTRÉE]  RECOMMENCER"), X + 22.f * S, BY + 4.f * S, A(Paper, TA), EHudFont::Mono, 18.f, 200);
	Text(TEXT("[ÉCHAP]  PAUSE / QUITTER"), X + 370.f * S, BY + 4.f * S, A(Dim, TA), EHudFont::Mono, 18.f, 200);
	const int32 Left = FMath::Max(0, FMath::CeilToInt(GM->GetStateTimer()));
	Text(FString::Printf(TEXT("RELANCE AUTOMATIQUE DANS %d S"), Left), X, BY + 62.f * S, A(Dim, TA * 0.7f), EHudFont::Mono, 13.f, 250);
}

void ABodycamHUD::DrawEnd(const ABodycamCharacter* P, const ABodycamGameMode* GM)
{
	if (GM->GetMatchState() == EBodycamMatchState::Lost && P && P->IsDead())
	{
		DrawDeath(P, GM);
		return;
	}
	const bool bWon = GM->GetMatchState() == EBodycamMatchState::Won;
	const float Since = GM->GetStateAge();
	const FBodycamLevelConfig& Lv = GM->GetLevel();

	// Coupure du signal : quelques bandes parasites, puis noir.
	const float Black = FMath::Clamp(Since / 1.2f, 0.f, 1.f) * (bWon ? 0.85f : 0.94f);
	Rect(0.f, 0.f, Canvas->ClipX, Canvas->ClipY, FLinearColor(0.f, 0.f, 0.f, Black));
	if (!bWon && Since < 0.7f)
	{
		for (int32 i = 0; i < 7; ++i)
		{
			const float BY = FMath::FRand() * Canvas->ClipY;
			Rect(0.f, BY, Canvas->ClipX, FMath::FRandRange(2.f, 26.f) * S, FLinearColor(0.f, 0.f, 0.f, FMath::FRandRange(0.4f, 0.9f)));
		}
	}

	const float TA = FMath::Clamp((Since - 0.8f) / 0.6f, 0.f, 1.f);
	if (TA <= 0.f) return;

	const FDateTime D = FDateTime::Now();
	if (!bWon)
	{
		Rect(Canvas->ClipX - 230.f * S, 47.f * S, 12.f * S, 12.f * S, A(Paper, TA));
		Text(FString::Printf(TEXT("STOP   %s"), *Stamp(D)), Canvas->ClipX - 48.f * S, 40.f * S, A(Paper, TA), EHudFont::Mono, 20.f, 100, 1);
	}

	const bool bLast = bWon && Lv.Index >= FBodycamLevelConfig::NumLevels;
	const bool bDead = P && P->IsDead();
	FString Title, Cause;
	FLinearColor CauseColor = Blood;
	if (bWon)
	{
		Title = bLast ? TEXT("DÉMO TERMINÉE") : TEXT("SECTEUR NETTOYÉ");
		Cause = FString::Printf(TEXT("%d INFECTÉS"), GM->GetKills());
		CauseColor = Toxic;
	}
	else if (bDead)
	{
		Title = TEXT("ENREGISTREMENT INTERROMPU");
		Cause = P->WasNeckBite() ? TEXT("MORSURE AU COU") : TEXT("BLESSURES MORTELLES");
	}
	else
	{
		Title = TEXT("TEMPS ÉCOULÉ");
		Cause = TEXT("QUOTA NON ATTEINT");
	}

	const float X = 140.f * S;
	const float Y = Canvas->ClipY * 0.38f;
	Text(FString::Printf(TEXT("%s  ·  NIVEAU %d"), *CameraLabel, Lv.Index), X, Y, A(Dim, TA), EHudFont::Mono, 17.f, 300);
	Text(Title, X - 4.f * S, Y + 26.f * S, A(Paper, TA), EHudFont::Display, 88.f, 120);

	const FString Rest = bWon
		? FString::Printf(TEXT("   ·   %s RESTANT   ·   $ %d"), *Clock(GM->GetTimeLeft()), GM->GetMoney())
		: FString::Printf(TEXT("   ·   %d INFECTÉS ÉLIMINÉS   ·   %s DE SURVIE"), GM->GetKills(), *Clock(GM->GetElapsed()));
	const FSlateFontInfo CF = Font(EHudFont::Mono, 20.f, 250);
	Text(Cause, X, Y + 140.f * S, A(CauseColor, TA), EHudFont::Mono, 20.f, 250);
	Text(Rest, X + Measure(Cause, CF).X, Y + 140.f * S, A(Paper, TA), EHudFont::Mono, 20.f, 250);

	const int32 Left = FMath::Max(0, FMath::CeilToInt(GM->GetStateTimer()));
	const FString Next = bWon ? (bLast ? FString::Printf(TEXT("RETOUR AU NIVEAU 1 DANS %d"), Left) : FString::Printf(TEXT("NIVEAU SUIVANT DANS %d"), Left))
							  : FString::Printf(TEXT("NOUVELLE TENTATIVE DANS %d"), Left);
	Text(Next, X, Canvas->ClipY - 110.f * S, A(Dim, TA), EHudFont::Mono, 16.f, 300);
}

void ABodycamHUD::DrawPause()
{
	Rect(0.f, 0.f, Canvas->ClipX, Canvas->ClipY, FLinearColor(0.f, 0.f, 0.f, 0.72f));
	const float X = 140.f * S, Y = Canvas->ClipY * 0.4f;
	Text(TEXT("PAUSE"), X - 4.f * S, Y, Paper, EHudFont::Display, 96.f, 200);
	Text(TEXT("[ÉCHAP]  REPRENDRE        [ENTRÉE]  QUITTER LE JEU"), X, Y + 130.f * S, Dim, EHudFont::Mono, 18.f, 250);
}

void ABodycamHUD::DrawDebug(const ABodycamCharacter* P)
{
	ABodycamPlayerController* PC = Cast<ABodycamPlayerController>(GetOwningPlayerController());
	const ABodycamGameMode* GM = GetWorld()->GetAuthGameMode<ABodycamGameMode>();
	const FString Line = FString::Printf(TEXT("DEBUG  vitesse %3.0f  stamina %3.0f  zombies %d  %s  pos %s  FPS %3.0f"),
		P->GetVelocity().Size2D(), P->GetStamina(), GM ? GM->GetAliveZombies() : 0,
		PC && PC->GetInputConfig()->IsAzerty() ? TEXT("AZERTY") : TEXT("QWERTY"),
		*P->GetActorLocation().ToCompactString(),
		1.f / FMath::Max(GetWorld()->GetDeltaSeconds(), 0.0001f));
	Text(Line, 48.f * S, Canvas->ClipY - 36.f * S, FLinearColor(1.f, 0.85f, 0.2f, 1.f), EHudFont::Mono, 14.f, 0);
}
