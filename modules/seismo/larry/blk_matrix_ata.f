	include 'common_para.h'
	parameter(m=200000)
	dimension nsqrs(nlatzomax),nsqtot(nlatzomax+1)
	dimension nsqrsh(nlatzhmax),nsqtoth(nlatzhmax+1)
	integer hitcount(20000)
	dimension iresolu(500000)
	dimension inew(500000),iold(500000),inewh(500000),ioldh(500000)
	dimension ire2(500000)
	character type*1,txtfile*500,filen*500,chn*3
	print*,"size of coarse pixels?"
	read*,eq_incr
	refgrid=eq_incr*1.
	nlatzones=180./eq_incr
	if(nlatzones.gt.nlatzomax)stop "coarse pixels too small"
	print*,"ratio to size of fine pixels?"
	read*,ifa
	if(ifa.ne.1)then
	   print *,'error: this version does not support local refinement'
	   print *,'recompile with proper boundary'
	   print *,'settings for high res region'
	   stop
	endif

	eq_hi=eq_incr/ifa
	nlatzohi=180./eq_hi
	if(nlatzohi.gt.nlatzhmax)stop "fine pixels too fine"

	print*,"input file? (use quotes if specifying whole path)"
	read*,txtfile
	print*,"root of name of output matrix files?"
	read*,filen

	print*,'number of latitudinal zones: ',nlatzones
	if(mod(nlatzones,2).ne.0)then
	   print*,'it should not be odd'
	   stop
	endif

	numto=0
	numhi=0
	colat=-eq_incr/2.
	do k=1,nlatzones
ctest	
	print*,k," lat zone"

c--increment colatitude (and therefore latitude) of the node
	   colat=colat+eq_incr
	   theta=(colat/180.)*pi
c--for this latitudinal zone, compute number of blocks (nsqrs)
	   deltalon=eq_incr/(sin(theta))
	   nsqrs(k)=(360./deltalon)+1
c--needs to be an even number 
	   if(mod(nsqrs(k),2).ne.0)nsqrs(k)=nsqrs(k)-1
c-------------------------------new
c--if requested, correct nsqrs(k) so the grid is compatible to reference grid
	   if(iswit.eq.1)then
	    if(360./nsqrs(k).ge.refgrid)then
100	     if(mod(360./nsqrs(k),refgrid).ne.0)then
	      nsqrs(k)=nsqrs(k)+1
c	      nsqrs(k)=nsqrs(k)-1
	      goto100
	     else
	     endif
	    elseif(360./nsqrs(k).lt.refgrid)then
101	     if(mod(refgrid,360./nsqrs(k)).ne.0)then
c	      nsqrs(k)=nsqrs(k)+1
	      nsqrs(k)=nsqrs(k)-1
	      goto101
	     else
	     endif
	    endif
	   endif
c----------------------------------

c--take care of finer grid:
	   do j=1,ifa
	      kfine=((k-1)*ifa)+j
	      nsqrsh(kfine)=nsqrs(k)*ifa
	      nsqtoth(kfine)=numhi
	      numhi=numhi+nsqrsh(kfine)
	   enddo
	   nsqtot(k)=numto
	   numto=numto+nsqrs(k)
	enddo
	print*,'numto=',numto

c--determine indexes of coarse and fine blocks 
c--within the high resolution area:
	kireso=0
	kire2=0
c	print*,westbo,eastbo,southbo,rthnobo,eq_incr
	do parall=rthnobo,southbo,-eq_incr
	   do rmerid=westbo,eastbo,eq_incr
	      kireso=kireso+1
c	      ilat=parall*100.+0.5
c	      ilon=rmerid*100.+0.5
	      iresolu(kireso)=
c     &superisqre(ilat,ilon,nsqrs,nsqtot,nlatzones,numto,eq_incr)
     &isqre(parall,rmerid,nsqrs,nsqtot,nlatzones,numto,eq_incr)
	      icoarse=iresolu(kireso)
