import math
from PIL import Image, ImageDraw, ImageFont
S=2; M=116; W=1696; H=1044; divX=1136
FM=lambda s: ImageFont.truetype('proj/Resources/Inter-Medium.ttf',int(s*S)); FR=lambda s: ImageFont.truetype('proj/Resources/Inter-Regular.ttf',int(s*S))
img=Image.new('RGB',(W*S,H*S),(255,255,255)); d=ImageDraw.Draw(img)
G=(60,60,60); LG=(150,150,150); FILL=(238,238,238)
sc=lambda *v:[int(x*S) for x in v]
def box(x0,y0,x1,y1,fill=FILL,ell=False):
    (d.ellipse if ell else d.rectangle)(sc(x0,y0,x1,y1),outline=G,width=3*S,fill=fill)
def txt(x,y,t,f,a='mm',c=G): d.text((x*S,y*S),t,font=f,fill=c,anchor=a)
def ticks(cx,cy,r,angles,major,gap=5,lmin=8,lmaj=12,w=3):
    for a in angles:
        t=math.radians(a); L=lmaj if a in major else lmin
        d.line(sc(cx+(r+gap)*math.sin(t),cy-(r+gap)*math.cos(t),cx+(r+gap+L)*math.sin(t),cy-(r+gap+L)*math.cos(t)),fill=G,width=int(w*S))
d.rectangle(sc(24,24,W-24,H-24),outline=G,width=3*S)
pt=24+M; dt=pt+112; db=dt+540; mb=db+112
box(140,dt,680,db,ell=True); box(796,pt,1020,pt+224,ell=True); box(796,mb-224,1020,mb,ell=True)
A13=[-135+22.5*i for i in range(13)]
ticks(908,pt+112,112,A13,{0.0},gap=5,lmin=8,lmaj=10)
d.line(sc(divX,60,divX,H-60),fill=LG,width=2*S)
fx=1314; box(fx-10,pt,fx+10,db,fill=(215,215,215)); box(fx-60,pt+150,fx+60,pt+186,fill=(255,255,255))
box(1492,pt,1556,db)
for i in range(7):   # marcas del fader a ambos lados (+6, 0, -6, -12, -24, -48, -inf)
    y=pt+(db-pt)*i/6
    d.line(sc(fx-90,y,fx-68,y),fill=G,width=int(3*S)); d.line(sc(fx+68,y,fx+90,y),fill=G,width=int(3*S))
L=FM(30); V=FR(26); TD=39
dtit=lambda x,t,top: ctitle(x,t,top-43.5-(d.textbbox((0,0),t,font=L,anchor='ls')[3]-d.textbbox((0,0),t,font=L,anchor='ls')[1])/(2*S))
def ctitle(x,t,ymid):   # titulo con la tinta centrada en ymid
    bb=d.textbbox((0,0),t,font=L,anchor='ls'); base=ymid-(bb[1]+bb[3])/(2*S); d.text((x*S,base*S),t,font=L,fill=G,anchor='ms'); print(t,'tinta',round(base+bb[1]/S,1),'->',round(base+bb[3]/S,1))
def vbelow(x,t,bottom):   # valor con su tinta a 43.5 px del borde inferior de la perilla (igual que el titulo arriba)
    bb=d.textbbox((0,0),t,font=V,anchor='ls'); base=bottom+43.5-bb[1]/S; d.text((x*S,base*S),t,font=V,fill=G,anchor='ms')
YM=(24+pt)/2   # centro entre el borde interno de la ventana (y=24) y el tope de la perilla / fader
dtit(410,'Depth',dt-49);   vbelow(410,'-6.00 dB',db)
ctitle(908,'Polarity',YM); vbelow(908,'45.0\u00b0',pt+224)
dtit(908,'Mode',mb-224)
ctitle(1405,'Output',YM); pass
bf=FM(24); GAP_T=10
bb=d.textbbox((0,0),'Input Listen',font=bf,anchor='ls'); th=(-bb[1])/S; desc=bb[3]/S
unit=64+GAP_T+th+desc; gap=((H-24)-db-unit)/2
by0=db+gap; by1=by0+64
YB=(by1+(H-24))/2; base=YB-(bb[1]+bb[3])/(2*S)  # tinta centrada entre fin de boton y borde
for cx,name in ((fx,'Bypass'),(1524,'Input Listen')):
    box(cx-32,by0,cx+32,by1); txt(cx,base,name,bf,'ms')
