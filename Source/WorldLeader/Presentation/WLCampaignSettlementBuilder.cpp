// Copyright World Leader project. See ROADMAP.md.

#include "Presentation/WLCampaignSettlementBuilder.h"

#include "ProceduralMeshComponent.h"

namespace
{
	struct FSettlementMeshBuffer
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
	};

	struct FSettlementBlock
	{
		FVector Center;
		FVector Extent;
		FLinearColor Color;
	};

	FLinearColor Shade(const FLinearColor& Color, float Amount)
	{
		return FLinearColor(
			FMath::Clamp(Color.R * Amount, 0.f, 1.f),
			FMath::Clamp(Color.G * Amount, 0.f, 1.f),
			FMath::Clamp(Color.B * Amount, 0.f, 1.f),
			Color.A);
	}

	FColor ToVertexColor(const FLinearColor& Color)
	{
		return Color.ToFColor(false);
	}

	float TypeScale(EWLCampaignSettlementType Type)
	{
		switch (Type)
		{
		case EWLCampaignSettlementType::Capital:
			return 1.42f;
		case EWLCampaignSettlementType::LargeCity:
			return 1.20f;
		case EWLCampaignSettlementType::Port:
			return 1.12f;
		case EWLCampaignSettlementType::Industrial:
			return 1.08f;
		case EWLCampaignSettlementType::Frontier:
			return 0.86f;
		default:
			return 1.0f;
		}
	}

	float DefaultYaw(const FWLCampaignSettlementSpec& Spec)
	{
		const float Seed = FMath::Sin(Spec.Lon * 12.9898f + Spec.Lat * 78.233f);
		return Spec.YawDegrees != 0.f ? Spec.YawDegrees : FMath::Fmod(FMath::Abs(Seed) * 360.f, 58.f) - 29.f;
	}

	void AddFace(
		FSettlementMeshBuffer& Buffer,
		const FVector& A,
		const FVector& B,
		const FVector& C,
		const FVector& D,
		const FLinearColor& Color)
	{
		const int32 Start = Buffer.Verts.Num();
		Buffer.Verts.Append({ A, B, C, D });
		Buffer.Tris.Append({ Start, Start + 1, Start + 2, Start, Start + 2, Start + 3 });

		const FVector Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		Buffer.Normals.Append({ Normal, Normal, Normal, Normal });
		Buffer.UVs.Append({
			FVector2D(0.f, 0.f),
			FVector2D(1.f, 0.f),
			FVector2D(1.f, 1.f),
			FVector2D(0.f, 1.f)
		});
		const FColor VertexColor = ToVertexColor(Color);
		Buffer.Colors.Append({ VertexColor, VertexColor, VertexColor, VertexColor });
	}

	void AddBox(
		FSettlementMeshBuffer& Buffer,
		const FVector& Center,
		const FRotator& Rotation,
		const FVector& Extent,
		const FLinearColor& Color)
	{
		const FVector X = Rotation.RotateVector(FVector(Extent.X, 0.f, 0.f));
		const FVector Y = Rotation.RotateVector(FVector(0.f, Extent.Y, 0.f));
		const FVector Z = FVector(0.f, 0.f, Extent.Z);

		const FVector P0 = Center - X - Y - Z;
		const FVector P1 = Center + X - Y - Z;
		const FVector P2 = Center + X + Y - Z;
		const FVector P3 = Center - X + Y - Z;
		const FVector P4 = Center - X - Y + Z;
		const FVector P5 = Center + X - Y + Z;
		const FVector P6 = Center + X + Y + Z;
		const FVector P7 = Center - X + Y + Z;

		AddFace(Buffer, P4, P5, P6, P7, Shade(Color, 1.20f));
		AddFace(Buffer, P0, P3, P2, P1, Shade(Color, 0.78f));
		AddFace(Buffer, P0, P1, P5, P4, Shade(Color, 0.92f));
		AddFace(Buffer, P1, P2, P6, P5, Shade(Color, 0.82f));
		AddFace(Buffer, P2, P3, P7, P6, Shade(Color, 0.70f));
		AddFace(Buffer, P3, P0, P4, P7, Shade(Color, 0.86f));
	}

	void AddLocalBox(
		FSettlementMeshBuffer& Buffer,
		const FVector& Origin,
		const FRotator& Rotation,
		const FVector& LocalCenter,
		const FVector& Extent,
		const FLinearColor& Color,
		float Scale)
	{
		AddBox(Buffer, Origin + Rotation.RotateVector(LocalCenter * Scale), Rotation, Extent * Scale, Color);
	}

	void AddBlocks(
		FSettlementMeshBuffer& Buffer,
		const FVector& Origin,
		const FRotator& Rotation,
		const TArray<FSettlementBlock>& Blocks,
		float Scale)
	{
		for (const FSettlementBlock& Block : Blocks)
		{
			AddLocalBox(Buffer, Origin, Rotation, Block.Center, Block.Extent, Block.Color, Scale);
		}
	}

	void AddCommonApproach(
		FSettlementMeshBuffer& Buffer,
		const FVector& Origin,
		const FRotator& Rotation,
		float Scale)
	{
		const FLinearColor Road = FLinearColor(0.48f, 0.47f, 0.40f, 1.f);
		AddLocalBox(Buffer, Origin, Rotation, FVector(-2450.f, 40.f, 34.f), FVector(1700.f, 110.f, 30.f), Road, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(1640.f, -760.f, 34.f), FVector(1350.f, 96.f, 28.f), Road, Scale);
	}

	void AddCapital(
		FSettlementMeshBuffer& Buffer,
		const FVector& Origin,
		const FRotator& Rotation,
		const FLinearColor& Accent,
		float Scale)
	{
		const FLinearColor Stone(0.28f, 0.27f, 0.23f, 1.f);
		const FLinearColor Glass(0.075f, 0.105f, 0.105f, 1.f);
		const FLinearColor Roof(0.24f, 0.19f, 0.135f, 1.f);

		AddLocalBox(Buffer, Origin, Rotation, FVector(0.f, 0.f, 42.f), FVector(2500.f, 1900.f, 42.f), Stone, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(0.f, 0.f, 96.f), FVector(840.f, 760.f, 24.f), Shade(Accent, 0.70f), Scale);
		AddBlocks(Buffer, Origin, Rotation, {
			{ FVector(-820.f, -520.f, 360.f), FVector(380.f, 340.f, 300.f), Stone },
			{ FVector(-150.f, -670.f, 520.f), FVector(330.f, 300.f, 460.f), Glass },
			{ FVector(610.f, -560.f, 420.f), FVector(420.f, 260.f, 360.f), Stone },
			{ FVector(-1060.f, 210.f, 470.f), FVector(320.f, 290.f, 410.f), Glass },
			{ FVector(-260.f, 160.f, 820.f), FVector(360.f, 360.f, 760.f), Glass },
			{ FVector(530.f, 230.f, 520.f), FVector(340.f, 330.f, 460.f), Stone },
			{ FVector(1210.f, 120.f, 360.f), FVector(270.f, 240.f, 300.f), Roof },
			{ FVector(340.f, 830.f, 380.f), FVector(500.f, 240.f, 320.f), Stone },
			{ FVector(-680.f, 910.f, 320.f), FVector(460.f, 220.f, 260.f), Roof }
		}, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(-260.f, 160.f, 1690.f), FVector(470.f, 470.f, 95.f), Accent, Scale);
	}

	void AddLargeOrMediumCity(
		FSettlementMeshBuffer& Buffer,
		const FVector& Origin,
		const FRotator& Rotation,
		const FLinearColor& Accent,
		float Scale,
		bool bLarge)
	{
		const FLinearColor Concrete(0.30f, 0.295f, 0.25f, 1.f);
		const FLinearColor Glass(0.070f, 0.100f, 0.100f, 1.f);
		const FLinearColor Roof(0.26f, 0.205f, 0.155f, 1.f);
		AddLocalBox(Buffer, Origin, Rotation, FVector(0.f, 0.f, 38.f), FVector(2050.f, 1450.f, 38.f), Concrete, Scale);
		AddBlocks(Buffer, Origin, Rotation, {
			{ FVector(-740.f, -430.f, 310.f), FVector(360.f, 280.f, 270.f), Concrete },
			{ FVector(-150.f, -470.f, 390.f), FVector(270.f, 250.f, 350.f), Glass },
			{ FVector(430.f, -360.f, 270.f), FVector(350.f, 250.f, 230.f), Roof },
			{ FVector(-690.f, 280.f, 250.f), FVector(300.f, 240.f, 210.f), Roof },
			{ FVector(30.f, 210.f, bLarge ? 600.f : 420.f), FVector(310.f, 300.f, bLarge ? 560.f : 380.f), Glass },
			{ FVector(670.f, 260.f, 310.f), FVector(340.f, 260.f, 270.f), Concrete },
			{ FVector(270.f, 830.f, 230.f), FVector(460.f, 170.f, 190.f), Concrete }
		}, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(30.f, 210.f, bLarge ? 1220.f : 850.f), FVector(390.f, 390.f, 64.f), Accent, Scale);
	}

	void AddPortDetails(
		FSettlementMeshBuffer& Buffer,
		const FVector& Origin,
		const FRotator& Rotation,
		const FLinearColor& Accent,
		float Scale)
	{
		const FLinearColor Dock(0.16f, 0.145f, 0.120f, 1.f);
		const FLinearColor Warehouse(0.30f, 0.285f, 0.235f, 1.f);
		const FLinearColor Container(0.46f, 0.30f, 0.16f, 1.f);

		AddLargeOrMediumCity(Buffer, Origin, Rotation, Accent, Scale * 0.92f, false);
		AddLocalBox(Buffer, Origin, Rotation, FVector(-180.f, -2150.f, 38.f), FVector(1900.f, 115.f, 38.f), Dock, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(760.f, -2860.f, 34.f), FVector(980.f, 95.f, 34.f), Dock, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(-1260.f, -1560.f, 200.f), FVector(480.f, 210.f, 160.f), Warehouse, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(-430.f, -1600.f, 165.f), FVector(420.f, 180.f, 125.f), Warehouse, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(510.f, -1580.f, 120.f), FVector(330.f, 130.f, 80.f), Container, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(1000.f, -1570.f, 118.f), FVector(260.f, 130.f, 78.f), Shade(Accent, 0.95f), Scale);
	}

	void AddIndustrial(
		FSettlementMeshBuffer& Buffer,
		const FVector& Origin,
		const FRotator& Rotation,
		const FLinearColor& Accent,
		float Scale)
	{
		const FLinearColor Ground(0.24f, 0.225f, 0.190f, 1.f);
		const FLinearColor Plant(0.15f, 0.165f, 0.155f, 1.f);
		const FLinearColor Tank(0.34f, 0.315f, 0.250f, 1.f);

		AddLocalBox(Buffer, Origin, Rotation, FVector(0.f, 0.f, 40.f), FVector(2350.f, 1500.f, 40.f), Ground, Scale);
		AddBlocks(Buffer, Origin, Rotation, {
			{ FVector(-880.f, -380.f, 300.f), FVector(520.f, 300.f, 260.f), Plant },
			{ FVector(90.f, -440.f, 240.f), FVector(620.f, 240.f, 200.f), Plant },
			{ FVector(780.f, -260.f, 360.f), FVector(310.f, 270.f, 320.f), Tank },
			{ FVector(1160.f, 310.f, 390.f), FVector(240.f, 240.f, 350.f), Tank },
			{ FVector(270.f, 470.f, 760.f), FVector(115.f, 115.f, 720.f), Accent },
			{ FVector(-160.f, 590.f, 620.f), FVector(95.f, 95.f, 580.f), Accent },
			{ FVector(-920.f, 520.f, 250.f), FVector(430.f, 230.f, 210.f), Plant }
		}, Scale);
	}

	void AddFrontier(
		FSettlementMeshBuffer& Buffer,
		const FVector& Origin,
		const FRotator& Rotation,
		const FLinearColor& Accent,
		float Scale)
	{
		const FLinearColor Dust(0.31f, 0.285f, 0.220f, 1.f);
		const FLinearColor Building(0.22f, 0.210f, 0.180f, 1.f);

		AddLocalBox(Buffer, Origin, Rotation, FVector(0.f, 0.f, 36.f), FVector(1650.f, 1050.f, 36.f), Dust, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(-350.f, -180.f, 210.f), FVector(420.f, 250.f, 175.f), Building, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(420.f, -80.f, 170.f), FVector(360.f, 220.f, 135.f), Building, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(-980.f, 420.f, 290.f), FVector(100.f, 100.f, 260.f), Accent, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(980.f, 420.f, 290.f), FVector(100.f, 100.f, 260.f), Accent, Scale);
		AddLocalBox(Buffer, Origin, Rotation, FVector(0.f, 420.f, 560.f), FVector(1080.f, 70.f, 58.f), Accent, Scale);
	}
}

