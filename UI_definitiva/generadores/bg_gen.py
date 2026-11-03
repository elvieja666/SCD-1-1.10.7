import math
from PIL import Image, ImageDraw, ImageFont
S=2; M=105; W=1711; H=1044; divX=1125   # ventana 523 x 321 (arte 1671 x 1044 con el marco de 24 unidades)
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
d.rectangle(sc(4.5,24,W-24,H-24),outline=(14,14,15),width=4*S); d.rectangle(sc(8.5,28,W-28,H-28),outline=(78,78,82),width=1*S)
pt=148.5; db=pt+652; fb=708.75   # pt = linea 1 (octavo de la altura);  fb = fondo de la escala del fader (-inf)
A13=[-150+25*i for i in range(13)]   # Depth: recorrido de 300 grados, 13 marcas grandes cada 25
A11=[-150+30*i for i in range(11)]   # Fade: recorrido de 300 grados, el 20 % y el 80 % quedan justo a 180 grados entre si (a -90 y +90)
def aa_ticks(cx,cy,r0,r1,angles,w=3.0,K=16):   # ticks radiales con borde suave exacto (se dibujan 16x mas grandes y se reducen), asi todos pesan igual
    pad=int(r1+8); sz=2*pad; m=Image.new('L',(sz*K,sz*K),0); md=ImageDraw.Draw(m)
    for a in angles:
        t=math.radians(a); sx,sy=math.sin(t),-math.cos(t); nx,ny=-sy,sx   # direccion radial y normal
        P=[(pad+(r*sx+s*nx*w/2)*1.0,pad+(r*sy+s*ny*w/2)*1.0) for r,s in ((r0,-1),(r1,-1),(r1,1),(r0,1))]
        md.polygon([(x*K,y*K) for x,y in P],fill=255)
    m=m.resize((sz*S,sz*S),Image.BOX); ox,oy=int((cx-pad)*S),int((cy-pad)*S)
    img.paste(Image.new('RGB',m.size,G),(ox,oy),m)
aa_ticks(877,273,125+9,125+9+8.5,A11)   # Fade: 11 ticks (0..100 %, de a 10, cada 30 grados), 8,5 de largo y 3 de ancho
d.line(sc(divX,60,divX,H-60),fill=(12,12,13),width=4*S); d.line(sc(divX+3,60,divX+3,H-60),fill=(84,84,88),width=1*S)
fx=1312
box(1497.5,pt-1.5,1559.75,fb+1.5)   # el marco del vumetro centrado en la marca de +6 / -inf (3 de trazo, igual que las marcas)
for i in range(8):   # marcas del fader a ambos lados (+6, +3, 0, -3, -6, -12, -24, -inf)
    y=pt+(fb-pt)*i/7
    d.line(sc(fx-60.5,y,fx-15,y),fill=G,width=int(2*S)); d.line(sc(fx+15,y,fx+60.5,y),fill=G,width=int(2*S))   # largas, iguales a los dos lados (a la izquierda con el numero), desde el recorrido
for i in range(7):   # marcas intermedias (+4.5, +1.5, -1.5, -4.5, -9, -18, -42 dB): a mitad de cada tramo, que es lineal en dB; mitad de largo que las cortas
    y=pt+(fb-pt)*(i+0.5)/7
    d.line(sc(fx-23.5,y,fx-15,y),fill=G,width=int(2*S)); d.line(sc(fx+15,y,fx+23.5,y),fill=G,width=int(2*S))
