import math
from PIL import Image, ImageDraw, ImageFont
S=2; M=105; W=1671; H=1044; divX=1153   # ventana 523 x 321 (arte 1671 x 1044 con el marco de 24 unidades)
FM=lambda s: ImageFont.truetype('fonts2/InterMedium.ttf',int(s*S))    # Inter Medium: titulos y botones
FL=lambda s: ImageFont.truetype('fonts2/InterLight.ttf',int(s*S))     # Inter Light: escalas, valores, Half / Whole y la frase de abajo
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
G=(240,240,240); FILL=(22,22,24)
sc=lambda *v:[int(x*S) for x in v]
def box(x0,y0,x1,y1,fill=FILL,ell=False):
    (d.ellipse if ell else d.rectangle)(sc(x0,y0,x1,y1),outline=G,width=3*S,fill=fill)
def txt(x,y,t,f,a='mm',c=G): d.text((x*S,y*S),t,font=f,fill=c,anchor=a)
def ticks(cx,cy,r,angles,major,gap=5,lmin=8,lmaj=12,w=3):
    for a in angles:
        t=math.radians(a); L=lmaj if a in major else lmin
        d.line(sc(cx+(r+gap)*math.sin(t),cy-(r+gap)*math.cos(t),cx+(r+gap+L)*math.sin(t),cy-(r+gap+L)*math.cos(t)),fill=G,width=int(w*S))
d.rectangle(sc(24,24,W-24,H-24),outline=(14,14,15),width=4*S); d.rectangle(sc(28,28,W-28,H-28),outline=(78,78,82),width=1*S)
pt=24+M; db=pt+652; fb=742   # fb = fondo de la escala del fader (-inf)
A13=[-135+22.5*i for i in range(13)]
A11=[-135+27*i for i in range(11)]
ticks(922,243,114,A11,{-135.0,135.0},gap=9,lmin=8.5,lmaj=8.5)   # Fade: 11 posiciones (0..100 %)
d.line(sc(divX,60,divX,H-60),fill=(12,12,13),width=4*S); d.line(sc(divX+3,60,divX+3,H-60),fill=(84,84,88),width=1*S)
fx=1309
box(1463,pt,1527,fb)
for i in range(7):   # marcas del fader a ambos lados (+6, 0, -6, -12, -24, -48, -inf)
    y=pt+(fb-pt)*i/6
    d.line(sc(fx-60.5,y,fx-15,y),fill=G,width=int(3*S)); d.line(sc(fx+15,y,fx+60.5,y),fill=G,width=int(3*S))   # largas, iguales a los dos lados (a la izquierda con el numero), desde el recorrido
for i in range(6):   # marcas intermedias (+3, -3, -9, -18, -36, -54 dB): a mitad de cada tramo, que es lineal en dB; mitad de largo que las cortas
    y=pt+(fb-pt)*(i+0.5)/6
    d.line(sc(fx-23.5,y,fx-15,y),fill=G,width=int(3*S)); d.line(sc(fx+15,y,fx+23.5,y),fill=G,width=int(3*S))
