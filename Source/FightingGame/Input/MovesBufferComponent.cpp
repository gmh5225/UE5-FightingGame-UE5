// Copyright (c) Giammarco Agazzotti

#include "MovesBufferComponent.h"
#include "Components/InputComponent.h"
#include "FightingGame/Character/FightingCharacter.h"
#include "FightingGame/Combat/InputSequenceResolver.h"
#include "FightingGame/Common/MathStatics.h"
#include "FightingGame/Debugging/Debug.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
    int32 loc_ShowInputBuffer = 0;
    FG_CVAR_FLAG_DESC( CVarShowInputBuffer, TEXT( "MovesBufferComponent.ShowInputBuffer" ), loc_ShowInputBuffer );

    int32 loc_ShowInputsSequenceBuffer = 0;
    FG_CVAR_FLAG_DESC( CVarShowMovesBuffer, TEXT( "MovesBufferComponent.ShowInputsSequenceBuffer" ), loc_ShowInputsSequenceBuffer );

    int32 loc_ShowDirectionalAngle = 0;
    FG_CVAR_FLAG_DESC( CVarShowDirectionalAngle, TEXT("MovesBufferComponent.ShowDirectionalAngle"), loc_ShowDirectionalAngle );
}

UMovesBufferComponent::UMovesBufferComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UMovesBufferComponent::BeginPlay()
{
    Super::BeginPlay();

    m_InputSequenceResolver = m_InputSequenceResolverClass
                                  ? NewObject<UInputSequenceResolver>( GetOwner(), m_InputSequenceResolverClass )
                                  : NewObject<UInputSequenceResolver>( GetOwner() );

    m_InputSequenceResolver->m_InputRouteEndedDelegate.AddUObject( this, &UMovesBufferComponent::OnInputRouteEnded );

    TArray<TObjectPtr<UInputsSequence>> inputs;
    TArray<TTuple<bool, bool>> groundedAirborneFlags;
    for( int32 i = 0; i < m_InputsList.Num(); ++i )
    {
        inputs.Emplace( m_InputsList[i] );

        // #TODO find a way to link these inputs to the actual moves
        groundedAirborneFlags.Emplace( TTuple<bool, bool>( true, true ) );
    }

    m_InputSequenceResolver->Init( inputs, groundedAirborneFlags );
}

void UMovesBufferComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
    Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

    m_IBElapsedFrameTime += DeltaTime;

    static float bufferFrameDuration = 1.f / m_InputBufferFrameRate;
    if( m_IBElapsedFrameTime >= bufferFrameDuration )
    {
        m_IBElapsedFrameTime = 0.f;

        if( !m_IBBufferChanged )
        {
            AddToInputBuffer( EInputEntry::None );
        }
    }

    m_ISBElapsedFrameTime += DeltaTime;

    static float movesBufferFrameDuration = 1.f / m_InputsSequencesBufferFrameRate;
    if( m_ISBElapsedFrameTime >= movesBufferFrameDuration )
    {
        m_ISBElapsedFrameTime = 0.f;
        if( !m_ISBBufferChanged )
        {
            AddToInputsSequenceBuffer( FInputsSequenceBufferEntry::s_SequenceNone, 0 );
        }
    }

    if( loc_ShowInputBuffer )
    {
        if( m_OwnerCharacter && m_OwnerCharacter->m_PlayerIndex == 0 )
        {
            for( int32 i = 0; i < m_InputsBuffer.size(); ++i )
            {
                FInputBufferEntry& entry = m_InputsBuffer.at( i );
                const bool isEmpty       = entry.m_InputEntry == EInputEntry::None;
                FString message          = isEmpty ? TEXT( "---" ) : InputEntryToString( entry.m_InputEntry );

                FColor color = entry.m_Used ? FColor::Red : FColor::Green;

                GEngine->AddOnScreenDebugMessage( i, 1.f, color, FString::Printf( TEXT( "%s" ), *message ) );
            }
        }
    }

    if( loc_ShowInputsSequenceBuffer )
    {
        if( m_OwnerCharacter && m_OwnerCharacter->m_PlayerIndex == 0 )
        {
            for( int32 i = 0; i < m_InputsSequenceBuffer.size(); ++i )
            {
                FInputsSequenceBufferEntry& entry = m_InputsSequenceBuffer.at( i );
                bool isEmpty                      = entry.m_InputsSequenceName == FInputsSequenceBufferEntry::s_SequenceNone;
                FString message                   = isEmpty ? TEXT( "---" ) : entry.m_InputsSequenceName.ToString();

                FColor color = entry.m_Used ? FColor::Red : FColor::Green;

                GEngine->AddOnScreenDebugMessage( i + 20, 1.f, color, FString::Printf( TEXT( "%s" ), *message ) );
            }
        }
    }

    m_IBBufferChanged  = false;
    m_ISBBufferChanged = false;

    UpdateMovementDirection();

    if( m_PlayerInput )
    {
        float horizontalMovement = m_PlayerInput->GetAxisValue( TEXT( "MoveHorizontal" ) );

        m_InputMovement = horizontalMovement;

        m_MovingRight = horizontalMovement > m_AnalogMovementDeadzone;
        m_MovingLeft  = horizontalMovement < -m_AnalogMovementDeadzone;

        UpdateDirectionalInputs( m_PlayerInput );
    }
}

