// Copyright Jeff Brown 2020

#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "NavigationSystem.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Curves/CurveFloat.h"
#include "MotionControllerComponent.h"

#include "DrawDebugHelpers.h"

// Sets default values
AVRCharacter::AVRCharacter()
{
  // Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
  PrimaryActorTick.bCanEverTick = true;

  VRRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VRRoot"));
  VRRoot->SetupAttachment(GetRootComponent());

  Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
  Camera->SetupAttachment(VRRoot);

  LeftHandController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftHandController"));
  LeftHandController->SetupAttachment(VRRoot);
  LeftHandController->SetTrackingSource(EControllerHand::Left);

  RightHandController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightHandController"));
  RightHandController->SetupAttachment(VRRoot);
  RightHandController->SetTrackingSource(EControllerHand::Right);

  DestinationMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DestinationMarker"));
  DestinationMarker->SetupAttachment(GetRootComponent());

  PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessComponent"));
  PostProcessComponent->SetupAttachment(GetRootComponent());
}

// Called when the game starts or when spawned
void AVRCharacter::BeginPlay()
{
  Super::BeginPlay();
  DestinationMarker->SetVisibility(false);
  if (!BlinkerMaterialBase)
    return;
  BlinkerMaterialInstance = UMaterialInstanceDynamic::Create(BlinkerMaterialBase, this);
  PostProcessComponent->AddOrUpdateBlendable(BlinkerMaterialInstance);
}

// Called every frame
void AVRCharacter::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);

  FVector NewCameraOffset = Camera->GetComponentLocation() - GetActorLocation(); // Difference in pos between actor and camera
  NewCameraOffset.Z = 0;                                                         // Only move horizontal
  AddActorWorldOffset(NewCameraOffset);                                          // Sets up the offset
  VRRoot->AddWorldOffset(-NewCameraOffset);                                      // Reset VR root to counteract offset

  UpdateDestinationMarker();
  UpdateBlinkers();
}

// Called to bind functionality to input
void AVRCharacter::SetupPlayerInputComponent(UInputComponent *PlayerInputComponent)
{
  Super::SetupPlayerInputComponent(PlayerInputComponent);

  PlayerInputComponent->BindAxis(TEXT("Forward"), this, &AVRCharacter::MoveForward);
  PlayerInputComponent->BindAxis(TEXT("Right"), this, &AVRCharacter::MoveRight);
  PlayerInputComponent->BindAction(TEXT("Teleport"), IE_Released, this, &AVRCharacter::BeginTeleport);
}

bool AVRCharacter::FindTeleportDestination(FVector &Location)
{
  //FVector Start = Camera->GetComponentLocation(); // Where our eyes are
  //FVector End = Start + Camera->GetForwardVector() * MaxTeleportDistance;
  FVector Start = RightHandController->GetComponentLocation(); // Where our hand is
  Start += FVector(0, 10, 0);
  FVector Look = RightHandController->GetForwardVector();

  Look = Look.RotateAngleAxis(30, RightHandController->GetRightVector());
  FVector End = Start + Look * MaxTeleportDistance;

  DrawDebugLine(
      GetWorld(),
      Start,
      End,
      FColor(0, 0, 255),
      false,
      0.f,
      0,
      5);

  FHitResult HitResult;
  bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility);

  if (!bHit)
    return false;

  FNavLocation NavLocation;
  bool bOnNavMesh = UNavigationSystemV1::GetCurrent(GetWorld())->ProjectPointToNavigation(HitResult.Location, NavLocation, TeleportProjectionExtent);

  if (!bOnNavMesh)
    return false;

  Location = NavLocation.Location;
  return true;
}

void AVRCharacter::UpdateDestinationMarker()
{
  FVector NavLocation;
  bool bHasDestination = FindTeleportDestination(NavLocation);

  if (bHasDestination)
  {
    DestinationMarker->SetVisibility(true);
    DestinationMarker->SetWorldLocation(NavLocation);
  }
  else
  {
    DestinationMarker->SetVisibility(false);
  }
}

void AVRCharacter::UpdateBlinkers()
{
  if (!RadiusVsVelocity)
    return;
  float Speed = GetVelocity().Size();
  float Radius = RadiusVsVelocity->GetFloatValue(Speed);
  BlinkerMaterialInstance->SetScalarParameterValue(TEXT("Radius"), Radius);

  FVector2D Center = GetBlinkerCenter();
  BlinkerMaterialInstance->SetVectorParameterValue(TEXT("Center"), FLinearColor(Center.X, Center.Y, 0));
}

FVector2D AVRCharacter::GetBlinkerCenter()
{
  // Get the direction we are moving
  FVector MovementDirection = GetVelocity().GetSafeNormal();
  if (MovementDirection.IsNearlyZero())
    return FVector2D(0.5, 0.5);

  // Find stationary point we are looking towards
  // Make sure it's a meter (we'll do 10) in front of camera
  // Use dotproduct to determine if moving forward or back
  FVector WorldStationaryLocation;
  if (FVector::DotProduct(Camera->GetForwardVector(), MovementDirection) > 0)
  {
    WorldStationaryLocation = Camera->GetComponentLocation() + MovementDirection * 1000;
  }
  else
  {
    WorldStationaryLocation = Camera->GetComponentLocation() - MovementDirection * 1000;
  }

  // Grab player controller
  APlayerController *PC = Cast<APlayerController>(GetController());
  if (!PC)
    return FVector2D(0.5, 0.5);

  // Project our location to the screen to get a stationary point
  FVector2D ScreenStationaryLocation;
  PC->ProjectWorldLocationToScreen(WorldStationaryLocation, ScreenStationaryLocation);

  // Convert the stationary location to UV coordinates (these are between 0 and 1)
  int32 SizeX, SizeY;
  PC->GetViewportSize(SizeX, SizeY);
  ScreenStationaryLocation.X /= SizeX;
  ScreenStationaryLocation.Y /= SizeY;

  return ScreenStationaryLocation;
}

void AVRCharacter::MoveForward(float throttle)
{
  AddMovementInput(throttle * Camera->GetForwardVector());
}
void AVRCharacter::MoveRight(float throttle)
{
  AddMovementInput(throttle * Camera->GetRightVector());
}

void AVRCharacter::BeginTeleport()
{
  StartFade(0, 1);

  FTimerHandle TimerHandle;
  GetWorldTimerManager().SetTimer(TimerHandle, this, &AVRCharacter::FinishTeleport, TeleportFadeTime);
}

void AVRCharacter::FinishTeleport()
{
  SetActorLocation(DestinationMarker->GetComponentLocation() + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));

  StartFade(1, 0);
}

void AVRCharacter::StartFade(float FromAlpha, float ToAlpha)
{
  APlayerController *PC = Cast<APlayerController>(GetController());
  if (PC != nullptr)
  {
    PC->PlayerCameraManager->StartCameraFade(FromAlpha, ToAlpha, TeleportFadeTime, FLinearColor::Black);
  }
}