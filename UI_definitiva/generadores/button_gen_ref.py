# Botones nuevos (segun las capturas de referencia): cuadrado oscuro con marco fino y linea de luz arriba;
# prendido: marco azul, fondo azul oscuro y centro blanco-celeste con resplandor. Salida: 120x120 (igual que antes).
import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter
S=4; N=120
def rr(X,Y,hw,hh,r):
    qx=np.abs(X)-(hw-r); qy=np.abs(Y)-(hh-r)
    return np.hypot(np.maximum(qx,0),np.maximum(qy,0))+np.minimum(np.maximum(qx,qy),0)-r
cov=lambda sd: np.clip(0.5-sd*S,0,1)    # sd en unidades de diseno
def draw(on):
    n=N*S; yy,xx=np.mgrid[0:n,0:n]; X=(xx-n/2+.5)/S; Y=(yy-n/2+.5)/S
    rng=np.random.default_rng(5 if on else 4)
    col=np.zeros((n,n,3)); al=np.zeros((n,n))
    def over(c,a):
        nonlocal col,al
        a=np.clip(a,0,1); c=np.broadcast_to(np.asarray(c,float),(n,n,3))
        na=a+al*(1-a); col=np.where(na[...,None]>0,(c*a[...,None]+col*al[...,None]*(1-a[...,None]))/np.maximum(na[...,None],1e-6),0); al=na
    well=rr(X,Y,34.5,34.5,8)
    mw=cov(well)
    # resplandor (prendido) y sombra
    if on:
        gl=np.clip(gaussian_filter(mw,5*S)*1.5,0,1)*0.36+np.clip(gaussian_filter(mw,13*S)*1.8,0,1)*0.30
        over([45,115,255],gl*(1-mw))
    else:
        over([0,0,0],gaussian_filter(np.roll(mw,int(1.5*S),0),2.5*S)*0.55*(1-mw))
    noise=gaussian_filter(rng.normal(0,1,(n,n)),0.5*S)*2.2+rng.normal(0,1,(n,n))*1.2
    spk=(rng.random((n,n))>0.996)*rng.normal(0,14,(n,n))
    # pozo negro con filo claro abajo
    wl=(8+noise*0.4)
    wl=wl+14*np.exp(-((-well-0.8)/0.8)**2)*np.clip(Y/34.5,0,1)
    over(np.stack([wl]*3,-1)+(np.array([0,6,24.]) if on else 0),mw)
    # marco fino
    ring=rr(X,Y,30.3,30.3,5); mrg=cov(ring)-cov(rr(X,Y,29.0,29.0,4.2))
    over([70,140,255] if on else [58,58,62],mrg*(0.95 if on else 0.9))
    # cara
    fc=rr(X,Y,29.0,29.0,4.2); mf=cov(fc)
    if on:
        base=np.stack([18+noise*1.2,34+noise*1.8,92+noise*3.4],-1)
        # mas claro hacia el centro (luz del foco)
        rad=np.hypot(X,Y)/29; base=base+np.exp(-(rad/0.85)**2)[...,None]*np.array([6,22,52.])
    else:
        base=np.stack([21+noise*0.9,21+noise*0.9,23+noise*0.9],-1)
        base=base+14*np.exp(-((Y+29)/3.0)**2)[...,None]*0.5
    base=base+spk[...,None]*(1 if not on else 1.4)
    over(np.clip(base,0,255),mf)
    # linea de luz arriba (con puntitas mas brillantes en las esquinas)
    ly=-26.2; xw=np.clip((29.5-np.abs(X))/1.5,0,1)
    line=np.exp(-((Y-ly)/0.75)**2)*xw
    ends=np.exp(-((np.abs(X)-28.2)/1.2)**2)*np.exp(-((Y-ly)/1.3)**2)
    band=np.exp(-((Y-(ly-2.2))/2.2)**2)*xw*0.35        # banda tenue arriba de la linea
    if on:
        over([205,208,215],line*0.75+ends*0.5); over([120,170,255],band*0.5)
    else:
        over([170,170,174],line*0.85+ends*0.5); over([90,90,94],band*0.5)
    over([0,0,0],mf*0)   # (sin efecto, mantiene el orden)
    if on:
        # centro blanco-celeste con halo
        sq=rr(X,Y,16.2,16.2,3.4)
        halo=np.clip(gaussian_filter(cov(rr(X,Y,15,15,3.4)),4.2*S),0,1)
        over([70,130,255],halo*0.9*mf)
        over([140,200,255],gaussian_filter(cov(sq),1.6*S)*0.85*mf)
        core=cov(rr(X,Y,13.8,13.8,3.0)); core=gaussian_filter(core,0.9*S)
        cc=np.stack([228+10*np.clip(1-np.hypot(X,Y)/18,0,1)]*1+[246+0*X]+[255+0*X],-1).astype(float)
        cc[...,0]=np.clip(215+22*np.clip(1-np.hypot(X,Y)/16,0,1)+noise*0.8,0,255); cc[...,1]=np.clip(238+14*np.clip(1-np.hypot(X,Y)/16,0,1)+noise*0.5,0,255); cc[...,2]=255
        over(cc,core*mf)
        # arco azul brillante arriba del marco y marquita abajo
        over([120,180,255],np.exp(-(((Y+32.6)/1.0)**2+(X/9.0)**2))*0.6*cov(well))
        over([90,150,255],np.exp(-(((Y-32.6)/0.8)**2+(X/2.5)**2))*0.5*cov(well))
    img=np.dstack([col,al*255]).clip(0,255).astype('uint8')
    return Image.fromarray(img,'RGBA').resize((N,N),Image.LANCZOS)
if __name__=='__main__':
    off=draw(False); on=draw(True); off.save('btn_off.png'); on.save('btn_on.png')
    bg=Image.new('RGBA',(260,130),(36,36,38,255)); bg.alpha_composite(off,(5,5)); bg.alpha_composite(on,(135,5))
    bg.resize((780,390),Image.LANCZOS).save('btn_prev.png')