c--finer grid
	      call rang(icoarse,xlamin,xlamax,
     &	xlomin,xlomax,nsqrs,nsqtot,nlatzones,n,eq_incr)
	      do ifila=1,ifa
	      do ifilo=1,ifa
	         kire2=kire2+1
	         xlafi=xlamin+((xlamax-xlamin)/ifa)*(ifila-0.5)
	         xlofi=xlomin+((xlomax-xlomin)/ifa)*(ifilo-0.5)
c	         ilat=xlafi*100.+0.5
c	         ilon=xlofi*100.+0.5
	         ire2(kire2)=
c     &	superisqre(ilat,ilon,nsqrsh,nsqtoth,nlatzohi,numhi,eq_hi)
     &	isqre(xlafi,xlofi,nsqrsh,nsqtoth,nlatzohi,numhi,eq_hi)
	      enddo
	      enddo
	   enddo
	enddo
	print*,kireso,' nonzero elements in array iresolu'
	print*,kire2,' nonzero elements in array ire2'

c--count
	icoar=0
	do icoblo=1,numto
	   do iche=1,kireso
	      if(icoblo.eq.iresolu(iche))then
	         icoar=icoar+1
	         goto37
	      endif
	   enddo
37	continue
	enddo
	ifine=0
	do icoblo=1,numhi
	   do iche=1,kire2
	      if(icoblo.eq.ire2(iche))then
	         ifine=ifine+1
	         goto38
	      endif
	   enddo
38	continue
	enddo

	print*,numto-icoar,' coarse blocks'
	print*,ifine,' fine blocks'
	nprime=numto-icoar+ifine
	print*,'parameterization: total',nprime,' blocks'
c	write(chn,"(i3.3)")int(eq_incr*10)
c	open(44,file="n_"//chn)
	open(44,file="nome")
	write(44,*)nprime
	close(44)

c--assign an index to each block used in the parameterization:
c--1)assign new indexes to the coarse blocks:
c	open(444,file='coarse.dat')
	call correspc(iresolu,kireso,numto,0,inew,iold,ico)
c	close(444)
c--2)assign new indexes to the fine blocks:
c	open(444,file='fine.dat')
c	iof=inew(numto)
	iof=numto-icoar
	call corresph(ire2,kire2,numhi,iof,inewh,ioldh,ico)
c	close(444)
c--now given a block number i from the original parameterization(s),
c--the corresponding block number in the new parameterization is 
c--inew(i) for the coarse grid and inewh(i) for the fine one.
c--conversely, if i is a block number in the new parameterization,
c--iold(i) and ioldh(i) map it onto the old indexes.
ctest
c	print*,"reindexing:"
c	do k=1,numto
c	   if(k.ne.inew(k))print*,k,inew(k)
c	enddo

	print*,'parameterization is defined'
c	open(144,file='para.dat')
c	do k=1,numhi
c	if(inewh(k).ne.0)write(144,*)k,inewh(k)
c	enddo
c	close(144)
c	stop

	call buildmatrix(hitcount,nprime,nsqrs,
     &	nsqtot,nlatzones,eq_incr,m,pi,
     &	iresolu,kireso,ifa,numbhi,nlatzohi,
     &	numhi,nsqrsh,nsqtoth,eq_hi,nprime,
     &	inew,inewh,type,input_per,txtfile,filen)
	end


c************************************************************
c************************************************************
	subroutine correspc(iresolu,kireso,n,ioffset,inew,iold,ico)
	dimension iresolu(10000),inew(50000),iold(50000)
	ico=0
	do i=1,n
	   do k=1,kireso
	   if(i.eq.iresolu(k))then
	      ico=ico+1
	      inew(i)=-1
	      goto42
	   endif
	   enddo
	   inew(i)=(i+ioffset)-ico
42	   iold(inew(i))=i
	enddo
	return
	end
c************************************************************
	subroutine corresph(iresolu,kireso,n,ioffset,inew,iold,ico)
	dimension iresolu(10000),inew(50000),iold(50000)
	ico=0
	do i=1,n
	   do k=1,kireso
	   if(i.eq.iresolu(k))then
	      inew(i)=(i+ioffset)-ico
	      goto42
	   endif
	   enddo
	   ico=ico+1
	   inew(i)=-1
42	   iold(inew(i))=i
	enddo
	return
	end