txt(796,base,'Half',bf,'ms'); txt(1020,base,'Whole',bf,'ms')
fv=FR(24); bbv=d.textbbox((0,0),'-3.00 dB',font=fv,anchor='ls'); ym=(db+by0)/2
d.text((fx*S,(ym-(bbv[1]+bbv[3])/(2*S))*S),'-3.00 dB',font=fv,fill=G,anchor='ms')
import numpy as np
from scipy.ndimage import gaussian_filter, shift as ndshift
# ===== Depth: perilla redibujada desde la referencia "PROCESS" + escala con dibujo unico por marca =====
def sh_rgb(m,col):  # m: mascara float 0..1 (n,n); devuelve (rgb, alpha) con luz fija arriba: filo superior claro, inferior oscuro, sombra proyectada hacia abajo
    k=1.4*S
    up=ndshift(m,(k,0),order=1); dn=ndshift(m,(-k,0),order=1)
    hl=np.clip(m-up,0,1); dk=np.clip(m-dn,0,1)
    base=np.array(col,float)[None,None,:]
    rgb=base*(1-0.35*dk[...,None])+ (255-base)*0.85*hl[...,None]
    return rgb
def blit(arr,x0,y0,m,col,shadow=0.75):
    # compone (con sombra) una mascara m en arr; (x0,y0)=esquina sup-izq en px de arr
    h,w=m.shape
    sx=ndshift(m,(3.2*S,0),order=1); sx=gaussian_filter(sx,1.6*S)*shadow
    ys,ye,xs,xe=max(y0,0),min(y0+h,arr.shape[0]),max(x0,0),min(x0+w,arr.shape[1])
    sl=(slice(ys-y0,ye-y0),slice(xs-x0,xe-x0)); reg=arr[ys:ye,xs:xe]
    reg[:]=reg*(1-sx[sl][...,None])
    rgb=sh_rgb(m,col)
    reg[:]=reg*(1-m[sl][...,None])+rgb[sl]*m[sl][...,None]
def glyph(text,ang,font,pad=14):
    # dibuja la marca rotada (la cabeza hacia afuera) y devuelve su mascara; la luz se aplica DESPUES de rotar
    bb=d.textbbox((0,0),text,font=font,anchor='lt'); w=bb[2]+2*pad*S; h=bb[3]+2*pad*S
    t=Image.new('L',(int(w),int(h)),0); ImageDraw.Draw(t).text((w/2,h/2),text,font=font,fill=255,anchor='mm')
    t=t.rotate(-ang,expand=True,resample=Image.BICUBIC)
    return np.array(t).astype(float)/255
