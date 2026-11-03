# Botones copiados de la foto de referencia (rojo -> azul del plugin). Uso: python3 button_from_ref.py <carpeta_salida>
import sys, numpy as np
from PIL import Image, ImageFilter
from matplotlib.colors import rgb_to_hsv, hsv_to_rgb
out = sys.argv[1] if len(sys.argv) > 1 else '../../Resources'
H = 217/360.0                                  # el azul de siempre (62,128,245)
M = 16                                         # margen alrededor del cuerpo (luz que se escapa)
TW = 62.0                                      # ancho del cuerpo en el png final (igual que antes)
def build(src, body, name, lit):
    im = np.array(Image.open(src).convert('RGB')).astype(float)/255.0
    x0, y0, x1, y1 = body                      # cuerpo del boton (inclusive) en la foto
    x0 += 1; y0 += 1; x1 -= 1; y1 -= 1         # 1 px menos por lado: el borde de la foto trae color del papel
    hsv = rgb_to_hsv(im); sat = hsv[..., 1]
    blue = hsv.copy(); blue[..., 0] = H
    rgb = hsv_to_rgb(blue)
    h, w = im.shape[:2]
    alpha = np.zeros((h, w))
    mask = np.zeros((h, w), bool); mask[y0:y1+1, x0:x1+1] = True
    alpha[mask] = 1.0
    if lit:                                    # luz azul que se escapa del cuerpo: halo suave y parejo (la foto trae ademas la sombra roja de abajo)
        yy, xx = np.mgrid[0:h, 0:w] + 0.5
        dx = np.maximum(np.maximum(x0 - xx, xx - (x1 + 1)), 0); dy = np.maximum(np.maximum(y0 - yy, yy - (y1 + 1)), 0)
        d = np.hypot(dx, dy)
        out_ = ~mask
        alpha[out_] = (0.55*np.exp(-(d[out_]**2)/(2*3.6**2)))
        rgb[out_] = hsv_to_rgb(np.array([H, 0.78, 0.98]))
    pad = np.zeros((h, w), bool)
    # recorte cuadrado alrededor del cuerpo
    cx, cy = (x0 + x1 + 1)/2.0, (y0 + y1 + 1)/2.0
    half = (TW/ (x1 - x0 + 1))*0 + 0
    R = (x1 - x0 + 1)/2.0 + M
    box = (cx - R, cy - R, cx + R, cy + R)
    pm = np.dstack([rgb*alpha[..., None], alpha])           # premultiplicado para que no salgan bordes
    big = Image.fromarray((np.clip(pm, 0, 1)*65535).astype(np.uint16)[..., 0]) if False else None
    chans = [Image.fromarray(pm[..., k].astype(np.float32), 'F') for k in range(4)]
    f = TW/(x1 - x0 + 1)                       # factor de escala al tamano final
    n = 120
    res = np.zeros((n, n, 4), np.float32)
    # transformacion: centro del cuerpo -> centro 60,60 del png final
    a_ = 1.0/f
    for k, c in enumerate(chans):
        t = c.transform((n, n), Image.AFFINE, (a_, 0, cx - 60*a_, 0, a_, cy - 60*a_), resample=Image.BICUBIC)
        res[..., k] = np.array(t)
    A = np.clip(res[..., 3], 0, 1)
    C = np.where(A[..., None] > 1e-4, res[..., :3]/np.maximum(A[..., None], 1e-4), 0)
    if not lit:                                # apagado: sombra suave oscura abajo
        sh = Image.fromarray((np.clip(A, 0, 1)*255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3))
        sh = np.roll(np.array(sh).astype(float)/255.0, 2, axis=0)*0.45
        A2 = A + sh*(1 - A); C = np.where(A[..., None] > 0.02, C, 0) * 1.0
        C = (C*A[..., None])/np.maximum(A2[..., None], 1e-4); A = A2
    o = np.dstack([np.clip(C, 0, 1), np.clip(A, 0, 1)])
    Image.fromarray((o*255 + 0.5).astype(np.uint8), 'RGBA').save(f'{out}/{name}')
build('../referencias/btn_ref_off.png', (20, 15, 54, 49), 'ui_btn_off.png', False)
build('../referencias/btn_ref_on.png',  (18, 16, 53, 51), 'ui_btn_on.png',  True)