c************************************************************
c************************************************************
	subroutine buildmatrix(hitcount,n,nsqrs,
     &	nsqtot,nlatzones,eq_incr,m,pi,
     &  iresolu,kireso,ifa,numbhi,nlatzohi,
     &	numhi,nsqrsh,nsqtoth,eq_hi,nprime,
     &	inew,inewh,type,input_per,txtfile,filen)

	dimension inew(500000),inewh(500000)
	dimension jsqrg(500),ddelg(500)
	dimension jsqrm(500),ddelm(500)
	parameter(nlatzomax=180,nlatzhmax=720)
	dimension nsqrs(nlatzomax),nsqtot(nlatzomax+1)
	dimension nsqrsh(nlatzhmax),nsqtoth(nlatzhmax+1)
c	dimension nsqrs(nlatzones),nsqtot(nlatzones)
c	dimension nsqrsh(nlatzohi),nsqtoth(nlatzohi)
	integer hitcount(20000)
        parameter(nmax=20000,natamax=nmax*(nmax+1)/2)
        dimension ata(natamax),atd(nmax)
	dimension xxx(500)
c--regional parameterization info
	dimension iresolu(500000)
c--also: kireso is the total number of nonzero elements of iresolu;
c--ifa is the factor between lateral size of low res/hi res blocks, 
c--numbhi the actual number of low resolution blocks within the
c--high resolution region.
c--all these info have to be passed over to the ray_tracing subroutine.

	character wave_type*1,type*1,perio4*4,txtfile*500
	character chincr*2,chfact*2,perio*3,filen*500

	ndimata=n*(n+1)/2
c------------------initialize
	do k=1,n
	   atd(k)=0.
	enddo
	do k=1,ndimata
	   ata(k)=0.
	enddo
c--rad=pi/180.
	rad=0.0174533
	geoco=0.993277
	nnn=0

c	write(perio4,"(i4.4)")input_per
c	write(perio,"(i3.3)")input_per
c	txtfile=
c     &	'/home/simona/tomografia/etl_summary_phase_data/wei_sum.02.'//type//perio4//'_1.txt'
	print*,'open ',txtfile
	open(2,file=txtfile)

