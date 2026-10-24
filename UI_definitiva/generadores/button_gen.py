import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter, shift as ndshift
from fader_gen import two, sm
S=2; BW=64; PADB=16
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
            halo=np.clip(gaussian_filter(m,6.5*S)*2.1,0,1)*(1-m)*0.95
            A[:]=A*(1-halo[...,None])+np.array([235,38,28.])[None,None,:]*halo[...,None]
        t=-Yd/(BW/2)                                         # +1 arriba, -1 abajo
        u=Xd/(BW/2)
        # --- pozo: negro (rojizo si esta encendido)
        well=np.stack([np.full((h,w),9.)+ (30 if on else 0)*0, np.full((h,w),9.), np.full((h,w),9.)],-1)
        if on: well=np.stack([np.full((h,w),70.),np.full((h,w),6.),np.full((h,w),5.)],-1)
        well=well+n1[...,None]*0.6
        col=well
        # --- bisel metalico
        db=np.clip(-sd_b/S,0,None)                           # distancia hacia adentro desde el borde del bisel
        Lb=150+50*t-12*np.abs(u)*0                           # claro arriba, oscuro abajo
        Lb=Lb+70*np.exp(-((Yd-(-(BW/2-WELL)+1.0))/0.9)**2)*sm(-sd_b/S,0,1)   # linea de luz superior
        Lb=Lb+22*np.exp(-((Yd-((BW/2-WELL)-1.2))/0.9)**2)*sm(-sd_b/S,0,1)    # reflejo fino inferior
        Lb=Lb-26*np.exp(-((np.abs(Xd)-((BW/2-WELL)-1.2))/1.1)**2)             # laterales algo mas oscuros
        Lb=Lb+n1
        if on:
            bez=np.stack([np.clip(Lb*1.22+38,0,255),Lb*0.36,Lb*0.30],-1)
        else:
            bez=np.stack([Lb]*3,-1)
        mb=cov(sd_b)[...,None]
        col=col*(1-mb)+bez*mb
        # linea oscura entre bisel y cara
        df=np.clip(-sd_f/S,0,None)
        ring=np.exp(-((sd_f/S-0.9)/0.55)**2)*(sd_f/S>-0.5)
        col=col*(1-0.65*ring[...,None]*mb)
        # --- cara (lente)
        mf=cov(sd_f)[...,None]
        tf=-Yd/((BW/2)-WELL-BEZ)                             # -1..1 dentro de la cara
        xf=Xd/((BW/2)-WELL-BEZ)
        if on:
            base=240-16*sm(-tf,-1,1)                         # blanco calido, algo mas bajo abajo
            vig=np.exp(-(np.clip(-sd_f/S,0,None)/3.0))      # borde con tinte rojo
            Lf=base+n1*0.5
            face=np.stack([Lf,Lf*0.965-14*vig,Lf*0.93-30*vig],-1)
            face[...,0]=np.minimum(face[...,0]+8*vig,255)
        else:
            base=208-22*sm(-tf,-1,1)
            diag=(-tf)-(0.35-0.55*xf)                        # reflejo diagonal: por debajo de la linea queda mas oscuro
            base=base-34*sm(diag,-0.07,0.07)
            vig=np.exp(-(np.clip(-sd_f/S,0,None)/3.2))
            Lf=base-48*vig+n1*0.5
            face=np.stack([Lf]*3,-1)
        # filo claro arriba dentro de la cara y sombra a los costados
        topl=np.exp(-((Yd-(-(BW/2-WELL-BEZ)+1.3))/0.8)**2)*sm(-sd_f/S,0,1)
        face=face+(40 if not on else 14)*topl[...,None]*np.array([1,1,1.0])
        col=col*(1-mf)+face*mf
        A[:]=A*(1-m[...,None])+np.clip(col,0,255)*m[...,None]
    n=(BW+2*PADB)*S
    return two(fn,n,n)
if __name__=='__main__':
    off=draw(False); on=draw(True); off.save('btn_off.png'); on.save('btn_on.png')
    bg=Image.new('RGBA',(300,130),(255,255,255,255)); bg.alpha_composite(off,(20,12)); bg.alpha_composite(on,(180,12))
    bg.resize((900,390),Image.LANCZOS).save('btn_prev.png')
