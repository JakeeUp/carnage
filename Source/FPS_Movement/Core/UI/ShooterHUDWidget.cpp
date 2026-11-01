// FPS_Movement: native combat HUD.

#include "ShooterHUDWidget.h"

#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "StyleMeterComponent.h"
#include "DoomMovementComponent.h"
#include "CarnageAttributeSet.h"
#include "BodycamComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"

// ─────────────────────────────────────────────────────────────────────────────
// Small math helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace ShooterHUD
{
	/** Critically damped smoothing toward Target (no overshoot). */
	static float SmoothDamp(float Current, float Target, float& Velocity, float SmoothTime, float Dt)
	{
		SmoothTime = FMath::Max(0.0001f, SmoothTime);
		const float Omega = 2.0f / SmoothTime;
		const float X = Omega * Dt;
		const float Exp = 1.0f / (1.0f + X + 0.48f * X * X + 0.235f * X * X * X);
		const float Change = Current - Target;
		const float Temp = (Velocity + Omega * Change) * Dt;
		Velocity = (Velocity - Omega * Temp) * Exp;
		return Target + (Change + Temp) * Exp;
	}

	static float ExpDecay(float Value, float Rate, float Dt)
	{
		return Value * FMath::Exp(-Rate * Dt);
	}

	static FLinearColor WithAlpha(FLinearColor C, float AlphaMul)
	{
		C.A *= AlphaMul;
		return C;
	}

	static FVector2f Polar(const FVector2f& Center, float AngleRad, float Radius)
	{
		return Center + FVector2f(FMath::Sin(AngleRad), -FMath::Cos(AngleRad)) * Radius;
	}

	/** Bodycam OSD text rows; a glitch frame tears one of them (or all). */
	enum EOSDRow : int32 { OSDRec, OSDStyle, OSDClock, OSDHeading, OSDVitals, OSDDash, OSDMag, OSDStatus, OSDDevice, OSDRowCount };

	/** Zero-padded integer, e.g. ZeroPad(8, 2) == "08". Negative values clamp to 0. */
	static FString ZeroPad(int32 Value, int32 Digits)
	{
		const FString S = FString::FromInt(FMath::Max(Value, 0));
		return S.Len() < Digits ? FString::ChrN(Digits - S.Len(), TEXT('0')) + S : S;
	}

	/** Text gauge like "[#####-----]". */
	static FString TextBar(float Fraction, int32 Chars)
	{
		const int32 Filled = FMath::Clamp(FMath::FloorToInt(Fraction * Chars), 0, Chars);
		return FString(TEXT("[")) + FString::ChrN(Filled, TEXT('#')) + FString::ChrN(Chars - Filled, TEXT('-')) + TEXT("]");
	}

	/** OSD monospace font: about HeightFraction of the screen tall, with a 1 px dark outline. */
	static FSlateFontInfo MakeOSDFont(float ScreenHeight, float HeightFraction, float Opacity)
	{
		// Slate font sizes are points at 96 DPI (1 pt = 4/3 px), so convert the requested pixel height.
		const float Size = FMath::Max(6.0f, FMath::RoundToFloat(ScreenHeight * HeightFraction * 0.75f));
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Mono", Size);
		Font.OutlineSettings.OutlineSize = 1;
		// The outline color isn't multiplied by the element tint, so it is faded here by hand.
		Font.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f * Opacity);
		return Font;
	}
}

using namespace ShooterHUD;

// ─────────────────────────────────────────────────────────────────────────────
// Painter: thin wrapper over FSlateDrawElement in widget-local (DPI independent) units
// ─────────────────────────────────────────────────────────────────────────────

struct UShooterHUDWidget::FPainter
{
	const FGeometry& Geo;
	FSlateWindowElementList& Out;
	int32 BaseLayer;
	const FSlateBrush* Brush;
	float Opacity;
	FVector2f Size;

	FLinearColor Tint(FLinearColor C) const
	{
		C.A *= Opacity;
		return C;
	}

	/** Axis-aligned rect at top-left Pos, optionally sheared (Skew) and rotated (degrees) about its center. */
	void Rect(const FVector2f& Pos, const FVector2f& RectSize, const FLinearColor& Color, float Skew = 0.0f, float AngleDeg = 0.0f, int32 Layer = 0) const
	{
		if (Color.A * Opacity <= 0.001f || RectSize.X <= 0.0f || RectSize.Y <= 0.0f)
		{
			return;
		}

		FSlateRenderTransform Render;
		if (Skew != 0.0f || AngleDeg != 0.0f)
		{
			const TMatrix2x2<float> M = Concatenate(FShear2f(Skew, 0.0f), FQuat2f(FMath::DegreesToRadians(AngleDeg)));
			Render = FSlateRenderTransform(M);
		}

		const FPaintGeometry PG = Geo.ToPaintGeometry(RectSize, FSlateLayoutTransform(1.0f, Pos), Render, FVector2f(0.5f, 0.5f));
		FSlateDrawElement::MakeBox(Out, BaseLayer + Layer, PG, Brush, ESlateDrawEffect::None, Tint(Color));
	}

	void RectCentered(const FVector2f& Center, const FVector2f& RectSize, const FLinearColor& Color, float Skew = 0.0f, float AngleDeg = 0.0f, int32 Layer = 0) const
	{
		Rect(Center - RectSize * 0.5f, RectSize, Color, Skew, AngleDeg, Layer);
	}

	void Lines(const TArray<FVector2f>& Points, const FLinearColor& Color, float Thickness, int32 Layer = 0) const
	{
		if (Points.Num() < 2 || Color.A * Opacity <= 0.001f)
		{
			return;
		}
		FSlateDrawElement::MakeLines(Out, BaseLayer + Layer, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, Tint(Color), true, Thickness);
	}

	static FVector2f Measure(const FString& Text, const FSlateFontInfo& Font)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FVector2f(Font.Size * 0.6f * Text.Len(), Font.Size * 1.2f);
		}
		const TSharedRef<FSlateFontMeasure> Measurer = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return FVector2f(Measurer->Measure(Text, Font, 1.0f));
	}

	/**
	 * Draws text so that the point Align (0..1 in each axis of the text box) lands on Anchor.
	 * Scale grows/shrinks around that same anchor point. Returns the scaled text size.
	 */
	FVector2f Text(const FString& Text, const FSlateFontInfo& Font, const FVector2f& Anchor, const FVector2f& Align,
		const FLinearColor& Color, float Scale = 1.0f, const FLinearColor* Shadow = nullptr, int32 Layer = 0) const
	{
		const FVector2f TextSize = Measure(Text, Font);
		if (Text.IsEmpty() || Color.A * Opacity <= 0.001f)
		{
			return TextSize * Scale;
		}

		const FVector2f TopLeft = Anchor - TextSize * Align * Scale;
		if (Shadow)
		{
			const FVector2f ShadowOffset(2.0f * Scale, 2.0f * Scale);
			FSlateDrawElement::MakeText(Out, BaseLayer + Layer, Geo.ToPaintGeometry(TextSize, FSlateLayoutTransform(Scale, TopLeft + ShadowOffset)),
				Text, Font, ESlateDrawEffect::None, Tint(FLinearColor(Shadow->R, Shadow->G, Shadow->B, Shadow->A * Color.A)));
		}
		FSlateDrawElement::MakeText(Out, BaseLayer + Layer + 1, Geo.ToPaintGeometry(TextSize, FSlateLayoutTransform(Scale, TopLeft)),
			Text, Font, ESlateDrawEffect::None, Tint(Color));
		return TextSize * Scale;
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Lifetime / binding
// ─────────────────────────────────────────────────────────────────────────────

UShooterHUDWidget::UShooterHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RankColors = {
		FLinearColor(0.62f, 0.60f, 0.58f),  // D   ash
		FLinearColor(0.40f, 0.70f, 1.00f),  // C   steel blue
		FLinearColor(0.55f, 1.00f, 0.35f),  // B   acid green
		FLinearColor(1.00f, 0.80f, 0.15f),  // A   gold
		FLinearColor(1.00f, 0.42f, 0.05f),  // S   hot orange
		FLinearColor(1.00f, 0.10f, 0.10f),  // SS  blood red
		FLinearColor(1.00f, 0.15f, 0.55f)   // SSS hot magenta
	};

	RankTitles = { TEXT("DRIFTER"), TEXT("CARNAGE"), TEXT("BRUTAL"), TEXT("APEX"), TEXT("SLAUGHTER"), TEXT("SAVAGE"), TEXT("SHATTERED") };
}

void UShooterHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::HitTestInvisible);
	// Everything animates every frame; keep the widget out of invalidation caching.
	ForceVolatile(true);

	Gap = CrosshairBaseGap;

	if (!BoundCharacter.IsValid())
	{
		BindToCharacter(Cast<AShooterCharacter>(GetOwningPlayerPawn()));
	}
}

void UShooterHUDWidget::BindToCharacter(AShooterCharacter* Character)
{
	if (AShooterCharacter* Old = BoundCharacter.Get())
	{
		if (Old == Character)
		{
			return;
		}
		Old->OnBulletCountUpdated.RemoveDynamic(this, &UShooterHUDWidget::HandleBulletCountUpdated);
		Old->OnReserveAmmoUpdated.RemoveDynamic(this, &UShooterHUDWidget::HandleReserveAmmoUpdated);
		Old->OnDamaged.RemoveDynamic(this, &UShooterHUDWidget::HandleDamaged);
		Old->OnHitConfirmed.RemoveDynamic(this, &UShooterHUDWidget::HandleHitConfirmed);
		Old->OnAimStateChanged.RemoveDynamic(this, &UShooterHUDWidget::HandleAimStateChanged);
	}

	BoundCharacter = Character;
	Movement = nullptr;
	StyleMeter = nullptr;
	ActiveWeapon = nullptr;

	// Fresh life: reset transient state.
	bHasAmmo = false;
	MagSize = Bullets = Reserve = 0;
	bReloading = false;
	Bloom = 0.0f;
	bAimTarget = false;
	HealthFrac = HealthTrail = LastLifePercent = 1.0f;
	ArmorFrac = ArmorTrail = 0.0f;
	bHasRank = false;
	DamageIndicators.Reset();
	DamageNumbers.Reset();

	if (!Character)
	{
		return;
	}

	Character->OnBulletCountUpdated.AddUniqueDynamic(this, &UShooterHUDWidget::HandleBulletCountUpdated);
	Character->OnReserveAmmoUpdated.AddUniqueDynamic(this, &UShooterHUDWidget::HandleReserveAmmoUpdated);
	Character->OnDamaged.AddUniqueDynamic(this, &UShooterHUDWidget::HandleDamaged);
	Character->OnHitConfirmed.AddUniqueDynamic(this, &UShooterHUDWidget::HandleHitConfirmed);
	Character->OnAimStateChanged.AddUniqueDynamic(this, &UShooterHUDWidget::HandleAimStateChanged);

	Movement = Character->FindComponentByClass<UDoomMovementComponent>();
	StyleMeter = Character->FindComponentByClass<UStyleMeterComponent>();
	bAimTarget = Character->IsAimingDownSights();

	if (const UDoomMovementComponent* Move = Movement.Get())
	{
		DashCharges = Move->GetDashCharges();
		MaxDashCharges = Move->MaxDashCharges;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Events
// ─────────────────────────────────────────────────────────────────────────────

void UShooterHUDWidget::HandleBulletCountUpdated(int32 InMagazineSize, int32 InBullets)
{
	ApplyAmmo(InMagazineSize, InBullets);
}

void UShooterHUDWidget::HandleReserveAmmoUpdated(int32 InReserveAmmo)
{
	Reserve = InReserveAmmo;
}

void UShooterHUDWidget::ApplyAmmo(int32 InMagazineSize, int32 InBullets)
{
	const bool bShot = bHasAmmo && InMagazineSize == MagSize && InMagazineSize > 0 && InBullets < Bullets;

	MagSize = InMagazineSize;
	Bullets = InBullets;
	bHasAmmo = true;

	if (bShot)
	{
		OnShotDetected();
	}
}

void UShooterHUDWidget::OnShotDetected()
{
	Bloom = FMath::Min(Bloom + ShotBloom, MaxBloom);
	AmmoPunch = 1.0f;
}

void UShooterHUDWidget::HandleDamaged(float LifePercent)
{
	const float Lost = LastLifePercent - LifePercent;
	if (Lost > KINDA_SMALL_NUMBER)
	{
		VignetteAlpha = FMath::Min(1.0f, VignetteAlpha + VignetteHitAlpha + Lost * VignettePerHealthLost);
	}
	LastLifePercent = LifePercent;
}

void UShooterHUDWidget::HandleHitConfirmed(AActor* Target, float Damage, bool bKilled, bool bHeadshot)
{
	NotifyHitConfirmed(Damage, bKilled, bHeadshot);
}

void UShooterHUDWidget::NotifyHitConfirmed(float Damage, bool bKilled, bool bHeadshot)
{
	const uint8 Kind = bKilled ? 2 : (bHeadshot ? 1 : 0);
	const bool bStacking = (Now - HitTime) < HitmarkerDuration;

	HitStack = bStacking ? FMath::Min(HitStack + 1, HitmarkerMaxStack) : 0;
	// A kill always wins; otherwise a headshot upgrades a stacked normal hit.
	HitKind = bStacking ? FMath::Max(HitKind, Kind) : Kind;
	if (bKilled)
	{
		HitKind = 2;
	}
	HitTime = Now;

	if (bShowDamageNumbers && Damage > 0.0f)
	{
		FShooterHUDDamageNumber& Num = DamageNumbers.AddDefaulted_GetRef();
		Num.Value = Damage;
		Num.SpawnTime = Now;
		const float Side = (DamageNumbers.Num() % 2 == 0) ? -1.0f : 1.0f;
		Num.Offset = FVector2f(Side * FMath::FRandRange(26.0f, 44.0f), FMath::FRandRange(-26.0f, -12.0f));
		Num.Color = bKilled ? ColorDanger : (bHeadshot ? ColorHeadshot : ColorPrimary);
		if (DamageNumbers.Num() > 8)
		{
			DamageNumbers.RemoveAt(0);
		}
	}
}

void UShooterHUDWidget::HandleAimStateChanged(bool bIsAimingDownSights)
{
	bAimTarget = bIsAimingDownSights;
}

void UShooterHUDWidget::NotifyDamageFrom(const FVector& WorldSourceLocation)
{
	// Merge with a very recent marker from (almost) the same place so duplicate reports don't double up.
	for (FShooterHUDDamageIndicator& Existing : DamageIndicators)
	{
		if ((Now - Existing.SpawnTime) < 0.15f && FVector::DistSquared(Existing.WorldSource, WorldSourceLocation) < FMath::Square(150.0f))
		{
			Existing.WorldSource = WorldSourceLocation;
			Existing.SpawnTime = Now;
			return;
		}
	}

	FShooterHUDDamageIndicator& Ind = DamageIndicators.AddDefaulted_GetRef();
	Ind.WorldSource = WorldSourceLocation;
	Ind.SpawnTime = Now;
	if (DamageIndicators.Num() > 8)
	{
		DamageIndicators.RemoveAt(0);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────────────────────────────────────

void UShooterHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Slate delta is real (undilated) time; clamp against hitches.
	const float Dt = FMath::Clamp(InDeltaTime, 0.0f, 0.1f);
	Now += Dt;

	// Auto re-bind if the owning pawn changed underneath us (respawn without an explicit bind).
	AShooterCharacter* OwnerPawn = Cast<AShooterCharacter>(GetOwningPlayerPawn());
	if (OwnerPawn && OwnerPawn != BoundCharacter.Get())
	{
		BindToCharacter(OwnerPawn);
	}

	TickAmmo(Dt);
	TickCrosshair(Dt);
	TickVitals(Dt);
	TickRank(Dt);
	TickDamageFeedback(Dt);

	TickBodycam(Dt);
}

void UShooterHUDWidget::TickBodycam(float Dt)
{
	const AShooterCharacter* Character = BoundCharacter.Get();
	const UBodycamComponent* Bodycam = Character ? Character->GetBodycam() : nullptr;
	BodycamAlpha = Bodycam ? Bodycam->GetBlendAlpha() : 0.0f;

	const bool bLowHealth = HealthFrac > 0.0f && HealthFrac < LowHealthThreshold;
	LowHealthBlend = FMath::FInterpTo(LowHealthBlend, bLowHealth ? 1.0f : 0.0f, Dt, 6.0f);
	RecPhase = FMath::Fmod(RecPhase + Dt * FMath::Lerp(BodycamRecBlinkRate, BodycamLowHealthRecBlinkRate, LowHealthBlend), 1.0f);

	if (BodycamAlpha <= 0.001f)
	{
		return;
	}

	// Slow brightness wander, never above 1 so the text stays at its authored brightness at peak.
	OSDFlicker = 1.0f - BodycamFlickerAmount * (0.5f + 0.5f * FMath::PerlinNoise1D(Now * BodycamFlickerRate));

	// Rare one-frame tear. Rolled on real time; the paint pass applies it only on this exact frame.
	if (FMath::FRand() < BodycamGlitchChancePerSecond * Dt)
	{
		GlitchFrame = GFrameCounter;
		GlitchRow = FMath::FRand() < 0.3f ? INDEX_NONE : FMath::RandRange(0, OSDRowCount - 1);
		GlitchOffset = FMath::FRandRange(0.4f, 1.0f) * BodycamGlitchMaxOffset * (FMath::RandBool() ? 1.0f : -1.0f);
	}

	if (const APlayerController* PC = GetOwningPlayer())
	{
		FVector CamLoc;
		FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		HeadingDeg = FRotator::ClampAxis(CamRot.Yaw);
	}
	SpeedMetersPerSec = Character ? Character->GetVelocity().Size2D() / 100.0f : 0.0f;
}

AShooterWeapon* UShooterHUDWidget::FindActiveWeapon() const
{
	const AShooterCharacter* Character = BoundCharacter.Get();
	return Character ? Character->GetCurrentWeapon() : nullptr;
}

float UShooterHUDWidget::ReadReloadDuration(const AShooterWeapon* Weapon) const
{
	const float Duration = Weapon ? Weapon->GetReloadDuration() : 0.0f;
	return Duration > KINDA_SMALL_NUMBER ? Duration : FallbackReloadDuration;
}

void UShooterHUDWidget::TickAmmo(float Dt)
{
	AmmoPunch = ExpDecay(AmmoPunch, 14.0f, Dt);

	AShooterWeapon* Weapon = FindActiveWeapon();
	if (Weapon != ActiveWeapon.Get())
	{
		// Weapon swap: resync without treating the change as a shot.
		ActiveWeapon = Weapon;
		bHasAmmo = false;
		bReloading = false;
	}

	if (!Weapon)
	{
		bReloading = false;
		return;
	}

	// Poll as a safety net (covers delegates that fired before we bound).
	ApplyAmmo(Weapon->GetMagazineSize(), Weapon->GetBulletCount());
	Reserve = Weapon->GetReserveAmmo();

	const UWorld* World = GetWorld();
	const float GameNow = World ? World->GetTimeSeconds() : Now;

	const bool bNowReloading = Weapon->IsReloading();
	if (bNowReloading && !bReloading)
	{
		ReloadStartGameTime = GameNow;
		ReloadDuration = ReadReloadDuration(Weapon);
	}
	bReloading = bNowReloading;
	ReloadProgress = bReloading ? FMath::Clamp((GameNow - ReloadStartGameTime) / FMath::Max(ReloadDuration, 0.01f), 0.0f, 1.0f) : 0.0f;
}

void UShooterHUDWidget::TickCrosshair(float Dt)
{
	// Read the character's ADS spring directly so the crosshair fades in lockstep with the viewmodel/FOV.
	if (const AShooterCharacter* AimChar = BoundCharacter.Get())
	{
		ADSAlpha = FMath::Clamp(AimChar->GetADSAlpha(), 0.0f, 1.0f);
		ADSVelocity = 0.0f;
	}
	else
	{
		ADSAlpha = FMath::Clamp(SmoothDamp(ADSAlpha, bAimTarget ? 1.0f : 0.0f, ADSVelocity, ADSSmoothTime, Dt), 0.0f, 1.0f);
	}

	Bloom = ExpDecay(Bloom, BloomRecoveryRate, Dt);

	float SpeedRatio = 0.0f;
	bool bAirborne = false;
	if (const UDoomMovementComponent* Move = Movement.Get())
	{
		SpeedRatio = Move->GetSpeedRatio();
		bAirborne = Move->IsFalling() && !Move->IsWallRunning();
	}

	float TargetGap = CrosshairBaseGap + Bloom * BloomGap + SpeedRatio * SpeedGap + (bAirborne ? AirborneGap : 0.0f);
	TargetGap *= FMath::Lerp(1.0f, ADSGapScale, ADSAlpha);

	// Under-damped spring for a slight kick; substep for stability at low frame rates.
	const float Omega = FMath::Sqrt(FMath::Max(GapSpringStiffness, 1.0f));
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dt / 0.008f), 1, 12);
	const float H = Dt / Steps;
	for (int32 i = 0; i < Steps; ++i)
	{
		const float Accel = GapSpringStiffness * (TargetGap - Gap) - 2.0f * GapSpringDamping * Omega * GapVelocity;
		GapVelocity += Accel * H;
		Gap += GapVelocity * H;
	}
	Gap = FMath::Max(Gap, 0.0f);
}

void UShooterHUDWidget::TickVitals(float Dt)
{
	const AShooterCharacter* Character = BoundCharacter.Get();
	const UCarnageAttributeSet* Attr = Character ? Character->GetAttributeSet() : nullptr;

	float NewHealth = LastLifePercent;
	float NewArmor = 0.0f;
	if (Attr)
	{
		const float MaxH = Attr->GetMaxHealth();
		NewHealth = MaxH > 0.0f ? Attr->GetHealth() / MaxH : LastLifePercent;
		HealthValue = Attr->GetHealth();
		const float MaxA = Attr->GetMaxArmor();
		bHasArmor = MaxA > 0.0f;
		ArmorValue = Attr->GetArmor();
		NewArmor = bHasArmor ? Attr->GetArmor() / MaxA : 0.0f;
	}
	NewHealth = FMath::Clamp(NewHealth, 0.0f, 1.0f);
	NewArmor = FMath::Clamp(NewArmor, 0.0f, 1.0f);

	auto UpdateTrail = [this, Dt](float NewValue, float& Value, float& Trail, float& HoldUntil)
	{
		if (NewValue < Value - KINDA_SMALL_NUMBER)
		{
			HoldUntil = Now + DamageTrailHold;
		}
		Value = NewValue;
		if (Trail < Value)
		{
			Trail = Value;
		}
		else if (Now >= HoldUntil)
		{
			Trail = FMath::Max(Value, Trail - DamageTrailDrainRate * Dt);
		}
	};

	UpdateTrail(NewHealth, HealthFrac, HealthTrail, HealthTrailHoldUntil);
	UpdateTrail(NewArmor, ArmorFrac, ArmorTrail, ArmorTrailHoldUntil);

	if (const UDoomMovementComponent* Move = Movement.Get())
	{
		MaxDashCharges = Move->MaxDashCharges;
		const int32 NewCharges = Move->GetDashCharges();
		if (NewCharges != DashCharges)
		{
			bDashGained = NewCharges > DashCharges;
			DashChangedPip = bDashGained ? NewCharges - 1 : NewCharges;
			DashChangeTime = Now;
			DashCharges = NewCharges;
		}
	}
}

void UShooterHUDWidget::TickRank(float Dt)
{
	const UStyleMeterComponent* Meter = StyleMeter.Get();
	if (!Meter)
	{
		bHasRank = false;
		return;
	}

	const int32 NewRank = static_cast<int32>(Meter->GetRank());
	if (bHasRank && NewRank != RankIndex)
	{
		bRankUp = NewRank > RankIndex;
		RankChangeTime = Now;
		// Snap the bar so it reads correctly for the new rank.
		RankProgress = bRankUp ? 0.0f : 1.0f;
	}
	RankIndex = NewRank;
	RankLetter = Meter->GetRankLetter();

	const float TargetProgress = FMath::Clamp(Meter->GetProgressInRank(), 0.0f, 1.0f);
	RankProgress = FMath::FInterpTo(RankProgress, TargetProgress, Dt, 12.0f);
	if (!bHasRank)
	{
		RankProgress = TargetProgress;
		bHasRank = true;
	}
}

void UShooterHUDWidget::TickDamageFeedback(float Dt)
{
	VignetteAlpha = ExpDecay(VignetteAlpha, VignetteDecayRate, Dt);

	// The bodycam edge flash reads the same markers, so keep them for whichever look lasts longer.
	const float IndicatorLife = FMath::Max(DamageIndicatorDuration, BodycamDamageFlashDuration);
	DamageIndicators.RemoveAll([this, IndicatorLife](const FShooterHUDDamageIndicator& I) { return (Now - I.SpawnTime) > IndicatorLife; });
	DamageNumbers.RemoveAll([this](const FShooterHUDDamageNumber& N) { return (Now - N.SpawnTime) > DamageNumberLifetime; });

	if (DamageIndicators.Num() == 0)
	{
		return;
	}

	const APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FRotator YawOnly(0.0f, CamRot.Yaw, 0.0f);
	const FVector Fwd = YawOnly.Vector();
	const FVector Right = FRotationMatrix(YawOnly).GetScaledAxis(EAxis::Y);

	for (FShooterHUDDamageIndicator& I : DamageIndicators)
	{
		const FVector Dir = (I.WorldSource - CamLoc).GetSafeNormal2D();
		if (!Dir.IsNearlyZero())
		{
			I.ScreenAngle = FMath::Atan2(FVector::DotProduct(Dir, Right), FVector::DotProduct(Dir, Fwd));
		}
	}
}

FLinearColor UShooterHUDWidget::GetRankColor(int32 InRankIndex) const
{
	if (RankColors.Num() == 0)
	{
		return ColorAccent;
	}
	return RankColors[FMath::Clamp(InRankIndex, 0, RankColors.Num() - 1)];
}

// ─────────────────────────────────────────────────────────────────────────────
// Paint
// ─────────────────────────────────────────────────────────────────────────────

int32 UShooterHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (!BoundCharacter.IsValid())
	{
		return MaxLayer;
	}

	const FPainter P{ AllottedGeometry, OutDrawElements, MaxLayer + 1, &SolidBrush,
		HUDOpacity * InWidgetStyle.GetColorAndOpacityTint().A, FVector2f(AllottedGeometry.GetLocalSize()) };

	// Crossfade: the game HUD and the bodycam OSD are two complete layouts that read the same state.
	// The game layout is painted with opacity (1 - alpha) and the OSD with opacity alpha, where alpha
	// is UBodycamComponent's fade. Toggling therefore morphs smoothly between them, and at alpha 0 the
	// game HUD paints exactly as it does without a bodycam (the OSD isn't drawn at all).
	// The OSD is Slate, drawn over the scene, so the lens material never warps it.
	FPainter Game = P;
	Game.Opacity *= 1.0f - BodycamAlpha;
	FPainter Cam = P;
	Cam.Opacity *= BodycamAlpha;

	// Layers: +0..1 vignette/backplates, +2..3 fills, +4..5 text, +6.. crosshair/markers, +10.. bodycam OSD.
	if (Game.Opacity > 0.001f)
	{
		PaintVignette(Game);
		PaintVitals(Game);
		PaintAmmo(Game);
		PaintRank(Game);
		PaintDamageIndicators(Game);
		PaintCrosshair(Game);
		PaintHitmarker(Game);
	}
	if (Cam.Opacity > 0.001f)
	{
		PaintBodycamDamage(Cam);
		PaintBodycamHitmarker(Cam);
		PaintBodycamOverlay(Cam);
	}

	return MaxLayer + 12;
}

