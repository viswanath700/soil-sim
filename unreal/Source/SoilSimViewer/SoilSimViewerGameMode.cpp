#include "SoilSimViewerGameMode.h"

#include "GameFramework/DefaultPawn.h"

ASoilSimViewerGameMode::ASoilSimViewerGameMode()
{
    DefaultPawnClass = ADefaultPawn::StaticClass();
}
