// Copyright Jeff Brown 2020

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MotionControllerComponent.h"
#include "HandController.generated.h"

UCLASS()
class ARCHEXPLORER_API AHandController : public AActor
{
  GENERATED_BODY()

public:
  // Sets default values for this actor's properties
  AHandController();

  void SetHand(EControllerHand Hand) { MotionController->SetTrackingSource(Hand); }

protected:
  // Called when the game starts or when spawned
  virtual void BeginPlay() override;

public:
  // Called every frame
  virtual void Tick(float DeltaTime) override;

private:
  //default subobject
  UPROPERTY(VisibleAnywhere)
  class UMotionControllerComponent *MotionController;
};