void UShooterHUDWidget::PaintVignette(const FPainter& P) const
{
	float Alpha = VignetteAlpha;
	if (HealthFrac > 0.0f && HealthFrac < LowHealthThreshold)
	{
		const float Severity = 1.0f - HealthFrac / FMath::Max(LowHealthThreshold, 0.01f);
		Alpha = FMath::Max(Alpha, (0.18f + 0.22f * Severity) * (0.65f + 0.35f * FMath::Sin(Now * 6.0f)));
	}
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	if (Alpha <= 0.01f)
	{
		return;
	}

	// Cheap edge gradient: stacked strips, quadratic falloff toward the center.
	const float Thick = FMath::Min(P.Size.X, P.Size.Y) * VignetteSize;
	constexpr int32 Strips = 12;
	const float StripW = Thick / Strips;
	for (int32 i = 0; i < Strips; ++i)
	{
		const float T = 1.0f - static_cast<float>(i) / Strips;
		const FLinearColor C = WithAlpha(ColorDanger, Alpha * 0.32f * T * T);
		const float O = i * StripW;
		P.Rect(FVector2f(O, O), FVector2f(P.Size.X - 2.0f * O, StripW), C);                                  // top
		P.Rect(FVector2f(O, P.Size.Y - O - StripW), FVector2f(P.Size.X - 2.0f * O, StripW), C);              // bottom
		P.Rect(FVector2f(O, O + StripW), FVector2f(StripW, P.Size.Y - 2.0f * (O + StripW)), C);              // left
		P.Rect(FVector2f(P.Size.X - O - StripW, O + StripW), FVector2f(StripW, P.Size.Y - 2.0f * (O + StripW)), C); // right
	}
}

