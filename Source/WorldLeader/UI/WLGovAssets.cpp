// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLGovAssets.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

namespace
{
	FString ContentPath(const FString& RelPath)
	{
		return FPaths::Combine(FPaths::ProjectContentDir(), RelPath);
	}

	// Textura de fondo generada en runtime: gradiente vertical oscuro + viñeta radial suave.
	// Da profundidad frente al relleno plano (que era parte del look "de debug").
	UTexture2D* BuildProceduralPanel()
	{
		const int32 W = 256, H = 256;
		UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
		if (!Tex)
		{
			return nullptr;
		}
		Tex->SRGB = true;
		Tex->Filter = TF_Bilinear;
		Tex->AddressX = TA_Clamp;
		Tex->AddressY = TA_Clamp;

		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(W * H);
		const FVector2D Center(W * 0.5f, H * 0.42f);
		const float MaxDist = FVector2D(W, H).Size() * 0.62f;
		for (int32 y = 0; y < H; ++y)
		{
			const float GradT = static_cast<float>(y) / H;            // 0 arriba -> 1 abajo
			for (int32 x = 0; x < W; ++x)
			{
				// Base pizarra que se aclara ligeramente hacia abajo.
				float base = FMath::Lerp(0.028f, 0.058f, GradT);
				// Viñeta: se oscurece hacia los bordes.
				const float Dist = FVector2D(x - Center.X, y - Center.Y).Size() / MaxDist;
				base *= FMath::Lerp(1.15f, 0.70f, FMath::Clamp(Dist, 0.f, 1.f));
				// Ruido fino determinista para textura sutil.
				const float N = (FMath::Frac(FMath::Sin((x * 12.9898f + y * 78.233f)) * 43758.5453f) - 0.5f) * 0.010f;
				base += N;
				const float r = FMath::Clamp(base * 0.86f, 0.f, 1.f);
				const float g = FMath::Clamp(base * 0.96f, 0.f, 1.f);
				const float b = FMath::Clamp(base * 1.20f, 0.f, 1.f);   // tinte frio azulado
				Pixels[y * W + x] = FLinearColor(r, g, b, 1.f).ToFColor(true);
			}
		}
		FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
		Mip.BulkData.Unlock();
		Tex->UpdateResource();
		Tex->AddToRoot();
		return Tex;
	}
}

UTexture2D* WLGovAssetsNS::LoadExternalTexture(const FString& RelPath)
{
	static TMap<FString, UTexture2D*> Cache;
	if (UTexture2D** Found = Cache.Find(RelPath))
	{
		return *Found;   // puede ser nullptr cacheado (no existe): no reintentar cada frame
	}

	UTexture2D* Result = nullptr;
	const FString FullPath = ContentPath(RelPath);
	if (IFileManager::Get().FileExists(*FullPath))
	{
		Result = FImageUtils::ImportFileAsTexture2D(FullPath);
		if (Result)
		{
			Result->AddToRoot();   // vive toda la sesion
		}
	}
	Cache.Add(RelPath, Result);
	return Result;
}

UTexture2D* WLGovAssetsNS::GetFlag(const FString& Iso)
{
	const FString Trim = Iso.TrimStartAndEnd();
	if (Trim.IsEmpty())
	{
		return nullptr;
	}
	// Acepta <ISO>.png y <iso>.png (los packs de banderas suelen ser minusculas tipo "br.png").
	if (UTexture2D* Upper = LoadExternalTexture(FString::Printf(TEXT("UI/Flags/%s.png"), *Trim.ToUpper())))
	{
		return Upper;
	}
	return LoadExternalTexture(FString::Printf(TEXT("UI/Flags/%s.png"), *Trim.ToLower()));
}

UTexture2D* WLGovAssetsNS::GetPanelBackground()
{
	if (UTexture2D* External = LoadExternalTexture(TEXT("UI/gov_panel_bg.png")))
	{
		return External;
	}
	static UTexture2D* Procedural = nullptr;
	if (!Procedural)
	{
		Procedural = BuildProceduralPanel();
	}
	return Procedural;
}

bool WLGovAssetsNS::HasFlags()
{
	// Coste barato: existe la carpeta y contiene al menos un .png.
	const FString Dir = ContentPath(TEXT("UI/Flags"));
	if (!IFileManager::Get().DirectoryExists(*Dir))
	{
		return false;
	}
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.png")), true, false);
	return Files.Num() > 0;
}
