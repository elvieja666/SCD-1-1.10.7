import math, sys, numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter, shift as ndshift
S=2
d=None
def sh_rgb(m,col):
    k=1.4*S
    up=ndshift(m,(k,0),order=1); dn=ndshift(m,(-k,0),order=1)
    hl=np.clip(m-up,0,1); dk=np.clip(m-dn,0,1)
    base=np.array(col,float)[None,None,:]
    return base*(1-0.35*dk[...,None])+(255-base)*0.85*hl[...,None]
from PIL import ImageDraw
exec(open('k4_src.py').read())      # knob_cap4 (Mode) original, usa 'arr' global

def to_rgba(Wr,Bk):
    a=np.clip(1-(Wr-Bk).mean(-1)/255,0,1)
    pm=Bk.copy()                       # premultiplicado (sobre negro)
    h,w=a.shape; f=lambda x: x.reshape(h//S,S,w//S,S,*x.shape[2:]).mean((1,3))
    a2=f(a); p2=f(pm)
    rgb=np.where(a2[...,None]>1e-4,p2/np.maximum(a2[...,None],1e-4),0)
    out=np.dstack([np.clip(rgb,0,255),np.clip(a2*255,0,255)])
    return Image.fromarray(np.round(out).astype('uint8'),'RGBA')
def two(fn,size):
    Wr=np.full((size,size,3),255.); Bk=np.zeros((size,size,3))
    fn(Wr); fn(Bk); return to_rgba(Wr,Bk)

# ---------- Depth: cuerpo (faldon + tapa) sin puntero ----------
RD=270
def depth_body(A):
    Rr=RD*S; n=int(2*Rr)+4; c=n/2; pad=int(0.12*Rr)
    yy,xx=np.mgrid[0:n,0:n]; dx=xx-c+.5; dy=yy-c+.5; rr=np.hypot(dx,dy)/Rr; th=np.arctan2(dy,dx)
    rng=np.random.default_rng(11); lit=np.clip(-np.sin(th),0,1)
    big=np.zeros((n+2*pad,n+2*pad)); big[pad:pad+n,pad:pad+n]=(rr<=1.0)
    shd=gaussian_filter(ndshift(big,(0.025*Rr,0),order=1),0.04*Rr)*0.80
    A[:]=A*(1-shd[...,None])
    skirt=rr<=1.0; t=np.clip((rr-0.83)/0.17,0,1)
    g=9+10*t**2+20*np.exp(-((rr-0.985)/0.012)**2)*(0.25+0.75*lit)
    g=g+gaussian_filter(rng.normal(0,1,(n,n)),0.9)*3+rng.normal(0,1,(n,n))*1.2
    S_=np.clip(g,0,255); reg2=A[pad:pad+n,pad:pad+n]; mk=skirt[...,None]
    reg2[:]=reg2*(1-mk)+np.stack([S_,S_,S_],-1)*mk
    groove=(rr>0.775)&(rr<=0.83); gv=4+7*np.exp(-((rr-0.83)/0.012)**2)*(0.3+0.7*lit)
    reg2[:]=np.where(groove[...,None],np.stack([gv,gv,gv],-1),reg2)
    cap=rr<=0.775; q=np.clip(rr/0.775,0,1)
    cg=14+8*(1-q**1.5)+gaussian_filter(rng.normal(0,1,(n,n)),0.8)*3.0+rng.normal(0,1,(n,n))*1.5
    rim=np.exp(-((rr-0.74)/0.014)**2)*(55*lit**1.3)
    cg=np.clip(cg+rim,0,255)
    reg2[:]=np.where(cap[...,None],np.stack([cg,cg,cg],-1),reg2)
DEPTH_N=int(2*RD*S)+4+2*int(0.12*RD*S)

# ---------- Polarity: cuerpo (tapa + arco de luz + sombra) sin puntero ----------
RP=112
def pol_body(A):
    r=RP; R=r*S; pad=int(0.12*R); n=int(2*R)+4; c=n/2
    yy,xx=np.mgrid[0:n,0:n]; dx=xx-c+.5; dy=yy-c+.5; q=np.hypot(dx,dy)/R; th=np.arctan2(dy,dx)
    de=(1-q)*r; rng=np.random.default_rng(5); lit=np.clip(-np.sin(th),0,1)
    big=np.zeros((n+2*pad,n+2*pad)); big[pad:pad+n,pad:pad+n]=(q<=1.0)
    shd=gaussian_filter(ndshift(big,(0.025*R,0),order=1),0.04*R)*0.80
    A[:]=A*(1-shd[...,None])
    sub=A[pad:pad+n,pad:pad+n]
    cg=14+8*(1-np.clip(q,0,1)**1.5)+gaussian_filter(rng.normal(0,1,(n,n)),0.8)*3.0+rng.normal(0,1,(n,n))*1.5
    cg=cg+np.exp(-((de-9.5)/3.8)**2)*(55*lit**1.3)
    cg=np.clip(cg,0,255); m=np.clip((1.0-q)*R+0.5,0,1)[...,None]
    sub[:]=sub*(1-m)+np.stack([cg,cg,cg],-1)*m
POL_N=int(2*RP*S)+4+2*int(0.12*RP*S)

# ---------- Mode: dibujo completo girado ----------
def mode_img(ang):
    global arr
    r=112; R=r*S; pad=int(0.12*R); n=int(2*R)+4; N=n+2*pad
    def fn(A):
        global arr
        arr=A; knob_cap4((pad+n/2)/S,(pad+n/2)/S,r,ang)
    return two(fn,N)

# ---------- punteros: celdas locales con la luz fija ----------
def ptr_cell(ang,Rk,s0,s1,clipR,colfun,H):
    # Rk,s0,s1,clipR en px de supersample; H = medio lado de la celda (px de diseño)
    t=math.radians(ang); ux,uy=math.sin(t),-math.cos(t); vx,vy=math.cos(t),math.sin(t)
    rm=(s0+s1)/2; Px,Py=rm*ux,rm*uy; hs=H*S
    yy,xx=np.mgrid[0:2*hs,0:2*hs]; X=Px+(xx-hs+.5); Y=Py+(yy-hs+.5)
    dist=np.hypot(X,Y); s=X*ux+Y*uy; v=X*vx+Y*vy
    w0=0.023*270*S
    rect=np.clip(s-s0+.5,0,1)*np.clip(s1-s+.5,0,1)*np.clip(w0-np.abs(v)+.5,0,1)
    circ=np.clip(w0-np.hypot(s-s0,v)+.5,0,1)
    pm=np.maximum(rect,circ)*np.clip(clipR-dist+.5,0,1)
    def fn(A):
        sx=gaussian_filter(ndshift(pm,(4.3*S,1.1*S),order=1),2.7*S)*0.95; A[:]=A*(1-sx[...,None])
        rgbp=sh_rgb(pm,(196,196,196)); w,colv=colfun(dist)
        rgbp=rgbp*(1-w[...,None])+np.stack([colv,colv,colv],-1)*w[...,None]
        A[:]=A*(1-pm[...,None])+rgbp*pm[...,None]
    return two(fn,2*hs)
def col_depth(dist):
    rr=dist/(RD*S); w=np.clip((rr-0.715)/0.07,0,1); w=w*w*(3-2*w)
    return w,196-56*w-24*np.clip((rr-0.80)/0.065,0,1)
def col_pol(dist):
    de=(RP*S-dist)/S; w=np.clip((16.2-de)/18.9,0,1); w=w*w*(3-2*w)
    return w,196-56*w

NF=136; COLS=12
def atlas(name,H,mk):
    cell=2*H; rows=(NF+COLS-1)//COLS
    A=Image.new('RGBA',(cell*COLS,cell*rows),(0,0,0,0))
    for i in range(NF):
        A.paste(mk(-135+2*i),((i%COLS)*cell,(i//COLS)*cell))
    A.save(name,optimize=True)

if __name__=='__main__':
    two(depth_body,DEPTH_N).save('depth_body.png'); print('depth_body',DEPTH_N//S)
    two(pol_body,POL_N).save('pol_body.png'); print('pol_body',POL_N//S)
    mode_img(-135).save('mode_half.png'); mode_img(135).save('mode_whole.png')
    HD=84; HP=56
    atlas('depth_ptr.png',HD,lambda a:ptr_cell(a,RD*S,0.40*RD*S,0.775*RD*S,0.775*RD*S,col_depth,HD))
    atlas('pol_ptr.png',HP,lambda a:ptr_cell(a,RP*S,(0.40/0.775)*RP*S,1.1*RP*S,RP*S,col_pol,HP))
