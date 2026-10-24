import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter, shift as ndshift
from fader_gen import rrect, sm, two
S=2; BW=64; PADB=16
def draw(on):
    def fn(A):
        h,w=A.shape[:2]; yy,xx=np.mgrid[0:h,0:w]; X=xx-w/2+.5; Y=yy-h/2+.5; Xd=X/S; Yd=Y/S
        rng=np.random.default_rng(21 if on else 20)
        m=rrect(X,Y,BW*S/2,BW*S/2,10*S)
        sh=gaussian_filter(ndshift(m,(3.2*S,1.0*S),order=1),2.4*S)*0.40
        A[:]=A*(1-sh[...,None])
        # marco: metal biselado, luz desde arriba (mas claro arriba, mas oscuro abajo)
        t=np.clip(-Yd/(BW/2),-1,1)                  # +1 arriba, -1 abajo
        fr=(150+55*t) if not on else (205+35*t)
        fr=fr+np.exp(-((np.hypot(np.maximum(np.abs(Xd)-(BW/2-10),0),np.maximum(np.abs(Yd)-(BW/2-10),0))+np.minimum(np.maximum(np.abs(Xd)-(BW/2-10),np.abs(Yd)-(BW/2-10)),0)-10+0.8)/0.9)**2)*35*np.clip(0.3+0.7*t,0,1)
        # cara interior hundida
        fm=rrect(X,Y,(BW-12)*S/2,(BW-12)*S/2,5*S)
        fd=np.maximum(0,(-1)*0)
        qx=np.abs(X)-((BW-12)*S/2-5*S); qy=np.abs(Y)-((BW-12)*S/2-5*S)
        sd=(np.hypot(np.maximum(qx,0),np.maximum(qy,0))+np.minimum(np.maximum(qx,qy),0)-5*S)/S   # <0 dentro; distancia al borde de la cara en px
        dist=np.clip(-sd,0,None)
        face=16+10*sm(-t,-1,1)+gaussian_filter(rng.normal(0,1,(h,w)),0.8)*3.0+rng.normal(0,1,(h,w))*1.5
        face=face-8*np.exp(-(dist/2.2)**2)*np.clip(t,0,1)        # sombra interior arriba (esta hundida)
        face=face+10*np.exp(-((dist-0.8)/0.8)**2)*np.clip(-t,0,1)  # filo claro abajo
        fr=np.clip(fr+gaussian_filter(rng.normal(0,1,(h,w)),0.7)*1.4,0,255); face=np.clip(face,0,255)
        if on:   # encendido: rojo. El marco se tine y la cara se ilumina desde los bordes
            e1=np.exp(-dist/5.5); e2=np.exp(-dist/16)
            facec=np.stack([np.clip(face+185*e1+60*e2,0,255),np.clip(face+22*e1+4*e2,0,255),np.clip(face+14*e1+2*e2,0,255)],-1)
            frc=np.stack([np.clip(fr*1.0+10,0,255),fr*0.52,fr*0.46],-1)
        else:
            facec=np.stack([face]*3,-1); frc=np.stack([fr]*3,-1)
        col=frc*(1-fm[...,None])+facec*fm[...,None]
        if on:   # halo rojo hacia afuera (se ve sobre el fondo blanco)
            outside=1-m; halo=np.clip(gaussian_filter(m,5.0*S)*1.6,0,1)*outside*0.75
            A[:]=A*(1-halo[...,None])+np.array([235,40,30.])[None,None,:]*halo[...,None]
        A[:]=A*(1-m[...,None])+col*m[...,None]
    n=(BW+2*PADB)*S
    return two(fn,n,n)
if __name__=='__main__':
    off=draw(False); on=draw(True); off.save('btn_off.png'); on.save('btn_on.png')
    bg=Image.new('RGBA',(300,120),(255,255,255,255)); bg.alpha_composite(off,(30,10)); bg.alpha_composite(on,(170,10))
    bg.resize((900,360),Image.LANCZOS).save('btn_prev.png')
