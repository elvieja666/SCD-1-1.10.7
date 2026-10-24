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
        FH=19.0                                              # medio lado de la lente (diseno)
        sd_o=sdf(X,Y,BW*S/2,BW*S/2,9*S)                      # cuerpo
        sd_g=sdf(X,Y,(FH+3.2)*S,(FH+3.2)*S,5.5*S)            # canal oscuro alrededor de la lente
        sd_f=sdf(X,Y,FH*S,FH*S,3.5*S)                        # lente
        m=cov(sd_o)
        lit=np.clip(-Yd/(BW/2),0,1)                           # misma luz cenital que las perillas
        # sombra proyectada, como las perillas (dos capas)
        big=m
        shd=1-(1-gaussian_filter(ndshift(big,(0.8*S,0),order=1),1.2*S)*0.9)*(1-gaussian_filter(ndshift(big,(3.4*S,0),order=1),4.2*S)*0.85)
        A[:]=A*(1-shd[...,None])
        if on:   # luz azul que se derrama alrededor (dos halos)
            hl=np.clip(gaussian_filter(m,5.0*S)*1.5,0,1)*(1-m)*0.34+np.clip(gaussian_filter(m,13.0*S)*1.8,0,1)*(1-m)*0.30
            A[:]=A*(1-hl[...,None])+np.array([45,115,255.])[None,None,:]*hl[...,None]
        g1=gaussian_filter(rng.normal(0,1,(h,w)),0.8)*3.0+rng.normal(0,1,(h,w))*1.5          # grano de las perillas
        dob=np.clip(-sd_o/S,0,None)
        # --- cuerpo negro mate con grano, arco de luz fino arriba (como la tapa de las perillas)
        rr=np.hypot(Xd,Yd)/(BW/2)
        Lb=13+7*(1-np.clip(rr,0,1.2)**1.5)+g1
        Lb=Lb+52*np.exp(-((dob-1.5)/1.1)**2)*lit**1.3
        Lb=Lb-6*np.exp(-(dob/1.0)**2)*(1-lit)
        col=np.stack([Lb]*3,-1)
        if on:   # el cuerpo recibe luz azul cerca de la lente
            dg=np.clip(sd_g/S,0,None); sp=np.exp(-dg/9.0)*0.55
            col=col+sp[...,None]*np.array([6,22,60.])[None,None,:]
        # --- canal alrededor de la lente: mas oscuro, con reborde fino de luz abajo
        gm=cov(sd_g)[...,None]; dg_in=np.clip(-sd_g/S,0,None)
        gv=4+5*np.exp(-((dg_in-0.8)/0.9)**2)*(0.3+0.7*lit)+g1*0.4
        gcol=np.stack([gv]*3,-1)
        if on: gcol=gcol+np.array([4,14,40.])[None,None,:]*np.exp(-dg_in/3.0)[...,None]
        col=col*(1-gm)+gcol*gm
        # --- lente
        dinn=np.clip(-sd_f/S,0,None); fm=cov(sd_f)[...,None]
        q=np.clip(np.hypot(Xd/FH,Yd/FH),0,1.3)
        if on:
            core=np.exp(-(q/0.85)**2.2)                       # nucleo luminoso
            L=70+150*core
            face=np.stack([L*0.36+10,L*0.64+22,L*1.0+45],-1)
            face=face+np.array([50,58,30.])[None,None,:]*(np.exp(-(q/0.35)**2)[...,None])   # centro casi blanco
        else:
            L=22+16*(1-np.clip(q,0,1)**1.4)+g1*0.7
            face=np.stack([L,L,L*1.04+1],-1)
        # sombra interior arriba (lente hundida) y borde de vidrio claro abajo
        sh_in=np.exp(-dinn/2.4)*np.clip(-Yd/FH,0,1)         # arriba (Y negativo)
        face=face*(1-0.50*sh_in[...,None])
        rim=np.exp(-((dinn-0.8)/0.8)**2)*np.clip(Yd/FH,0,1)   # reborde abajo
        face=face+(18 if not on else 30)*rim[...,None]
        # reflejo suave del vidrio (arriba-izquierda), muy tenue
        spx=np.exp(-(((Xd+FH*0.25)/(FH*0.75))**2+((Yd+FH*0.45)/(FH*0.38))**2))
        face=face+(7 if not on else 22)*spx[...,None]
        face=face+g1[...,None]*(0.5 if not on else 0.9)
        col=col*(1-fm)+face*fm
        A[:]=A*(1-m[...,None])+np.clip(col,0,255)*m[...,None]
    n=(BW+2*PADB)*S
    return two(fn,n,n)
if __name__=='__main__':
    off=draw(False); on=draw(True); off.save('btn_off.png'); on.save('btn_on.png')
    bg=Image.new('RGBA',(300,130),(255,255,255,255)); bg.alpha_composite(off,(20,12)); bg.alpha_composite(on,(180,12))
    bg.resize((900,390),Image.LANCZOS).save('btn_prev.png')
