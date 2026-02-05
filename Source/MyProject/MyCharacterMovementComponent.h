#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MyCharacterMovementComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;

UCLASS()
class MYPROJECT_API UMyCharacterMovementComponent : public UCharacterMovementComponent
{
public:
	GENERATED_BODY()

	UMyCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);
	virtual bool DoJump(bool bReplayingMoves) override;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;

protected:
	virtual void InitializeComponent() override;
};
