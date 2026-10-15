#include "SoilSimPlaybackActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "GameFramework/Pawn.h"
#include "ProceduralMeshComponent.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"

#include "soilsim/heightfield.h"

#include <filesystem>
#include <exception>

namespace
{
constexpr float kCentimetresPerMetre = 100.0f;
}

ASoilSimPlaybackActor::ASoilSimPlaybackActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HeightfieldMesh"));
    RootComponent = Mesh;
    SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    SunLight->SetupAttachment(RootComponent);
    SunLight->SetMobility(EComponentMobility::Movable);
    SunLight->SetIntensity(10.0f);
    SunLight->SetRelativeRotation(FRotator(-50.0f, -35.0f, 0.0f));
    SunLight->SetAtmosphereSunLight(true);
    SunLight->SetAtmosphereSunLightIndex(0);
    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(RootComponent);
    SkyLight->SetMobility(EComponentMobility::Movable);
    SkyLight->SetIntensity(1.0f);
    SkyLight->SetRealTimeCaptureEnabled(true);
    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    SkyAtmosphere->SetupAttachment(RootComponent);
    HeightfieldFile = TEXT("../out/sample/heightfield_final.bin");
}

void ASoilSimPlaybackActor::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Display, TEXT("SoilSim playback actor started in level %s"),
        *GetWorld()->GetMapName());
    if (LoadHeightfield()) {
        BuildMesh();
        const FVector center(
            static_cast<float>(OriginX + (static_cast<double>(Columns) * CellSize) * 0.5) *
                kCentimetresPerMetre,
            static_cast<float>(OriginY + (static_cast<double>(Rows) * CellSize) * 0.5) *
                kCentimetresPerMetre,
            0.0f);
        const FVector cameraLocation = center + FVector(0.0f, -1000.0f, 800.0f);
        if (APlayerController* playerController = UGameplayStatics::GetPlayerController(this, 0)) {
            APawn* pawn = playerController->GetPawn();
            if (pawn == nullptr) {
                UE_LOG(LogTemp, Warning, TEXT("SoilSim could not find a player pawn for free-fly playback"));
                return;
            }
            pawn->SetActorLocation(cameraLocation);
            const FRotator cameraRotation = (center - cameraLocation).Rotation();
            pawn->SetActorRotation(cameraRotation);
            playerController->SetControlRotation(cameraRotation);
            playerController->SetViewTarget(pawn);
            UE_LOG(LogTemp, Display, TEXT("SoilSim positioned free-fly camera pawn at %s"),
                *cameraLocation.ToString());
        } else {
            UE_LOG(LogTemp, Warning, TEXT("SoilSim could not find a player controller for playback"));
        }
    }
}

bool ASoilSimPlaybackActor::LoadHeightfield()
{
    try {
        const FString absolutePath = FPaths::ConvertRelativePathToFull(
            FPaths::ProjectDir(), HeightfieldFile);
        UE_LOG(LogTemp, Display, TEXT("SoilSim loading heightfield: %s"), *absolutePath);
        const std::filesystem::path filePath(TCHAR_TO_UTF8(*absolutePath));
        const soilsim::Heightfield heightfield = soilsim::Heightfield::readBinary(filePath);
        Columns = static_cast<uint64>(heightfield.columns());
        Rows = static_cast<uint64>(heightfield.rows());
        CellSize = heightfield.cellSize();
        OriginX = heightfield.originX();
        OriginY = heightfield.originY();
        Heights.Reserve(static_cast<int32>(Columns * Rows));
        for (const double height : heightfield.heights()) {
            Heights.Add(height);
        }
        UE_LOG(LogTemp, Display, TEXT("SoilSim loaded heightfield: %llu x %llu, cell size %.3f"),
            Columns, Rows, CellSize);
        return true;
    } catch (const std::exception& error) {
        UE_LOG(LogTemp, Error, TEXT("SoilSim heightfield load failed: %s"), UTF8_TO_TCHAR(error.what()));
        return false;
    }
}

void ASoilSimPlaybackActor::BuildMesh()
{
    if (Columns < 2 || Rows < 2 || Heights.Num() != static_cast<int32>(Columns * Rows)) {
        UE_LOG(LogTemp, Error, TEXT("SoilSim heightfield has insufficient samples for a mesh"));
        return;
    }

    TArray<FVector> vertices;
    TArray<int32> triangles;
    TArray<FVector> normals;
    TArray<FVector2D> coordinates;
    TArray<FColor> colors;
    TArray<FProcMeshTangent> tangents;
    vertices.Reserve(Heights.Num());
    coordinates.Reserve(Heights.Num());
    colors.Reserve(Heights.Num());
    for (uint64 row = 0; row < Rows; ++row) {
        for (uint64 column = 0; column < Columns; ++column) {
            const int32 index = static_cast<int32>(row * Columns + column);
            vertices.Add(FVector(
                static_cast<float>(OriginX + (static_cast<double>(column) + 0.5) * CellSize),
                static_cast<float>(OriginY + (static_cast<double>(row) + 0.5) * CellSize),
                static_cast<float>(Heights[index])) *
                kCentimetresPerMetre);
            coordinates.Add(FVector2D(
                static_cast<float>(column) / static_cast<float>(Columns - 1),
                static_cast<float>(row) / static_cast<float>(Rows - 1)));
            colors.Add(FColor::White);
        }
    }
    for (uint64 row = 0; row + 1 < Rows; ++row) {
        for (uint64 column = 0; column + 1 < Columns; ++column) {
            const int32 lowerLeft = static_cast<int32>(row * Columns + column);
            const int32 lowerRight = lowerLeft + 1;
            const int32 upperLeft = static_cast<int32>((row + 1) * Columns + column);
            const int32 upperRight = upperLeft + 1;
            triangles.Append({lowerLeft, upperRight, lowerRight, lowerLeft, upperLeft, upperRight});
        }
    }
    Mesh->CreateMeshSection(0, vertices, triangles, normals, coordinates, colors, tangents, true);
    UE_LOG(LogTemp, Display, TEXT("SoilSim created playback mesh: %d vertices, %d triangles"),
        vertices.Num(), triangles.Num() / 3);
}