void UMovesBufferComponent::OnSetupPlayerInputComponent( UInputComponent* PlayerInputComponent )
{
    m_PlayerInput = PlayerInputComponent;
    if( m_PlayerInput )
    {
        FName jumpAction           = TEXT( "Jump" );
        FName attackAction         = TEXT( "Attack" );
        FName specialAction        = TEXT( "Special" );
        FName moveHorizontalAction = TEXT( "MoveHorizontal" );
        FName moveVerticalAction   = TEXT( "MoveVertical" );

        m_PlayerInput->BindAction( jumpAction, IE_Pressed, this, &UMovesBufferComponent::OnStartJump );
        m_PlayerInput->BindAction( jumpAction, IE_Released, this, &UMovesBufferComponent::OnStopJump );
        m_PlayerInput->BindAction( attackAction, IE_Pressed, this, &UMovesBufferComponent::OnAttack );
        m_PlayerInput->BindAction( specialAction, IE_Pressed, this, &UMovesBufferComponent::OnSpecial );

        m_PlayerInput->BindAxis( moveHorizontalAction );
        m_PlayerInput->BindAxis( moveVerticalAction );
    }

    InitInputBuffer();

    InitInputsSequenceBuffer();
}

bool UMovesBufferComponent::IsInputBuffered( EInputEntry Input, bool ConsumeEntry )
{
    for( int32 i = 0; i < m_InputsBuffer.size(); ++i )
    {
        FInputBufferEntry& entry = m_InputsBuffer.at( i );
        if( entry.m_InputEntry != EInputEntry::None && entry.m_InputEntry == Input && !entry.m_Used )
        {
            if( ConsumeEntry )
            {
                entry.m_Used = true;
            }
            return true;
        }
    }

    return false;
}

float UMovesBufferComponent::GetMovementDirection() const
{
    return m_MovementDirection;
}

EInputEntry UMovesBufferComponent::GetDirectionalInputEntryFromAngle( float Angle ) const
{
    static float forwardAngle = 90.f;
    static float downAngle    = 180.f;
    static float backAngle    = -90.f;
    static float upAngle      = 0.f;

    // Up
    if( Angle > upAngle - m_DirectionalChangeRotationEpsilon && Angle < upAngle + m_DirectionalChangeRotationEpsilon )
    {
        return EInputEntry::Up;
    }

    // Up-forward
    if( Angle >= upAngle + m_DirectionalChangeRotationEpsilon && Angle < forwardAngle - m_DirectionalChangeRotationEpsilon )
    {
        return EInputEntry::UpForward;
    }

    // Forward
    if( Angle > forwardAngle - m_DirectionalChangeRotationEpsilon && Angle < forwardAngle + m_DirectionalChangeRotationEpsilon )
    {
        return EInputEntry::Forward;
    }

    // Forward-down
    if( Angle >= forwardAngle + m_DirectionalChangeRotationEpsilon && Angle < downAngle - m_DirectionalChangeRotationEpsilon )
    {
        return EInputEntry::ForwardDown;
    }

    // Down
    if( (Angle >= downAngle && Angle >= downAngle - m_DirectionalChangeRotationEpsilon) ||
        (Angle > -downAngle && Angle < -downAngle + m_DirectionalChangeRotationEpsilon) )
    {
        return EInputEntry::Down;
    }

    // Down-back
    if( Angle > -downAngle + m_DirectionalChangeRotationEpsilon && Angle < backAngle - m_DirectionalChangeRotationEpsilon )
    {
        return EInputEntry::DownBackward;
    }

    // Back
    if( Angle > backAngle - m_DirectionalChangeRotationEpsilon && Angle < backAngle + m_DirectionalChangeRotationEpsilon )
    {
        return EInputEntry::Backward;
    }

    // Back-Up
    if( Angle >= backAngle + m_DirectionalChangeRotationEpsilon && Angle < upAngle - m_DirectionalChangeRotationEpsilon )
    {
        return EInputEntry::BackwardUp;
    }

    return EInputEntry::None;
}