void UShooterHUDWidget::PaintCrosshair(const FPainter& P) const
{
	const FVector2f C = P.Size * 0.5f;
	const float TickAlpha = 1.0f - ADSAlpha;
	const float DotAlpha = FMath::Lerp(1.0f, ADSDotAlpha, ADSAlpha);

	const bool bEmpty = bHasAmmo && MagSize > 0 && Bullets == 0;
	const FLinearColor Base = bEmpty ? ColorDanger : ColorPrimary;
	const float Len = CrosshairTickLength * FMath::Lerp(1.0f, 0.6f, ADSAlpha);
	const float Th = CrosshairTickThickness;
	const float O = 1.0f; // outline

	if (TickAlpha > 0.01f)
	{
		const FLinearColor Col = WithAlpha(Base, TickAlpha);
		const FLinearColor Sh = WithAlpha(ColorShadow, TickAlpha);
		const float R = Gap + Len * 0.5f;

		const FVector2f Vert(Th, Len);
		const FVector2f Horz(Len, Th);
		const FVector2f Centers[4] = { C + FVector2f(0, -R), C + FVector2f(0, R), C + FVector2f(-R, 0), C + FVector2f(R, 0) };
		const FVector2f Sizes[4] = { Vert, Vert, Horz, Horz };
		for (int32 i = 0; i < 4; ++i)
		{
			P.RectCentered(Centers[i], Sizes[i] + FVector2f(2 * O, 2 * O), Sh, 0.0f, 0.0f, 6);
			P.RectCentered(Centers[i], Sizes[i], Col, 0.0f, 0.0f, 7);
		}
	}

	if (DotAlpha > 0.01f)
	{
		const FVector2f D(CrosshairDotSize, CrosshairDotSize);
		P.RectCentered(C, D + FVector2f(2 * O, 2 * O), WithAlpha(ColorShadow, DotAlpha), 0.0f, 0.0f, 6);
		P.RectCentered(C, D, WithAlpha(Base, DotAlpha), 0.0f, 0.0f, 7);
	}
}

void UShooterHUDWidget::PaintHitmarker(const FPainter& P) const
{
	const FVector2f C = P.Size * 0.5f;

	const float Duration = HitKind == 2 ? KillHitmarkerDuration : HitmarkerDuration;
	const float Age = Now - HitTime;
	if (Age >= 0.0f && Age < Duration)
	{
		const float T = Age / Duration;
		const float Alpha = 1.0f - T * T;
		const float Punch = 1.0f + HitmarkerPunch * FMath::Exp(-Age * 28.0f);
		const float Stack = 1.0f + HitmarkerStackGrowth * HitStack;
		const float KindScale = HitKind == 2 ? KillHitmarkerScale : 1.0f;
		const float S = Punch * Stack * KindScale;

		const FLinearColor Col = HitKind == 2 ? ColorDanger : (HitKind == 1 ? ColorHeadshot : FLinearColor::White);
		const float Len = HitmarkerLength * S;
		const float Th = HitmarkerThickness * (HitKind == 2 ? 1.3f : 1.0f);
		const float Inner = HitmarkerInnerRadius * FMath::Lerp(1.0f, 1.35f, T) * FMath::Sqrt(S);
		const float Dist = Inner + Len * 0.5f;

		static const FVector2f Diag[4] = { FVector2f(1, -1), FVector2f(1, 1), FVector2f(-1, 1), FVector2f(-1, -1) };
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector2f Dir = Diag[i] * UE_INV_SQRT_2;
			const FVector2f Center = C + Dir * Dist;
			const float Angle = (i % 2 == 0) ? 45.0f : -45.0f;
			P.RectCentered(Center, FVector2f(Th + 2.0f, Len + 2.0f), WithAlpha(ColorShadow, Alpha), 0.0f, Angle, 8);
			P.RectCentered(Center, FVector2f(Th, Len), WithAlpha(Col, Alpha), 0.0f, Angle, 9);
		}
	}

	if (bShowDamageNumbers && DamageNumbers.Num() > 0)
	{
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("BoldCondensed", DamageNumberFontSize);
		for (const FShooterHUDDamageNumber& N : DamageNumbers)
		{
			const float T = FMath::Clamp((Now - N.SpawnTime) / FMath::Max(DamageNumberLifetime, 0.01f), 0.0f, 1.0f);
			const float Alpha = 1.0f - FMath::Pow(T, 3.0f);
			const float Rise = DamageNumberRise * (1.0f - FMath::Square(1.0f - T));
			const float Scale = 1.0f + 0.35f * FMath::Exp(-(Now - N.SpawnTime) * 20.0f);
			const FString Str = FString::FromInt(FMath::RoundToInt(N.Value));
			P.Text(Str, Font, C + N.Offset - FVector2f(0.0f, Rise), FVector2f(0.5f, 0.5f), WithAlpha(N.Color, Alpha * 0.85f), Scale, &ColorShadow, 8);
		}
	}
}

