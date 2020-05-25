// Copyright Jeff Brown 2020

#include "HandController.h"

// Sets default values
AHandController::AHandController()
{
  // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
  PrimaryActorTick.bCanEverTick = true;

  MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionController"));
  SetRootComponent(MotionController);
}

// Called when the game starts or when spawned
void AHandController::BeginPlay()
{
  Super::BeginPlay();
}

// Called every frame
void AHandController::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);
}
