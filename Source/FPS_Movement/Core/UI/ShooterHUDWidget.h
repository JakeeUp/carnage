// FPS_Movement: native combat HUD.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "ShooterHUDWidget.generated.h"

class AShooterCharacter;
class AShooterWeapon;
class UDoomMovementComponent;
class UStyleMeterComponent;
struct FSlateFontInfo;

/** One floating damage number near the crosshair. */
USTRUCT()
struct FShooterHUDDamageNumber
{
	GENERATED_BODY()

	float Value = 0.0f;
	float SpawnTime = 0.0f;
	FVector2f Offset = FVector2f::ZeroVector;
	FLinearColor Color = FLinearColor::White;
};

/** One incoming-damage direction marker. The screen angle is refreshed every tick from the camera yaw. */
USTRUCT()
struct FShooterHUDDamageIndicator
{
	GENERATED_BODY()

	FVector WorldSource = FVector::ZeroVector;
	float SpawnTime = 0.0f;
	/** Radians, 0 = straight ahead (screen up), positive = clockwise (to the right). */
	float ScreenAngle = 0.0f;
};

/**
 * Fully native combat HUD. Has no widget tree: everything is drawn in NativePaint with
 * FSlateDrawElement primitives, and all animated state is computed in NativeTick.
 *
 * Animations run on unscaled (Slate) time, so kill hit-stop time dilation never freezes the HUD.
 * All sizes are Slate units (DPI scaled by the viewport) and every panel is anchored to a screen
 * corner/edge with margins, so it holds up at 16:9 and ultrawide.
 *
 * Every tunable is an EditAnywhere UPROPERTY so a Blueprint subclass can restyle it without code.
 */
