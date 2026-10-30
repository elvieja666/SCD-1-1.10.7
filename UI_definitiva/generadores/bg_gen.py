import math
from PIL import Image, ImageDraw, ImageFont
S=2; M=116; W=1389; H=1044; divX=871   # ancho 1476: la seccion izquierda se achico (x0.80) y la derecha se corrio 220 a la izquierda
FM=lambda s: ImageFont.truetype('proj/Resources/Inter-Medium.ttf',int(s*S)); FR=lambda s: ImageFont.truetype('proj/Resources/Inter-Regular.ttf',int(s*S))
import numpy as np
from scipy.ndimage import gaussian_filter
_r=np.random.default_rng(77)
_h,_w=H*S,W*S
_lf=gaussian_filter(_r.normal(0,1,(-(-_h//8),-(-_w//8))),6); _lf=np.kron(_lf,np.ones((8,8)))[:_h,:_w]
_pl=40+_lf*2.0+gaussian_filter(_r.normal(0,1,(_h,_w)),0.7)*5.2+_r.normal(0,1,(_h,_w))*2.6
def _gr(sig,amp,seed):   # grano de tamano medio/grande, normalizado
    f=gaussian_filter(np.random.default_rng(seed).normal(0,1,(_h,_w)),sig); return f/f.std()*amp
_pl=_pl+_gr(1.5,1.1,301)+_gr(3.2,0.9,302)+_r.normal(0,1,(_h,_w))*0.4     # apenas mas grano
_sp=(_r.random((_h,_w))>0.997)*_r.normal(0,18,(_h,_w))                # motitas sueltas de la pintura
_yy,_xx=np.mgrid[0:_h,0:_w]; _ed=np.minimum(np.minimum(_xx,_w-_xx),np.minimum(_yy,_h-_yy))/S
def _sn(sig,seed):   # ruido suave de gran escala (sig en unidades de diseno), normalizado
    f=gaussian_filter(np.random.default_rng(seed).normal(0,1,(H//4,W//4)),sig/4.0); f=f/f.std()
    return np.asarray(Image.fromarray(f.astype('float32')).resize((_w,_h),Image.BICUBIC))
_ny=_yy/_h                                                                 # 0 arriba .. 1 abajo
_pl=_pl+5.5-11.0*_ny**0.9                                                  # 1) luz suave desde arriba: mas claro arriba, mas oscuro abajo
_pl=_pl+4.0*np.exp(-(((_xx/S-W*0.42)/(W*0.45))**2+((_yy/S-60)/(H*0.30))**2))   #    charco de luz difusa arriba
_pl=_pl+0.68*_sn(130,201)+0.4*_sn(45,202)+0.18*_sn(14,203)                   # 2) manchas de tono muy suaves (pintura no del todo pareja)
_pl=_pl-7*np.exp(-_ed/70)                                                # sombra apenas perceptible hacia los bordes
_pl=np.clip(_pl+_sp,0,255)
img=Image.fromarray(np.stack([_pl,_pl,_pl*1.02],-1).clip(0,255).astype('uint8')); d=ImageDraw.Draw(img)
G=(240,240,240); LG=(95,95,98); FILL=(22,22,24)
sc=lambda *v:[int(x*S) for x in v]
def box(x0,y0,x1,y1,fill=FILL,ell=False):
    (d.ellipse if ell else d.rectangle)(sc(x0,y0,x1,y1),outline=G,width=3*S,fill=fill)
def txt(x,y,t,f,a='mm',c=G): d.text((x*S,y*S),t,font=f,fill=c,anchor=a)
def ticks(cx,cy,r,angles,major,gap=5,lmin=8,lmaj=12,w=3):
    for a in angles:
        t=math.radians(a); L=lmaj if a in major else lmin
        d.line(sc(cx+(r+gap)*math.sin(t),cy-(r+gap)*math.cos(t),cx+(r+gap+L)*math.sin(t),cy-(r+gap+L)*math.cos(t)),fill=G,width=int(w*S))
d.rectangle(sc(24,24,W-24,H-24),outline=(14,14,15),width=4*S); d.rectangle(sc(28,28,W-28,H-28),outline=(78,78,82),width=1*S)
pt=24+M; dt=pt+112; db=dt+540; mb=db+112; fb=753   # fb = fondo de la escala del fader (-inf)
A13=[-135+22.5*i for i in range(13)]
A11=[-135+27*i for i in range(11)]
ticks(230,825,90,A11,{-135.0,135.0},gap=9,lmin=8,lmaj=10)   # Fade: 11 posiciones (0..100 %)
d.line(sc(divX,60,divX,H-60),fill=(12,12,13),width=4*S); d.line(sc(divX+3,60,divX+3,H-60),fill=(84,84,88),width=1*S)
fx=1027; pass
box(1181,pt,1245,fb)
for i in range(7):   # marcas del fader a ambos lados (+6, 0, -6, -12, -24, -48, -inf)
    y=pt+(fb-pt)*i/6
    d.line(sc(fx-60.5,y,fx-15,y),fill=G,width=int(3*S)); d.line(sc(fx+15,y,fx+60.5,y),fill=G,width=int(3*S))   # largas, iguales a los dos lados (a la izquierda con el numero), desde el recorrido
for i in range(6):   # marcas intermedias (+3, -3, -9, -18, -36, -54 dB): a mitad de cada tramo, que es lineal en dB; mitad de largo que las cortas
    y=pt+(fb-pt)*(i+0.5)/6
    d.line(sc(fx-23.5,y,fx-15,y),fill=G,width=int(3*S)); d.line(sc(fx+15,y,fx+23.5,y),fill=G,width=int(3*S))
L=FM(30); V=FR(26); TD=39
for i,t in enumerate(['+6','0','\u22126','\u221212','\u221224','\u221248','\u2212\u221e']):   # valor de cada marca del fader (como la escala del Depth)
    d.text(((fx-60.5-8)*S,(pt+(fb-pt)*i/6)*S),t,font=FM(26),fill=G,anchor='rm')
dtit=lambda x,t,top: ctitle(x,t,top-43.5-(d.textbbox((0,0),t,font=L,anchor='ls')[3]-d.textbbox((0,0),t,font=L,anchor='ls')[1])/(2*S))
def ctitle(x,t,ymid):   # titulo con la tinta centrada en ymid
    bb=d.textbbox((0,0),t,font=L,anchor='ls'); base=ymid-(bb[1]+bb[3])/(2*S); d.text((x*S,base*S),t,font=L,fill=G,anchor='ms'); print(t,'tinta',round(base+bb[1]/S,1),'->',round(base+bb[3]/S,1))
def vbelow(x,t,bottom):   # valor con su tinta a 43.5 px del borde inferior de la perilla (igual que el titulo arriba)
    bb=d.textbbox((0,0),t,font=V,anchor='ls'); base=bottom+43.5-bb[1]/S; d.text((x*S,base*S),t,font=V,fill=G,anchor='ms')
YM=(24+pt)/2   # centro entre el borde interno de la ventana (y=24) y el tope de la perilla / fader
ctitle(447,'Depth',YM)
ctitle(230,'Fade',677)
ctitle(664,'Mode',677)
ctitle(fx,'Output',YM); ctitle(1213,'SC Input',YM); pass
bf=FM(30); GAP_T=10
bb=d.textbbox((0,0),'Input Listen',font=bf,anchor='ls'); th=(-bb[1])/S; desc=bb[3]/S
unit=64+GAP_T+th+desc; gap=((H-24)-db-unit)/2
by0=db+gap; by1=by0+64
YB=(by1+(H-24))/2; base=YB-(bb[1]+bb[3])/(2*S)  # tinta centrada entre fin de boton y borde
for cx,name in ((fx,"Bypass"),(1213,"SC Listen")):
    txt(cx,base,name,bf,'ms')
hf=FM(30)   # Fabricado en...: tamano 30
BH=978   # linea de base de Half / Whole = la de los valores de abajo
HW=FM(26); txt(574,BH,'Half',HW,'ms'); txt(754,BH,'Whole',HW,'ms')
fv=FR(24); bbv=d.textbbox((0,0),'-3.00 dB',font=fv,anchor='ls'); ym=(db+by0)/2
pass
import numpy as np
arr=np.array(img).astype(float)
img=Image.fromarray(np.clip(arr,0,255).astype('uint8')); d=ImageDraw.Draw(img)
# escala de Depth estilo referencia: puntitos + numeros (0 dB al centro, +6 a la derecha lineal, mitad izquierda logaritmica)
LAB=['\u2212\u221e','\u221248','\u221224','\u221212','\u22126','\u22123','0','1','2','3','4','5','6']
cx_,cy_,R_=447,416,217; fs=FM(26)
for i,a in enumerate(A13):
    t=math.radians(a)
    rd=R_+14.5; x=cx_+rd*math.sin(t); y=cy_-rd*math.cos(t); rr_=3.4
    d.ellipse(sc(x-rr_,y-rr_,x+rr_,y+rr_),fill=G)
    if i<12:
        t2=math.radians(a+11.25); x=cx_+rd*math.sin(t2); y=cy_-rd*math.cos(t2); rr_=2.0
        d.ellipse(sc(x-rr_,y-rr_,x+rr_,y+rr_),fill=G)
    bb_=d.textbbox((0,0),LAB[i],font=fs,anchor='mm'); w_=(bb_[2]-bb_[0])/S; h_=(bb_[3]-bb_[1])/S
    rt=rd+13+abs(math.sin(t))*w_/2+abs(math.cos(t))*h_/2; x=cx_+rt*math.sin(t); y=cy_-rt*math.cos(t)   # el numero queda siempre a la misma distancia de los puntitos, sin pisarlos
    d.text((x*S,y*S),LAB[i],font=fs,fill=G,anchor='mm')
# ticks de Mode: salen en diagonal (angulo +-135) hasta el borde de la perilla y bajan a 90 hasta justo antes del texto
cy=825; r0=90+5; tt=BH+(d.textbbox((0,0),'Half',font=FM(26),anchor='ls')[1])/S-28
for sg,xe in ((-1,574),(1,754)):
    x0=664+sg*r0*math.sin(math.radians(45)); y0=cy+r0*math.cos(math.radians(45))
    dx=abs(xe-x0); P=[(x0,y0),(xe,y0+dx),(xe,tt)]
    d.line([(px*S,py*S) for px,py in P],fill=G,width=3*S,joint='curve')
# firma: las tres estrellas pasan por detras del texto
_st=Image.open('ui_assets/art/estrellas_alpha.png'); _k=310/_st.width
_st=_st.resize((int(_st.width*_k*S),int(_st.height*_k*S)),Image.LANCZOS)
_lay=Image.new("RGB",_st.size,(122,122,126))
# img.paste(_lay,(int(60*S),int(942*S-_st.height/2)),_st.point(lambda v:int(v*0.9)))
d=ImageDraw.Draw(img)
img.resize((W,H),Image.LANCZOS).save(__import__('sys').argv[2])
