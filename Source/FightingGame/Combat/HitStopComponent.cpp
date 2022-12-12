﻿// Copyright (c) Giammarco Agazzotti

#include "HitStopComponent.h"

#include "FightingGame/Character/FightingCharacter.h"
#include "FightingGame/Common/CombatStatics.h"
#include "FightingGame/Debug/Debug.h"

UHitStopComponent::UHitStopComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UHitStopComponent::BeginPlay()
{
	Super::BeginPlay();

	m_Character = Cast<AFightingCharacter>( GetOwner() );
	if( !m_Character )
	{
		FG_SLOG_ERR( TEXT("Cast to AFightingCharacter failed.") );
	}
}

void UHitStopComponent::EnableHitStop( float Duration, bool Shake )
{
	StartBeginHitStopTimer( Duration, Shake );
}

void UHitStopComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	if( m_UpdateMeshShake )
	{
		m_Character->UpdateMeshShake();
	}
}

void UHitStopComponent::StartBeginHitStopTimer( float Duration, bool Shake )
{
	FTimerManager& TimerManager = GetOwner()->GetWorldTimerManager();

	if( TimerManager.IsTimerActive( m_HitStopBeginTimerHandle ) )
	{
		TimerManager.ClearTimer( m_HitStopBeginTimerHandle );
	}

	m_CachedHitStopDuration = Duration;
	m_CachedDoMeshShake     = Shake;

	//m_CachedConsiderShake   = ConsiderShake;

	// #TODO instead of using GetHitStunInitialDelay, interpolate directly to the next reaction animation to ensure the correcto pose is always visible
	TimerManager.SetTimer( m_HitStopBeginTimerHandle, this, &UHitStopComponent::OnHitStopBeginTimerEnded, UCombatStatics::GetHitStunInitialDelay() );
}

void UHitStopComponent::OnHitStopBeginTimerEnded()
{
	StartStopHitStopTimer();

	if( m_CachedDoMeshShake )
	{
		m_UpdateMeshShake = true;
	}
}

void UHitStopComponent::StartStopHitStopTimer()
{
	FTimerManager& TimerManager = GetOwner()->GetWorldTimerManager();

	if( TimerManager.IsTimerActive( m_HitStopStopTimerHandle ) )
	{
		TimerManager.ClearTimer( m_HitStopStopTimerHandle );
	}

	// #TODO pass shake from hitdata
	m_Character->PushTimeDilation( UCombatStatics::GetMinCustomTimeDilation() );
	TimerManager.SetTimer( m_HitStopStopTimerHandle, this, &UHitStopComponent::OnHitStopStopTimerEnded, m_CachedHitStopDuration );
}

void UHitStopComponent::OnHitStopStopTimerEnded()
{
	m_Character->PopTimeDilation();
	m_UpdateMeshShake = false;

	m_Character->ResetMeshRelativeLocation();
}