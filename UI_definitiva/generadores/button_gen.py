import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter, shift as ndshift
from fader_gen import two, sm
S=2; BW=64; PADB=24
def sdf(X,Y,hw,hh,r):       # distancia con signo (px de supersample), negativa dentro
    qx=np.abs(X)-(hw-r); qy=np.abs(Y)-(hh-r)
    return np.hypot(np.maximum(qx,0),np.maximum(qy,0))+np.minimum(np.maximum(qx,qy),0)-r
cov=lambda sd: np.clip(0.5-sd,0,1)
def draw(on):
    def fn(A):
        h,w=A.shape[:2]; yy,xx=np.mgrid[0:h,0:w]; X=xx-w/2+.5; Y=yy-h/2+.5; Xd=X/S; Yd=Y/S
        rng=np.random.default_rng(31 if on else 30)
        n1=gaussian_filter(rng.normal(0,1,(h,w)),0.8)*2.4+rng.normal(0,1,(h,w))*1.2
        WELL,BEZ=3.0,6.0                                    # grosor del pozo negro y del bisel (unidades de diseno)
        sd_o=sdf(X,Y,BW*S/2,BW*S/2,8*S)                    # contorno exterior (pozo)
        sd_b=sdf(X,Y,(BW/2-WELL)*S,(BW/2-WELL)*S,6*S)       # bisel
        sd_f=sdf(X,Y,(BW/2-WELL-BEZ)*S,(BW/2-WELL-BEZ)*S,3*S)  # cara
        m=cov(sd_o)
        # sombra proyectada abajo-derecha, como las perillas
        sh=gaussian_filter(ndshift(m,(3.2*S,1.0*S),order=1),2.4*S)*0.40
        A[:]=A*(1-sh[...,None])
        if on:
            halo=np.clip(gaussian_filter(m,8.0*S)*2.2,0,1)*(1-m)*0.62
            A[:]=A*(1-halo[...,None])+np.array([40,110,255.])[None,None,:]*halo[...,None]
        t=-Yd/(BW/2)                                         # +1 arriba, -1 abajo
        u=Xd/(BW/2)
        lit=np.clip(-Yd/(BW/2),0,1)                           # luz cenital fija (igual que las perillas)
        g1=gaussian_filter(rng.normal(0,1,(h,w)),0.8)*3.0+rng.normal(0,1,(h,w))*1.5   # mismo grano que las perillas
        # --- bisel: grafito con grano, arco fino de luz arriba y borde exterior apenas mas oscuro
        dob=np.clip(-sd_o/S,0,None)
        Lb=44+22*lit-14*(1-lit)*0+g1
        Lb=Lb+55*np.exp(-((dob-1.6)/1.5)**2)*lit**1.3                                  # arco de luz del borde
        Lb=Lb-18*np.exp(-(dob/1.2)**2)*(1-lit)                                         # borde exterior inferior oscuro
        Lb=Lb+14*np.exp(-((np.clip(-sd_b/S,0,None)-0.0)/0.0001)**2)*0
        if on: bez=np.stack([Lb*0.40+4,Lb*0.60+12,Lb*1.0+70*sm(lit,0,1)+28],-1)+np.array([0,0,0])
        else:  bez=np.stack([Lb]*3,-1)
        mb=cov(sd_b)[...,None]
        col=np.zeros((h,w,3))+bez
        # --- cara hundida: sombra interior suave arriba y reborde claro abajo
        mf0=cov(sd_f)
        dinn=np.clip(-sd_f/S,0,None)
        tf=-Yd/((BW/2)-WELL-BEZ); xf=Xd/((BW/2)-WELL-BEZ)
        rr=np.hypot(xf*0.9,tf*1.0)
        if on:
            Lf=246-30*np.clip(rr,0,1.3)**2-10*sm(-tf,-1,1)+g1*0.55
            face=np.stack([Lf*0.90-34*np.clip(rr,0,1.3)**2,Lf*0.95-18*np.clip(rr,0,1.3)**2,Lf],-1)
            face=face+np.array([0,0,0])
        else:
            Lf=196-58*np.clip(rr,0,1.3)**2-14*sm(-tf,-1,1)+10*sm(tf,0.4,1)*0+g1*0.7
            diag=(-tf)-(0.30-0.50*xf)
            Lf=Lf-22*sm(diag,-0.28,0.28)                     # reflejo diagonal ancho y suave
            face=np.stack([Lf]*3,-1)
        shin=np.exp(-(dinn/2.6))*sm(tf,-0.2,1)               # sombra interior arriba (esta hundida)
        face=face*(1-0.38*shin[...,None])
        rim=np.exp(-((dinn-0.7)/0.8)**2)*sm(-tf,-0.2,1)      # reborde claro abajo
        face=face+(26 if not on else 10)*rim[...,None]
        mf=mf0[...,None]
        col=col*(1-mf)+face*mf
        # separacion bisel/cara: sombra blanda
        ring=np.exp(-((sd_f/S-1.1)/0.9)**2)*(sd_f/S>-0.2)*cov(sd_b)
        col=col*(1-0.35*ring[...,None])
        A[:]=A*(1-m[...,None])+np.clip(col,0,255)*m[...,None]
    n=(BW+2*PADB)*S
    return two(fn,n,n)
if __name__=='__main__':
    off=draw(False); on=draw(True); off.save('btn_off.png'); on.save('btn_on.png')
    bg=Image.new('RGBA',(300,130),(255,255,255,255)); bg.alpha_composite(off,(20,12)); bg.alpha_composite(on,(180,12))
    bg.resize((900,390),Image.LANCZOS).save('btn_prev.png')
