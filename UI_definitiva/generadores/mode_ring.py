import numpy as np, math
from PIL import Image
from scipy.ndimage import gaussian_filter, shift as ndshift
import knobs_gen as K
S=2; RO=112; RC=87          # radio exterior (igual que antes) y radio de la tapa (~0.775 del exterior, como Depth)
def skirt(A):
    Rr=RO*S; n=int(2*Rr)+4; c=n/2; pad=int(0.12*Rr)
    yy,xx=np.mgrid[0:n,0:n]; dx=xx-c+.5; dy=yy-c+.5; rr=np.hypot(dx,dy)/Rr; th=np.arctan2(dy,dx)
    rng=np.random.default_rng(11); lit=np.clip(-np.sin(th),0,1)
    big=np.zeros((n+2*pad,n+2*pad)); big[pad:pad+n,pad:pad+n]=(rr<=1.0)
    shd=gaussian_filter(ndshift(big,(0.025*Rr,0),order=1),0.04*Rr)*0.35
    A[:]=A*(1-shd[...,None])
    t=np.clip((rr-0.83)/0.17,0,1)
    g=9+10*t**2+20*np.exp(-((rr-0.985)/0.012)**2)*(0.25+0.75*lit)
    g=g+gaussian_filter(rng.normal(0,1,(n,n)),0.9)*3+rng.normal(0,1,(n,n))*1.2
    S_=np.clip(g,0,255); reg=A[pad:pad+n,pad:pad+n]; mk=(rr<=1.0)[...,None]
    reg[:]=reg*(1-mk)+np.stack([S_,S_,S_],-1)*mk
    groove=(rr>0.775)&(rr<=0.83); gv=4+7*np.exp(-((rr-0.83)/0.012)**2)*(0.3+0.7*lit)
    reg[:]=np.where(groove[...,None],np.stack([gv,gv,gv],-1),reg)
def cap_img(ang):
    R=RC*S; pad=int(0.12*R); n=int(2*R)+4; N=n+2*pad
    def fn(A):
        K.arr=A; K.knob_cap4((pad+n/2)/S,(pad+n/2)/S,RC,ang)
    im=K.two(fn,N)
    a=np.array(im).astype(float); h,w=a.shape[:2]; yy,xx=np.mgrid[0:h,0:w]
    d=np.hypot(xx-w/2+.5,yy-w/2+.5)*S      # px de supersample
    d=np.hypot(xx-w/2+.5,yy-h/2+.5)*S/S
    # recortar al disco de la tapa (la sombra propia no cae sobre el aro)
    m=np.clip((RC-np.hypot(xx-w/2+.5,yy-h/2+.5)),0,1)
    a[...,3]*=np.clip(RC-np.hypot(xx-w/2+.5,yy-h/2+.5)+0.5,0,1)
    return Image.fromarray(np.round(a).astype('uint8'),'RGBA')
def build(ang):
    N=int(2*RO*S)+4+2*int(0.12*RO*S)
    base=K.two(skirt,N)
    c=cap_img(ang); base.alpha_composite(c,((base.width-c.width)//2,(base.height-c.height)//2))
    return base
if __name__=='__main__':
    build(-135).save('mode_half.png'); build(135).save('mode_whole.png')
    bg=Image.new('RGBA',(252*2+40,252+20),(255,255,255,255))
    for i,f in enumerate(('mode_half.png','mode_whole.png')): bg.alpha_composite(Image.open(f),(10+i*(252+20),10))
    bg.save('mode_ring_prev.png')