void UMovesBufferComponent::OnInputRouteEnded( TObjectPtr<UInputsSequence> InputsSequence )
{
    AddToInputsSequenceBuffer( InputsSequence->m_Name, InputsSequence->m_Priority );
}

void UMovesBufferComponent::AddToInputBuffer( EInputEntry InputEntry )
{
    EInputEntry targetEntry = m_OwnerCharacter->IsFacingRight() ? InputEntry : GetMirrored( InputEntry );

    m_InputsBuffer.emplace_back( FInputBufferEntry{targetEntry, false} );
    m_InputsBuffer.pop_front();

    m_IBBufferChanged = true;

    if( InputEntry != EInputEntry::None )
    {
        m_InputSequenceResolver->RegisterInput( targetEntry );
    }
}

bool UMovesBufferComponent::InputBufferContainsConsumable( EInputEntry InputEntry ) const
{
    for( const FInputBufferEntry& entry : m_InputsBuffer )
    {
        if( entry.m_InputEntry == InputEntry && !entry.m_Used )
        {
            return true;
        }
    }

    return false;
}

void UMovesBufferComponent::AddToInputsSequenceBuffer( const FName& InputsSequenceName, int32 Priority )
{
    m_InputsSequenceBuffer.emplace_back( FInputsSequenceBufferEntry{InputsSequenceName, Priority, false} );
    m_InputsSequenceBuffer.pop_front();

    m_ISBBufferChanged = true;
}

bool UMovesBufferComponent::InputsSequenceBufferContainsConsumable( const FName& MoveName )
{
    for( const FInputsSequenceBufferEntry& entry : m_InputsSequenceBuffer )
    {
        if( entry.m_InputsSequenceName == MoveName && !entry.m_Used )
        {
            return true;
        }
    }

    return false;
}

void UMovesBufferComponent::ClearInputsBuffer()
{
    m_InputsBuffer.clear();
}

void UMovesBufferComponent::InitInputBuffer()
{
    ClearInputsBuffer();

    for( int32 i = 0; i < m_InputBufferSizeFrames; ++i )
    {
        m_InputsBuffer.emplace_back( FInputBufferEntry{EInputEntry::None, false} );
    }
}

void UMovesBufferComponent::UseBufferedInputsSequence( const FName& InputsSequenceName )
{
    verify( InputsSequenceBufferContainsConsumable( InputsSequenceName ) );

    for( FInputsSequenceBufferEntry& entry : m_InputsSequenceBuffer )
    {
        if( entry.m_InputsSequenceName == InputsSequenceName )
        {
            entry.m_Used = true;
        }
    }
}

void UMovesBufferComponent::ClearInputsSequenceBuffer()
{
    m_InputsSequenceBuffer.clear();
}

void UMovesBufferComponent::InitInputsSequenceBuffer()
{
    ClearInputsSequenceBuffer();

    for( int i = 0; i < m_InputsSequencesBufferSizeFrames; ++i )
    {
        m_InputsSequenceBuffer.emplace_back( FInputsSequenceBufferEntry{FInputsSequenceBufferEntry::s_SequenceNone, false} );
    }
}

