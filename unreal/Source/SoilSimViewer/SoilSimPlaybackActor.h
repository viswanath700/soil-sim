#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoilSimPlaybackActor.generated.h"

class UProceduralMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;

UCLASS()
class SOILSIMVIEWER_API ASoilSimPlaybackActor final : public AActor
{
    GENERATED_BODY()

public:
    ASoilSimPlaybackActor();

    UPROPERTY(EditAnywhere, Category = "SoilSim")
    FString HeightfieldFile;

protected:
    virtual void BeginPlay() override;

private:
    bool LoadHeightfield();
    void BuildMesh();

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UDirectionalLightComponent> SunLight;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkyLightComponent> SkyLight;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

    uint64 Columns = 0;
    uint64 Rows = 0;
    double CellSize = 0.0;
    double OriginX = 0.0;
    double OriginY = 0.0;
    TArray<double> Heights;
};