arr=np.array(img).astype(float)
cx_,cy_,R_=410,dt+270,270; fs=FM(24)
LAB=['OFF','\u221248','\u221224','\u221212','\u22126','\u22123','0','1','2','3','4','5','6']
# --- sombra del cuerpo sobre el panel
n=int(2*R_*S)+4; c=n/2; Rr=R_*S
yy,xx=np.mgrid[0:n,0:n]; dx=xx-c+.5; dy=yy-c+.5; rr=np.hypot(dx,dy)/Rr; th=np.arctan2(dy,dx)
rng=np.random.default_rng(11)
lit=np.clip(-np.sin(th),0,1)            # 1 = arriba, 0 = abajo (luz cenital fija, vista plana)
X0=int(cx_*S-c); Y0=int(cy_*S-c)
pad=int(0.12*Rr); big=np.zeros((n+2*pad,n+2*pad)); big[pad:pad+n,pad:pad+n]=(rr<=1.0)
shd=gaussian_filter(ndshift(big,(0.025*Rr,0),order=1),0.04*Rr)*0.35
ys,xs=Y0-pad,X0-pad; reg=arr[ys:ys+n+2*pad,xs:xs+n+2*pad]; reg[:]=reg*(1-shd[...,None])
# --- faldon (perilla de abajo): negro, borde biselado con filo claro arriba
skirt=rr<=1.0; t=np.clip((rr-0.83)/0.17,0,1)
g=9+10*t**2+ 20*np.exp(-((rr-0.985)/0.012)**2)*(0.25+0.75*lit)           # filo exterior
g=g+gaussian_filter(rng.normal(0,1,(n,n)),0.9)*3+rng.normal(0,1,(n,n))*1.2
S_=np.clip(g,0,255); reg2=arr[Y0:Y0+n,X0:X0+n]; mk=skirt[...,None]
reg2[:]=reg2*(1-mk)+np.stack([S_,S_,S_],-1)*mk
# anillo de brillo suave entre faldon y tapa
groove=(rr>0.775)&(rr<=0.83); gv=4+7*np.exp(-((rr-0.83)/0.012)**2)*(0.3+0.7*lit)
reg2[:]=np.where(groove[...,None],np.stack([gv,gv,gv],-1),reg2)
# --- tapa: negro mate con grano fino y estrias circulares muy suaves
cap=rr<=0.775; q=np.clip(rr/0.775,0,1)
rad=gaussian_filter(rng.normal(0,1,4000),2.0); rn=rad[(np.clip(rr,0,1)*3900).astype(int)]
cg=14+8*(1-q**1.5)+gaussian_filter(rng.normal(0,1,(n,n)),0.8)*3.0+rng.normal(0,1,(n,n))*1.5
rim=np.exp(-((rr-0.74)/0.014)**2)*(55*lit**1.3)                    # arco fino de luz junto al borde de la tapa
cg=np.clip(cg+rim,0,255)
reg2[:]=np.where(cap[...,None],np.stack([cg,cg,cg],-1),reg2)
# --- indicador (-6 dB = -45 deg): afilado, claro, con filo superior luminoso
ang=-45.0; ta=math.radians(ang)
pm=Image.new('L',(n*4,n*4),0); pd=ImageDraw.Draw(pm)
p0=(c*4+0.40*Rr*4*math.sin(ta),c*4-0.40*Rr*4*math.cos(ta)); p1=(c*4+0.85*Rr*4*math.sin(ta),c*4-0.85*Rr*4*math.cos(ta))
nx,ny=math.cos(ta),math.sin(ta); w0=0.023*Rr*4; w1=0.023*Rr*4
pd.polygon([(p0[0]-nx*w0,p0[1]-ny*w0),(p0[0]+nx*w0,p0[1]+ny*w0),(p1[0]+nx*w1,p1[1]+ny*w1),(p1[0]-nx*w1,p1[1]-ny*w1)],fill=255)
for p,wd in ((p0,w0),): pd.ellipse((p[0]-wd,p[1]-wd,p[0]+wd,p[1]+wd),fill=255)
pm=np.array(pm.resize((n,n),Image.LANCZOS)).astype(float)/255
pm=pm*np.clip((0.775-rr)*Rr+0.5,0,1)   # el extremo exterior se corta siguiendo el borde circular de la tapa
blit(reg2:=arr[Y0:Y0+n,X0:X0+n],0,0,pm,(205,205,205),shadow=0.6) if False else None
sub=arr[Y0:Y0+n,X0:X0+n]
sx=gaussian_filter(ndshift(pm,(0.016*Rr,0.004*Rr),order=1),0.010*Rr)*0.95; sub[:]=sub*(1-sx[...,None])
rgbp=sh_rgb(pm,(196,196,196))
w=np.clip((rr-0.715)/0.07,0,1); w=w*w*(3-2*w)                 # la marca baja por el chaflan: pierde el filo y se oscurece
colv=196-56*w-24*np.clip((rr-0.80)/0.065,0,1)                  # sigue oscureciendose al apoyarse sobre el faldon
rgbp=rgbp*(1-w[...,None])+np.stack([colv,colv,colv],-1)*w[...,None]
sub[:]=sub*(1-pm[...,None])+rgbp*pm[...,None]

