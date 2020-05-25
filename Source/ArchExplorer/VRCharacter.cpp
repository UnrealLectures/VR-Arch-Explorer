// Copyright Jeff Brown 2020

#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "TimerManager.h"
#include "NavigationSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Curves/CurveFloat.h"
#include "Kismet/GameplayStatics.h"
#include "HandController.h"

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

  TeleportPath = CreateDefaultSubobject<USplineComponent>(TEXT("TeleportPath"));
  TeleportPath->SetupAttachment(VRRoot);

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
  if (BlinkerMaterialBase != nullptr)
  {
    BlinkerMaterialInstance = UMaterialInstanceDynamic::Create(BlinkerMaterialBase, this);
    PostProcessComponent->AddOrUpdateBlendable(BlinkerMaterialInstance);
  }

  LeftHandController = GetWorld()->SpawnActor<AHandController>(HandControllerClass);
  if (LeftHandController != nullptr)
  {
    LeftHandController->AttachToComponent(VRRoot, FAttachmentTransformRules::KeepRelativeTransform);
    LeftHandController->SetHand(EControllerHand::Left);
    LeftHandController->SetOwner(this); // FIX FOR 4.22
  }

  RightHandController = GetWorld()->SpawnActor<AHandController>(HandControllerClass);
  if (RightHandController != nullptr)
  {
    RightHandController->SetActorRelativeScale3D(FVector(1.f, -1.f, 1.f)); // Mirror right hand
    RightHandController->AttachToComponent(VRRoot, FAttachmentTransformRules::KeepRelativeTransform);
    RightHandController->SetHand(EControllerHand::Right);
    RightHandController->SetOwner(this); // FIX FOR 4.22
  }
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
  PlayerInputComponent->BindAxis(TEXT("ShowTeleport"), this, &AVRCharacter::EnableTeleportation);

  PlayerInputComponent->BindAction(TEXT("Teleport"), IE_Released, this, &AVRCharacter::BeginTeleport);
}

bool AVRCharacter::FindTeleportDestination(TArray<FVector> &OutPath, FVector &OutLocation)
{
  FVector Start = RightHandController->GetActorLocation(); // Where our hand is
  FVector Look = RightHandController->GetActorForwardVector();

  FPredictProjectilePathParams Params(
      TeleportProjectileRadius,
      Start,
      Look * TeleportProjectileSpeed,
      TeleportSimulationTime,
      ECollisionChannel::ECC_Visibility,
      this);

  Params.bTraceComplex = true;
  FPredictProjectilePathResult Result;
  bool bHit = UGameplayStatics::PredictProjectilePath(this, Params, Result);

  if (!bHit)
    return false;

  FNavLocation NavLocation;
  bool bOnNavMesh = UNavigationSystemV1::GetCurrent(GetWorld())->ProjectPointToNavigation(Result.HitResult.Location, NavLocation, TeleportProjectionExtent);

  if (!bOnNavMesh)
    return false;

  for (FPredictProjectilePathPointData PointData : Result.PathData)
  {
    OutPath.Add(PointData.Location);
  }

  OutLocation = NavLocation.Location;
  return true;
}

void AVRCharacter::UpdateDestinationMarker()
{
  FVector NavLocation;
  TArray<FVector> Path;
  bool bHasDestination = FindTeleportDestination(Path, NavLocation);

  if (bHasDestination && bTeleportEnabled)
  {
    DestinationMarker->SetVisibility(true);
    DestinationMarker->SetWorldLocation(NavLocation);
    DrawTeleportPath(Path);
  }
  else
  {
    DestinationMarker->SetVisibility(false);

    TArray<FVector> EmptyPath;
    DrawTeleportPath(EmptyPath);
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

void AVRCharacter::DrawTeleportPath(const TArray<FVector> &Path)
{
  UpdateSpline(Path);

  for (USplineMeshComponent *SplineMesh : TeleportPathMeshPool)
  {
    SplineMesh->SetVisibility(false);
  }

  int32 SegmentNum = Path.Num() - 1;
  for (int32 i = 0; i < SegmentNum; ++i)
  {
    if (TeleportPathMeshPool.Num() <= i)
    {
      USplineMeshComponent *SplineMesh = NewObject<USplineMeshComponent>(this);
      SplineMesh->SetMobility(EComponentMobility::Movable);
      SplineMesh->AttachToComponent(TeleportPath, FAttachmentTransformRules::KeepRelativeTransform);
      SplineMesh->SetStaticMesh(TeleportArcMesh);
      SplineMesh->SetMaterial(0, TeleportArcMaterial);
      SplineMesh->RegisterComponent(); // Need this to make sure component exists

      TeleportPathMeshPool.Add(SplineMesh);
    }

    USplineMeshComponent *SplineMesh = TeleportPathMeshPool[i];
    SplineMesh->SetVisibility(true);

    FVector StartPos, StartTangent, EndPos, EndTangent;
    TeleportPath->GetLocalLocationAndTangentAtSplinePoint(i, StartPos, StartTangent);
    TeleportPath->GetLocalLocationAndTangentAtSplinePoint(i + 1, EndPos, EndTangent);
    SplineMesh->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent);
  }
}

void AVRCharacter::UpdateSpline(const TArray<FVector> &Path)
{
  TeleportPath->ClearSplinePoints(false); // false so we don't update spline until we add all points and update once at end
  for (int32 i = 0; i < Path.Num(); ++i)
  {
    FVector LocalPosition = TeleportPath->GetComponentTransform().InverseTransformPosition(Path[i]);
    FSplinePoint Point(i, LocalPosition, ESplinePointType::Curve);
    TeleportPath->AddPoint(Point, false);
  }
  TeleportPath->UpdateSpline(); // Update here
}

void AVRCharacter::MoveForward(float throttle)
{
  AddMovementInput(throttle * Camera->GetForwardVector());
}
void AVRCharacter::MoveRight(float throttle)
{
  AddMovementInput(throttle * Camera->GetRightVector());
}

void AVRCharacter::EnableTeleportation(float throttle)
{
  if (throttle < TeleportThumbstickThreshold)
  {
    bTeleportEnabled = true;
  }
  else
  {
    bTeleportEnabled = false;
  }
  //UE_LOG(LogTemp, Warning, TEXT("RThumbstick value: %f"), throttle);
}

void AVRCharacter::BeginTeleport()
{
  if (!bTeleportEnabled)
  {
    return;
  }
  StartFade(0, 1);

  FTimerHandle TimerHandle;
  GetWorldTimerManager().SetTimer(TimerHandle, this, &AVRCharacter::FinishTeleport, TeleportFadeTime);
}

void AVRCharacter::FinishTeleport()
{
  FVector Destination = DestinationMarker->GetComponentLocation();
  Destination += GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * GetActorUpVector();
  SetActorLocation(Destination);

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