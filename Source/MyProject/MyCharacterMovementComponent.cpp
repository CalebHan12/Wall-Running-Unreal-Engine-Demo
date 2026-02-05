#include "MyCharacterMovementComponent.h"
#include "MyProjectCharacter.h"
#include "GameFramework/Character.h"

UMyCharacterMovementComponent::UMyCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	this->JumpZVelocity *= 1.5;
}

void UMyCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

bool UMyCharacterMovementComponent::DoJump(bool bReplayingMoves)
{
	return DoJump(bReplayingMoves, 0.0f);
}

bool UMyCharacterMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("UMyCharacterMovementComponent::DoJump"));
	AMyProjectCharacter* MyChar = Cast<AMyProjectCharacter>(GetCharacterOwner());
	if(CharacterOwner && (CharacterOwner->CanJump() || MyChar->isWallrunning)) {
		// Don't jump if we can't move up/down.
		if(!bConstrainToPlane || !FMath::IsNearlyEqual(FMath::Abs(GetGravitySpaceZ(PlaneConstraintNormal)), 1.f)) {
			// If first frame of DoJump, we want to always inject the initial jump velocity.
			// For subsequent frames, during the time Jump is held, it depends... 
			// bDontFallXXXX == true means we want to ensure the character's Z velocity is never less than JumpZVelocity in this period
			// bDontFallXXXX == false means we just want to leave Z velocity alone and "let the chips fall where they may" (e.g. fall properly in physics)

			// NOTE: 
			// Checking JumpCurrentCountPreJump instead of JumpCurrentCount because Character::CheckJumpInput might have
			// incremented JumpCurrentCount just before entering this function... in order to compensate for the case when
			// on the first frame of the jump, we're already in falling stage. So we want the original value before any 
			// modification here.
			// 
			const bool bFirstJump = CharacterOwner->JumpCurrentCountPreJump == 0;

			if(MyChar->isWallrunning || bFirstJump || bDontFallBelowJumpZVelocityDuringJump) {
				Velocity.Z = FMath::Max<FVector::FReal>(Velocity.Z, JumpZVelocity);
				if(MyChar->isWallrunning) {
					Velocity.X = FMath::Max<FVector::FReal>(Velocity.X, JumpZVelocity) * MyChar->signOfX;
					
					MyChar->isWallrunning = false;
					MyChar->timeSinceLastWallrun = 0.0f;
					MyChar->JumpCurrentCount = 0;
				}
			}
			SetMovementMode(MOVE_Falling);
			return true;
		}
	}
	return false;
}