UCLASS(Blueprintable, BlueprintType)
class FPS_MOVEMENT_API UShooterHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UShooterHUDWidget(const FObjectInitializer& ObjectInitializer);

	/** Bind (or re-bind after respawn) to a character's delegates. Passing null unbinds. */
	UFUNCTION(BlueprintCallable, Category="HUD")
	void BindToCharacter(AShooterCharacter* Character);

	/** Spawn a damage direction marker pointing at a world-space source (attacker / projectile origin). */
	UFUNCTION(BlueprintCallable, Category="HUD")
	void NotifyDamageFrom(const FVector& WorldSourceLocation);

	/** Hit confirm entry point (also bound to AShooterCharacter::OnHitConfirmed automatically). */
	UFUNCTION(BlueprintCallable, Category="HUD")
	void NotifyHitConfirmed(float Damage, bool bKilled, bool bHeadshot);

	UFUNCTION(BlueprintPure, Category="HUD")
	AShooterCharacter* GetBoundCharacter() const { return BoundCharacter.Get(); }

	// ── Palette ─────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Palette")
	FLinearColor ColorPrimary = FLinearColor(0.94f, 0.92f, 0.87f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Palette")
	FLinearColor ColorAccent = FLinearColor(1.0f, 0.33f, 0.05f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Palette")
	FLinearColor ColorDanger = FLinearColor(1.0f, 0.08f, 0.06f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Palette")
	FLinearColor ColorHeadshot = FLinearColor(1.0f, 0.76f, 0.12f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Palette")
	FLinearColor ColorArmor = FLinearColor(0.42f, 0.72f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Palette")
	FLinearColor ColorBackplate = FLinearColor(0.015f, 0.012f, 0.012f, 0.62f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Palette")
	FLinearColor ColorEmptySlot = FLinearColor(1.0f, 1.0f, 1.0f, 0.10f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Palette")
	FLinearColor ColorShadow = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);

	/** Global multiplier on every element's alpha. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Palette", meta=(ClampMin=0, ClampMax=1))
	float HUDOpacity = 1.0f;

	// ── Layout ──────────────────────────────────────────────────────────

	/** Distance of corner panels from the screen edges (Slate units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Layout")
	FVector2f ScreenMargin = FVector2f(56.0f, 48.0f);

	/** Horizontal shear applied to backplates and bars (the aggressive slant). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Layout", meta=(ClampMin=-1, ClampMax=1))
	float PanelSkew = -0.28f;

	/** Vertical position of the rank panel as a fraction of screen height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Layout", meta=(ClampMin=0, ClampMax=1))
	float RankPanelHeightFraction = 0.40f;

	// ── Crosshair ───────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair")
	float CrosshairBaseGap = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair")
	float CrosshairTickLength = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair")
	float CrosshairTickThickness = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair")
	float CrosshairDotSize = 3.0f;

	/** Bloom added per detected shot (1 = one "shot" unit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair")
	float ShotBloom = 1.0f;

	/** Gap in Slate units per unit of bloom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair")
	float BloomGap = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair")
	float MaxBloom = 3.0f;

	/** Exponential bloom recovery rate (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair")
	float BloomRecoveryRate = 7.0f;

	/** Extra gap at full movement speed (UDoomMovementComponent::GetSpeedRatio == 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair")
	float SpeedGap = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair")
	float AirborneGap = 7.0f;

	/** Gap spring stiffness and damping ratio (<1 gives a small overshoot "kick"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair")
	float GapSpringStiffness = 520.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair", meta=(ClampMin=0.05, ClampMax=2))
	float GapSpringDamping = 0.55f;

	/** Critically damped smoothing time for the ADS blend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair", meta=(Units="s"))
	float ADSSmoothTime = 0.18f;

	/** Gap multiplier at full ADS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair", meta=(ClampMin=0, ClampMax=1))
	float ADSGapScale = 0.35f;

	/** Center dot alpha at full ADS (ticks fade fully). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Crosshair", meta=(ClampMin=0, ClampMax=1))
	float ADSDotAlpha = 0.0f;

	// ── Hitmarker ───────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker", meta=(Units="s"))
	float HitmarkerDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker", meta=(Units="s"))
	float KillHitmarkerDuration = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker")
	float HitmarkerInnerRadius = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker")
	float HitmarkerLength = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker")
	float HitmarkerThickness = 2.5f;

	/** Size multiplier for kill hitmarkers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker")
	float KillHitmarkerScale = 1.55f;

	/** Initial scale punch on spawn (decays fast). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker")
	float HitmarkerPunch = 0.65f;

	/** Size growth per stacked rapid hit, and the stack cap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker")
	float HitmarkerStackGrowth = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker")
	int32 HitmarkerMaxStack = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker")
	bool bShowDamageNumbers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker", meta=(Units="s"))
	float DamageNumberLifetime = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker")
	float DamageNumberRise = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Hitmarker")
	int32 DamageNumberFontSize = 13;

	// ── Damage feedback ─────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Damage", meta=(Units="s"))
	float DamageIndicatorDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Damage")
	float DamageIndicatorRadius = 118.0f;

	/** Half-width of the arc, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Damage")
	float DamageIndicatorArcHalfAngle = 17.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Damage")
	float DamageIndicatorThickness = 5.0f;

	/** Vignette intensity added per hit, plus extra per fraction of health lost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Damage")
	float VignetteHitAlpha = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Damage")
	float VignettePerHealthLost = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Damage")
	float VignetteDecayRate = 2.6f;

	/** Vignette thickness as a fraction of the shorter screen side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Damage", meta=(ClampMin=0, ClampMax=0.5))
	float VignetteSize = 0.16f;

	/** Below this health fraction the vignette and health bar pulse continuously. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Damage", meta=(ClampMin=0, ClampMax=1))
	float LowHealthThreshold = 0.3f;

	// ── Vitals (bottom-left) ────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Vitals")
	int32 HealthSegments = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Vitals")
	float VitalsBarWidth = 270.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Vitals")
	float HealthBarHeight = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Vitals")
	float ArmorBarHeight = 7.0f;

	/** Seconds the white "trailing damage" chunk holds before draining. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Vitals", meta=(Units="s"))
	float DamageTrailHold = 0.38f;

	/** Trail drain speed in bar-fractions per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Vitals")
	float DamageTrailDrainRate = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Vitals")
	FVector2f DashPipSize = FVector2f(34.0f, 9.0f);

	// ── Ammo (bottom-right) ─────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Ammo")
	int32 MagazineFontSize = 54;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Ammo")
	int32 ReserveFontSize = 22;

	/** Magazine fraction at or below which the count turns red. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Ammo", meta=(ClampMin=0, ClampMax=1))
	float LowAmmoFraction = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Ammo")
	float AmmoShotPunch = 0.2f;

	/** Used for the reload bar if the weapon's own reload duration can't be read. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Ammo", meta=(Units="s"))
	float FallbackReloadDuration = 1.5f;

	// ── Carnage rank (right) ────────────────────────────────────────────

	/** One color per rank, D..SSS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Rank")
	TArray<FLinearColor> RankColors;

	/** One title per rank, D..SSS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Rank")
	TArray<FString> RankTitles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Rank")
	int32 RankFontSize = 56;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Rank")
	float RankBarWidth = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Rank")
	float RankUpPunch = 0.7f;

	/** Panel alpha while idling at D with no style points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Rank", meta=(ClampMin=0, ClampMax=1))
	float RankIdleAlpha = 0.35f;

	// ── Bodycam OSD (shown while UBodycamComponent is on) ───────────────
	//
	// In bodycam mode the game HUD is replaced by a burned-in camera OSD: small monospace text
	// fields in the corners instead of panels. Both layouts are always evaluated and crossfaded
	// by the bodycam blend alpha, so toggling morphs between them.

	/** Draw a dot crosshair in bodycam mode (the crosshair ticks are always hidden). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam")
	bool bBodycamCenterDot = true;

	/**
	 * Place the dot where the gun actually points rather than at screen centre. The bodycam view
	 * trails the aim through a spring, so during fast turns the two differ by a few degrees.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(EditCondition="bBodycamCenterDot"))
	bool bBodycamDotFollowsAim = true;

	/** Bodycam dot diameter (DPI-scaled units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=1, ClampMax=16, EditCondition="bBodycamCenterDot"))
	float BodycamDotSize = 4.0f;

	/** Bodycam dot opacity at full ADS (the sights take over). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0, ClampMax=1, EditCondition="bBodycamCenterDot"))
	float BodycamDotADSAlpha = 0.0f;

	/** Fictional device / unit line drawn along the bottom edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam")
	FString BodycamDeviceId = TEXT("CARNAGE // UNIT 07 // CAM-02");

	/** Overlay inset from the screen edges, as a fraction of each screen dimension. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0, ClampMax=0.25))
	float BodycamEdgeInset = 0.04f;

	/** OSD text size as a fraction of screen height (sets the monospace font size). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0.005, ClampMax=0.05))
	float BodycamTextHeight = 0.018f;

	/** Distance between stacked OSD rows, in multiples of the text line height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=1, ClampMax=2))
	float BodycamRowSpacing = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam")
	FLinearColor BodycamTextColor = FLinearColor(0.93f, 0.93f, 0.89f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam")
	FLinearColor BodycamRecColor = FLinearColor(1.0f, 0.06f, 0.04f, 1.0f);

	/** OSD text color below LowHealthThreshold (also used for low ammo and the damage tick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam")
	FLinearColor BodycamAlertColor = FLinearColor(1.0f, 0.2f, 0.16f, 0.95f);

	/** REC dot blink rate in Hz, normally and at low health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0.1))
	float BodycamRecBlinkRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0.1))
	float BodycamLowHealthRecBlinkRate = 3.0f;

	/** Show the HDG (camera yaw) and SPD (m/s) line under the clock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam")
	bool bBodycamShowHeading = true;

	/** How long the STYLE tag blinks inverted after a rank change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(Units="s"))
	float BodycamRankFlashDuration = 0.6f;

	/** Character count of the text reload bar, e.g. [#####-----]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=2, ClampMax=40))
	int32 BodycamReloadBarChars = 10;

	/** Chance per second (real time) that one OSD row (or the whole OSD) tears sideways for a single frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0, ClampMax=1))
	float BodycamGlitchChancePerSecond = 0.005f;

	/** Maximum horizontal tear offset of a glitched row (Slate units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0))
	float BodycamGlitchMaxOffset = 6.0f;

	/** Depth of the slow OSD brightness flicker (0 = steady). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0, ClampMax=0.5))
	float BodycamFlickerAmount = 0.04f;

	/** Speed of the brightness flicker noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0))
	float BodycamFlickerRate = 9.0f;

	/** Bodycam hitmarker size relative to the game hitmarker, and its line thickness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0.1, ClampMax=2))
	float BodycamHitmarkerScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0.5))
	float BodycamHitmarkerThickness = 1.0f;

	/** Red edge flash on the side a hit came from: lifetime, depth (fraction of the shorter screen side) and peak alpha. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(Units="s"))
	float BodycamDamageFlashDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0, ClampMax=0.5))
	float BodycamDamageFlashSize = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0, ClampMax=1))
	float BodycamDamageFlashAlpha = 0.4f;

	/** Battery percentage when the HUD is created, and its slow drain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0, ClampMax=100))
	float BodycamBatteryStart = 87.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD|Bodycam", meta=(ClampMin=0))
	float BodycamBatteryDrainPerMinute = 1.0f;

protected:

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// ── Delegate handlers ───────────────────────────────────────────────

	UFUNCTION()
	void HandleBulletCountUpdated(int32 MagazineSize, int32 Bullets);

	UFUNCTION()
	void HandleReserveAmmoUpdated(int32 ReserveAmmo);

	UFUNCTION()
	void HandleDamaged(float LifePercent);

	UFUNCTION()
	void HandleHitConfirmed(AActor* Target, float Damage, bool bKilled, bool bHeadshot);

	UFUNCTION()
	void HandleAimStateChanged(bool bIsAimingDownSights);

private:

	void ApplyAmmo(int32 MagazineSize, int32 Bullets);
	void OnShotDetected();
	AShooterWeapon* FindActiveWeapon() const;
	float ReadReloadDuration(const AShooterWeapon* Weapon) const;

	void TickCrosshair(float Dt);
	void TickVitals(float Dt);
	void TickAmmo(float Dt);
	void TickRank(float Dt);
	void TickDamageFeedback(float Dt);
	void TickBodycam(float Dt);

	struct FPainter;
	void PaintVignette(const FPainter& P) const;
	void PaintCrosshair(const FPainter& P) const;
	void PaintHitmarker(const FPainter& P) const;
	void PaintDamageIndicators(const FPainter& P) const;
	void PaintVitals(const FPainter& P) const;
	void PaintAmmo(const FPainter& P) const;
	void PaintRank(const FPainter& P) const;
	void PaintBodycamOverlay(const FPainter& P) const;
	void PaintBodycamHitmarker(const FPainter& P) const;
	void PaintBodycamDamage(const FPainter& P) const;

	FLinearColor GetRankColor(int32 RankIndex) const;

	// ── Bound sources ───────────────────────────────────────────────────

	TWeakObjectPtr<AShooterCharacter> BoundCharacter;
	TWeakObjectPtr<UDoomMovementComponent> Movement;
	TWeakObjectPtr<UStyleMeterComponent> StyleMeter;
	TWeakObjectPtr<AShooterWeapon> ActiveWeapon;

	FSlateColorBrush SolidBrush = FSlateColorBrush(FLinearColor::White);

	FSlateRoundedBoxBrush DotBrush = FSlateRoundedBoxBrush(FLinearColor::White, 6.0f);

	/** Unscaled HUD clock (accumulated Slate delta). */
	float Now = 0.0f;

	/** UBodycamComponent blend alpha of the bound character (0 = normal HUD). */
	float BodycamAlpha = 0.0f;

	// Bodycam OSD
	float OSDFlicker = 1.0f;       // brightness multiplier, ~1
	float LowHealthBlend = 0.0f;   // 0..1, eases the OSD toward BodycamAlertColor
	float RecPhase = 0.0f;         // accumulated blink phase (rate changes don't jump)
	uint64 GlitchFrame = 0;        // GFrameCounter of the one frame that tears
	int32 GlitchRow = -1;          // row that tears, INDEX_NONE = every row
	float GlitchOffset = 0.0f;
	float HeadingDeg = 0.0f;
	float SpeedMetersPerSec = 0.0f;

	// Crosshair
	float Bloom = 0.0f;
	float Gap = 7.0f;
	float GapVelocity = 0.0f;
	bool bAimTarget = false;
	float ADSAlpha = 0.0f;

	/** Where the gun's aim ray lands on screen, in widget space (valid when bHasBodycamAimPoint). */
	FVector2f BodycamAimPoint = FVector2f::ZeroVector;
	bool bHasBodycamAimPoint = false;

	/** Projects the control rotation (the gun's aim) through the current, possibly lagging, view. */
	void UpdateBodycamAimPoint();
	float ADSVelocity = 0.0f;

	// Hitmarker
	float HitTime = -100.0f;
	uint8 HitKind = 0; // 0 normal, 1 headshot, 2 kill
	int32 HitStack = 0;
	TArray<FShooterHUDDamageNumber> DamageNumbers;

	// Damage feedback
	TArray<FShooterHUDDamageIndicator> DamageIndicators;
	float VignetteAlpha = 0.0f;

	// Vitals
	float HealthFrac = 1.0f;
	float HealthTrail = 1.0f;
	float HealthTrailHoldUntil = 0.0f;
	float HealthValue = 0.0f;
	float ArmorFrac = 0.0f;
	float ArmorValue = 0.0f;
	float ArmorTrail = 0.0f;
	float ArmorTrailHoldUntil = 0.0f;
	bool bHasArmor = false;
	float LastLifePercent = 1.0f;
	int32 DashCharges = 0;
	int32 MaxDashCharges = 0;
	float DashChangeTime = -100.0f;
	int32 DashChangedPip = -1;
	bool bDashGained = false;

	// Ammo
	bool bHasAmmo = false;
	int32 MagSize = 0;
	int32 Bullets = 0;
	int32 Reserve = 0;
	float AmmoPunch = 0.0f;
	bool bReloading = false;
	float ReloadStartGameTime = 0.0f;
	float ReloadDuration = 1.5f;
	float ReloadProgress = 0.0f;

	// Rank
	bool bHasRank = false;
	int32 RankIndex = 0;
	FString RankLetter;
	float RankProgress = 0.0f;
	float RankChangeTime = -100.0f;
	bool bRankUp = false;
};
