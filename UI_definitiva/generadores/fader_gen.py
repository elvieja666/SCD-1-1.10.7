import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter, shift as ndshift
import importlib.util,sys
sys.argv=['x']
S=2
def to_rgba(Wr,Bk):
    a=np.clip(1-(Wr-Bk).mean(-1)/255,0,1); h,w=a.shape
    f=lambda x: x.reshape(h//S,S,w//S,S,*x.shape[2:]).mean((1,3))
    a2=f(a); p2=f(Bk); rgb=np.where(a2[...,None]>1e-4,p2/np.maximum(a2[...,None],1e-4),0)
    return Image.fromarray(np.round(np.dstack([np.clip(rgb,0,255),np.clip(a2*255,0,255)])).astype('uint8'),'RGBA')
def two(fn,h,w):
    Wr=np.full((h,w,3),255.); Bk=np.zeros((h,w,3)); fn(Wr); fn(Bk); return to_rgba(Wr,Bk)
def sm(x,a,b): t=np.clip((x-a)/(b-a),0,1); return t*t*(3-2*t)
def rrect(X,Y,hw,hh,r):   # mascara redondeada antialias (px de supersample)
    qx=np.abs(X)-(hw-r); qy=np.abs(Y)-(hh-r)
    sd=np.hypot(np.maximum(qx,0),np.maximum(qy,0))+np.minimum(np.maximum(qx,qy),0)-r
    return np.clip(0.5-sd,0,1)

# ---------- ranura (recorrido) ----------
SW,SH,PAD=18,670,10
def slot(A):
    h,w=A.shape[:2]; yy,xx=np.mgrid[0:h,0:w]; X=xx-w/2+.5; Y=yy-h/2+.5
    rng=np.random.default_rng(3)
    m=rrect(X,Y,SW*S/2,SH*S/2,SW*S/2)
    sh=gaussian_filter(ndshift(m,(2.2*S,0.6*S),order=1),2.4*S)*0.75
    A[:]=A*(1-sh[...,None])
    d=np.clip((SW*S/2-np.abs(X))/(SW*S/2),0,1)           # 0 en el borde lateral, 1 al centro
    g=6+10*(1-sm(d,0,0.35))+gaussian_filter(rng.normal(0,1,(h,w)),0.8)*1.6
    g=g+14*np.exp(-((Y-(-SH*S/2+4*S))/(2.5*S))**2)*0     # sin brillo arriba
    # borde inferior iluminado (labio donde pega la luz) y sombra interior arriba
    top=np.exp(-((Y+SH*S/2-3*S)/(3*S))**2)
    g=g-5*top*np.clip(1-np.abs(X)/(SW*S/2),0,1)
    lip=np.exp(-((np.abs(X)-SW*S/2+1.2*S)/(1.0*S))**2)*(0.25+0.75*np.clip(-Y/(SH*S/2)*0+0.5,0,1))
    g=np.clip(g+14*lip,0,255)
    A[:]=A*(1-m[...,None])+np.stack([g,g,g],-1)*m[...,None]
# ---------- capuchon ----------
CW,CH,CP=121,48,10
def cap(A):
    h,w=A.shape[:2]; yy,xx=np.mgrid[0:h,0:w]; X=xx-w/2+.5; Y=yy-h/2+.5; Yd=Y/S
    rng=np.random.default_rng(8)
    m=rrect(X,Y,CW*S/2,CH*S/2,8*S)
    sh=gaussian_filter(ndshift(m,(3.2*S,1.0*S),order=1),2.4*S)*0.90
    A[:]=A*(1-sh[...,None])
    hh=CH/2
    # mismo negro y mismo grano que las perillas (base 14-22, grano fino 3.0 + ruido 1.5), luz cenital fija
    top=sm(-Yd,hh-6,hh); bot=sm(Yd,hh-6,hh)
    P=np.load('fader_ref_profile.npy'); rows=(np.arange(len(P))+.5)/len(P)*CH-CH/2     # perfil vertical medido en la referencia
    v=np.minimum(np.interp(Yd,rows,gaussian_filter(P,0.6)),50.0)   # sin el brillo: la marca blanca se dibuja aparte, igual que en las perillas
    # a nuestros colores: cuerpo negro de las perillas (10..30) y el pico del brillo = blanco del indicador (196)
    L=np.where(v<=62,10+(v-17)*0.33,25+(v-62)/157*(196-25))
    L=L+(gaussian_filter(rng.normal(0,1,Yd.shape),0.8)*3.0+rng.normal(0,1,Yd.shape)*1.5)*np.clip(1-(L-30)/120,0.25,1)
    L=L+46*np.exp(-((Yd+hh-2.2)/1.2)**2)                       # arco fino de luz del borde superior, como las perillas
    L=L+9*np.exp(-((np.abs(X)/S-(CW/2-1.5))/1.0)**2)*np.clip(0.4+0.6*sm(-Yd,-hh,hh),0,1)
    # cuerpo convexo (cilindro): mas claro al centro y cayendo hacia los bordes, para que no se vea cubico
    yp=np.cos(np.pi/2*np.clip(Yd/hh,-1,1))
    xp=sm(np.abs(X)/S,CW/2-16,CW/2)
    L=L+16*yp**1.3-12*xp
    # la marca blanca queda elevada: las partes negras se hunden a su alrededor (oclusion suave)
    L=L-14*np.exp(-((np.abs(Yd)-3.1)/5.5)**2)*(1-sm(np.abs(Yd),3.1,3.6)*0)
    L=np.clip(L,0,255)
    col=np.stack([L,L,L],-1)
    # marca blanca IDENTICA a la de las perillas: 196 plano, filo superior claro, parte de abajo mas oscura, misma sombra proyectada
    hw=3.1*S; pm=np.clip(hw-np.abs(Y)+0.5,0,1)*m
    sx=np.clip(gaussian_filter(ndshift(pm,(5.5*S,1.4*S),order=1),3.2*S)*1.15,0,1)*m
    col=col*(1-sx[...,None])
    k=1.4*S; up=ndshift(pm,(k,0),order=1); dn=ndshift(pm,(-k,0),order=1)
    hl=np.clip(pm-up,0,1); dk=np.clip(pm-dn,0,1); b=196.0
    rgbp=np.stack([b*(1-0.35*dk)+(255-b)*0.85*hl]*3,-1)
    col=col*(1-pm[...,None])+rgbp*pm[...,None]
    A[:]=A*(1-m[...,None])+col*m[...,None]
if __name__=='__main__':
    two(slot,(SH+2*PAD)*S,(SW+2*PAD+4)*S).save('fader_slot.png')
    two(cap,(CH+2*CP)*S,(CW+2*CP)*S).save('fader_cap.png')
    # vista previa sobre blanco
    bg=Image.new('RGBA',(300,720),(255,255,255,255))
    s=Image.open('fader_slot.png'); c=Image.open('fader_cap.png')
    bg.alpha_composite(s,(150-s.width//2,20)); bg.alpha_composite(c,(150-c.width//2,300))
    bg.crop((60,250,240,420)).resize((540,510),Image.LANCZOS).save('fader_prev.png'); bg.save('fader_prev_full.png')