def knob_cap(cx,cy,r,ang,seed=5):
    # solo tapa + indicador. Marca blanca, sombra, arco de luz y grano con el MISMO tamaño en pixeles que en Depth (no escalan con la perilla)
    R=r*S; pad=int(0.12*R); n=int(2*R)+4; c=n/2
    yy,xx=np.mgrid[0:n,0:n]; dx=xx-c+.5; dy=yy-c+.5; q=np.hypot(dx,dy)/R; th=np.arctan2(dy,dx)
    de=(1-q)*r                                # distancia al borde en px (sin supersample)
    rng=np.random.default_rng(seed); lit=np.clip(-np.sin(th),0,1)
    X0=int(cx*S-c); Y0=int(cy*S-c)
    big=np.zeros((n+2*pad,n+2*pad)); big[pad:pad+n,pad:pad+n]=(q<=1.0)
    shd=gaussian_filter(ndshift(big,(0.025*R,0),order=1),0.04*R)*0.35
    reg=arr[Y0-pad:Y0+n+pad,X0-pad:X0+n+pad]; reg[:]=reg*(1-shd[...,None])
    sub=arr[Y0:Y0+n,X0:X0+n]
    cg=14+8*(1-np.clip(q,0,1)**1.5)+gaussian_filter(rng.normal(0,1,(n,n)),0.8)*3.0+rng.normal(0,1,(n,n))*1.5
    cg=cg+np.exp(-((de-9.5)/3.8)**2)*(55*lit**1.3)        # arco de luz: a 9.5 px del borde, 3.8 px de ancho (igual que Depth)
    cg=np.clip(cg,0,255); m=np.clip((1.0-q)*R+0.5,0,1)[...,None]
    sub[:]=sub*(1-m)+np.stack([cg,cg,cg],-1)*m
    ta=math.radians(ang)
    HW=0.023*270*S*4                           # medio ancho de la marca en Depth (6.2 px)
    L=(1-0.40/0.775)*r*S               # largo proporcional a la perilla: misma relacion que Depth (el extremo interior a 0.516 del radio de la tapa)
    pm=Image.new('L',(n*4,n*4),0); pd=ImageDraw.Draw(pm)
    p0=(c*4+(R-L)*4*math.sin(ta),c*4-(R-L)*4*math.cos(ta)); p1=(c*4+1.1*R*4*math.sin(ta),c*4-1.1*R*4*math.cos(ta))
    nx,ny=math.cos(ta),math.sin(ta); w0=HW/270/S*S*270/270*1.0
    w0=0.023*270*S*4   # medio ancho en la escala x4 (igual que Depth)
    pd.polygon([(p0[0]-nx*w0,p0[1]-ny*w0),(p0[0]+nx*w0,p0[1]+ny*w0),(p1[0]+nx*w0,p1[1]+ny*w0),(p1[0]-nx*w0,p1[1]-ny*w0)],fill=255)
    pd.ellipse((p0[0]-w0,p0[1]-w0,p0[0]+w0,p0[1]+w0),fill=255)
    pm=np.array(pm.resize((n,n),Image.LANCZOS)).astype(float)/255
    pm=pm*np.clip((1.0-q)*R+0.5,0,1)
    sx=gaussian_filter(ndshift(pm,(4.3*S,1.1*S),order=1),2.7*S)*0.95; sub[:]=sub*(1-sx[...,None])
    rgbp=sh_rgb(pm,(196,196,196))
    w=np.clip((16.2-de)/18.9,0,1); w=w*w*(3-2*w); colv=196-56*w
    rgbp=rgbp*(1-w[...,None])+np.stack([colv,colv,colv],-1)*w[...,None]
    sub[:]=sub*(1-pm[...,None])+rgbp*pm[...,None]