void UShooterHUDWidget::PaintDamageIndicators(const FPainter& P) const
{
	if (DamageIndicators.Num() == 0)
	{
		return;
	}

	const FVector2f C = P.Size * 0.5f;
	const float Half = FMath::DegreesToRadians(DamageIndicatorArcHalfAngle);
	constexpr int32 Segs = 14;

	for (const FShooterHUDDamageIndicator& I : DamageIndicators)
	{
		const float Age = Now - I.SpawnTime;
		const float T = FMath::Clamp(Age / FMath::Max(DamageIndicatorDuration, 0.01f), 0.0f, 1.0f);
		const float Alpha = (1.0f - T) * (1.0f - T);
		// Starts slightly further out and settles in.
		const float R = DamageIndicatorRadius + 18.0f * FMath::Exp(-Age * 14.0f);

		TArray<FVector2f> Arc;
		Arc.Reserve(Segs + 1);
		for (int32 s = 0; s <= Segs; ++s)
		{
			const float A = I.ScreenAngle - Half + 2.0f * Half * s / Segs;
			Arc.Add(Polar(C, A, R));
		}
		P.Lines(Arc, WithAlpha(ColorShadow, Alpha * 0.8f), DamageIndicatorThickness + 3.0f, 6);
		P.Lines(Arc, WithAlpha(ColorDanger, Alpha), DamageIndicatorThickness, 7);

		// Outward chevron at the arc center.
		const float Tip = R + 20.0f;
		const float Base = R + 7.0f;
		const float Spread = Half * 0.45f;
		TArray<FVector2f> Chev = { Polar(C, I.ScreenAngle - Spread, Base), Polar(C, I.ScreenAngle, Tip), Polar(C, I.ScreenAngle + Spread, Base) };
		P.Lines(Chev, WithAlpha(ColorShadow, Alpha * 0.8f), 6.0f, 6);
		P.Lines(Chev, WithAlpha(ColorDanger, Alpha), 3.5f, 7);
	}
}

void UShooterHUDWidget::PaintVitals(const FPainter& P) const
{
	const FVector2f PlateSize(VitalsBarWidth + 130.0f, 104.0f);
	const FVector2f Origin(ScreenMargin.X, P.Size.Y - ScreenMargin.Y - PlateSize.Y); // plate top-left

	const bool bLow = HealthFrac < LowHealthThreshold;
	const float LowPulse = bLow ? 0.5f + 0.5f * FMath::Sin(Now * 9.0f) : 0.0f;

	// Backplate + accent edge.
	P.Rect(Origin, PlateSize, ColorBackplate, PanelSkew, 0.0f, 0);
	P.Rect(Origin + FVector2f(0.0f, PlateSize.Y - 3.0f), FVector2f(PlateSize.X * 0.45f, 3.0f), ColorAccent, 0.0f, 0.0f, 1);
	P.Rect(Origin + FVector2f(-12.0f, 0.0f), FVector2f(6.0f, PlateSize.Y), bLow ? FMath::Lerp(ColorAccent, ColorDanger, LowPulse) : ColorAccent, PanelSkew, 0.0f, 1);

	// Health number.
	const FSlateFontInfo BigFont = FCoreStyle::GetDefaultFontStyle("Black", 34);
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("BoldCondensed", 11);
	const FLinearColor HealthCol = bLow ? FMath::Lerp(ColorPrimary, ColorDanger, 0.6f + 0.4f * LowPulse) : ColorPrimary;
	const FString HealthStr = FString::FromInt(FMath::CeilToInt(FMath::Max(HealthValue, 0.0f)));
	P.Text(HealthStr, BigFont, Origin + FVector2f(98.0f, 8.0f), FVector2f(1.0f, 0.0f), HealthCol, 1.0f, &ColorShadow, 4);
	P.Text(TEXT("HEALTH"), LabelFont, Origin + FVector2f(98.0f, 54.0f), FVector2f(1.0f, 0.0f), WithAlpha(ColorPrimary, 0.55f), 1.0f, nullptr, 4);

	// Segmented health bar with trailing chunk.
	const float BarX = Origin.X + 114.0f;
	const float HealthY = Origin.Y + 16.0f;
	const int32 N = FMath::Max(HealthSegments, 1);
	constexpr float SegGap = 3.0f;
	const float SegW = (VitalsBarWidth - SegGap * (N - 1)) / N;
	const FLinearColor FillCol = bLow ? FMath::Lerp(ColorDanger, ColorPrimary, 0.25f * LowPulse) : ColorPrimary;
	for (int32 i = 0; i < N; ++i)
	{
		const float X = BarX + i * (SegW + SegGap);
		const float SegStart = static_cast<float>(i) / N;
		const float Fill = FMath::Clamp((HealthFrac - SegStart) * N, 0.0f, 1.0f);
		const float Trail = FMath::Clamp((HealthTrail - SegStart) * N, 0.0f, 1.0f);

		P.Rect(FVector2f(X, HealthY), FVector2f(SegW, HealthBarHeight), ColorEmptySlot, PanelSkew, 0.0f, 1);
		if (Trail > Fill)
		{
			P.Rect(FVector2f(X, HealthY), FVector2f(SegW * Trail, HealthBarHeight), FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), PanelSkew, 0.0f, 2);
		}
		P.Rect(FVector2f(X, HealthY), FVector2f(SegW * Fill, HealthBarHeight), FillCol, PanelSkew, 0.0f, 3);
	}

	// Armor bar (continuous) under health.
	const float ArmorY = HealthY + HealthBarHeight + 7.0f;
	if (bHasArmor)
	{
		P.Rect(FVector2f(BarX, ArmorY), FVector2f(VitalsBarWidth, ArmorBarHeight), ColorEmptySlot, PanelSkew, 0.0f, 1);
		if (ArmorTrail > ArmorFrac)
		{
			P.Rect(FVector2f(BarX, ArmorY), FVector2f(VitalsBarWidth * ArmorTrail, ArmorBarHeight), FLinearColor(1.0f, 1.0f, 1.0f, 0.85f), PanelSkew, 0.0f, 2);
		}
		P.Rect(FVector2f(BarX, ArmorY), FVector2f(VitalsBarWidth * ArmorFrac, ArmorBarHeight), ColorArmor, PanelSkew, 0.0f, 3);
	}

	// Dash pips.
	const float PipY = ArmorY + ArmorBarHeight + 16.0f;
	P.Text(TEXT("DASH"), LabelFont, FVector2f(BarX, PipY + DashPipSize.Y * 0.5f), FVector2f(0.0f, 0.5f), WithAlpha(ColorPrimary, 0.55f), 1.0f, nullptr, 4);
	const float PipX0 = BarX + 40.0f;
	for (int32 i = 0; i < MaxDashCharges; ++i)
	{
		const FVector2f PipPos(PipX0 + i * (DashPipSize.X + 6.0f), PipY);
		const bool bFilled = i < DashCharges;
		const float Since = Now - DashChangeTime;
		const float Flash = (i == DashChangedPip) ? FMath::Exp(-Since * 7.0f) : 0.0f;

		P.Rect(PipPos, DashPipSize, ColorEmptySlot, PanelSkew * 2.0f, 0.0f, 1);
		if (bFilled)
		{
			P.Rect(PipPos, DashPipSize, FMath::Lerp(ColorAccent, FLinearColor::White, Flash), PanelSkew * 2.0f, 0.0f, 3);
			if (Flash > 0.01f && bDashGained)
			{
				const float Grow = 1.0f + 0.6f * (1.0f - Flash);
				P.RectCentered(PipPos + DashPipSize * 0.5f, DashPipSize * Grow, WithAlpha(FLinearColor::White, 0.5f * Flash), PanelSkew * 2.0f, 0.0f, 2);
			}
		}
		else if (Flash > 0.01f && !bDashGained)
		{
			P.Rect(PipPos, DashPipSize, WithAlpha(ColorAccent, Flash), PanelSkew * 2.0f, 0.0f, 3);
		}
	}
}

