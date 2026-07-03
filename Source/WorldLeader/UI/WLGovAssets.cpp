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

namespace
{
	// --- Banderas procedurales de America: patrones reales de franjas + acentos simples ---
	// (H = horizontales, V = verticales). No son pixel-perfect pero son RECONOCIBLES por color.
	struct FFlagDef { const TCHAR* Iso; char Kind; TArray<FLinearColor> Bands; char Emblem; };

	const FLinearColor C_Red   (0.79f, 0.09f, 0.13f);
	const FLinearColor C_Blue  (0.00f, 0.22f, 0.66f);
	const FLinearColor C_LBlue (0.45f, 0.68f, 0.90f);
	const FLinearColor C_Yellow(1.00f, 0.82f, 0.00f);
	const FLinearColor C_Green (0.00f, 0.42f, 0.24f);
	const FLinearColor C_White (0.97f, 0.97f, 0.98f);
	const FLinearColor C_Black (0.06f, 0.06f, 0.07f);

	const TArray<FFlagDef>& FlagTable()
	{
		static const TArray<FFlagDef> T = {
			{ TEXT("AR"), 'H', { C_LBlue, C_White, C_LBlue }, 'o' },   // sol de mayo -> circulo
			{ TEXT("BO"), 'H', { C_Red, C_Yellow, C_Green }, 0 },
			{ TEXT("BR"), 'X', { C_Green }, 'd' },                     // verde + rombo amarillo + circulo azul
			{ TEXT("CA"), 'V', { C_Red, C_White, C_Red }, 'l' },      // hoja -> punto rojo
			{ TEXT("CL"), 'C', { C_White, C_Red }, 's' },             // mitad blanca/roja + canton azul con estrella
			{ TEXT("CO"), 'T', { C_Yellow, C_Blue, C_Red }, 0 },      // amarillo mitad
			{ TEXT("CR"), 'F', { C_Blue, C_White, C_Red, C_White, C_Blue }, 0 },
			{ TEXT("CU"), 'U', { C_Blue, C_White }, 't' },            // franjas + triangulo rojo + estrella
			{ TEXT("DO"), 'W', { C_Blue, C_Red }, 0 },                // cruz blanca sobre cuadrantes
			{ TEXT("EC"), 'T', { C_Yellow, C_Blue, C_Red }, 0 },
			{ TEXT("GT"), 'V', { C_LBlue, C_White, C_LBlue }, 0 },
			{ TEXT("HN"), 'H', { C_LBlue, C_White, C_LBlue }, 0 },
			{ TEXT("MX"), 'V', { C_Green, C_White, C_Red }, 0 },
			{ TEXT("NI"), 'H', { C_LBlue, C_White, C_LBlue }, 0 },
			{ TEXT("PA"), 'Q', { C_White, C_Red, C_Blue, C_White }, 0 },
			{ TEXT("PE"), 'V', { C_Red, C_White, C_Red }, 0 },
			{ TEXT("PY"), 'H', { C_Red, C_White, C_Blue }, 0 },
			{ TEXT("SV"), 'H', { C_Blue, C_White, C_Blue }, 0 },
			{ TEXT("US"), 'S', { C_Red, C_White }, 'c' },             // franjas + canton azul
			{ TEXT("UY"), 'S', { C_White, C_LBlue }, 'c' },
			{ TEXT("VE"), 'H', { C_Yellow, C_Blue, C_Red }, 0 },
			{ TEXT("GY"), 'V', { C_Green, C_Yellow, C_Red }, 0 },
			{ TEXT("SR"), 'F', { C_Green, C_White, C_Red, C_White, C_Green }, 0 },
			{ TEXT("GF"), 'V', { C_Blue, C_White, C_Red }, 0 },       // Francia
			{ TEXT("BS"), 'H', { C_LBlue, C_Yellow, C_LBlue }, 't' },
			{ TEXT("JM"), 'H', { C_Green, C_Black, C_Green }, 0 },
			{ TEXT("TT"), 'H', { C_Red, C_Black, C_Red }, 0 },
			{ TEXT("BB"), 'V', { C_Blue, C_Yellow, C_Blue }, 0 },
			{ TEXT("HT"), 'H', { C_Blue, C_Red }, 0 },
			{ TEXT("DO"), 'W', { C_Blue, C_Red }, 0 },
			{ TEXT("PR"), 'S', { C_Red, C_White }, 't' },
			{ TEXT("AG"), 'H', { C_Black, C_Red, C_Black }, 0 },
			{ TEXT("GD"), 'H', { C_Red, C_Yellow, C_Green }, 0 },
			{ TEXT("LC"), 'H', { C_LBlue, C_LBlue, C_LBlue }, 't' },
			{ TEXT("VC"), 'V', { C_Blue, C_Yellow, C_Green }, 0 },
			{ TEXT("KN"), 'H', { C_Green, C_Red, C_Black }, 0 },
			{ TEXT("DM"), 'H', { C_Green, C_Green, C_Green }, 0 },
			{ TEXT("BZ"), 'H', { C_Blue, C_Red, C_Blue }, 0 },
			{ TEXT("GL"), 'H', { C_White, C_Red }, 'o' },
		};
		return T;
	}