def knob_cap4(cx,cy,r,ang,seed=9):
    # solo tapa + indicador. Marca blanca, sombra, arco de luz y grano con el MISMO tamaño en pixeles que en Depth (no escalan con la perilla)
    R=r*S; pad=int(0.12*R); n=int(2*R)+4; c=n/2
    yy,xx=np.mgrid[0:n,0:n]; dx=xx-c+.5; dy=yy-c+.5; q=np.hypot(dx,dy)/R; th=np.arctan2(dy,dx)
    de=(1-q)*r                                # distancia al borde en px (sin supersample)
    rng=np.random.default_rng(seed); lit=np.clip(-np.sin(th),0,1)
    X0=int(cx*S-c); Y0=int(cy*S-c)
    big=np.zeros((n+2*pad,n+2*pad)); big[pad:pad+n,pad:pad+n]=(q<=1.0)
    shd=gaussian_filter(ndshift(big,(0.025*R,0),order=1),0.04*R)*0.35
    reg=arr[Y0-pad:Y0+n+pad,X0-pad:X0+n+pad]; reg[:]=reg*(1-shd[...,None])
    sub=arr[Y0:Y0+n,X0:X0+n]
    cg=14+8*(1-np.clip(q,0,1)**1.5)+gaussian_filter(rng.normal(0,1,(n,n)),0.8)*3.0+rng.normal(0,1,(n,n))*1.5
    cg=cg+np.exp(-((de-9.5)/3.8)**2)*(55*lit**1.3)        # arco de luz: a 9.5 px del borde, 3.8 px de ancho (igual que Depth)
    th_=math.radians(ang); ux,uy=math.sin(th_),-math.cos(th_); vx,vy=math.cos(th_),math.sin(th_)
    sa=(dx*ux+dy*uy)/R; ta_=(dx*vx+dy*vy)/R            # sa: a lo largo de la barra (+ hacia el indicador), ta_: a lo ancho (+ lado del brillo grande)
    sm=lambda x,a0,a1: np.clip((x-a0)/(a1-a0),0,1)**2*(3-2*np.clip((x-a0)/(a1-a0),0,1))
    # perfil de luminosidad a lo ancho MEDIDO en la captura (marco de la barra), dibujado con mis propias curvas; plano a lo largo de la barra
    ta_pts=np.array([-1.0,-0.97,-0.9,-0.8,-0.7,-0.65,-0.60,-0.55,-0.5,-0.4,-0.3,-0.26, 0.26,0.30,0.40,0.50,0.55,0.60,0.65,0.70,0.80,0.90,0.97,1.0])
    Y_pts =np.array([ 110, 150, 164, 168, 167, 120, 76, 50, 43, 42, 46, 60,  100, 86, 76, 82, 95, 125, 170, 200, 205, 204, 190, 150],float)
    tt=np.linspace(-1,1,2001); prof=np.interp(tt,ta_pts,Y_pts); prof=gaussian_filter(prof,0.012*1000)
    thw=th_; vxw,vyw=math.cos(thw),math.sin(thw)
    taw=(dx*vxw+dy*vyw)/R                      # los brillos laterales quedan FIJOS en el espacio (la luz no gira con la perilla)
    Yside=np.interp(np.clip(taw,-1,1),tt,prof)
    sa_b=-sa                                   # la captura tiene el indicador del lado negativo
    HWb=0.235
    barm=np.clip((HWb-np.abs(ta_))*R/1.2+0.5,0,1)*np.clip((0.94-np.abs(sa_b)*0-sa)*R/1.2+0.5,0,1)*np.clip((sa+0.90)*R/1.2+0.5,0,1)
    Ybar=146+5*np.sin(np.pi*np.clip(ta_/HWb,-1,1))
    cast=gaussian_filter(ndshift(barm,(0.075*R,0.12*R),order=1),0.07*R)*(1-barm)
    Yall=Yside*(1-barm)+Ybar*barm
    gmap=8+8*(1-np.clip(q,0,1)**1.5)+(Yall-40)*0.13                      # a la paleta oscura de las otras perillas
    # bisel de la barra: filo iluminado donde mira a la luz (arriba-izquierda), oscuro en el lado contrario
    bm=gaussian_filter(barm,1.3*S); gy,gx=np.gradient(bm); mg=np.hypot(gx,gy)+1e-9
    nxo,nyo=-gx/mg,-gy/mg; ld=nxo*(-0.5)+nyo*(-0.85)
    edge=np.clip(mg*S*3.2,0,1)
    bev=16*edge*np.clip(ld,0,1)-7*edge*np.clip(-ld,0,1)
    rings=0.8*np.sin(q*R/1.6)*np.clip(1-q/0.7,0,1)
    rimk=1-0.5*np.clip((q-0.93)/0.07,0,1)       # un poco mas oscuro justo en el borde
    cg=(gmap+bev+rings*barm)*rimk+gaussian_filter(rng.normal(0,1,(n,n)),0.8)*3.0+rng.normal(0,1,(n,n))*1.5+np.exp(-((de-9.5)/3.8)**2)*(24*lit**1.3)
    cg=np.clip(cg,0,255); m=np.clip((1.0-q)*R+0.5,0,1)[...,None]
    sub[:]=sub*(1-m)+np.stack([cg,cg,cg],-1)*m
    ta=math.radians(ang)
    HW=0.023*270*S*4                           # medio ancho de la marca en Depth (6.2 px)
    L=(1-0.40/0.775)*r*S               # largo proporcional a la perilla: misma relacion que Depth (el extremo interior a 0.516 del radio de la tapa)
    pm=Image.new('L',(n*4,n*4),0); pd=ImageDraw.Draw(pm)
    p0=(c*4+(R-L)*4*math.sin(ta),c*4-(R-L)*4*math.cos(ta)); p1=(c*4+1.1*R*4*math.sin(ta),c*4-1.1*R*4*math.cos(ta))
    nx,ny=math.cos(ta),math.sin(ta); w0=HW/270/S*S*270/270*1.0
    w0=0.023*270*S*4   # medio ancho en la escala x4 (igual que Depth)
    pd.polygon([(p0[0]-nx*w0,p0[1]-ny*w0),(p0[0]+nx*w0,p0[1]+ny*w0),(p1[0]+nx*w0,p1[1]+ny*w0),(p1[0]-nx*w0,p1[1]-ny*w0)],fill=255)
    pd.ellipse((p0[0]-w0,p0[1]-w0,p0[0]+w0,p0[1]+w0),fill=255)
    pm=np.array(pm.resize((n,n),Image.LANCZOS)).astype(float)/255
    pm=pm*np.clip((1.0-q)*R+0.5,0,1)
    sx=gaussian_filter(ndshift(pm,(4.3*S,1.1*S),order=1),2.7*S)*0.95; sub[:]=sub*(1-sx[...,None])
    rgbp=sh_rgb(pm,(196,196,196))
    w=np.clip((16.2-de)/18.9,0,1); w=w*w*(3-2*w); colv=196-56*w
    rgbp=rgbp*(1-w[...,None])+np.stack([colv,colv,colv],-1)*w[...,None]
    sub[:]=sub*(1-pm[...,None])+rgbp*pm[...,None]
