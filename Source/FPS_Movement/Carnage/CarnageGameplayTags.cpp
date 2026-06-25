// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarnageGameplayTags.h"

namespace CarnageTags
{
	// ── State ───────────────────────────────────────────────────────────
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Sprinting,    "Carnage.State.Sprinting",    "Character is sprinting on the ground");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Sliding,      "Carnage.State.Sliding",      "Character is in a momentum-preserving slide");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dashing,      "Carnage.State.Dashing",      "Character is in the active phase of a dash");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_WallRunning,  "Carnage.State.WallRunning",  "Character is running along a wall");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Mantling,     "Carnage.State.Mantling",     "Character is interpolating up to a ledge");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_InAir,        "Carnage.State.InAir",        "Character is airborne (falling, jumping)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead,         "Carnage.State.Dead",         "Character is dead — blocks most abilities");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned,      "Carnage.State.Stunned",      "Character is stunned — blocks input");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Parrying,     "Carnage.State.Parrying",     "Active parry window");

	// ── Damage ──────────────────────────────────────────────────────────
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Physical,    "Carnage.Damage.Physical",    "Standard kinetic damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Headshot,    "Carnage.Damage.Headshot",    "Damage applied via headshot — triggers golden VFX");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Critical,    "Carnage.Damage.Critical",    "Critical hit roll succeeded — yellow/orange numbers");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Melee,       "Carnage.Damage.Melee",       "Melee strike damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Explosion,   "Carnage.Damage.Explosion",   "Explosive radial damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Falling,     "Carnage.Damage.Falling",     "Fall damage");

	// ── SetByCaller (GE magnitude IDs) ──────────────────────────────────
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage,       "Carnage.SetByCaller.Damage",       "Raw damage magnitude passed into a damage GE");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Heal,         "Carnage.SetByCaller.Heal",         "Heal magnitude passed into a heal GE");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_HeadshotMult, "Carnage.SetByCaller.HeadshotMult", "Headshot damage multiplier (e.g. 2.0)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_CritMult,     "Carnage.SetByCaller.CritMult",     "Critical hit damage multiplier");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_SurfaceMult,  "Carnage.SetByCaller.SurfaceMult",  "Surface-type damage multiplier");

	// ── Ability ─────────────────────────────────────────────────────────
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Dash,         "Carnage.Ability.Dash",         "Air dash ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Slide,        "Carnage.Ability.Slide",        "Slide ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_DoubleJump,   "Carnage.Ability.DoubleJump",   "Double jump ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Melee_Light,  "Carnage.Ability.Melee.Light",  "Light melee strike");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Melee_Heavy,  "Carnage.Ability.Melee.Heavy",  "Heavy melee strike (stagger)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Parry,        "Carnage.Ability.Parry",        "Active parry");

	// ── Events ──────────────────────────────────────────────────────────
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_DamageDealt,    "Carnage.Event.DamageDealt",    "Fired after damage execution completes");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_KillConfirmed,  "Carnage.Event.KillConfirmed",  "Fired when damage source kills a target");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Headshot,       "Carnage.Event.Headshot",       "Fired on headshot confirmation");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Parried,        "Carnage.Event.Parried",        "Fired when an incoming attack is parried");

	// ── Cooldowns ───────────────────────────────────────────────────────
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Dash,   "Carnage.Cooldown.Dash",   "Applied while dash is on cooldown");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Melee,  "Carnage.Cooldown.Melee",  "Applied while melee is on cooldown");
}
