// Copyright World Leader project. See ROADMAP.md.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"

// Helper compartido. Antes estaba DUPLICADO (misma funcion en anon-namespace en View/LOD/Visual.cpp);
// al limpiar el working set, el unity build juntaba esos .cpp en un mismo blob -> redefinicion. Una sola
// definicion inline (las inline pueden vivir en varios TU sin romper ODR).
namespace WLRoadDetail
{
	inline UProceduralMeshComponent* FindRoadDetailMesh(AActor* Owner)
	{
		if (!Owner)
		{
			return nullptr;
		}
		TArray<UProceduralMeshComponent*> Components;
		Owner->GetComponents<UProceduralMeshComponent>(Components);
		for (UProceduralMeshComponent* Component : Components)
		{
			if (Component && Component->GetFName() == TEXT("RoadDetailMesh"))
			{
				return Component;
			}
		}
		return nullptr;
	}
}
