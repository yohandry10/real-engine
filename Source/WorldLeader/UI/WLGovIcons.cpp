// Copyright World Leader project. See ROADMAP.md.

#include "UI/WLGovIcons.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

namespace
{
	// --- Tests de "dentro de la forma" en coordenadas normalizadas [0,1] (centro 0.5,0.5) ---
	FORCEINLINE bool InDisc(float u, float v, float cx, float cy, float r)
	{
		const float dx = u - cx, dy = v - cy;
		return dx * dx + dy * dy <= r * r;
	}
	FORCEINLINE bool InRing(float u, float v, float cx, float cy, float rOut, float rIn)
	{
		const float dx = u - cx, dy = v - cy, d2 = dx * dx + dy * dy;
		return d2 <= rOut * rOut && d2 >= rIn * rIn;
	}
	FORCEINLINE bool InRect(float u, float v, float x0, float y0, float x1, float y1)
	{
		return u >= x0 && u <= x1 && v >= y0 && v <= y1;
	}
	FORCEINLINE bool InCapsule(float u, float v, float ax, float ay, float bx, float by, float halfThick)
	{
		const float dx = bx - ax, dy = by - ay;
		const float len2 = dx * dx + dy * dy;
		float t = len2 > 1e-6f ? ((u - ax) * dx + (v - ay) * dy) / len2 : 0.f;
		t = FMath::Clamp(t, 0.f, 1.f);
		const float px = ax + t * dx, py = ay + t * dy;
		const float ex = u - px, ey = v - py;
		return ex * ex + ey * ey <= halfThick * halfThick;
	}
	FORCEINLINE float EdgeSign(float px, float py, float ax, float ay, float bx, float by)
	{
		return (px - bx) * (ay - by) - (ax - bx) * (py - by);
	}
	FORCEINLINE bool InTri(float u, float v, float ax, float ay, float bx, float by, float cx, float cy)
	{
		const float d1 = EdgeSign(u, v, ax, ay, bx, by);
		const float d2 = EdgeSign(u, v, bx, by, cx, cy);
		const float d3 = EdgeSign(u, v, cx, cy, ax, ay);
		const bool bNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
		const bool bPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
		return !(bNeg && bPos);
	}
	// Estrella de 5 puntas por even-odd sobre 10 vertices.
	bool InStar(float u, float v, float cx, float cy, float rOut, float rIn)
	{
		FVector2D Pts[10];
		for (int32 i = 0; i < 10; ++i)
		{
			const float Ang = -PI / 2.f + i * (PI / 5.f);
			const float R = (i % 2 == 0) ? rOut : rIn;
			Pts[i] = FVector2D(cx + FMath::Cos(Ang) * R, cy + FMath::Sin(Ang) * R);
		}
		bool bInside = false;
		for (int32 i = 0, j = 9; i < 10; j = i++)
		{
			if (((Pts[i].Y > v) != (Pts[j].Y > v)) &&
				(u < (Pts[j].X - Pts[i].X) * (v - Pts[i].Y) / (Pts[j].Y - Pts[i].Y) + Pts[i].X))
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	// Predicado del icono: true si (u,v) normalizado esta pintado.
	bool IconInside(EWLGovIcon Icon, float u, float v)
	{
		switch (Icon)
		{
		case EWLGovIcon::Treasury: // moneda: aro + trazo vertical tipo $
			return InRing(u, v, 0.5f, 0.5f, 0.44f, 0.33f)
				|| InCapsule(u, v, 0.5f, 0.24f, 0.5f, 0.76f, 0.055f);
		case EWLGovIcon::Balance: // barras ascendentes
			return InRect(u, v, 0.16f, 0.56f, 0.34f, 0.86f)
				|| InRect(u, v, 0.41f, 0.38f, 0.59f, 0.86f)
				|| InRect(u, v, 0.66f, 0.20f, 0.84f, 0.86f);
		case EWLGovIcon::Population: // tres personas (cabeza + busto)
		{
			auto Person = [&](float cx, float hy) {
				return InDisc(u, v, cx, hy, 0.11f)
					|| (InDisc(u, v, cx, hy + 0.34f, 0.20f) && v <= hy + 0.36f);
			};
			return Person(0.5f, 0.30f) || Person(0.26f, 0.38f) || Person(0.74f, 0.38f);
		}
		case EWLGovIcon::Provinces: // pin de mapa
			return (InDisc(u, v, 0.5f, 0.38f, 0.26f)
					|| InTri(u, v, 0.26f, 0.46f, 0.74f, 0.46f, 0.5f, 0.88f))
				&& !InDisc(u, v, 0.5f, 0.37f, 0.10f);
		case EWLGovIcon::Growth: // flecha de tendencia al alza
			return InCapsule(u, v, 0.18f, 0.74f, 0.80f, 0.28f, 0.065f)
				|| InTri(u, v, 0.82f, 0.20f, 0.56f, 0.30f, 0.80f, 0.52f);
		case EWLGovIcon::Order: // escudo
			return InRect(u, v, 0.24f, 0.18f, 0.76f, 0.54f)
				|| InTri(u, v, 0.24f, 0.52f, 0.76f, 0.52f, 0.5f, 0.88f);
		case EWLGovIcon::Capital: // estrella
			return InStar(u, v, 0.5f, 0.52f, 0.42f, 0.18f);
		case EWLGovIcon::Politics: // balanza
			return InCapsule(u, v, 0.5f, 0.16f, 0.5f, 0.74f, 0.04f)          // poste
				|| InCapsule(u, v, 0.20f, 0.30f, 0.80f, 0.30f, 0.045f)        // viga
				|| InRect(u, v, 0.38f, 0.74f, 0.62f, 0.80f)                   // base
				|| InRing(u, v, 0.22f, 0.52f, 0.13f, 0.09f)                   // platillo izq
				|| InRing(u, v, 0.78f, 0.52f, 0.13f, 0.09f)                   // platillo der
				|| InCapsule(u, v, 0.22f, 0.30f, 0.22f, 0.44f, 0.02f)         // cuerda izq
				|| InCapsule(u, v, 0.78f, 0.30f, 0.78f, 0.44f, 0.02f);        // cuerda der
		case EWLGovIcon::Diplomacy: // globo
			return InRing(u, v, 0.5f, 0.5f, 0.44f, 0.36f)
				|| (InDisc(u, v, 0.5f, 0.5f, 0.40f)
					&& (InCapsule(u, v, 0.5f, 0.06f, 0.5f, 0.94f, 0.03f)
						|| InCapsule(u, v, 0.06f, 0.5f, 0.94f, 0.5f, 0.03f)
						|| InRing(u, v, 0.5f, 0.5f, 0.24f, 0.20f)));
		case EWLGovIcon::Military: // escudo con cruz
			return (InRect(u, v, 0.24f, 0.18f, 0.76f, 0.54f)
					|| InTri(u, v, 0.24f, 0.52f, 0.76f, 0.52f, 0.5f, 0.88f))
				&& !(InCapsule(u, v, 0.5f, 0.28f, 0.5f, 0.66f, 0.055f)
					|| InCapsule(u, v, 0.34f, 0.44f, 0.66f, 0.44f, 0.055f));
		case EWLGovIcon::Approval: // circulo con check
			return InRing(u, v, 0.5f, 0.5f, 0.44f, 0.35f)
				|| InCapsule(u, v, 0.31f, 0.52f, 0.45f, 0.66f, 0.055f)
				|| InCapsule(u, v, 0.45f, 0.66f, 0.70f, 0.34f, 0.055f);
		case EWLGovIcon::Crisis: // triangulo de alerta con exclamacion
			return (InTri(u, v, 0.5f, 0.14f, 0.10f, 0.84f, 0.90f, 0.84f)
					&& !InTri(u, v, 0.5f, 0.30f, 0.24f, 0.74f, 0.76f, 0.74f))
				|| InRect(u, v, 0.47f, 0.42f, 0.53f, 0.62f)
				|| InDisc(u, v, 0.5f, 0.70f, 0.035f);
		default:
			return InDisc(u, v, 0.5f, 0.5f, 0.4f);
		}
	}

	uint32 IconCacheKey(EWLGovIcon Icon, int32 SizePx, const FLinearColor& Color)
	{
		const FColor C = Color.ToFColor(true);
		uint32 Key = static_cast<uint32>(Icon);
		Key = Key * 131u + static_cast<uint32>(SizePx);
		Key = Key * 131u + C.R;
		Key = Key * 131u + C.G;
		Key = Key * 131u + C.B;
		return Key;
	}
}

namespace
{
	uint32 PortraitHash(const FString& S)
	{
		uint32 H = 2166136261u;
		for (const TCHAR C : S) { H ^= static_cast<uint32>(C); H *= 16777619u; }
		return H;
	}
}

UTexture2D* WLGovIconsNS::GetPortraitTexture(const FString& Seed, const FLinearColor& Accent, int32 W, int32 H)
{
	W = FMath::Clamp(W, 32, 256);
	H = FMath::Clamp(H, 32, 256);
	static TMap<FString, UTexture2D*> Cache;
	const FString Key = FString::Printf(TEXT("%s|%dx%d"), *Seed, W, H);
	if (UTexture2D** Found = Cache.Find(Key))
	{
		return *Found;
	}

	const uint32 Hash = PortraitHash(Seed);
	// Paletas variadas de forma determinista.
	const FLinearColor Skins[] = {
		FLinearColor(0.86f, 0.70f, 0.56f), FLinearColor(0.74f, 0.57f, 0.44f),
		FLinearColor(0.60f, 0.45f, 0.35f), FLinearColor(0.90f, 0.78f, 0.66f),
		FLinearColor(0.48f, 0.35f, 0.27f) };
	const FLinearColor Hairs[] = {
		FLinearColor(0.10f, 0.08f, 0.07f), FLinearColor(0.22f, 0.15f, 0.10f),
		FLinearColor(0.35f, 0.30f, 0.28f), FLinearColor(0.55f, 0.52f, 0.50f),
		FLinearColor(0.28f, 0.20f, 0.13f) };
	const FLinearColor Skin = Skins[Hash % 5];
	const FLinearColor Hair = Hairs[(Hash / 7) % 5];
	// Ropa: tono derivado del acento de la cartera, oscurecido.
	const FLinearColor Cloth = Accent * 0.55f + FLinearColor(0.06f, 0.07f, 0.09f);
	// Fondo: gradiente frio con un toque del acento.
	const FLinearColor BgTop = FLinearColor(0.09f, 0.11f, 0.15f);
	const FLinearColor BgBot = Accent * 0.16f + FLinearColor(0.05f, 0.06f, 0.09f);

	UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
	if (!Tex) { return nullptr; }
	Tex->SRGB = true; Tex->Filter = TF_Bilinear; Tex->AddressX = TA_Clamp; Tex->AddressY = TA_Clamp;

	const int32 SS = 3;
	TArray<FColor> Px; Px.SetNumUninitialized(W * H);
	auto InDisc = [](float u, float v, float cx, float cy, float r){ return FMath::Square(u - cx) + FMath::Square(v - cy) <= r * r; };
	auto InEllipse = [](float u, float v, float cx, float cy, float rx, float ry){ return FMath::Square((u - cx) / rx) + FMath::Square((v - cy) / ry) <= 1.f; };

	for (int32 y = 0; y < H; ++y)
	{
		for (int32 x = 0; x < W; ++x)
		{
			float rr = 0, gg = 0, bb = 0;
			for (int32 sy = 0; sy < SS; ++sy) for (int32 sx = 0; sx < SS; ++sx)
			{
				const float u = (x + (sx + 0.5f) / SS) / W;
				const float v = (y + (sy + 0.5f) / SS) / H;
				FLinearColor Csub;
				// Frente a fondo: cara > pelo > cuello > ropa(hombros) > fondo.
				if (InDisc(u, v, 0.5f, 0.46f, 0.165f))                 Csub = Skin;                 // cara
				else if (InDisc(u, v, 0.5f, 0.40f, 0.205f))            Csub = Hair;                 // pelo (enmarca)
				else if (u > 0.44f && u < 0.56f && v > 0.55f && v < 0.68f) Csub = Skin;             // cuello
				else if (InEllipse(u, v, 0.5f, 1.06f, 0.42f, 0.52f) && v > 0.60f) Csub = Cloth;     // hombros
				else { const float t = FMath::Clamp(v, 0.f, 1.f); Csub = FMath::Lerp(BgTop, BgBot, t); }
				const FColor F = Csub.ToFColor(true);
				rr += F.R; gg += F.G; bb += F.B;
			}
			const int32 Div = SS * SS;
			Px[y * W + x] = FColor(static_cast<uint8>(rr / Div), static_cast<uint8>(gg / Div), static_cast<uint8>(bb / Div), 255);
		}
	}

	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Px.GetData(), Px.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	Tex->UpdateResource();
	Tex->AddToRoot();
	Cache.Add(Key, Tex);
	return Tex;
}

UTexture2D* WLGovIconsNS::GetIconTexture(EWLGovIcon Icon, int32 SizePx, const FLinearColor& Color)
{
	SizePx = FMath::Clamp(SizePx, 12, 128);
	static TMap<uint32, UTexture2D*> Cache;
	const uint32 Key = IconCacheKey(Icon, SizePx, Color);
	if (UTexture2D** Found = Cache.Find(Key))
	{
		if (*Found)
		{
			return *Found;
		}
	}

	UTexture2D* Tex = UTexture2D::CreateTransient(SizePx, SizePx, PF_B8G8R8A8);
	if (!Tex)
	{
		return nullptr;
	}
	Tex->SRGB = true;
	Tex->Filter = TF_Trilinear;
	Tex->AddressX = TA_Clamp;
	Tex->AddressY = TA_Clamp;

	const FColor Tint = Color.ToFColor(true);
	const int32 SS = 4;   // 4x4 supersampling -> anti-aliasing por cobertura
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(SizePx * SizePx);
	for (int32 py = 0; py < SizePx; ++py)
	{
		for (int32 px = 0; px < SizePx; ++px)
		{
			int32 Hits = 0;
			for (int32 sy = 0; sy < SS; ++sy)
			{
				for (int32 sx = 0; sx < SS; ++sx)
				{
					const float u = (px + (sx + 0.5f) / SS) / SizePx;
					const float v = (py + (sy + 0.5f) / SS) / SizePx;
					if (IconInside(Icon, u, v))
					{
						++Hits;
					}
				}
			}
			const uint8 Alpha = static_cast<uint8>((Hits * 255) / (SS * SS));
			Pixels[py * SizePx + px] = FColor(Tint.R, Tint.G, Tint.B, Alpha);
		}
	}

	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	Tex->UpdateResource();
	Tex->AddToRoot();   // el set de iconos vive toda la sesion; evita que el GC lo recolecte

	Cache.Add(Key, Tex);
	return Tex;
}