void UShooterHUDWidget::PaintAmmo(const FPainter& P) const
{
	if (!bHasAmmo || MagSize <= 0)
	{
		return;
	}

	const FVector2f PlateSize(250.0f, 104.0f);
	const FVector2f BR(P.Size.X - ScreenMargin.X, P.Size.Y - ScreenMargin.Y);
	const FVector2f Origin = BR - PlateSize;

	const bool bEmpty = Bullets <= 0;
	const bool bLow = Bullets <= FMath::CeilToInt(MagSize * LowAmmoFraction);
	const float Blink = 0.5f + 0.5f * FMath::Sin(Now * 10.0f);

	P.Rect(Origin, PlateSize, ColorBackplate, PanelSkew, 0.0f, 0);
	P.Rect(Origin + FVector2f(PlateSize.X * 0.55f, PlateSize.Y - 3.0f), FVector2f(PlateSize.X * 0.45f, 3.0f), ColorAccent, 0.0f, 0.0f, 1);
	P.Rect(Origin + FVector2f(PlateSize.X + 6.0f, 0.0f), FVector2f(6.0f, PlateSize.Y), bLow ? ColorDanger : ColorAccent, PanelSkew, 0.0f, 1);

	// "/ reserve" on the right, magazine count right-aligned against it.
	const FSlateFontInfo MagFont = FCoreStyle::GetDefaultFontStyle("Black", MagazineFontSize);
	const FSlateFontInfo ResFont = FCoreStyle::GetDefaultFontStyle("BoldCondensed", ReserveFontSize);
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("BoldCondensed", 11);

	const float Baseline = Origin.Y + 76.0f;
	const FString ResStr = FString::Printf(TEXT("/ %d"), Reserve);
	const FVector2f ResSize = P.Text(ResStr, ResFont, FVector2f(BR.X - 18.0f, Baseline), FVector2f(1.0f, 1.0f),
		WithAlpha(Reserve > 0 ? ColorPrimary : ColorDanger, 0.7f), 1.0f, &ColorShadow, 4);

	const FLinearColor MagCol = bLow ? (bEmpty ? WithAlpha(ColorDanger, 0.55f + 0.45f * Blink) : ColorDanger) : ColorPrimary;
	const float MagScale = 1.0f + AmmoShotPunch * AmmoPunch;
	// Anchor at the bottom-right of the number so the punch grows up and left.
	P.Text(FString::FromInt(Bullets), MagFont, FVector2f(BR.X - 26.0f - ResSize.X, Baseline + 8.0f), FVector2f(1.0f, 1.0f), MagCol, MagScale, &ColorShadow, 4);

	// Reload bar / prompt.
	const float BarY = Origin.Y + PlateSize.Y - 18.0f;
	const float BarW = PlateSize.X - 48.0f;
	const float BarX = Origin.X + 30.0f;
	if (bReloading)
	{
		P.Rect(FVector2f(BarX, BarY), FVector2f(BarW, 5.0f), ColorEmptySlot, PanelSkew, 0.0f, 1);
		P.Rect(FVector2f(BarX, BarY), FVector2f(BarW * ReloadProgress, 5.0f), ColorAccent, PanelSkew, 0.0f, 3);
		P.Text(TEXT("RELOADING"), LabelFont, FVector2f(BarX, Origin.Y + 10.0f), FVector2f(0.0f, 0.0f), WithAlpha(ColorAccent, 0.9f), 1.0f, nullptr, 4);
	}
	else if (bEmpty)
	{
		const FSlateFontInfo PromptFont = FCoreStyle::GetDefaultFontStyle("Black", 16);
		const FString Prompt = Reserve > 0 ? TEXT("RELOAD") : TEXT("NO AMMO");
		const FVector2f PromptAnchor(BR.X - 4.0f, Origin.Y - 8.0f);
		const FVector2f PromptSize = FPainter::Measure(Prompt, PromptFont);
		P.Rect(PromptAnchor - FVector2f(PromptSize.X + 18.0f, PromptSize.Y + 4.0f), PromptSize + FVector2f(18.0f, 4.0f),
			WithAlpha(ColorDanger, 0.35f + 0.5f * Blink), PanelSkew, 0.0f, 1);
		P.Text(Prompt, PromptFont, PromptAnchor - FVector2f(9.0f, 2.0f), FVector2f(1.0f, 1.0f), FLinearColor::White, 1.0f, nullptr, 4);
	}
	else
	{
		P.Text(TEXT("AMMO"), LabelFont, FVector2f(BarX, Origin.Y + 10.0f), FVector2f(0.0f, 0.0f), WithAlpha(ColorPrimary, 0.55f), 1.0f, nullptr, 4);
		// Magazine pips along the bottom edge.
		const int32 Pips = FMath::Min(MagSize, 40);
		const float PipGap = 2.0f;
		const float PipW = (BarW - PipGap * (Pips - 1)) / FMath::Max(Pips, 1);
		for (int32 i = 0; i < Pips; ++i)
		{
			const bool bLoaded = i < Bullets;
			P.Rect(FVector2f(BarX + i * (PipW + PipGap), BarY), FVector2f(PipW, 5.0f),
				bLoaded ? (bLow ? ColorDanger : WithAlpha(ColorPrimary, 0.8f)) : ColorEmptySlot, PanelSkew, 0.0f, 2);
		}
	}
}

void UShooterHUDWidget::PaintRank(const FPainter& P) const
{
	if (!bHasRank)
	{
		return;
	}

	const FVector2f PanelSize(RankBarWidth + 40.0f, 132.0f);
	const FVector2f TR(P.Size.X - ScreenMargin.X, P.Size.Y * RankPanelHeightFraction);
	const FVector2f Origin(TR.X - PanelSize.X, TR.Y - PanelSize.Y * 0.5f);

	const float Since = Now - RankChangeTime;
	const float PunchT = FMath::Exp(-Since * 9.0f);
	const float Flash = bRankUp ? FMath::Exp(-Since * 7.0f) : 0.0f;
	const bool bIdle = RankIndex == 0 && RankProgress < 0.01f && Since > 1.0f;
	const float PanelAlpha = bIdle ? RankIdleAlpha : 1.0f;

	FLinearColor RankCol = GetRankColor(RankIndex);
	// SSS flickers between its color and white.
	if (RankColors.Num() > 0 && RankIndex >= RankColors.Num() - 1)
	{
		RankCol = FMath::Lerp(RankCol, FLinearColor::White, 0.25f * (0.5f + 0.5f * FMath::Sin(Now * 22.0f)));
	}
	const FLinearColor LetterCol = WithAlpha(FMath::Lerp(RankCol, FLinearColor::White, Flash), PanelAlpha);

	P.Rect(Origin, PanelSize, WithAlpha(ColorBackplate, PanelAlpha), PanelSkew, 0.0f, 0);
	P.Rect(FVector2f(TR.X + 6.0f, Origin.Y), FVector2f(6.0f, PanelSize.Y), WithAlpha(RankCol, PanelAlpha), PanelSkew, 0.0f, 1);

	// Rank-up flash: a bright slab that fades out behind the letter.
	if (Flash > 0.01f)
	{
		const float Grow = 1.0f + 0.25f * (1.0f - Flash);
		P.RectCentered(Origin + PanelSize * 0.5f, PanelSize * Grow, WithAlpha(RankCol, 0.45f * Flash), PanelSkew, 0.0f, 1);
	}

	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("BoldCondensed", 11);
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("BoldCondensed", 14);
	const FSlateFontInfo LetterFont = FCoreStyle::GetDefaultFontStyle("Black", RankFontSize);

	P.Text(TEXT("STYLE"), LabelFont, FVector2f(TR.X - 4.0f, Origin.Y - 6.0f), FVector2f(1.0f, 1.0f), WithAlpha(ColorPrimary, 0.55f * PanelAlpha), 1.0f, nullptr, 4);

	const float Punch = 1.0f + (bRankUp ? RankUpPunch : -0.15f) * PunchT;
	// Small horizontal shake on rank-down.
	const float Shake = bRankUp ? 0.0f : 6.0f * PunchT * FMath::Sin(Since * 70.0f);
	const FVector2f LetterAnchor(TR.X - 14.0f + Shake, Origin.Y + 92.0f);
	P.Text(RankLetter, LetterFont, LetterAnchor, FVector2f(1.0f, 1.0f), LetterCol, Punch, &ColorShadow, 4);

	const FString Title = RankTitles.IsValidIndex(RankIndex) ? RankTitles[RankIndex] : FString();
	P.Text(Title, TitleFont, FVector2f(TR.X - 14.0f, Origin.Y + 94.0f), FVector2f(1.0f, 0.0f), WithAlpha(RankCol, 0.85f * PanelAlpha), 1.0f, &ColorShadow, 4);

	// Progress to next rank.
	const FVector2f BarPos(TR.X - 14.0f - RankBarWidth, Origin.Y + PanelSize.Y - 12.0f);
	P.Rect(BarPos, FVector2f(RankBarWidth, 5.0f), WithAlpha(ColorEmptySlot, PanelAlpha), PanelSkew, 0.0f, 1);
	P.Rect(BarPos, FVector2f(RankBarWidth * RankProgress, 5.0f), WithAlpha(RankCol, PanelAlpha), PanelSkew, 0.0f, 3);
}


