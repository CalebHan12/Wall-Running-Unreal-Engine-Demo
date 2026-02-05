#include "MyProjectCharacter.h"
#include "MyProjectProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MyCharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"

/*
	Wallrunning notes:
	TODO: Tilt camera when running on the wall

	TODO: Allow vertical movement when on the wall so that it doesn't feel so stiff. Currently, when
	running the x and y velocity are set constantly. Probably would be cool if the speed went up or down
	depending on where the player looked or whatever they did in Titanfall

	Currently the way to determine when to end the run is see if the player crossed the end of
	the y-coordinate. If we have two side-by-side walls that are flush with each other then the run
	will end for seamingly no reason. Either find a new way to determine when the player doesn't
	have a wall next to them or just make sure all walls are one continuous component so that the
	return bounding box isn't partitioned.
	
	Also, fetching the bounding box might not work with certain components (I think they need to have
	a collision?). Add this requirement to the SweepBySideChannel.

	Probably a good idea to move the wallrunning code into on component hit or until MyCharacterMovementComponent.
	I think it's a requirement for networking?
*/

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

AMyProjectCharacter::AMyProjectCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UMyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));
}

void AMyProjectCharacter::BeginPlay()
{
	Super::BeginPlay();
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("AMyProjectCharacter::OnBeginPlay"));
	FString className = *GetCharacterMovement()->GetClass()->GetName();
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, "Movement component class: " + className);

	// Allow player to wall run on spawn
	this->timeSinceLastWallrun = wallrunGracePeriod;
	this->JumpMaxCount = 2;
	//GetCapsuleComponent()->SetGenerateOverlapEvents(true);
	//GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &AMyProjectCharacter::OnOverlapBegin);
	//GetCapsuleComponent()->OnComponentEndOverlap.AddDynamic(this, &AMyProjectCharacter::OnOverlapEnd);
}

void AMyProjectCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	//if(!isWallrunning && timeSinceLastWallrun <= wallrunGracePeriod)
	//	timeSinceLastWallrun += DeltaTime;
	
	UMyCharacterMovementComponent* MoveComp = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());
	// TODO: Move to seperate event
	// TODO: Have people speed up when wallrunning
	// TODO: What if a user runs into a wall? Does this get solved when vaulting is enabled?
	if(isWallrunning) {
		// This is a REALLY overkill and stupid way of doing this
		FVector capsuleLocation = GetCapsuleComponent()->GetComponentLocation();
		FVector start = capsuleLocation + FVector(15, 0, 0), end = capsuleLocation - FVector(15, 0, 0);
		FHitResult result;
		FCollisionQueryParams traceParams;
		traceParams.AddIgnoredActor(this);
		float circleRadius = 50.0f;
		bool hit = GetWorld()->SweepSingleByChannel(result, start, end, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(circleRadius), traceParams);

		if(hit)
 			MoveComp->Velocity = this->wallrunningDirection * this->wallHitSpeed * 1.25;
		else
			this->isWallrunning = false;
		return;
	}
	
	FVector velocity = GetVelocity();
	// Are we falling? If yes, start checking for a wall nearby
	if(velocity.Z < 0/* && timeSinceLastWallrun >= wallrunGracePeriod*/) {
		FVector CapsuleLocation = GetCapsuleComponent()->GetComponentLocation();
		int wallrunningDistance = 15;
		FVector start = CapsuleLocation + FVector(wallrunningDistance, 0, 0), end = CapsuleLocation - FVector(wallrunningDistance, 0, 0);
		
		// Is the player standing next to a wall?
		FHitResult result;
		FCollisionQueryParams TraceParams; // <--- Add "only static walls" or something
		TraceParams.AddIgnoredActor(this);
		float circleRadius = 50.0f;
        bool hit = GetWorld()->SweepSingleByChannel(result, start, end, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(circleRadius), TraceParams);
		// TODO: Handle this better in case more things to check are added below
		if(!hit)
			return;
		// Is the player looking at the wall?
		FRotator xyz = UKismetMathLibrary::FindLookAtRotation(MoveComp->GetActorLocation(), result.GetActor()->GetActorLocation());
		float playerYaw = std::abs(xyz.Yaw);
		float yawThreshold = 10.0;
		float rightAngle = 90.0;
		float maxAngle = rightAngle + yawThreshold;
		float minAngle = rightAngle - yawThreshold;
d		/*if(!(playerYaw <= maxAngle && playerYaw >= minAngle))
			return;
		*/
		//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("BEGIN WALLRUN"));
		// TODO: Check FHitResult to check the normal (or rotation?), maybe only run on directly upright walls or add in some kind of tolerance
		this->isWallrunning = true;

		FVector charVelocity = MoveComp->Velocity;
		// Save the sign to determine which way to launch the character when jumping off
		this->signOfX = UKismetMathLibrary::SignOfFloat(result.Normal.X);
		// What is the difference between the rotation of the wall to origin and where the player is looking to origin?
		// Sign of the yaw will determine which direction to wall run along, i.e., do we rotate the object's normal left or right?
		FRotator x = UKismetMathLibrary::FindLookAtRotation(FVector(0, 0, 0), charVelocity);
		FRotator y = UKismetMathLibrary::FindLookAtRotation(FVector(0, 0, 0), result.Normal);
		FRotator z = UKismetMathLibrary::NormalizedDeltaRotator(x, y);
		double wallrunningSign = UKismetMathLibrary::SignOfFloat(z.Yaw);
		
		UPrimitiveComponent* HitComponent = result.GetComponent();
		FBox box = HitComponent->Bounds.GetBox();
		
		double wallExtent = wallrunningSign > 0.0 ? box.Max.Y : box.Min.Y;
		double hitLocation = HitComponent->GetComponentLocation().Y;
		this->wallrunningYCutoff += wallExtent;

		FRotator z2(0, wallrunningSign * 90, 0);
		FVector x2(result.Normal.X, result.Normal.Y, 0);
		this->wallrunningDirection = z2.RotateVector(x2);

		// Maintain the same speed the player had when they hit the wall
		this->wallHitSpeed = FVector(charVelocity.X, charVelocity.Y, 0.0).Length();
	}
}

/*
void AMyProjectCharacter::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("AMyProjectCharacter::OnOverlapBegin"));	
}

void AMyProjectCharacter::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("AMyProjectCharacter::OnOverlapEnd"));
}
*/

void AMyProjectCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
}

void AMyProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	if(UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AMyProjectCharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Move);

		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyProjectCharacter::Look);
	}
}

void AMyProjectCharacter::CheckJumpInput(float DeltaTime)
{
	JumpCurrentCountPreJump = JumpCurrentCount;
	UMyCharacterMovementComponent* CharMov = Cast<UMyCharacterMovementComponent>(GetCharacterMovement());
	if(!CharMov || !bPressedJump) // Guard
		return;
	
	// If this is the first jump and we're already falling then increment the JumpCount to compensate.
	const bool bFirstJump = JumpCurrentCount == 0;
	if(bFirstJump && CharMov->IsFalling())
		JumpCurrentCount++;
	
	const bool bDidJump = CanJump() && (true) ? CharMov->DoJump(bClientUpdating) : CharMov->DoJump(bClientUpdating, DeltaTime);
	if(bDidJump && !bWasJumping) {
		// Transition from not (actively) jumping to jumping.
		JumpCurrentCount++;
		JumpForceTimeRemaining = GetJumpMaxHoldTime();
		OnJumped();
	}
	bWasJumping = bDidJump;
}

//void AMyProjectCharacter::Jump()
//{
//	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("AMyProjectCharacter::Jump"));	
//	Super::Jump();
//}

void AMyProjectCharacter::Move(const FInputActionValue& Value)
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if(!Controller)
		return;
	FVector2D MovementVector = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), MovementVector.Y);
	AddMovementInput(GetActorRightVector(), MovementVector.X);
}

void AMyProjectCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	if(!Controller)
		return;
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