c	ieq_incr=eq_incr
c	write(chincr,'(i2.2)')ieq_incr
c	write(chfact,'(i2.2)')ifa
c	filen=type//'_'//perio//'_'//chincr//'_'//chfact

	do k=1,80
	   if(filen(k:k).eq." ")goto74
	enddo

 74	k=k-1
	open(77,file=filen(1:k)//'.xxx',access='direct',recl=4,
     &  form='unformatted')
	open(99,file=filen(1:k)//'.ind',access='direct',recl=4,
     &  form='unformatted')
	open(66,file=filen(1:k)//'.rhs')
	open(88,file=filen(1:k)//'.pnt')
	open(4,file=filen(1:k)//'.htc')

	do iblo=1,n
	   hitcount(iblo)=0.
	enddo

	print*,'m=',m
c--read header
	read(2,*)wave_type,period,c0
cTEST
	print*,wave_type,period,c0
c	pause

c	if(wave_type.ne.type)stop
	read(2,*)
c       old, formatted header format
1003	format(8x,a1,7x,f8.4,9x,f8.5)
	print*,'start reading'

	j=0
23	j=j+1
c--consider only the first m source/station couples?
c	if(j.gt.m)goto 888
	if(mod(j,1000).eq.0)print*,j," data read"
c
c
c
	read(2,*,end=9)eplat,eplon,stlat,stlon,dphi_k,error
c	print *,j,eplat,eplon,stlat,stlon,dphi_k,error
c       this was the old format, read unformatted now
1002	format(4(f11.4,1x),1x,f11.6,1x,f11.7)

	call ocav(nsqrs,nsqtot,nlatzones,eq_incr,n,
     &	eplat,eplon,stlat,stlon,ientm,jsqrm,ddelm,
     &	ient,jsqrg,ddelg,
     &  pi,iresolu,kireso,ifa,numbhi,
     &	numhi,nsqrsh,nsqtoth,nlatzohi,eq_hi,
     &	inew,inewh,iflga,delta)

        t0=(delta/rad)*111.2/c0

c--the following loop will take into account the minor arc
c--arrivals and define the the j,i element of the xxx matrix
c--as the projection of the j-th raypath on the i-th cell.
c--we are now considering the j-th path and this loop will go
c--over every sampled cell(labeled i).
      do 44 i=1,ientm
	kcell=jsqrm(i)
c	xxx(i)=ddelm(i)/rad
	xxx(i)=-ddelm(i)/(c0/6371.) !to invert for relative dv
	xxx(i)=xxx(i)/t0
c--store the non zero elements in a file
	if(xxx(i).ne.0.)then
	   write(77,rec=nnn+1)xxx(i)
	   write(99,rec=nnn+1)kcell
	   nnn=nnn+1
c--i want to know how many of the raypaths cross each cell	
	   hitcount(kcell)=hitcount(kcell)+1
	endif
   44 continue
	write(88,*)nnn
c	write(66,*)dphi_k
	rhsentry=dphi_k/t0
	write(66,*)rhsentry

c---------augment ata matrix accordingly
        call contribution_ata(xxx,jsqrm,ata,rhsentry,
     &	atd,n,ndimata,ientm)

      go to 23

9	print*,'number of paths used:',j-1
888	continue
	write(4,*)wave_type,period
	do iblo=1,nprime
	   write(4,*)iblo,hitcount(iblo)
	enddo
	write(4,*)eq_incr,eq_hi

	close(4)
	close(77)
	close(66)
	close(88)
	close(99)
c---------now store ata matrix and atd vector on files
        open(61,file=filen(1:k)//'.ata',access='direct',
     &  recl=4,form='unformatted')
        open(62,file=filen(1:k)//'.atd',access='direct',
     &  recl=4,form='unformatted')
        do kata=1,ndimata
           write(61,rec=kata)ata(kata)
        enddo
        do ib=1,n
           write(62,rec=ib)atd(ib)
        enddo
	close(61)
	close(62)
	return
	end

c*************
	function isqre(xlat,xlon,nsqrs,nsqtot,nlatzones,n,eq_incr)
c--finds the number of the square where (xlat,xlon) is

c	dimension nsqrs(nlatzones),nsqtot(nlatzones+1)
	parameter(nlatzomax=180,nlatzhmax=720)
	dimension nsqrs(nlatzomax),nsqtot(nlatzomax+1)
	dimension nsqrsh(nlatzhmax),nsqtoth(nlatzhmax+1)
       	lazone=(90.-xlat)/eq_incr+1
	if(lazone.gt.nlatzones)lazone=nlatzones
c	llon=lon
c	if(llon.lt.0)llon=36000+llon
	isqre=(xlon/360.)*nsqrs(lazone)+1
	isqre=isqre+nsqtot(lazone)
c	if(isqre.gt.n)isqre=n
	return
	end

	subroutine rang(nsq,xlamin,xlamax,xlomin,xlomax,
     &	nsqrs,nsqtot,nlatzones,n,eq_incr)
c
c finds the coordinate range of square number 'nsq'
c
c	dimension nsqrs(nlatzones),nsqtot(nlatzones)
	parameter(nlatzomax=180,nlatzhmax=720)
	dimension nsqrs(nlatzomax),nsqtot(nlatzomax+1)
	dimension nsqrsh(nlatzhmax),nsqtoth(nlatzhmax+1)
c
	lazone=2
	do while (nsq.gt.nsqtot(lazone))
	   lazone=lazone+1
	enddo
	lazone=lazone-1
	nnsq=nsq-nsqtot(lazone)
	xlamin=90.-lazone*eq_incr
	xlamax=xlamin+eq_incr
	grsize=360./nsqrs(lazone)
	xlomax=nnsq*grsize
	xlomin=xlomax-grsize

	return
	end

cprog rsoinc
cxref 
      subroutine rsoinc(a,n,idx)
      dimension a(1),idx(1) 
      if (n.eq.1) go to 65
      if (n.le.0) go to 60
      do 1 i = 1,n
      idx(i) = i
    1 continue
      n2 = n/2
      n21 = n2 + 2
      ict=1 
      i=2 
   11 n1=n21-i
      nn=n
      ik=n1 
   15 c=a(ik) 
      ic=idx(ik)
  100 jk=2*ik 
      if (jk.gt.nn) go to 140 
      if (jk.eq.nn) go to 120 
       if (a(jk+1).le.a(jk)) go to 120
      jk=jk+1 
  120 if (a(jk).le. c) go to 140
      a(ik)=a(jk) 
      idx(ik)=idx(jk) 
      ik=jk 
      go to 100 
  140 a(ik)=c 
      idx(ik)=ic
      go to (3,45) ,ict 
    3 if (i.ge.n2) go to 35 
      i=i+1 
      go to 11
   35 ict=2 
      np2=n+2 
      i=2 
   37 n1=np2-i
      nn=n1 
      ik=1
      go to 15
  45  continue
      t = a(1)
      a(1) = a(n1)
      a(n1) = t 
      it = idx(1) 
      idx(1) = idx(n1)
      idx(n1) = it
      if (i.ge.n) go to 55
      i=i+1 
      go to 37
   55 return
   60 write(16,500)
  500 format('error return from sortd1 - n less than or equal to 1')
      stop
   65 idx(1)=1
      return
      end

c===========================================================
c===========================================================
cprog ocav
cxref
	subroutine ocav(nsqrs,nsqtot,nlatzones,eq_incr,n,
     &	eplat,eplon,stlat,stlon,ientm,jsqrm,ddelm,
     &	ient,jsqrg,ddelg,
     &  pi,iresolu,kireso,ifa,numbhi,
     &	numhi,nsqrsh,nsqtoth,nlatzohi,eq_hi,inew,inewh,iflga,
     &	del)

	dimension inew(50000),inewh(50000)
	dimension pht(500),idx(500)
	dimension jsqrg(500),ddelg(500)
	dimension jsqrm(500),ddelm(500)
	parameter(nlatzomax=180,nlatzhmax=720)
	dimension nsqrs(nlatzomax),nsqtot(nlatzomax+1)
	dimension nsqrsh(nlatzhmax),nsqtoth(nlatzhmax+1)
c	dimension nsqrs(nlatzones),nsqtot(nlatzones)
c	dimension nsqrsh(nlatzohi),nsqtoth(nlatzohi)

c--regional parameterization info
	dimension iresolu(10000)
c--also: kireso is the total number of nonzero elements of iresolu;
c--ifa is the factor between lateral size of low res/hi res blocks, 
c--numbhi the actual number of low resolution blocks within the
c--high resolution region.

c===========================================================
c--determine constants used in following calculations
	dth=pi/(nlatzones*1.)
c	dth=(eq_incr/180.)*pi
	pi2=pi*2.
	forpi=4.*pi
	radian=180./pi
	nth1=nlatzones+1

c--convert to radians 
      th1=(90.-eplat)/radian
      ph1=eplon/radian
      th2=(90.-stlat)/radian
      ph2=stlon/radian
      sth1=sin(th1)
      sth2=sin(th2)
      cth1=cos(th1)
      cth2=cos(th2)
      sph1=sin(ph1)
      sph2=sin(ph2)
      cph1=cos(ph1)
      cph2=cos(ph2)
c--find out the coordinates of the "north pole of the ray" knowing
c--the coordinates of the source and of the receiver:
      cph21= cph1*cph2+sph1*sph2
      sph21=sph2*cph1-sph1*cph2
      cdel=sth1*sth2*cph21+cth1*cth2
      ccapth=sth1*sth2*sph21/sqrt(1.-cdel*cdel)
      scapth=sqrt(1.-ccapth*ccapth)
      capth=atan2(scapth,ccapth)
      scapph=cth1*sth2*cph2-cth2*sth1*cph1
      ccapph=sth1*cth2*sph1-sth2*cth1*sph2
      capph=atan2(scapph,ccapph)
      scapph=sin(capph)
      ccapph=cos(capph)
c--capth (capital theta) and capph (capital phi) are now colatitude and
c--longitude of the "north pole" of the ray's great circle w/ respect to
c--the true north pole. cdel is cosine of epi. distance.

c--determine del (epicentral distance) from cosine(del)
      del=atan2(sqrt(1.-cdel*cdel),cdel)
c--this delta is in general slightly different from the one determined by
c--ttmain; in fact here an exact formula is used while ttmain works by
c--successive approximations.
      cphsp=ccapth*sth1*(cph1*ccapph+sph1*scapph)-scapth*cth1
      sphsp=sth1*(sph1*ccapph-cph1*scapph)
      phsp=atan2(sphsp,cphsp)
c--phsp is (?) probably the longitude of the source on the great circle;
c--how is it defined = what is its zero?
c--phsp is not used to compute the intersections with the grid-
c--those are determined with respect to an arbitrary (?) point;
c--it's used later when jsqre is figured out

c--the following determines the max & min latitudes reached 
c--by the great circle
      thet=capth
      if(capth.gt.0.5*pi) thet=pi-capth
      thmin=0.5*pi-thet
      thmax=0.5*pi+thet
c===========================================================
      lat_zone=0
      ient=0
      if(scapth.eq.0) goto 10

c***
c--first loop!
c--find deltas of intersections with the coarser grid:
c--(they depend only on source and station locations)
      do 20 i=2,nth1
      lat_zone=lat_zone+1
c--th is cumulative latitudinal increment:
      th=float(i-1)*dth
      cth=cos(th)
c--cumulative delta(pht): cos (only spherical trigonometry eq.):
      cpht=-cth/scapth
      cpht2=cpht*cpht
c--skip latitude zone if it's out of the lat. range of the ray:
      if(th.lt.thmin.or.th-dth.gt.thmax)goto20
c--the following takes care of the last latitude zone, for which
c--there is no intersection with the "th" parallel, only with "th-dth":
      if(cpht2.gt.1.)goto21
c--if everything's ok, store in pht the cumulative delta
c--these are "latitudinal intersections"
c--in general a ray crosses a parallel in 2 points (cumulative
c--4 "latitudinal intersections" for each lat.zone)
      ient=1+ient
      pht(ient)=atan2(sqrt(1.-cpht2),cpht)
      spht=sin(pht(ient))
      ient=1+ient
      pht(ient)=-pht(ient-1)

  21  numlong=nsqrs(lat_zone)
c      dphi=pi2/float(numlong)
      dphi=pi2/numlong
c--dphi will be longitude increment
c--since the longitude increment changes depending on the lat.zone,
c--we have to nest the following loop, that computes the intersections
c--with meridians, inside the loop over latitude zones...
c===========================================================
c--now in each latitudinal zone find the epicentral distances
c--of intersections of the great circle with the longitudinal
c--boundaries of the grid.
      do 40 j=1,numlong
c--ph is cumulative longitudinal increment
         ph=float(j-1)*dphi
         angr=ph-capph
c--thlo is the colatitude at which the meridian ph intersects
c--the great circle:
         thlo=atan(-ccapth/(scapth*cos(angr)))
         if(thlo.lt.0.) thlo=pi+thlo
         if(thlo.gt.th-dth.and.thlo.lt.thmin) go to 40
         if(thlo.lt.th+dth.and.thlo.gt.thmax) go to 40
         if(thlo.gt.th.or.thlo.lt.th-dth) go to 40
         sph=sin(ph)
         cph=cos(ph)
         ient=ient+1
         pht(ient)=atan2(ccapth*(sph*ccapph-cph*scapph),
     &        cph*ccapph+sph*scapph)
c--what?:
         if(pht(ient).gt.pi) pht(ient)=pht(ient)-pi2
   40 continue
   20 continue
   10 continue

c--now repeat the first loop for the finer grid, but store the deltas
c--of the intersections only if they lay within the region of interest.
	l_z_fine=0
	dth_fine=pi/(nlatzohi*1.)
	nth1_fine=nlatzohi+1
	do 25 i=2,nth1_fine
	   l_z_fine=l_z_fine+1
           th=float(i-1)*dth_fine
	   cth=cos(th)
	   cpht=-cth/scapth
	   cpht2=cpht*cpht
	   if(th.lt.thmin.or.th-dth_fine.gt.thmax)goto25
	   if(cpht2.gt.1.)goto26
c--ok, now find coordinates of intersections (remember, 
c--there are two intersections for each parallel, hence the loop):
	   do 37 isegno=-1,1,2
	   dis=isegno*atan2(sqrt(1.-cpht2),cpht)
	   cdis=cos(dis)
	   sdis=sin(dis)
	   cthdis=-cdis*scapth
	   thdis=atan2(sqrt(1.-cthdis*cthdis),cthdis)
	   cphdis=cdis*ccapth*ccapph-sdis*scapph
	   sphdis=cdis*ccapth*scapph+sdis*ccapph
	   if(sphdis.eq.0..and.cphdis.eq.0.) then
	      phdis=0.
	   else
	      phdis=atan2(sphdis,cphdis)
	      if(phdis.lt.0.)phdis=phdis+pi2
	      if(phdis.gt.pi2)phdis=phdis-pi2
	   endif
c--find coarse block to which thdis,phdis belongs:
	   xlat=90.-thdis*radian
c	   ilat=xlat*100.
	   xlon=phdis*radian
c	   ilon=xlon*100.
c	   icoblo=superisqre(ilat,ilon,nsqrs,nsqtot,nlatzones,n,eq_incr)
	   icoblo=isqre(xlat,xlon,nsqrs,nsqtot,nlatzones,n,eq_incr)
c--check whether such block belongs to the high resolution region:
	   do iche=1,kireso
	      if(icoblo.eq.iresolu(iche))then
	         ient=1+ient
	         pht(ient)=dis
	         goto37
	      endif
	   enddo
37	   continue
c--now, longitudinal intersections:
26	   numlong=nsqrsh(l_z_fine)
	   dphi=pi2/float(numlong)

	   do 42 j=1,numlong
	      ph=float(j-1)*dphi
	      angr=ph-capph
	      thlo=atan(-ccapth/(scapth*cos(angr)))
	      if(thlo.lt.0.) thlo=pi+thlo
	      if(thlo.gt.th-dth_fine.and.thlo.lt.thmin)goto42
	      if(thlo.lt.th+dth_fine.and.thlo.gt.thmax)goto42
	      if(thlo.gt.th.or.thlo.lt.th-dth_fine)goto42
	      sph=sin(ph)
	      cph=cos(ph)
c--epicentral distance of the intersection of the g.c. 
c--with the meridian ph:
	      dis=atan2(ccapth*(sph*ccapph-cph*scapph),
     &	cph*ccapph+sph*scapph)
	      cdis=cos(dis)
	      sdis=sin(dis)
c--again, find coordinates of intersection
	      phdis=ph
	      if(phdis.lt.0.)phdis=phdis+pi2
	      if(phdis.gt.pi2)phdis=phdis-pi2
	      cthdis=-cdis*scapth
	      thdis=atan2(sqrt(1.-cthdis*cthdis),cthdis)
c--find coarse block to which thdis,phdis belongs:
	      xlat=90.-thdis*radian
c	      ilat=xlat*100.
	      xlon=phdis*radian
c	      ilon=xlon*100.
c	      icoblo=superisqre(ilat,ilon,nsqrs,nsqtot,nlatzones,n,eq_incr)
	      icoblo=isqre(xlat,xlon,nsqrs,nsqtot,nlatzones,n,eq_incr)
c--check whether such block belongs to the high resolution region:
	      do iche=1,kireso
	         if(icoblo.eq.iresolu(iche))then
	            ient=1+ient
	            pht(ient)=dis
	            goto42
	         endif
	      enddo
42	   continue
25	continue


c--now the cumulative deltas corresponding to each square crossed by
c--the ray have been computed, and stored in pht.

      do 60 i=1,ient
   60 pht(i)=amod(pht(i)-phsp+forpi,pi2)
      ient=ient+1
      pht(ient)=0.
      ient=ient+1
      pht(ient)=del

c--sort!!
      call rsoinc(pht,ient,idx)

      pht(ient+1)=pi2
      ientm=0
      ientg=0
      cdelta=0.
	iflga=0

c--second loop!
c===========================================================
c--now determine the indexes of the blocks crossed by the raypath
c===========================================================
c--loop over the whole great circle:
      do 50 i=1,ient
      i1=1+i

c--the following calculates the coordinates of the mid-point of
c--each segment of the ray, that is phtt
      phtt=.5*(pht(i)+pht(i1))+phsp
      cpht=cos(phtt)
      spht=sin(phtt)
      cth=-cpht*scapth
      th=atan2(sqrt(1.-cth*cth),cth)
      cph=cpht*ccapth*ccapph-spht*scapph
      sph=cpht*ccapth*scapph+spht*ccapph
      if(sph.eq.0..and.cph.eq.0.) goto 9873
      ph=atan2(sph,cph)
      if(ph.lt.0.) ph=ph+pi2
      if(ph.gt.pi2) ph=ph-pi2
      goto 9874
 9873 ph=0.
 9874 continue
      xlat=90.-th*radian
c      ilat=xlat*100.+0.5
      xlon=ph*radian
c      ilon=xlon*100.+0.5

c--check: actually look at coordinates of intersections
c--(at this stage, source and station locations are also included):
      phtt=pht(i)+phsp
      cpht=cos(phtt)
      spht=sin(phtt)
      cth=-cpht*scapth
      th=atan2(sqrt(1.-cth*cth),cth)
      cph=cpht*ccapth*ccapph-spht*scapph
      sph=cpht*ccapth*scapph+spht*ccapph
      if(sph.eq.0..and.cph.eq.0.)then
         ph=0.
      else 	
         ph=atan2(sph,cph)
      endif
      if(ph.lt.0.) ph=ph+pi2
      if(ph.gt.pi2) ph=ph-pi2
      xlatint=90.-th*radian
      xlonint=ph*radian
c	write(14,*)xlonint,xlatint

c--...then the function isqre finds the index
c      jsqre=superisqre(ilat,ilon,nsqrs,nsqtot,nlatzones,n,eq_incr)
      jsqre=isqre(xlat,xlon,nsqrs,nsqtot,nlatzones,n,eq_incr)

c--incremental delta:
      rd=(pht(i1)-pht(i))

c--if this block is sampled by the minor arc...
	if(pht(i1).le.del)then
	ientm=ientm+1
c--if jsqre is within the high resolution region...
	do iche=1,kireso
	   if(jsqre.eq.iresolu(iche))then
c	      isqrh=superisqre(ilat,ilon,nsqrsh,nsqtoth,nlatzohi,
c     &	numhi,eq_hi)
	      isqrh=isqre(xlat,xlon,nsqrsh,nsqtoth,nlatzohi,numhi,eq_hi)
	      jsqre=inewh(isqrh)
	      iflga=1
	      goto56
	   endif
	enddo
	jsqre=inew(jsqre)
56	continue
	jsqrm(ientm)=jsqre
	ddelm(ientm)=rd
c--fix repeated index problems:
	if(ientm.gt.1.and.jsqrm(ientm).
     &	eq.jsqrm(ientm-1)) then
	ientm=ientm-1
	ddelm(ientm)=ddelm(ientm)+rd
	endif
	cdelta=cdelta+rd
	endif

      ientg=ientg+1
      jsqrg(ientg)=jsqre
      ddelg(ientg)=rd
      if(ientg.gt.1.and.jsqrg(ientg).eq.jsqrg(ientg-1)) then
      ientg=ientg-1
      ddelg(ientg)=rd+ddelg(ientg)
      endif
   50 continue
      return
      end
c************************************************************
       subroutine contribution_ata(row,index,ata,d,b,n,nata,nz)
c----given a row of a and corresponding datum augments ata accordingly
        real*4 ata(nata),b(n),row(n),d
        integer index(500)
        do i=1,nz
          do j=i,nz
ctest
c	write(95,*)index(i),index(j),row(i),row(j)

	    if(index(j).ge.index(i))then
               ind=(((index(j)-1)*index(j))/2)+index(i)
	    else
               ind=(((index(i)-1)*index(i))/2)+index(j)
	    endif
            ata(ind)=ata(ind)+row(i)*row(j)
          enddo
c--add to rhs vector the contribution of j-th row:
          b(index(i))=b(index(i))+(row(i)*d)
        enddo
ctest
c	pause

        return
        end
c==========================================================================