// ─────────────────────────────────────────────────────────────────────────────
// Bodycam OSD
//
// Burned-in body-worn-camera metadata: small outlined monospace fields in the corners, inset by
// BodycamEdgeInset, instead of panels. Every function here is painted with opacity = bodycam
// blend alpha (see NativePaint), mirroring the game layout's (1 - alpha).
// ─────────────────────────────────────────────────────────────────────────────

void UShooterHUDWidget::PaintBodycamOverlay(const FPainter& P) const
{
	const FVector2f Inset(FMath::RoundToFloat(P.Size.X * BodycamEdgeInset), FMath::RoundToFloat(P.Size.Y * BodycamEdgeInset));
	const FSlateFontInfo Font = MakeOSDFont(P.Size.Y, BodycamTextHeight, P.Opacity);
	const FVector2f CharSize = FPainter::Measure(TEXT("0"), Font);
	const float LineH = CharSize.Y;
	const float RowStep = FMath::RoundToFloat(LineH * BodycamRowSpacing);
	constexpr int32 Layer = 10;

	// Off-white text that eases toward the alert color at low health, with a faint brightness flicker.
	auto Flicker = [this](FLinearColor C)
	{
		C.R *= OSDFlicker;
		C.G *= OSDFlicker;
		C.B *= OSDFlicker;
		return C;
	};
	const FLinearColor TextCol = Flicker(FMath::Lerp(BodycamTextColor, BodycamAlertColor, LowHealthBlend));
	const FLinearColor AlertCol = Flicker(BodycamAlertColor);
	const float Blink = FMath::Fmod(Now * 2.0f, 1.0f) < 0.6f ? 1.0f : 0.3f;

	// Rare single-frame horizontal tear of one row (or all of them).
	const bool bGlitchFrame = GFrameCounter == GlitchFrame;
	auto RowX = [&](int32 Row, float X)
	{
		return (bGlitchFrame && (GlitchRow == INDEX_NONE || GlitchRow == Row)) ? X + GlitchOffset : X;
	};
	auto Row = [&](int32 RowId, const FString& Str, float X, float Y, float AlignX, const FLinearColor& Col)
	{
		return P.Text(Str, Font, FVector2f(RowX(RowId, X), Y), FVector2f(AlignX, 0.0f), Col, 1.0f, nullptr, Layer);
	};

	const float Left = Inset.X;
	const float Right = P.Size.X - Inset.X;
	const float Top = Inset.Y;
	const float Bottom = P.Size.Y - Inset.Y - LineH; // top of the lowest row

	// ── Top-left: REC dot (blinks faster at low health) + STYLE tag ─────
	const float DotSize = FMath::Min(12.0f, FMath::RoundToFloat(LineH * 0.6f));
	const FVector2f DotTopLeft(RowX(OSDRec, Left), Top + FMath::RoundToFloat((LineH - DotSize) * 0.5f));
	if (RecPhase < 0.5f)
	{
		const FVector2f Ring(1.0f, 1.0f);
		FSlateDrawElement::MakeBox(P.Out, P.BaseLayer + Layer, P.Geo.ToPaintGeometry(FVector2f(DotSize + 2.0f, DotSize + 2.0f), FSlateLayoutTransform(1.0f, DotTopLeft - Ring)),
			&DotBrush, ESlateDrawEffect::None, P.Tint(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f)));
		FSlateDrawElement::MakeBox(P.Out, P.BaseLayer + Layer + 1, P.Geo.ToPaintGeometry(FVector2f(DotSize, DotSize), FSlateLayoutTransform(1.0f, DotTopLeft)),
			&DotBrush, ESlateDrawEffect::None, P.Tint(BodycamRecColor));
	}
	Row(OSDRec, TEXT("REC"), Left + DotSize + FMath::RoundToFloat(CharSize.X * 0.6f), Top, 0.0f, TextCol);

	if (bHasRank)
	{
		const FString Title = RankTitles.IsValidIndex(RankIndex) ? RankTitles[RankIndex] : FString();
		const FString Tag = Title.IsEmpty() ? FString::Printf(TEXT("STYLE %s"), *RankLetter) : FString::Printf(TEXT("STYLE %s // %s"), *RankLetter, *Title);
		const float Y = Top + RowStep;
		// On a rank change the tag blinks inverted (dark text on a light slab) a few times.
		const float Since = Now - RankChangeTime;
		if (Since < BodycamRankFlashDuration && FMath::Fmod(Since, 0.2f) < 0.12f)
		{
			const float X = RowX(OSDStyle, Left);
			const float Pad = FMath::RoundToFloat(CharSize.X * 0.3f);
			const FVector2f TagSize = FPainter::Measure(Tag, Font);
			P.Rect(FVector2f(X - Pad, Y), FVector2f(TagSize.X + 2.0f * Pad, LineH), TextCol, 0.0f, 0.0f, Layer);
			FSlateFontInfo Plain = Font;
			Plain.OutlineSettings = FFontOutlineSettings::NoOutline;
			P.Text(Tag, Plain, FVector2f(X, Y), FVector2f(0.0f, 0.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.9f), 1.0f, nullptr, Layer);
		}
		else
		{
			Row(OSDStyle, Tag, Left, Y, 0.0f, TextCol);
		}
	}

	// ── Top-right: wall clock + heading / speed ─────────────────────────
	Row(OSDClock, FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")), Right, Top, 1.0f, TextCol);
	if (bBodycamShowHeading)
	{
		const FString Hdg = FString::Printf(TEXT("HDG %s  SPD %s m/s"),
			*ZeroPad(FMath::RoundToInt(HeadingDeg) % 360, 3), *ZeroPad(FMath::RoundToInt(SpeedMetersPerSec), 2));
		Row(OSDHeading, Hdg, Right, Top + RowStep, 1.0f, TextCol);
	}

	// ── Bottom-left: vitals + dash charges ──────────────────────────────
	const FString Armor = bHasArmor ? ZeroPad(FMath::CeilToInt(ArmorValue), 3) : FString(TEXT("---"));
	Row(OSDVitals, FString::Printf(TEXT("HR %s  ARM %s"), *ZeroPad(FMath::CeilToInt(HealthValue), 3), *Armor), Left, Bottom - RowStep, 0.0f, TextCol);
	if (MaxDashCharges > 0)
	{
		const int32 Charges = FMath::Clamp(DashCharges, 0, MaxDashCharges);
		const FString Dash = FString::Printf(TEXT("DASH [%s%s]"), *FString::ChrN(Charges, TEXT('#')), *FString::ChrN(MaxDashCharges - Charges, TEXT('-')));
		Row(OSDDash, Dash, Left, Bottom, 0.0f, TextCol);
	}

	// ── Bottom-right: magazine + reload / empty status ──────────────────
	if (bHasAmmo && MagSize > 0)
	{
		const bool bEmpty = Bullets <= 0;
		const bool bLow = Bullets <= FMath::CeilToInt(MagSize * LowAmmoFraction);
		const int32 MagDigits = FMath::Max(2, FString::FromInt(MagSize).Len());
		const FString Mag = FString::Printf(TEXT("MAG %s / %s"), *ZeroPad(Bullets, MagDigits), *ZeroPad(Reserve, 3));
		Row(OSDMag, Mag, Right, Bottom, 1.0f, bLow ? AlertCol : TextCol);

		const float StatusY = Bottom - RowStep;
		if (bReloading)
		{
			// Steady gauge on the right, blinking label to its left.
			const FVector2f BarSize = Row(OSDStatus, TextBar(ReloadProgress, BodycamReloadBarChars), Right, StatusY, 1.0f, TextCol);
			Row(OSDStatus, TEXT("RELOADING"), Right - BarSize.X - CharSize.X, StatusY, 1.0f, WithAlpha(TextCol, Blink));
		}
		else if (bEmpty)
		{
			Row(OSDStatus, Reserve > 0 ? TEXT("RELOAD") : TEXT("NO AMMO"), Right, StatusY, 1.0f, WithAlpha(AlertCol, Blink));
		}
	}

	// ── Bottom center: device id + battery ──────────────────────────────
	const float Battery = FMath::Clamp(BodycamBatteryStart - BodycamBatteryDrainPerMinute * (Now / 60.0f), 1.0f, 100.0f);
	const bool bBatteryLow = Battery < 20.0f;
	const FString Percent = FString::Printf(TEXT("%d%%"), FMath::CeilToInt(Battery));

	const FVector2f IdSize = FPainter::Measure(BodycamDeviceId, Font);
	const FVector2f BattSize(FMath::RoundToFloat(LineH * 1.5f), FMath::RoundToFloat(LineH * 0.6f));
	constexpr float NubW = 3.0f;
	const float IdGap = FMath::RoundToFloat(CharSize.X * 2.0f);
	const FVector2f PctSize = FPainter::Measure(Percent, Font);
	const float TotalW = IdSize.X + IdGap + BattSize.X + NubW + 6.0f + PctSize.X;
	const float X0 = RowX(OSDDevice, FMath::RoundToFloat((P.Size.X - TotalW) * 0.5f));
	const float Y0 = Bottom;

	P.Text(BodycamDeviceId, Font, FVector2f(X0, Y0), FVector2f(0.0f, 0.0f), TextCol, 1.0f, nullptr, Layer);

	const FVector2f BattPos(X0 + IdSize.X + IdGap, FMath::RoundToFloat(Y0 + (LineH - BattSize.Y) * 0.5f));
	const FLinearColor Dark(0.0f, 0.0f, 0.0f, 0.75f);
	const FLinearColor BattCol = bBatteryLow ? BodycamRecColor : TextCol;
	constexpr float Th = 1.5f;
	// Dark backing doubles as the 1px outline.
	P.Rect(BattPos - FVector2f(1.0f, 1.0f), BattSize + FVector2f(NubW + 2.0f, 2.0f), Dark, 0.0f, 0.0f, Layer);
	P.Rect(BattPos, FVector2f(BattSize.X, Th), BattCol, 0.0f, 0.0f, Layer + 1);
	P.Rect(BattPos + FVector2f(0.0f, BattSize.Y - Th), FVector2f(BattSize.X, Th), BattCol, 0.0f, 0.0f, Layer + 1);
	P.Rect(BattPos, FVector2f(Th, BattSize.Y), BattCol, 0.0f, 0.0f, Layer + 1);
	P.Rect(BattPos + FVector2f(BattSize.X - Th, 0.0f), FVector2f(Th, BattSize.Y), BattCol, 0.0f, 0.0f, Layer + 1);
	P.Rect(BattPos + FVector2f(BattSize.X, BattSize.Y * 0.3f), FVector2f(NubW, BattSize.Y * 0.4f), BattCol, 0.0f, 0.0f, Layer + 1);
	const float Pad = Th + 1.5f;
	const FVector2f FillMax = BattSize - FVector2f(2.0f * Pad, 2.0f * Pad);
	P.Rect(BattPos + FVector2f(Pad, Pad), FVector2f(FillMax.X * Battery / 100.0f, FillMax.Y), BattCol, 0.0f, 0.0f, Layer + 1);

	P.Text(Percent, Font, FVector2f(BattPos.X + BattSize.X + NubW + 6.0f, Y0), FVector2f(0.0f, 0.0f), BattCol, 1.0f, nullptr, Layer);

	// ── Optional center dot (replaces the hidden crosshair) ─────────────
	if (bBodycamCenterDot)
	{
		const FVector2f C = P.Size * 0.5f;
		const FVector2f D(CrosshairDotSize, CrosshairDotSize);
		P.RectCentered(C, D + FVector2f(2.0f, 2.0f), ColorShadow, 0.0f, 0.0f, Layer);
		P.RectCentered(C, D, TextCol, 0.0f, 0.0f, Layer + 1);
	}
}