def _ink(t,font,anchor):   # caja de tinta real (se dibuja aparte), relativa al punto de anclaje
    W_,H_=int(font.size*len(t)*1.2)+200,int(font.size*3); im_=Image.new('L',(W_,H_),0); ImageDraw.Draw(im_).text((W_//2,H_//2),t,font=font,fill=255,anchor=anchor)
    b_=im_.point(lambda v:255 if v>100 else 0).getbbox(); return (b_[0]-W_//2,b_[1]-H_//2,b_[2]-W_//2,b_[3]-H_//2)
FS=38   # tamano de los textos de titulos / Half / Whole / Bypass / SC Listen (= el texto 'Window' de la referencia)
L=FM(FS); L30=FM(30)
for i,t in enumerate(['+6','+3','0','\u22123','\u22126','\u221212','\u221224','\u2212\u221e']):   # valor de cada marca del fader (como la escala del Depth)
    _f=FL(FS); _y=pt+(fb-pt)*i/7; _bb=_ink(t,_f,'rm')   # caja de tinta: borde derecho y centro vertical exactos en la marca
    d.text(((fx-60.5-10)*S-_bb[2],(_y*S)-(_bb[1]+_bb[3])/2),t,font=_f,fill=G,anchor='rm')
def ctitle(x,t,ymid):   # titulo con la tinta centrada en ymid (vertical) y en x (horizontal)
    bb=d.textbbox((0,0),t,font=L,anchor='ls'); base=ymid-(bb[1]+bb[3])/(2*S); bx=_ink(t,L,'ms')
    d.text((x*S-(bx[0]+bx[2])/2,base*S),t,font=L,fill=G,anchor='ms'); print(t,'tinta',round(base+bb[1]/S,1),'->',round(base+bb[3]/S,1))
TY=86.25   # titulos Fade / Output / SC Input centrados sobre la linea a mitad entre el borde superior y la primera linea de la grilla
YM=76.5   # centro entre el borde interno de la ventana (y=24) y el tope de la perilla / fader
ctitle(378,'Depth',148.5)
ctitle(877,'Fade',TY)   # 41,5 sobre la perilla (igual que Mode)
ctitle(877,'Mode',584.25)
ctitle(fx,'Output',TY); ctitle(1528.5,'SC Input',TY)
bf=FM(FS); bf30=FM(30); GAP_T=10
bb=d.textbbox((0,0),'Input Listen',font=bf30,anchor='ls'); th=(-bb[1])/S; desc=bb[3]/S
unit=64+GAP_T+th+desc; gap=((H-24)-db-unit)/2
by0=db+gap; by1=by0+64
_bo=d.textbbox((0,0),'Output',font=L30,anchor='ls'); _top=YM-(_bo[1]+_bo[3])/(2*S)+_bo[1]/S-24   # distancia del techo de la tinta de Output al borde superior del plugin
_bb2=d.textbbox((0,0),'Bypass',font=bf,anchor='ls'); BY=957.75; base=BY-(_bb2[1]+_bb2[3])/(2*S)   # Bypass / SC Listen / Half / Whole con la tinta centrada sobre la linea a mitad entre la ultima linea de la grilla y el borde inferior (los 4 comparten linea de base)
_unused=(H-24-_top)-d.textbbox((0,0),'Bypass',font=bf30,anchor='ls')[3]/S   # el fondo de la tinta de Bypass queda a esa misma distancia del borde inferior
print('borde: arriba',round(_top,1),'abajo',round(H-24-(base+_bb2[3]/S),1))
for cx,name in ((fx,"Bypass"),(1528.5,"SC Listen")):
    _bx=_ink(name,bf,'ms'); d.text((cx*S-(_bx[0]+_bx[2])/2,base*S),name,font=bf,fill=G,anchor='ms')   # centrado por la tinta
BH=base   # linea de base de Half / Whole = la de los valores de abajo
HW=FL(FS)
_r0=125+5; _y0=771+_r0*math.cos(math.radians(30)); _ttop=BH+d.textbbox((0,0),'Half',font=HW,anchor='ls')[1]/S   # techo de Half / Whole
_tt=(_y0+34+_ttop)/2   # donde termina la linea: justo arriba del texto (igual que antes)
XH=877-(_r0*math.sin(math.radians(30))+(_tt-_y0)*math.tan(math.radians(30)))   # la linea sigue el angulo del tick hasta _tt; Half queda centrado ahi (Whole es simetrico)
_dx=125.0   # Half / Whole centrados en los bordes izquierdo y derecho de la perilla de Mode (radio 125)
_dx_old=(BY-771)*math.tan(math.radians(30))   # prolongacion del tick de Mode (+-30 grados desde abajo) hasta la altura del texto
for _t,_xc in (('Half',877-_dx),('Whole',877+_dx)):   # centrados por la tinta sobre esa prolongacion (simetricos respecto del centro de la perilla)
    _bx=_ink(_t,HW,'ms'); d.text((_xc*S-(_bx[0]+_bx[2])/2,BH*S),_t,font=HW,fill=G,anchor='ms')
# lineas de Mode: salen del borde de la perilla en diagonal (+-30 grados desde abajo, como el puntero), se doblan y bajan hasta el punto medio entre el quiebre y el texto; 3 de ancho, mismo color y borde suave que los ticks de Fade
def aa_path(P,w=3.0,K=16):
    xs=[p[0] for p in P]; ys=[p[1] for p in P]; x0,y0=int(min(xs))-6,int(min(ys))-6; x1,y1=int(max(xs))+6,int(max(ys))+6
    m=Image.new('L',((x1-x0)*K*S,(y1-y0)*K*S),0); ImageDraw.Draw(m).line([((px-x0)*K*S,(py-y0)*K*S) for px,py in P],fill=255,width=int(w*K*S),joint='curve')
    m=m.resize(((x1-x0)*S,(y1-y0)*S),Image.BOX); img.paste(Image.new('RGB',m.size,G),(x0*S,y0*S),m)
_cy=800; _r0=114+5; _ttop=BH+d.textbbox((0,0),'Half',font=HW,anchor='ls')[1]/S   # techo de Half / Whole
_ys=771+134*math.cos(math.radians(30)); _xs=134*math.sin(math.radians(30)); _yend=BH+d.textbbox((0,0),'Half',font=HW,anchor='ls')[1]/S-10   # sale donde salia el tick, va horizontal hasta el centro de la palabra y baja a 90 grados hasta 10 sobre el texto
for _sg in (-1,1): aa_path([(877+_sg*_xs,_ys),(877+_sg*_dx,_ys),(877+_sg*_dx,_yend)],w=2.0)   # Mode: los mismos ticks que Fade (8,5 de largo, 3 de ancho) en Half y Whole
# escala de Depth estilo referencia: puntitos + numeros (0 dB al centro, +6 a la derecha lineal, mitad izquierda logaritmica)
LAB=['\u2212\u221e','\u221248','\u221224','\u221212','\u22126','\u22123','0','+1','+2','+3','+4','+5','+6']
cx_,cy_,R_=378,522,249; fs=FL(FS)
for i,a in enumerate(A13):
    t=math.radians(a)
    rd=R_+14.5; x=cx_+rd*math.sin(t); y=cy_-rd*math.cos(t); rr_=3.4
    d.ellipse(sc(x-rr_,y-rr_,x+rr_,y+rr_),fill=G)
    if i<12:
        t2=math.radians(a+12.5); x=cx_+rd*math.sin(t2); y=cy_-rd*math.cos(t2); rr_=2.0
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
# nombre del plugin (Michroma, ancha)
_bot=_unused-1.0; _top=(914.75+_unused)/2+0.5   # el logo va de la linea inferior de texto (base) hasta la mitad entre esa linea y el borde de abajo de los botones
_b52=ImageFont.truetype('fonts2/Michroma-Regular.ttf',52*S).getbbox('rmd-1',anchor='ls'); _sz=52*(_bot-_top)/((_b52[3]-_b52[1])/S)
_lf2=ImageFont.truetype('fonts2/Michroma-Regular.ttf',int(round(_sz*S))); _bb=d.textbbox((0,0),'rmd-1',font=_lf2,anchor='ls'); _by=_bot-_bb[3]/S; print('logo tamano',round(_sz,1),'techo',round(_by+_bb[1]/S,1))
d.text((64*S,_by*S),'rmd-1',font=_lf2,fill=(240,240,240),anchor='ls',stroke_width=int(1.2*S),stroke_fill=(240,240,240))
_o=img.resize((W,H),Image.LANCZOS)
# separacion Depth -> columna Fade/Mode: se agrega SHIFT de ancho en la zona vacia entre ambas (x=SPLIT), asi todo lo de la derecha queda corrido SHIFT unidades
_n=img.resize((W,H),Image.LANCZOS)
_n.save(__import__('sys').argv[2])