	void FillRect(TArray<FColor>& Px, int32 W, int32 H, int32 x0, int32 y0, int32 x1, int32 y1, const FColor& C)
	{
		for (int32 y = FMath::Max(0, y0); y < FMath::Min(H, y1); ++y)
			for (int32 x = FMath::Max(0, x0); x < FMath::Min(W, x1); ++x)
				Px[y * W + x] = C;
	}
	void FillDisc(TArray<FColor>& Px, int32 W, int32 H, float cx, float cy, float r, const FColor& C)
	{
		for (int32 y = 0; y < H; ++y)
			for (int32 x = 0; x < W; ++x)
				if (FMath::Square(x + 0.5f - cx) + FMath::Square(y + 0.5f - cy) <= r * r)
					Px[y * W + x] = C;
	}

	UTexture2D* BuildProceduralFlag(const FString& Iso)
	{
		const FFlagDef* Def = FlagTable().FindByPredicate([&Iso](const FFlagDef& D){ return Iso.Equals(D.Iso, ESearchCase::IgnoreCase); });
		if (!Def || Def->Bands.Num() == 0)
		{
			return nullptr;
		}
		const int32 W = 96, H = 64;
		TArray<FColor> Px;
		Px.SetNumUninitialized(W * H);
		const int32 N = Def->Bands.Num();
		auto Col = [](const FLinearColor& L){ return L.ToFColor(true); };

		// Fondo por patron.
		switch (Def->Kind)
		{
		case 'V': // franjas verticales iguales
			for (int32 i = 0; i < N; ++i) FillRect(Px, W, H, (W * i) / N, 0, (W * (i + 1)) / N, H, Col(Def->Bands[i]));
			break;
		case 'T': // primera franja horizontal ocupa la mitad, resto se reparte
			FillRect(Px, W, H, 0, 0, W, H / 2, Col(Def->Bands[0]));
			for (int32 i = 1; i < N; ++i) FillRect(Px, W, H, 0, H / 2 + (H / 2) * (i - 1) / (N - 1), W, H / 2 + (H / 2) * i / (N - 1), Col(Def->Bands[i]));
			break;
		case 'C': // mitad superior/inferior + canton
			FillRect(Px, W, H, 0, 0, W, H / 2, Col(Def->Bands[0]));
			FillRect(Px, W, H, 0, H / 2, W, H, Col(Def->Bands[N > 1 ? 1 : 0]));
			FillRect(Px, W, H, 0, 0, W / 3, H / 2, Col(C_Blue));
			break;
		case 'U': // franjas horizontales + triangulo (Cuba)
			for (int32 i = 0; i < 5; ++i) FillRect(Px, W, H, 0, (H * i) / 5, W, (H * (i + 1)) / 5, Col((i % 2 == 0) ? C_Blue : C_White));
			for (int32 y = 0; y < H; ++y) { const int32 tw = static_cast<int32>((1.f - FMath::Abs(y - H * 0.5f) / (H * 0.5f)) * (W * 0.42f)); FillRect(Px, W, H, 0, y, tw, y + 1, Col(C_Red)); }
			break;
		case 'W': // cruz blanca sobre cuadrantes (Dominicana/simpl)
			FillRect(Px, W, H, 0, 0, W / 2, H / 2, Col(C_Blue));   FillRect(Px, W, H, W / 2, 0, W, H / 2, Col(C_Red));
			FillRect(Px, W, H, 0, H / 2, W / 2, H, Col(C_Red));    FillRect(Px, W, H, W / 2, H / 2, W, H, Col(C_Blue));
			FillRect(Px, W, H, W / 2 - 6, 0, W / 2 + 6, H, Col(C_White)); FillRect(Px, W, H, 0, H / 2 - 6, W, H / 2 + 6, Col(C_White));
			break;
		case 'Q': // cuartos (Panama/simpl)
			FillRect(Px, W, H, 0, 0, W / 2, H / 2, Col(C_White));  FillRect(Px, W, H, W / 2, 0, W, H / 2, Col(C_Red));
			FillRect(Px, W, H, 0, H / 2, W / 2, H, Col(C_Blue));   FillRect(Px, W, H, W / 2, H / 2, W, H, Col(C_White));
			break;
		case 'F': // 5 franjas horizontales desiguales aprox iguales
			for (int32 i = 0; i < N; ++i) FillRect(Px, W, H, 0, (H * i) / N, W, (H * (i + 1)) / N, Col(Def->Bands[i]));
			break;
		case 'S': // franjas horizontales (barras) + canton azul
			for (int32 i = 0; i < 7; ++i) FillRect(Px, W, H, 0, (H * i) / 7, W, (H * (i + 1)) / 7, Col((i % 2 == 0) ? Def->Bands[0] : Def->Bands[1]));
			FillRect(Px, W, H, 0, 0, W * 2 / 5, H * 4 / 7, Col(C_Blue));
			break;
		case 'X': // Brasil: verde + rombo amarillo + disco azul
			FillRect(Px, W, H, 0, 0, W, H, Col(C_Green));
			for (int32 y = 0; y < H; ++y) for (int32 x = 0; x < W; ++x)
			{ const float dx = FMath::Abs(x + 0.5f - W * 0.5f) / (W * 0.42f), dy = FMath::Abs(y + 0.5f - H * 0.5f) / (H * 0.42f); if (dx + dy <= 1.f) Px[y * W + x] = Col(C_Yellow); }
			FillDisc(Px, W, H, W * 0.5f, H * 0.5f, H * 0.20f, Col(C_Blue));
			break;
		case 'H':
		default: // franjas horizontales iguales
			for (int32 i = 0; i < N; ++i) FillRect(Px, W, H, 0, (H * i) / N, W, (H * (i + 1)) / N, Col(Def->Bands[i]));
			break;
		}

		// Emblema simple centrado.
		switch (Def->Emblem)
		{
		case 'o': FillDisc(Px, W, H, W * 0.5f, H * 0.5f, H * 0.14f, Col(C_Yellow)); break;   // sol/disco
		case 'l': FillDisc(Px, W, H, W * 0.5f, H * 0.5f, H * 0.16f, Col(C_Red)); break;      // hoja -> punto
		case 's': FillDisc(Px, W, H, W * 0.16f, H * 0.25f, H * 0.10f, Col(C_White)); break;  // estrella -> punto en canton
		case 't': FillDisc(Px, W, H, W * 0.16f, H * 0.5f, H * 0.10f, Col(C_White)); break;
		case 'c': break;
		default: break;
		}

		UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
		if (!Tex) return nullptr;
		Tex->SRGB = true; Tex->Filter = TF_Bilinear; Tex->AddressX = TA_Clamp; Tex->AddressY = TA_Clamp;
		FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
		void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Px.GetData(), Px.Num() * sizeof(FColor));
		Mip.BulkData.Unlock();
		Tex->UpdateResource();
		Tex->AddToRoot();
		return Tex;
	}
}

UTexture2D* WLGovAssetsNS::GetFlag(const FString& Iso)
{
	const FString Trim = Iso.TrimStartAndEnd();
	if (Trim.IsEmpty())
	{
		return nullptr;
	}
	// 1) Bandera real dejada por el usuario (mayus o minus). 2) Bandera procedural. 3) nullptr -> chip.
	if (UTexture2D* Upper = LoadExternalTexture(FString::Printf(TEXT("UI/Flags/%s.png"), *Trim.ToUpper())))
	{
		return Upper;
	}
	if (UTexture2D* Lower = LoadExternalTexture(FString::Printf(TEXT("UI/Flags/%s.png"), *Trim.ToLower())))
	{
		return Lower;
	}
	static TMap<FString, UTexture2D*> ProcCache;
	const FString Key = Trim.ToUpper();
	if (UTexture2D** Found = ProcCache.Find(Key))
	{
		return *Found;
	}
	UTexture2D* Proc = BuildProceduralFlag(Key);
	ProcCache.Add(Key, Proc);
	return Proc;
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
