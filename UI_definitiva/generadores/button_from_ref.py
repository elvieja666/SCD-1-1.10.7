# Botones copiados de la foto de referencia (rojo -> azul del plugin). Uso: python3 button_from_ref.py <carpeta_salida>
import sys, numpy as np
from PIL import Image, ImageFilter
from matplotlib.colors import rgb_to_hsv, hsv_to_rgb
out = sys.argv[1] if len(sys.argv) > 1 else '../../Resources'
H = 217/360.0                                  # el azul de siempre (62,128,245)
M = 16                                         # margen alrededor del cuerpo (luz que se escapa)
TW = 62.0                                      # ancho total del boton en el png final (igual que antes)
RING = 3.0                                     # grosor del marquito negro (sale de achicar el cuerpo; el total no cambia)
def build(src, body, name, lit):
    ring = RING + (1.4 if lit else 0.0)         # prendido = apretado: marco un poco mas grande, cuerpo un poco mas chico
    im = np.array(Image.open(src).convert('RGB')).astype(float)/255.0
    x0, y0, x1, y1 = body                      # cuerpo del boton (inclusive) en la foto
    x0 += 1; y0 += 1; x1 -= 1; y1 -= 1         # 1 px menos por lado: el borde de la foto trae color del papel
    hsv = rgb_to_hsv(im); sat = hsv[..., 1]
    blue = hsv.copy(); blue[..., 0] = H
    if not lit: blue[..., 1] = blue[..., 1]*0.10   # apagado: gris con apenas un tonito de azul
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
        alpha[out_] = 0.0
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
    f = (TW - 2*ring)/(x1 - x0 + 1)            # factor de escala al tamano final (cuerpo + marquito = TW)
    n = 120
    res = np.zeros((n, n, 4), np.float32)
    # transformacion: centro del cuerpo -> centro 60,60 del png final
    a_ = 1.0/f
    for k, c in enumerate(chans):
        t = c.transform((n, n), Image.AFFINE, (a_, 0, cx - 60*a_, 0, a_, cy - 60*a_), resample=Image.BICUBIC)
        res[..., k] = np.array(t)
    mk = Image.fromarray(mask.astype(np.float32), 'F').transform((n, n), Image.AFFINE, (a_, 0, cx - 60*a_, 0, a_, cy - 60*a_), resample=Image.BICUBIC)
    body_a = np.clip(np.array(mk), 0, 1)
    K = 8                                      # marquito negro: cuadrado redondeado de TW x TW, dibujado 8x mas grande y reducido
    bl = Image.new('L', (n*K, n*K), 0)
    from PIL import ImageDraw
    ImageDraw.Draw(bl).rounded_rectangle(((60 - TW/2)*K, (60 - TW/2)*K, (60 + TW/2)*K - 1, (60 + TW/2)*K - 1), radius=3.5*K, fill=255)
    black = np.array(bl.resize((n, n), Image.LANCZOS)).astype(float)/255.0
    if lit:                                    # un poco de luz azul por fuera del marco negro
        yy_g, xx_g = np.mgrid[0:n, 0:n] + 0.5
        cross = np.maximum(np.exp(-((xx_g - 60)**2)/(2*8.0**2)), np.exp(-((yy_g - 60)**2)/(2*8.0**2)))   # mas luz saliendo del medio de cada lado (forma de cruz +)
        gl = np.array(Image.fromarray((black*255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(4.5))).astype(float)/255.0*(0.85 + 0.85*cross)*(1 - black)
        gl = np.clip(gl, 0, 0.95)
        GC = hsv_to_rgb(np.array([H, 0.78, 0.98]))
        res[..., :3] = res[..., :3]*(1 - gl[..., None]) + GC*gl[..., None]; res[..., 3] = res[..., 3]*(1 - gl) + gl
    Ab = black*(1 - body_a)                    # negro solo donde no hay cuerpo
    BLK = np.array([8, 8, 10])/255.0
    res[..., :3] = res[..., :3]*(1 - Ab[..., None]) + BLK*Ab[..., None]
    res[..., 3] = res[..., 3]*(1 - Ab) + Ab
    A = np.clip(res[..., 3], 0, 1)
    C = np.where(A[..., None] > 1e-4, res[..., :3]/np.maximum(A[..., None], 1e-4), 0)
    if not lit:                                # apagado: sombra suave oscura abajo
        sh = Image.fromarray((np.clip(A, 0, 1)*255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3))
        sh = np.roll(np.array(sh).astype(float)/255.0, 2, axis=0)*0.45
        A2 = A + sh*(1 - A); C = np.where(A[..., None] > 0.02, C, 0) * 1.0
        C = (C*A[..., None])/np.maximum(A2[..., None], 1e-4); A = A2
    o = np.dstack([np.clip(C, 0, 1), np.clip(A, 0, 1)])
    im_ = Image.fromarray((o*255 + 0.5).astype(np.uint8), 'RGBA')
    r_, g_, b_, a_im = im_.split()
    rgb0 = Image.merge('RGB', (r_, g_, b_)); ycc = rgb0.convert('YCbCr'); yy_, cb_, cr_ = ycc.split()
    yy_ = yy_.filter(ImageFilter.UnsharpMask(radius=1.4, percent=200, threshold=1))   # se afina solo la luz (no el color) para que no salgan bordes de otro tono
    rgb_s = Image.merge('YCbCr', (yy_, cb_, cr_)).convert('RGB')   # mas definicion: la foto es chica y al agrandarla se ablanda
    rgb_s.putalpha(a_im); rgb_s.save(f'{out}/{name}')
build('../referencias/btn_ref_off.png', (20, 15, 54, 49), 'ui_btn_off.png', False)
build('../referencias/btn_ref_on.png',  (18, 16, 53, 51), 'ui_btn_on.png',  True)
