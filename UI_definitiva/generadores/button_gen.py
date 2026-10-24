import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter, shift as ndshift
from fader_gen import two, sm
S=2; BW=64; PADB=28
def sdf(X,Y,hw,hh,r):       # distancia con signo (px de supersample), negativa dentro
    qx=np.abs(X)-(hw-r); qy=np.abs(Y)-(hh-r)
    return np.hypot(np.maximum(qx,0),np.maximum(qy,0))+np.minimum(np.maximum(qx,qy),0)-r
cov=lambda sd: np.clip(0.5-sd,0,1)
def draw(on):
    def fn(A):
        h,w=A.shape[:2]; yy,xx=np.mgrid[0:h,0:w]; X=xx-w/2+.5; Y=yy-h/2+.5; Xd=X/S; Yd=Y/S
        rng=np.random.default_rng(31 if on else 30)
        RING=3.6; TB=5.0; GP=1.8                              # anillo negro exterior, grosor del bisel, canal oscuro
        HB0=BW/2; FH0=HB0-TB-GP
        sd_r=sdf(X,Y,(HB0+RING)*S,(HB0+RING)*S,(7+RING)*S)   # anillo/pozo negro (incrustado)
        # apagado = boton saliente (sube un poco); prendido = apretado (se hunde y baja un poco)
        HB=HB0-(1.4 if on else 0.0); FH=FH0-(1.4 if on else 0.0)
        oy=1.0 if on else -0.6
        Y=Y-oy*S; Yd=Yd-oy
        sd_b=sdf(X,Y,HB*S,HB*S,7*S)                           # bisel metalico
        sd_l=sdf(X,Y,(FH+GP)*S,(FH+GP)*S,4.2*S)               # borde del canal
        sd_f=sdf(X,Y,FH*S,FH*S,2.6*S)                         # lente
        mr=cov(sd_r); mb=cov(sd_b)[...,None]; ml=cov(sd_l)[...,None]; mf=cov(sd_f)[...,None]
        # sombra proyectada al fondo (el boton esta hundido: sombra suave y corta)
        shd=1-(1-gaussian_filter(ndshift(mr,(0.6*S,0),order=1),1.0*S)*0.8)*(1-gaussian_filter(ndshift(mr,(2.6*S,0),order=1),3.6*S)*0.7)
        A[:]=A*(1-shd[...,None])
        if on:
            hl=np.clip(gaussian_filter(mr,5.0*S)*1.5,0,1)*(1-mr)*0.36+np.clip(gaussian_filter(mr,13.0*S)*1.8,0,1)*(1-mr)*0.30
            ang=np.arctan2(Yd,Xd); cw=np.abs(np.sin(2*ang))**1.6        # 1 en las diagonales (esquinas), 0 en el centro de cada lado
            hl=hl*(1-0.36*cw)                                          # menos resplandor en las esquinas: no es parejo por todos lados
            A[:]=A*(1-hl[...,None])+np.array([45,115,255.])[None,None,:]*hl[...,None]
        t=np.clip(-Yd/HB,-1,1); xn=np.clip(Xd/HB,-1,1)       # t=+1 arriba
        g1=gaussian_filter(rng.normal(0,1,(h,w)),0.8)*3.0+rng.normal(0,1,(h,w))*1.5
        # --- anillo negro: pozo profundo, luz que atrapa solo el borde inferior
        dor=np.clip(-sd_r/S,0,None)
        Lr=6+g1*0.5+3*np.clip(-t,0,1)
        Lr=Lr+30*np.exp(-((dor-0.7)/0.7)**2)*np.clip(-t,0,1)**0.8-4*np.exp(-(dor/0.8)**2)*np.clip(t,0,1)
        col=np.stack([Lr]*3,-1)
        if on: col=col+(np.exp(-np.clip(sd_b/S,0,None)/3.0)*(1-0.28*np.abs(np.sin(2*np.arctan2(Yd,Xd)))**1.6))[...,None]*np.array([0,8,30.])[None,None,:]
        # --- bisel metalico: arista clara arriba, mas oscuro a la derecha, filo claro abajo
        dib=np.clip(-sd_b/S,0,None)
        Lb=84+16*t-14*np.clip(xn,0,1)+7*np.clip(-xn,0,1)+g1*0.8
        Lb=Lb+105*np.exp(-((dib-0.9)/0.9)**2)*np.clip(t,0,1)**0.6+45*np.exp(-((dib-0.7)/0.7)**2)*np.clip(-t,0,1)**0.8
        Lb=Lb+22*np.exp(-((dib-0.7)/0.7)**2)*np.clip(-xn,0,1)
        if on: Lb=Lb*0.82-6
        else: Lb=Lb+10*np.exp(-((dib-0.9)/0.9)**2)*np.clip(t,0,1)
        if on: bez=np.stack([Lb*0.50+12,Lb*0.72+34,Lb*1.0+78],-1)
        else:  bez=np.stack([Lb]*3,-1)
        cast=gaussian_filter(ndshift(cov(sd_b),((0.5 if on else 2.0)*S,0),order=1),(0.9 if on else 1.7)*S)*(0.35 if on else 0.85)*mr
        col=col*(1-cast[...,None])
        col=col*(1-mb)+np.clip(bez,0,255)*mb
        # --- canal oscuro entre bisel y lente (mas sombra arriba y a la derecha)
        dl=np.clip(-sd_l/S,0,None)
        gv=10+g1*0.4-5*np.clip(t,0,1)*np.exp(-dl/1.2)+8*np.exp(-((dl-0.4)/0.5)**2)*np.clip(-t,0,1)
        gcol=np.stack([gv]*3,-1)
        if on: gcol=gcol+np.array([2,10,34.])[None,None,:]
        col=col*(1-ml)+gcol*ml
        # --- lente: degradado, linea de luz arriba, reflejo diagonal con filo, franja oscura a la derecha
        dinn=np.clip(-sd_f/S,0,None); q=np.clip(np.hypot(Xd/FH*0.9,Yd/FH),0,1.4)
        dytop=Yd+FH
        topl=np.exp(-((dytop-1.1)/0.8)**2)*(dinn>0)
        diag=sm((Yd+0.34*Xd)/FH,-0.05,0.05)                   # 1 por debajo de la diagonal (sube hacia la derecha)
        rs=np.exp(-np.clip(FH-Xd,0,None)/1.6)                  # franja oscura derecha
        if on:
            core=np.clip(1-0.75*np.clip(q,0,1.4)**2.4,0,1)
            E=np.array([95,150,250.]); Cc=np.array([236,246,255.])
            face=E[None,None,:]+(Cc-E)[None,None,:]*core[...,None]
            face=face-14*diag[...,None]*np.array([1,0.8,0.5])[None,None,:]
            face=face-38*rs[...,None]*np.array([1,0.8,0.4])[None,None,:]
            face=face+40*topl[...,None]
            # foco de luz: domo redondo con nucleo brillante, aro mas saturado y media luna abajo (sutil)
            R0=FH*0.74; rd=np.hypot(Xd,Yd-0.02*FH)/R0
            dome=sm(1.0-rd,0.0,0.06)
            ring=np.exp(-((rd-0.93)/0.10)**2)                       # aro del domo
            satur=np.clip(rd,0,1)**2*dome
            face=face-34*satur[...,None]*np.array([1.0,0.6,0.1])[None,None,:]            # borde del domo mas azul
            face=face-26*ring[...,None]*np.array([0.8,0.6,0.2])[None,None,:]*np.clip(-(Yd)/FH+0.7,0,1)[...,None]
            face=face+20*np.exp(-((rd-0.97)/0.07)**2)[...,None]*np.clip(-Yd/FH,0,1)[...,None]   # brillo del borde arriba
            core=np.exp(-(np.hypot(Xd,Yd+0.14*FH)/(FH*0.30))**2)
            face=face+(90*core)[...,None]*np.array([0.55,0.6,0.6])[None,None,:]*dome[...,None]
            cres=np.exp(-((np.hypot(Xd,Yd-0.05*FH)/(FH*0.58)-1.0)/0.14)**2)*np.clip((Yd/FH-0.20)/0.2,0,1)
            face=face+(34*cres*dome)[...,None]*np.array([0.7,0.8,1.0])[None,None,:]       # reflejo inferior
        else:
            L=152+46*np.exp(-(q/0.9)**2.2)+g1*0.7
            L=L-20*diag-34*rs+46*topl
            L=L-14*np.exp(-dinn/2.0)*np.clip(-t,0,1)*(dinn>0)*0+10*np.exp(-((dinn-0.9)/0.9)**2)*np.clip(-t,0,1)
            # foco de luz apagado: misma forma, mucho mas tenue (vidrio con el domo apenas insinuado)
            R0=FH*0.74; rd=np.hypot(Xd,Yd-0.02*FH)/R0
            dome=sm(1.0-rd,0.0,0.06)
            ring=np.exp(-((rd-0.93)/0.10)**2)*np.clip(-(Yd)/FH+0.7,0,1)
            core=np.exp(-(np.hypot(Xd,Yd+0.14*FH)/(FH*0.30))**2)
            cres=np.exp(-((np.hypot(Xd,Yd-0.05*FH)/(FH*0.58)-1.0)/0.14)**2)*np.clip((Yd/FH-0.20)/0.2,0,1)
            L=L-10*np.clip(rd,0,1)**2*dome-9*ring+9*np.exp(-((rd-0.97)/0.07)**2)*np.clip(-Yd/FH,0,1)+16*core*dome+8*cres*dome
            face=np.stack([L,L,L*1.01],-1)
        col=col*(1-mf)+np.clip(face,0,255)*mf
        A[:]=A*(1-mr[...,None])+np.clip(col,0,255)*mr[...,None]
    n=(BW+2*PADB)*S
    return two(fn,n,n)
if __name__=='__main__':
    off=draw(False); on=draw(True); off.save('btn_off.png'); on.save('btn_on.png')
    bg=Image.new('RGBA',(300,130),(255,255,255,255)); bg.alpha_composite(off,(20,12)); bg.alpha_composite(on,(180,12))
    bg.resize((900,390),Image.LANCZOS).save('btn_prev.png')
