// Copyright Jeff Brown 2020

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HandController.h"
#include "VRCharacter.generated.h"

UCLASS()
class ARCHEXPLORER_API AVRCharacter : public ACharacter
{
  GENERATED_BODY()

public:
  // Sets default values for this character's properties
  AVRCharacter();

protected:
  // Called when the game starts or when spawned
  virtual void BeginPlay() override;

public:
  // Called every frame
  virtual void Tick(float DeltaTime) override;

  // Called to bind functionality to input
  virtual void SetupPlayerInputComponent(class UInputComponent *PlayerInputComponent) override;

private: // Methods
  bool FindTeleportDestination(TArray<FVector> &OutPath, FVector &OutLocation);
  void UpdateDestinationMarker();
  void UpdateBlinkers();
  FVector2D GetBlinkerCenter();
  void DrawTeleportPath(const TArray<FVector> &Path);
  void UpdateSpline(const TArray<FVector> &Path);

  void MoveForward(float throttle);
  void MoveRight(float throttle);

  void GripLeft() { LeftHandController->Grip(); }
  void ReleaseLeft() { LeftHandController->Release(); }
  void GripRight() { RightHandController->Grip(); }
  void ReleaseRight() { RightHandController->Release(); }

  void EnableTeleportation(float throttle);
  void BeginTeleport();
  void FinishTeleport();

  void StartFade(float FromAlpha, float ToAlpha);

private: // Components
  UPROPERTY(VisibleAnywhere)
  class USceneComponent *VRRoot;

  UPROPERTY(VisibleAnywhere)
  class UCameraComponent *Camera;

  UPROPERTY(VisibleAnywhere)
  AHandController *LeftHandController;

  UPROPERTY(VisibleAnywhere)
  AHandController *RightHandController;

  UPROPERTY(VisibleAnywhere)
  class USplineComponent *TeleportPath;

  UPROPERTY(VisibleAnywhere)
  class UStaticMeshComponent *DestinationMarker;

  UPROPERTY(VisibleAnywhere)
  class UPostProcessComponent *PostProcessComponent;

  UPROPERTY(VisibleAnywhere)
  class UMaterialInstanceDynamic *BlinkerMaterialInstance;

  UPROPERTY(VisibleAnywhere)
  TArray<class USplineMeshComponent *> TeleportPathMeshPool;

private: // configurable parameters
  UPROPERTY(EditAnywhere)
  bool bTeleportEnabled = false;

  UPROPERTY(EditAnywhere)
  float TeleportThumbstickThreshold = -0.4f;

  UPROPERTY(EditAnywhere)
  float TeleportProjectileRadius = 10.f;

  UPROPERTY(EditAnywhere)
  float TeleportProjectileSpeed = 1000.f; // 10 cm/s

  UPROPERTY(EditAnywhere)
  float TeleportSimulationTime = 3.f; // 10 s

  UPROPERTY(EditAnywhere)
  float TeleportFadeTime = 0.2f; // Half-Second fade

  UPROPERTY(EditAnywhere)
  FVector TeleportProjectionExtent = FVector(100, 100, 100);

  UPROPERTY(EditAnywhere)
  class UMaterialInterface *BlinkerMaterialBase;

  UPROPERTY(EditAnywhere)
  class UCurveFloat *RadiusVsVelocity;

  UPROPERTY(EditDefaultsOnly)
  class UStaticMesh *TeleportArcMesh;

  UPROPERTY(EditDefaultsOnly)
  class UMaterialInstance *TeleportArcMaterial;

  UPROPERTY(EditDefaultsOnly)
  TSubclassOf<AHandController> HandControllerClass;
};