knob_cap(908,pt+112,112,-67.5)
knob_cap4(908,mb-112,112,float(__import__('sys').argv[1]))   # Polarity: 45 grados dentro de 0..180
img=Image.fromarray(np.clip(arr,0,255).astype('uint8')); d=ImageDraw.Draw(img)
# escala de Depth estilo referencia: puntitos + numeros (0 dB al centro, +6 a la derecha lineal, mitad izquierda logaritmica)
LAB=['\u2212\u221e','\u221248','\u221224','\u221212','\u22126','\u22123','0','1','2','3','4','5','6']
cx_,cy_,R_=410,dt+270,270; fs=FM(24)
for i,a in enumerate(A13):
    t=math.radians(a)
    rd=R_+12; x=cx_+rd*math.sin(t); y=cy_-rd*math.cos(t); rr_=3.4
    d.ellipse(sc(x-rr_,y-rr_,x+rr_,y+rr_),fill=G)
    if i<12:
        t2=math.radians(a+11.25); x=cx_+rd*math.sin(t2); y=cy_-rd*math.cos(t2); rr_=2.0
        d.ellipse(sc(x-rr_,y-rr_,x+rr_,y+rr_),fill=G)
    rt=R_+40; x=cx_+rt*math.sin(t); y=cy_-rt*math.cos(t)
    d.text((x*S,y*S),LAB[i],font=fs,fill=G,anchor='mm')
# ticks de Mode: salen en diagonal (angulo +-135) hasta el borde de la perilla y bajan a 90 hasta justo antes del texto
cy=mb-112; r0=112+5; tt=base+(d.textbbox((0,0),'Half',font=bf,anchor='ls')[1])/S-28
for sg,xe in ((-1,796),(1,1020)):
    x0=908+sg*r0*math.sin(math.radians(45)); y0=cy+r0*math.cos(math.radians(45))
    dx=abs(xe-x0); P=[(x0,y0),(xe,y0+dx),(xe,tt)]
    d.line([(px*S,py*S) for px,py in P],fill=G,width=3*S,joint='curve')
img.resize((W,H),Image.LANCZOS).save(__import__('sys').argv[2])