void UMovesBufferComponent::GetInputsSequenceBufferSnapshot( TArray<FInputsSequenceBufferEntry>& OutEntries, bool SkipEmptyEntries )
{
    OutEntries.Reset();

    for( const auto& entry : m_InputsSequenceBuffer )
    {
        bool currentEntryIsNone = entry.m_InputsSequenceName == FInputsSequenceBufferEntry::s_SequenceNone;
        if( SkipEmptyEntries && currentEntryIsNone )
        {
            continue;
        }

        OutEntries.Emplace( entry );
    }
}

bool UMovesBufferComponent::IsInputsSequenceBuffered( const FName& InputsSequenceName, bool ConsumeEntry /*= true*/ )
{
    for( int32 i = 0; i < m_InputsSequenceBuffer.size(); ++i )
    {
        FInputsSequenceBufferEntry& entry = m_InputsSequenceBuffer.at( i );

        bool isValid     = entry.m_InputsSequenceName != FInputsSequenceBufferEntry::s_SequenceNone;
        bool hasSameName = entry.m_InputsSequenceName == InputsSequenceName;
        bool canBeUsed   = !entry.m_Used;

        if( isValid && hasSameName && canBeUsed )
        {
            if( ConsumeEntry )
            {
                entry.m_Used = true;
            }

            return true;
        }
    }

    return false;
}

void UMovesBufferComponent::OnMoveHorizontal( float Value )
{
    if( FMath::Abs( Value ) > m_AnalogMovementDeadzone )
    {
        m_InputMovement = Value;

        m_MovingRight = FMath::Sign( Value ) > 0;
        m_MovingLeft  = FMath::Sign( Value ) < 0;
    }
}

// TODO: these are all the same, make it generic maybe?
void UMovesBufferComponent::OnStartJump()
{
    AddToInputBuffer( EInputEntry::StartJump );
}

void UMovesBufferComponent::OnStopJump()
{
    AddToInputBuffer( EInputEntry::StopJump );
}

void UMovesBufferComponent::OnAttack()
{
    AddToInputBuffer( EInputEntry::Attack );
}

void UMovesBufferComponent::OnSpecial()
{
    AddToInputBuffer( EInputEntry::Special );
}

void UMovesBufferComponent::UpdateMovementDirection()
{
    if( (m_MovingRight && m_MovingLeft) || (!m_MovingRight && !m_MovingLeft) )
    {
        m_MovementDirection = 0.f;
    }
    else
    {
        if( m_MovingRight )
        {
            m_MovementDirection = 1.f;
        }
        else if( m_MovingLeft )
        {
            m_MovementDirection = -1.f;
        }
    }
}

void UMovesBufferComponent::UpdateDirectionalInputs( UInputComponent* InputComponent )
{
    m_DirectionalInputVector.X = InputComponent->GetAxisValue( "MoveHorizontal" );
    m_DirectionalInputVector.Y = InputComponent->GetAxisValue( "MoveVertical" );

    if( m_DirectionalInputVector.Length() > m_MinDirectionalInputVectorLength )
    {
        float angle       = UMathStatics::GetSignedAngle( m_DirectionalInputVector, FVector2d( 0.f, 1.f ) );
        EInputEntry entry = GetDirectionalInputEntryFromAngle( angle );

        if( loc_ShowDirectionalAngle )
        {
            UKismetSystemLibrary::DrawDebugString( GetWorld(), GetOwner()->GetActorLocation(),
                                                   FString::Printf( TEXT( "[Input: %s]" ), *InputEntryToString( entry ) ) );
        }

        if( entry != m_LastDirectionalInputEntry )
        {
            m_LastDirectionalInputEntry = entry;

            AddToInputBuffer( entry );
        }
    }

    m_LastDirectionalInputVector = m_DirectionalInputVector;
}

void UMovesBufferComponent::UseBufferedInput( EInputEntry Input )
{
    verify( InputBufferContainsConsumable( Input ) );

    for( FInputBufferEntry& entry : m_InputsBuffer )
    {
        if( entry.m_InputEntry == Input )
        {
            entry.m_Used = true;
        }
    }
}
