// Copyright World Leader project. See ROADMAP.md.

#include "Map/WLWorldMap.h"
#include "Campaign/WLDataRegistry.h"
#include "WorldLeader.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

AWLWorldMap::AWLWorldMap()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		CubeMesh = CubeFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		BaseMaterial = MatFinder.Object;
	}
}

void AWLWorldMap::BeginPlay()
{
	Super::BeginPlay();
	BuildMap();
	SetupViewAndLighting();
}

void AWLWorldMap::SetupViewAndLighting()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Luz para que el mapa se vea aunque el nivel base no tenga iluminacion.
	World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(),
		FVector(0.f, 0.f, 5000.f), FRotator(-60.f, 30.f, 0.f));
	World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(),
		FVector(0.f, 0.f, 5000.f), FRotator::ZeroRotator);

	// Camara cenital fijada como vista del jugador (mapa estrategico tipo Total War).
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
		FVector(0.f, 0.f, CameraHeight), FRotator(-90.f, 0.f, 0.f));
	if (Camera)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetViewTargetWithBlend(Camera, 0.4f);
		}
	}
}

UWLDataRegistry* AWLWorldMap::GetRegistry() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UWLDataRegistry>() : nullptr;
}

void AWLWorldMap::BuildMap()
{
	const UWLDataRegistry* Registry = GetRegistry();
	if (!Registry)
	{
		UE_LOG(LogWorldLeader, Warning, TEXT("WorldMap: sin registro de datos."));
		return;
	}

	int32 Count = 0;
	for (const FWLProvinceData& Province : Registry->GetAllProvinces())
	{
		const FVector Location(
			(Province.MapLat - OriginLat) * DegreeScale,   // X = norte
			(Province.MapLon - OriginLon) * DegreeScale,   // Y = este
			0.f);

		// Color de la nacion duena (de Nations.json).
		FLinearColor Color = FLinearColor::Gray;
		FWLNationData Nation;
		if (Registry->GetNation(Province.CountryIso, Nation))
		{
			Color = Nation.MapColor;
		}

		// Cubo marcador.
		UStaticMeshComponent* Cube = NewObject<UStaticMeshComponent>(this);
		Cube->SetupAttachment(SceneRoot);
		Cube->RegisterComponent();
		Cube->SetRelativeLocation(Location);
		Cube->SetRelativeScale3D(MarkerScale);
		if (CubeMesh)
		{
			Cube->SetStaticMesh(CubeMesh);
		}
		if (BaseMaterial)
		{
			if (UMaterialInstanceDynamic* Dyn = Cube->CreateDynamicMaterialInstance(0, BaseMaterial))
			{
				Dyn->SetVectorParameterValue(TEXT("Color"), Color);
			}
		}

		// Etiqueta con el id de la provincia, coloreada por nacion.
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
		Label->SetupAttachment(SceneRoot);
		Label->RegisterComponent();
		Label->SetRelativeLocation(Location + FVector(0.f, 0.f, MarkerScale.Z * 60.f));
		Label->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetWorldSize(120.f);
		Label->SetText(FText::FromString(Province.Id));
		Label->SetTextRenderColor(Color.ToFColor(true));

		++Count;
	}

	UE_LOG(LogWorldLeader, Log, TEXT("WorldMap: %d marcadores de provincia generados."), Count);
}