void UShooterHUDWidget::PaintBodycamHitmarker(const FPainter& P) const
{
	// Same timing and stacking state as the game hitmarker, drawn as four thin, small ticks with no
	// punch: white, or the alert red for a kill.
	const float Duration = HitKind == 2 ? KillHitmarkerDuration : HitmarkerDuration;
	const float Age = Now - HitTime;
	if (Age < 0.0f || Age >= Duration)
	{
		return;
	}

	const float T = Age / Duration;
	const float Alpha = 1.0f - T * T;
	const bool bKill = HitKind == 2;
	const float S = BodycamHitmarkerScale * (bKill ? 1.25f : 1.0f);
	const FLinearColor Col = WithAlpha(bKill ? BodycamAlertColor : FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), Alpha);
	const FLinearColor Dark(0.0f, 0.0f, 0.0f, 0.5f * Alpha);
	const float Len = HitmarkerLength * S;
	const float Th = BodycamHitmarkerThickness;
	const float Dist = HitmarkerInnerRadius * S + Len * 0.5f;
	const FVector2f C = P.Size * 0.5f;

	static const FVector2f Diag[4] = { FVector2f(1, -1), FVector2f(1, 1), FVector2f(-1, 1), FVector2f(-1, -1) };
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector2f Center = C + Diag[i] * UE_INV_SQRT_2 * Dist;
		const float Angle = (i % 2 == 0) ? 45.0f : -45.0f;
		P.RectCentered(Center, FVector2f(Th + 2.0f, Len + 2.0f), Dark, 0.0f, Angle, 8);
		P.RectCentered(Center, FVector2f(Th, Len), Col, 0.0f, Angle, 9);
	}
}

void UShooterHUDWidget::PaintBodycamDamage(const FPainter& P) const
{
	if (DamageIndicators.Num() == 0)
	{
		return;
	}

	// A brief red flash along the screen edge the hit came from, plus a jittering "!" just inside
	// the OSD inset on that side. Uses the same markers (and screen angles) as the game arcs.
	const FVector2f C = P.Size * 0.5f;
	const float Depth = FMath::Min(P.Size.X, P.Size.Y) * BodycamDamageFlashSize;
	constexpr int32 Strips = 8;
	const float StripW = Depth / Strips;
	const float FlashLife = FMath::Max(BodycamDamageFlashDuration, 0.01f);
	const float TickLife = FMath::Min(0.25f, FlashLife);
	const FSlateFontInfo Font = MakeOSDFont(P.Size.Y, BodycamTextHeight, P.Opacity);

	for (const FShooterHUDDamageIndicator& I : DamageIndicators)
	{
		const float Age = Now - I.SpawnTime;
		if (Age < 0.0f || Age > FlashLife)
		{
			continue;
		}

		const float T = Age / FlashLife;
		const float Alpha = BodycamDamageFlashAlpha * (1.0f - T) * (1.0f - T);

		// The edge a ray from the center along the marker angle leaves through.
		const FVector2f Dir(FMath::Sin(I.ScreenAngle), -FMath::Cos(I.ScreenAngle));
		const bool bSideEdge = FMath::Abs(Dir.X) * C.Y > FMath::Abs(Dir.Y) * C.X;

		for (int32 s = 0; s < Strips; ++s)
		{
			const float Fall = 1.0f - static_cast<float>(s) / Strips;
			const FLinearColor Col = WithAlpha(BodycamAlertColor, Alpha * Fall * Fall);
			const float O = s * StripW;
			if (bSideEdge)
			{
				P.Rect(FVector2f(Dir.X > 0.0f ? P.Size.X - O - StripW : O, 0.0f), FVector2f(StripW, P.Size.Y), Col, 0.0f, 0.0f, 2);
			}
			else
			{
				P.Rect(FVector2f(0.0f, Dir.Y > 0.0f ? P.Size.Y - O - StripW : O), FVector2f(P.Size.X, StripW), Col, 0.0f, 0.0f, 2);
			}
		}

		// "!" glitch tick: short-lived, drops out on some frames and hops sideways at 30 Hz.
		if (Age < TickLife && FMath::Fmod(Age * 20.0f, 1.0f) < 0.75f)
		{
			const FVector2f Half = C - FVector2f(P.Size.X, P.Size.Y) * BodycamEdgeInset * 1.5f;
			const float Tx = FMath::Abs(Dir.X) > KINDA_SMALL_NUMBER ? Half.X / FMath::Abs(Dir.X) : TNumericLimits<float>::Max();
			const float Ty = FMath::Abs(Dir.Y) > KINDA_SMALL_NUMBER ? Half.Y / FMath::Abs(Dir.Y) : TNumericLimits<float>::Max();
			FVector2f Pos = C + Dir * FMath::Min(Tx, Ty);
			Pos.X += (FMath::FloorToInt(Age * 30.0f) % 2 == 0) ? -3.0f : 3.0f;
			P.Text(TEXT("!"), Font, Pos, FVector2f(0.5f, 0.5f), BodycamAlertColor, 1.0f, nullptr, 10);
		}
	}
}