void FWLCampaignSettlementBuilder::AddSettlement(
	UProceduralMeshComponent* SettlementMesh,
	UMaterialInterface* SettlementMaterial,
	const FWLCampaignSettlementSpec& Spec,
	TFunctionRef<FVector(float Lon, float Lat)> ProjectLonLat)
{
	if (!SettlementMesh)
	{
		return;
	}

	FSettlementMeshBuffer Buffer;
	const float Scale = FMath::Max(0.25f, Spec.Scale) * TypeScale(Spec.Type);
	const FRotator Rotation(0.f, DefaultYaw(Spec), 0.f);
	const FVector Origin = ProjectLonLat(Spec.Lon, Spec.Lat) + FVector(0.f, 0.f, 900.f);

	AddCommonApproach(Buffer, Origin, Rotation, Scale);
	switch (Spec.Type)
	{
	case EWLCampaignSettlementType::Capital:
		AddCapital(Buffer, Origin, Rotation, Spec.AccentColor, Scale);
		break;
	case EWLCampaignSettlementType::LargeCity:
		AddLargeOrMediumCity(Buffer, Origin, Rotation, Spec.AccentColor, Scale, true);
		break;
	case EWLCampaignSettlementType::Port:
		AddPortDetails(Buffer, Origin, Rotation, Spec.AccentColor, Scale);
		break;
	case EWLCampaignSettlementType::Frontier:
		AddFrontier(Buffer, Origin, Rotation, Spec.AccentColor, Scale);
		break;
	case EWLCampaignSettlementType::Industrial:
		AddIndustrial(Buffer, Origin, Rotation, Spec.AccentColor, Scale);
		break;
	default:
		AddLargeOrMediumCity(Buffer, Origin, Rotation, Spec.AccentColor, Scale, false);
		break;
	}

	if (Buffer.Verts.IsEmpty())
	{
		return;
	}

	const int32 SectionIndex = SettlementMesh->GetNumSections();
	SettlementMesh->CreateMeshSection(
		SectionIndex,
		Buffer.Verts,
		Buffer.Tris,
		Buffer.Normals,
		Buffer.UVs,
		Buffer.Colors,
		Buffer.Tangents,
		false);

	if (SettlementMaterial)
	{
		SettlementMesh->SetMaterial(SectionIndex, SettlementMaterial);
	}
}