def _ink(t,font,anchor):   # caja de tinta real (se dibuja aparte), relativa al punto de anclaje
    W_,H_=int(font.size*len(t)*1.2)+200,int(font.size*3); im_=Image.new('L',(W_,H_),0); ImageDraw.Draw(im_).text((W_//2,H_//2),t,font=font,fill=255,anchor=anchor)
    b_=im_.point(lambda v:255 if v>100 else 0).getbbox(); return (b_[0]-W_//2,b_[1]-H_//2,b_[2]-W_//2,b_[3]-H_//2)
L=FM(30)
for i,t in enumerate(['+6','0','\u22126','\u221212','\u221224','\u221248','\u2212\u221e']):   # valor de cada marca del fader (como la escala del Depth)
    _f=FL(30); _y=pt+(fb-pt)*i/6; _bb=_ink(t,_f,'rm')   # caja de tinta: borde derecho y centro vertical exactos en la marca
    d.text(((fx-60.5-10)*S-_bb[2],(_y*S)-(_bb[1]+_bb[3])/2),t,font=_f,fill=G,anchor='rm')
def ctitle(x,t,ymid):   # titulo con la tinta centrada en ymid (vertical) y en x (horizontal)
    bb=d.textbbox((0,0),t,font=L,anchor='ls'); base=ymid-(bb[1]+bb[3])/(2*S); bx=_ink(t,L,'ms')
    d.text((x*S-(bx[0]+bx[2])/2,base*S),t,font=L,fill=G,anchor='ms'); print(t,'tinta',round(base+bb[1]/S,1),'->',round(base+bb[3]/S,1))
YM=(24+pt)/2   # centro entre el borde interno de la ventana (y=24) y el tope de la perilla / fader
ctitle(416,'Depth',147)
ctitle(922,'Fade',YM)
ctitle(922,'Mode',633.5)
ctitle(fx,'Output',YM); ctitle(1495,'SC Input',YM)
bf=FM(30); GAP_T=10
bb=d.textbbox((0,0),'Input Listen',font=bf,anchor='ls'); th=(-bb[1])/S; desc=bb[3]/S
unit=64+GAP_T+th+desc; gap=((H-24)-db-unit)/2
by0=db+gap; by1=by0+64
YB=(by1+(H-24))/2; base=YB-(bb[1]+bb[3])/(2*S)  # tinta centrada entre fin de boton y borde
for cx,name in ((fx,"Bypass"),(1495,"SC Listen")):
    txt(cx,base,name,bf,'ms')
BH=base   # linea de base de Half / Whole = la de los valores de abajo
HW=FL(30); txt(808,BH,'Half',HW,'ms'); txt(1036,BH,'Whole',HW,'ms')
# escala de Depth estilo referencia: puntitos + numeros (0 dB al centro, +6 a la derecha lineal, mitad izquierda logaritmica)
LAB=['\u2212\u221e','\u221248','\u221224','\u221212','\u22126','\u22123','0','1','2','3','4','5','6']
cx_,cy_,R_=416,522,275; fs=FL(30)
for i,a in enumerate(A13):
    t=math.radians(a)
    rd=R_+14.5; x=cx_+rd*math.sin(t); y=cy_-rd*math.cos(t); rr_=3.4
    d.ellipse(sc(x-rr_,y-rr_,x+rr_,y+rr_),fill=G)
    if i<12:
        t2=math.radians(a+11.25); x=cx_+rd*math.sin(t2); y=cy_-rd*math.cos(t2); rr_=2.0
        d.ellipse(sc(x-rr_,y-rr_,x+rr_,y+rr_),fill=G)
    bb_=d.textbbox((0,0),LAB[i],font=fs,anchor='mm'); ox0,oy0,ox1,oy1=[v/S for v in bb_]   # caja de la tinta respecto del centro del texto
    px_,py_=cx_+rd*math.sin(t),cy_-rd*math.cos(t)    # el puntito
    def _gap(s):   # distancia del puntito a la caja de tinta si el texto se centra a s del puntito, hacia afuera
        x=px_+s*math.sin(t); y=py_-s*math.cos(t)
        ddx=max(x+ox0-px_,0,px_-(x+ox1)); ddy=max(y+oy0-py_,0,py_-(y+oy1)); return math.hypot(ddx,ddy)
    lo,hi=0.0,200.0
    for _ in range(40):
        mid=(lo+hi)/2
        if _gap(mid)<13.0: lo=mid
        else: hi=mid
    x=px_+hi*math.sin(t); y=py_-hi*math.cos(t)   # cada numero queda a 13 unidades exactas de su puntito (distancia real a la tinta)
    d.text((x*S,y*S),LAB[i],font=fs,fill=G,anchor='mm')
# ticks de Mode: salen en diagonal (angulo +-135) hasta el borde de la perilla y bajan a 90 hasta justo antes del texto
cy=800; r0=114+5; ttop=BH+(d.textbbox((0,0),'Half',font=HW,anchor='ls')[1])/S   # techo de los textos Half / Whole
for sg,xe in ((-1,808),(1,1036)):
    x0=922+sg*r0*math.sin(math.radians(45)); y0=cy+r0*math.cos(math.radians(45))
    dx=abs(xe-x0); tt=((y0+dx)+ttop)/2; print('leader',round(y0+dx,1),'->',round(tt,1),'texto',round(ttop,1)); P=[(x0,y0),(xe,y0+dx),(xe,tt)]   # la linea baja hasta el punto medio entre el quiebre y el techo del texto
    d.line([(px*S,py*S) for px,py in P],fill=G,width=3*S,joint='curve')
# nombre del plugin (Michroma, ancha), justo arriba de la frase
_fl=FL(25); _ft=base+d.textbbox((0,0),'Fabricado en Buenos Aires Argentina',font=_fl,anchor='ls')[1]/S   # techo de la frase
_bot=_ft-12; _top=915.5   # el logo termina 12 sobre la frase y su techo queda en el borde de abajo de los botones (donde terminan)
_b52=ImageFont.truetype('fonts2/Michroma-Regular.ttf',52*S).getbbox('rmd-1',anchor='ls'); _sz=52*(_bot-_top)/((_b52[3]-_b52[1])/S)
_lf2=ImageFont.truetype('fonts2/Michroma-Regular.ttf',int(round(_sz*S))); _bb=d.textbbox((0,0),'rmd-1',font=_lf2,anchor='ls'); _by=_bot-_bb[3]/S; print('logo tamano',round(_sz,1),'techo',round(_by+_bb[1]/S,1))
d.text((64*S,_by*S),'rmd-1',font=_lf2,fill=(240,240,240),anchor='ls',stroke_width=int(1.2*S),stroke_fill=(240,240,240))
d.text((64*S,base*S),'Fabricado en Buenos Aires Argentina',font=_fl,fill=(240,240,240),anchor='ls')   # misma linea que Bypass / Half / Whole
img.resize((W,H),Image.LANCZOS).save(__import__('sys').argv[2])
