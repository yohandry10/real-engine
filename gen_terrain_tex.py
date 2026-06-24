"""
Genera TEXTURAS de terreno tileables (seamless) PBR-ish para el material lit del mapa Campaign 3D:
pasto, roca, tierra, nieve -> cada una albedo + normal map. Ruido fractal periodico (iFFT) = sin
costuras. Salida: ExternalAssets/Generated/Terrain/. Importar a UE con import_terrain.py.
"""
import numpy as np
from PIL import Image
import os

OUT = r"C:\Users\PC\Desktop\rome-actual\ExternalAssets\Generated\Terrain"
os.makedirs(OUT, exist_ok=True)
SZ = 1024

def seamless(size, beta, seed):
    rng = np.random.default_rng(seed)
    fy = np.fft.fftfreq(size)[:, None]
    fx = np.fft.fftfreq(size)[None, :]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1e-6
    amp = 1.0 / (f ** beta)
    amp[0, 0] = 0.0
    ph = rng.uniform(0, 2 * np.pi, (size, size))
    img = np.fft.ifft2(amp * np.exp(1j * ph)).real
    img -= img.min()
    img /= (img.max() + 1e-9)
    return img

def height_field(seed):
    h = 0.68 * seamless(SZ, 2.6, seed) + 0.32 * seamless(SZ, 1.25, seed + 91)
    h -= h.min(); h /= (h.max() + 1e-9)
    return h

def to_normal(h, strength):
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    nx, ny, nz = -gx * strength, -gy * strength, np.ones_like(h)
    l = np.sqrt(nx * nx + ny * ny + nz * nz)
    out = np.stack([nx / l * 0.5 + 0.5, ny / l * 0.5 + 0.5, nz / l * 0.5 + 0.5], -1)
    return (np.clip(out, 0, 1) * 255).astype(np.uint8)

def make(name, dark, light, beta, seed, nrm_strength, specks=None):
    h = height_field(seed)
    hb = h[..., None]
    col = np.array(dark)[None, None, :] * (1 - hb) + np.array(light)[None, None, :] * hb
    if specks is not None:
        spk = (height_field(seed + 7) > 0.74)[..., None]
        col = np.where(spk, np.array(specks)[None, None, :], col)
    Image.fromarray(np.clip(col, 0, 255).astype(np.uint8)).save(os.path.join(OUT, "T_%s_D.png" % name))
    Image.fromarray(to_normal(h, nrm_strength)).save(os.path.join(OUT, "T_%s_N.png" % name))
    print("WL_TEX_DONE:", name)

# Paleta acercada al verde olivo del mapa, con contraste para leer relieve.
make("Grass", (52, 78, 36), (118, 142, 64), 2.6, 11, 2.2, specks=(96, 104, 52))
make("Rock",  (86, 84, 80), (164, 160, 150), 2.2, 23, 3.4)
make("Dirt",  (86, 64, 42), (150, 116, 74), 2.5, 37, 2.4, specks=(70, 52, 34))
make("Snow",  (206, 216, 228), (250, 252, 255), 2.8, 51, 1.6)
print("WL_TERRAIN_TEX_ALL_DONE", OUT)
