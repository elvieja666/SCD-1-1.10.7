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
