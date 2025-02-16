c modifications Lapo 4/2010: outliers are downweighted
c
c modifications Lapo 7/2009: reads in and combines independent matrices/data vectors (joint_lsqr)
c
c modifications Lapo 3/2009: dynamic allocation of memory done in a f90 program that 
c invokes this program. generalized for radial anisotropy in the mantle.
c compile with ifort -132 -o joint_lsqr_vx main.f90 joint_lsqr_vx.f gradamp.f readmatrix.f
c ifort -132 -o joint_lsqr_vx_isodamp main.f90 joint_lsqr_vx_isodamp.f gradamp.f readmatrix.f

c	compute LSQR inversion of tomography data
c       from Lapo Boschi, 2/2009
c
c
c       modifications by TWB
c
c       - added layer number as input parameter, not hard coded anymore 
c	- added possibility of depth-variable roughness damping
c       - added possibility of stronger mid-mantle norm damping (as per Lapos new code)
c
c	$Id: lsqr_vx.f,v 1.5 2009/02/28 04:12:25 becker Exp $
c
c
	subroutine lsqrinv(values,indx,nonz,t,ttt,mpoin,m)
c	program lsqrinv
c
c
c	implicit real*8 (a-h,o-z)
c
c       maximum layer number
c
c	parameter(nlaym=50,nlayouc0=10)
	parameter(nlaym=60,nlayouc0=10)
	parameter(cmbradius=3480.)
c
c       fraction of upper and lower boundary layer for additional
c       norm damping
c
	parameter(xumd_frac=0.13333333, xlmd_frac = 0.13333333)


c	parameter(n_0=4592)
c	parameter(n_1=n_0*nlaym) 

	parameter(n0max=600000)
	parameter(n1max=n0max*nlaym)

c--iflaouc=1/0 include/not include outer core structure in the inversion
	parameter(iflaouc=0)
c--parameterization: number of layers total (nlay) and mantle (nlaym)
	parameter(nlayouc=nlayouc0*iflaouc)
c	parameter(nlay=nlaym+nlayouc)

c	parameter(nparouc=iflaouc*n_0*nlayouc)
	parameter(nparoucmax=iflaouc*n0max*nlayouc)

c--iflacmb=1/0 include/not include CMB structure in the inversion
	parameter(iflacmb=0)

c	parameter(ngridcmb=n_0*iflacmb) 
	parameter(ngridcmbmax=n0max*iflacmb)

c--outer core damping
	parameter(idampoucn=0,dampoucn=100.)
	parameter(idampoucr=0,dampoucr=100.)
c--cmb damping:
	parameter(idampcmb=0,dampcmb=1500.)
	parameter(idampcmb2=0,dampcmb2=300.)

c--npar= number of anisotropy parameters 
c--npar=1: invert for isotropic Vp
	parameter(npar=1)

c	parameter(n=npar*n_1+ngridcmb+nparouc)
	parameter(nmax=npar*n1max+ngridcmbmax+nparoucmax)


c--M>=number of rows; NONZ>=number of nonzero elements
c	parameter(m=700000)
c	parameter(nonz=250000000)

c--roughness damping, additional norm damping and anisotropy  
c--damping (only for anisotropic inversions)
	parameter(idamp=1,iaddnormdmp=0)
c	parameter(isodamp=1,wisot=100000.)
	parameter(isodamp=0)
c--LSQR norm damping
c	parameter(damp=500.)

	integer ipo(100)
	integer itcount(nmax)
	real*4 ritcount(nmax)
	dimension v(nmax),w(nmax),x(nmax),se(nmax),aty(nmax)
	dimension t(m),ttt(m)
	dimension indx(nonz),values(nonz),mpoin(0:m)
	character*80 namexxx,namepoi,nameind,namerhs,wdir,nomfil
	dimension wgrad(npar),znmnmz(npar)
	dimension wgradv(nlaym)
c	parameter(iswit=1)
c	parameter(eq_incr=3.)
c	parameter(refgrid=eq_incr,iswit=0,nlatzones=180/eq_incr)
c        integer nsqrs(nlatzones),nsqtot(nlatzones+1)

	parameter(nlatzomax=180)
        integer nsqrs(nlatzomax),nsqtot(nlatzomax+1)

c	character chnpar*8

	print*,"what pixel size?"
	read*,eq_incr
	print*,"compatible with crustal model of same gridsize (0=no,1=yes)?"
	read*,iswit
	refgrid=eq_incr
	nlatzones=180/eq_incr
	print*,"what horizontal roughness damping?"
	read*,wgrad(1)
	print*,"what vertical roughness damping (in fractions of horizontal damping)?"
	read*,wgradv(1)
	print*,"what norm damping?"
	read*,damp
	print*,"what additional norm damping for mid-mantle layers?"
	read*,dampmm
	print*,"what anisotropy damping?"
	read*,wisot
	print*,"number of layers?"
	read*,nlayi
	nlay=nlayi+nlayouc
	if(nlay.gt.nlaym)then
	   print*,'number of layers out of bounds ',nlaym
	   stop
	endif
        print*,"working directory?"
        read*,wdir
        do k=1,80
           if(wdir(k:k).eq." ")goto708
        enddo
708     kchwdir=k-1

c
c       compute the mid mantle layers in case of damping
c       
c       fraction of upper boundary layer
	nlayum=int(xumd_frac * nlay+.5)
c       lower mantle boundary layer
	nlaylmm=int(xlmd_frac*nlay+.5)
	nlaymm=nlay-nlaylmm-nlayum
	if(dampmm.gt.0)then
	   print *,'extra damping of ',dampmm
	   print *,'for top ',nlayum,' and bottom ',nlaylmm,' layers'
	endif

c-----define parameterization
        call param(eq_incr,nsqrs,nsqtot,nlatzones,n1layer,iswit,refgrid,
     &nlatzomax)
c	if(n1layer.ne.n_0)stop "parameter n_0 has wrong value"

	n_0=n1layer
	n=n_0*nlay*npar
c	write(chnpar,"(i8.8)")n
	n_1=n_0*nlay
	if(n_0.gt.n0max)stop "too many pixels"
	if(n.gt.nmax)stop "too many voxels"
	ngridcmb=n_0*iflacmb

c=================================================================
c--set weights of roughness damping
c	wgrad(1)=100.
	wgrad(2)=wgrad(1)
c--set weights of additional norm damping
	znmnmz(1)=0.
	znmnmz(2)=0.
c=================================================================

	print*,'total number of parameters',n

	do ihit=1,n
	   itcount(ihit)=0
	   Ritcount(ihit)=0
	enddo

	icol=1
	jj=1
	mpoin(0)=0

	konta=1
	tinitial=secnds(0.0)
111	continue
	ipo(konta)=icol
	print*,namexxx
	print*,"a matrix?"
	read(*,*)namexxx
	print*,"matrix:",namexxx
	if(namexxx.eq."finished")then
           print*,namexxx," no more matrices to read"
           goto112
        endif
	print*,"ok continue with input"
	print*,"index array?"
	read*,nameind
	print*,"pointer array?"
	read*,namepoi
	print*,"data vector?"
	read*,namerhs
	print*,"weight?"
	read*,relwei
        print*,"threshold delay times for downweighting? (pos,neg)"
        read*,cutoff,cutoffn
	print*,"number of observations in this subset?"
	read*,ndata
	call readmatrix(ndata,namexxx,nameind,namepoi,namerhs,
     &mpoin,t,indx,values,icol,jj,nonz,n,m,relwei,cutoff,cutoffn,itcount,nmax,ritcount)
        print*,"matrix read by routine readmatrix"!TEST
c	call readmatrix2(ndata,namexxx,nameind,namepoi,namerhs,
c     &mpoin,t,indx,itcount,values,icol,jj,nonz,n,m,relwei,ritcount,nmax)
c	ipo(konta+1)=icol
	print*,"number of values read:",icol,ndata!TEST
	konta=konta+1
	goto111
c=================================================================
112	continue
        print*,"measuring time..."
	print*,"matrices read in",secnds(tinitial)," seconds"
	nelrhs=icol-1
	nelm=jj-1

	open(27,file='hits.dat')
	open(37,file='sensitivity.dat')
	ik=1
	do ihit=1,n-ngridcmb
	   write(27,*)ik,itcount(ihit)
	   write(37,*)ik,ritcount(ihit)
	   ik=ik+1
	enddo
	close(27)
	close(37)

cTEST
	print*,'check...',icol
	print*,values(nelm),values(jj)
	print*,indx(nelm),indx(jj)
	print*,mpoin(nelrhs),mpoin(icol)
	print*,t(nelrhs),t(icol)
	print*,nelrhs,nelm,mpoin(nelrhs)
	print*,'check finished'
	if(mpoin(nelrhs).eq.0)stop
	ave=1.
	nelrhsold=nelrhs

c--damp the roughness of each parameter 
	if(idamp.eq.1)then
	   print*,'damp the roughness'
	   do ird=1,npar
	      wgradh=wgrad(ird)
c	      wgradh=wgrad(ird)/ave
c	      wgradv=wgradh
	      print*,'damp roughness of parameter',ird
	      inp=n_1*(ird-1)
c------------------------------------------------vertical damping
c-------------------constants
	      if(wgradv(1).ge.0)then
	         do ll=2,nlay
		    wgradv(ll) = wgradv(1)
	         enddo
	      else ! if wgradv<0 is given, read vert varying values from file
c-------------------read in weights
	         print *,'reading relative vertical weights'
	         open(7878,file='rdamp.dat',status="old")
	         do ll=1,nlay
		    read(7878,*)wgradv(ll)
	         enddo
	         close(7878)
	      endif
	      call gradamp4(wgradh,wgradv,nelm,nelrhs,values,indx,mpoin,
     &t,m,n,NONZ,nlay,inp,nsqrs,nsqtot,nlatzones,n_0,eq_incr,nlatzomax)
	   enddo
	   print*,'nelrhs after roughness damping:',nelrhs
	   print*,'nelm after roughness damping:',nelm
	endif

c--damp the norm of each parameter 
	if(iaddnormdmp.eq.1)then
	   print*,'extra norm damping'
 	   do isd=1,npar
	      damsize=znmnmz(isd)
	      print*,'damp parameter',isd,';',damsize
	      inp=n_1*(isd-1)
	      call damp_size(damsize,nelm,nelrhs,values,indx,mpoin,t,m,
     &inp,NONZ,n_1)
	   enddo
	   print*,'nelrhs after extra norm damping:',nelrhs
	endif

c--additional norm damping limited to mid-mantle (after work of Grand and Simmons)
	if(dampmm.ne.0.)then ! MODIFIED LAPO 30.3.2009 
	   print*,'additional mid-mantle norm damping for ',nlayum,nlaylmm
	   do ird=1,npar
	   do isd=1,nlay
	      if(isd.le.nlayum.or.isd.gt.nlay-nlaylmm)then
		 damsize=0.
	      else
		 damsize=dampmm
	      endif
	      inp=n_1*(ird-1)+n_0*(isd-1)
	      print*,'damp layer',isd,damsize,"starting from element",inp+1
	      call damp_size(damsize,nelm,nelrhs,values,indx,mpoin,t,m,inp,NONZ,n_0)
	   enddo
	   enddo
	   print*,'nelrhs after norm damping:',nelrhs
	else
	   print*,'no additional mid-mantle damping'
	endif

c--damp cmb structure:
	if(iflacmb.eq.1)then
	   IF(idampcmb.eq.1)then !damp cmb roughness
	      print*,'DAMP CMB: ROUGHNESS:',dampcmb
	      inp=n_1*npar+NPAROUC
	      call damp_rough_cmb(dampcmb,nelm,nelrhs,values,indx,mpoin,
     &t,m,n,NONZ,inp,NGRIDCMB,nsqrs,n1layer,nlatzones,nlatzomax)
	      print*,'nelrhs after damping cmb roughness:',nelrhs
	   ENDIF
	   IF(idampcmb2.eq.1)then !damp cmb norm
	      print*,'DAMP CMB: SIZE:',dampcmb2
	      inp=npar*n_1+NPAROUC
	      call damp_size_cmb(dampcmb2,nelm,nelrhs,values,indx,mpoin,t,m,
     &inp,ngridcmb,NONZ)
	      print*,'nelrhs after damping cmb size:',nelrhs
	   ENDIF
	endif

c--damp anisotropy 
	if(isodamp.eq.1)then
	   print*,'DAMP ANISOTROPY',wisot
c	   wisot0=wisot/ave
	   wisot0=wisot
cTEST
        print*,"calling dampaniso",n_1,nelrhs,nelm
	   call dampaniso(wisot0,nelm,nelrhs,values,indx,mpoin,t,m,n_1,nonz)
	   print*,'nelrhs after aniso damping:',nelrhs
	   print*,'nelm after aniso damping:',nelm
	   else
	   print*,'NO anisotropy damping'
	endif

c--damp outer core
	IF(iflaouc.EQ.1)THEN
	   IF(idampoucr.eq.1)then !damp outer core roughness
	      print*,'DAMP OUTER CORE: ROUGHNESS',dampoucr
	      wgradh=dampoucr
	      wgradv=dampoucr
	      inp=npar*n_1
	      call gradamp4(wgradh,wgradv,nelm,nelrhs,values,indx,mpoin,
     &t,m,n,NONZ,NLAYOUC,inp,nsqrs,nsqtot,nlatzones,n_0,eq_incr)
	      print*,'nelrhs after outer core rough. damping:',nelrhs
	   ENDIF
	   IF(idampoucn.eq.1)then ! damp outer core norm
	      print*,'DAMP OUTER CORE: SIZE',dampoucn
	      inp=npar*n_1
	      call damp_size(dampoucn,nelm,nelrhs,values,indx,mpoin,t,m,
     &inp,NONZ,NPAROUC)
	      print*,'nelrhs after outer core norm damping:',nelrhs
	   ENDIF
	ENDIF

c--check: print a column of A.
c	icoluche=50000
c	print*,'n of rows=',nelrhs
c	print*,'nonzero elements of column',icoluche
c	do k11=1,nelrhs
c	   do k22=mpoin(k11-1)+1,mpoin(k11)
c	      if(indx(k22).eq.icoluche)then
c	      print*,k11,indx(k22),values(k22)
c	      endif
c	   enddo
c	enddo
cc	pause

c--scale norm damping parameter (if c.weighting was done)
	DAMP0=DAMP/AVE
	print*,'norm damping=',damp0

c=========================================================
c--going back to 13 I call LSQR w/ a different damping parameter.
13	atol=0.0001
	btol=0.0001
	conlim=0.
	itnlim=n
	nout=1

c--save t; we are going to need it to compute variance reduction.
	print*,'save r.h.s.'
	do it=1,nelrhsold
	   ttt(it)=t(it)
	enddo

	print*,'call LSQR'
	tinitial=secnds(0.0)
	call lsqr( M, N, DAMP0,
     $             1, n, iw, aty,
     $             t, V, W, X, SE,
     $             ATOL, BTOL, CONLIM, ITNLIM, NOUT,
     $       ISTOP, ITN, ANORM, ACOND, RNORM, ARNORM, XNORM,
     &       indx,values,mpoin,nonz)
	print*,"lsqr inversion ran in",secnds(tinitial)," seconds"
cTEST
c        print*,npar,"chnpar:",chnpar
c	open(27,file="lsqr_cost."//chnpar,access="append")
c	write(27,"(i7,1x,i3,1x,3(f7.1,1x),i7,1x,f7.1)")
c     &  n_0,nlay,eq_incr,damp,wgrad(1),itn,secnds(tinitial)

c--compute variance reduction
	rnumer=0.
	denom=0.
	do l=1,nelrhsold
	   denom=denom+(ttt(l)*ttt(l))
	   tot=0.
	   do ll=mpoin(l-1)+1,mpoin(l)
	      tot=tot+(values(ll)*x(indx(ll)) )
	   enddo
	   rnumer=rnumer+(ttt(l)-tot)**2
	enddo
	varred=1.-(rnumer/denom)
cTEST
	print*,"cumulative variance reduction",varred

c----------------------------------------------------------------------------
c---store cumulative variance reduction and log-likelihood
 	open(72,file="fit.txt",access="append")
	write(72,*)wgradh,wgradv(1),itn,varred
	close(72)
	open(72,file="loglike.txt",access="append")
c--log-likelihood function
	write(72,*)damp0,wgradh,-0.5*float(nelrhsold)*log(rnumer),itn,n
	close(72)
c----------------------------------------------------------------------------

c--compute variance reduction for all datasets used in this inversion
	print*,'compute vr for',konta-1,' subsets of data'
	open(99,file="vr.txt",access='append')
	do ivre=1,konta-1
	   rnumer=0.
	   denom=0.
	   xsolrms=0.
	   do l=ipo(ivre),ipo(ivre+1)-1
	      denom=denom+(ttt(l)*ttt(l))
	      tot=0.
c	write(99,*)ivre,mpoin(l-1)+1,mpoin(l)!TEST
	      do ll=mpoin(l-1)+1,mpoin(l)
	         tot=tot+(values(ll)*x(indx(ll)))
	      enddo
	      xsolrms = xsolrms + tot**2
	      rnumer=rnumer+(ttt(l)-tot)**2
	   enddo
	   subvr=1.-(rnumer/denom)
	   xsolrms = sqrt(xsolrms/(ipo(ivre+1)-ipo(ivre)-1))! rms of solution
	   print*,'set',ivre,' vr ',subvr,' rms ',xsolrms,' np ',ipo(ivre+1)-ipo(ivre)-1
	   write(99,*)subvr
	enddo

        print*,"write solution to disc"
        nomfil='solution.txt'
        open(1,file=nomfil)
        do k=1,n
           write(1,*)k,x(k)*100.
        enddo

	print*,'variance reduction=',varred

	end
c=======================================================================

c=======================================================================
* From rfischer@seismology.harvard.edu Wed Jul 23 15:30:22 1997
* Retrieved by Bob Fischer from: http://www.netlib.org/linalg/lsqr
* From arpa!sol-michael.stanford.edu!mike 5 May 89 23:53:00 PDT
      SUBROUTINE LSQR  ( M, N, DAMP,
     $                   LENIW, LENRW, IW, rw,
     $                   U, V, W, X, SE,
     $                   ATOL, BTOL, CONLIM, ITNLIM, NOUT,
     $   ISTOP, ITN, ANORM, ACOND, RNORM, ARNORM, XNORM,
     &       indx,values,mpoin,nonz)

c--I had to add next line to pass the matrix to aprod without using a common
	dimension indx(nonz),values(nonz),mpoin(0:m)

c      EXTERNAL           APROD
      INTEGER            M, N, LENIW, LENRW, ITNLIM, NOUT, ISTOP, ITN
      INTEGER            IW(LENIW)
c      DOUBLE PRECISION   RW(LENRW), U(M), V(N), W(N), X(N), SE(N),
      real*4  RW(LENRW), U(M), V(N), W(N), X(N), SE(N),
     $                   ATOL, BTOL, CONLIM, DAMP,
     $                   ANORM, ACOND, RNORM, ARNORM, XNORM

*-----------------------------------------------------------------------
*     Intrinsics and local variables

      INTRINSIC          ABS, MOD, SQRT
      INTEGER            I, NCONV, NSTOP
c      DOUBLE PRECISION   DNRM2
	real*4 dnrm2
c      DOUBLE PRECISION   ALFA, BBNORM, BETA, BNORM,
      real*4   ALFA, BBNORM, BETA, BNORM,
     $                   CS, CS1, CS2, CTOL, DAMPSQ, DDNORM, DELTA,
     $                   GAMMA, GAMBAR, PHI, PHIBAR, PSI,
     $                   RES1, RES2, RHO, RHOBAR, RHBAR1, RHBAR2,
     $                   RHS, RTOL, SN, SN1, SN2,
     $                   T, TAU, TEST1, TEST2, TEST3,
     $                   THETA, T1, T2, T3, XXNORM, Z, ZBAR

c      DOUBLE PRECISION   ZERO,           ONE
      PARAMETER        ( ZERO = 0.,  ONE = 1. )

      CHARACTER*16       ENTER, EXIT
      CHARACTER*60       MSG(0:7)

      DATA               ENTER /' Enter LSQR.    '/,
     $                   EXIT  /' Exit  LSQR.    '/

      DATA               MSG
     $ / 'The exact solution is  X = 0',
     $   'Ax - b is small enough, given ATOL, BTOL',
     $   'The least-squares solution is good enough, given ATOL',
     $   'The estimate of cond(Abar) has exceeded CONLIM',
     $   'Ax - b is small enough for this machine',
     $   'The least-squares solution is good enough for this machine',
     $   'Cond(Abar) seems to be too large for this machine',
     $   'The iteration limit has been reached' /
*-----------------------------------------------------------------------


*     Initialize.

      IF (NOUT .GT. 0)
     $   WRITE(NOUT, 1000) ENTER, M, N, DAMP, ATOL, CONLIM, BTOL, ITNLIM
      ITN    =   0
      ISTOP  =   0
      NSTOP  =   0
      CTOL   =   ZERO
      IF (CONLIM .GT. ZERO) CTOL = ONE / CONLIM
      ANORM  =   ZERO
      ACOND  =   ZERO
      BBNORM =   ZERO
      DAMPSQ =   DAMP**2
      DDNORM =   ZERO
      RES2   =   ZERO
      XNORM  =   ZERO
      XXNORM =   ZERO
      CS2    = - ONE
      SN2    =   ZERO
      Z      =   ZERO

      DO 10  I = 1, N
         V(I)  =  ZERO
         X(I)  =  ZERO
        SE(I)  =  ZERO
   10 CONTINUE

*     Set up the first vectors U and V for the bidiagonalization.
*     These satisfy  BETA*U = b,  ALFA*V = A(transpose)*U.

      ALFA   =   ZERO
      BETA   =   DNRM2 ( M, U, 1 )

      IF (BETA .GT. ZERO) THEN
         CALL DSCAL ( M, (ONE / BETA), U, 1 )
         CALL APROD ( 2, M, N, V, U, LENIW, LENRW, IW, RW ,
     &       indx,values,mpoin,nonz)
         ALFA   =   DNRM2 ( N, V, 1 )
      END IF

      IF (ALFA .GT. ZERO) THEN
         CALL DSCAL ( N, (ONE / ALFA), V, 1 )
         CALL DCOPY ( N, V, 1, W, 1 )
      END IF

      ARNORM =   ALFA * BETA
      IF (ARNORM .EQ. ZERO) GO TO 800

      RHOBAR =   ALFA
      PHIBAR =   BETA
      BNORM  =   BETA
      RNORM  =   BETA

      IF (NOUT   .GT.  0  ) THEN
         IF (DAMPSQ .EQ. ZERO) THEN
             WRITE(NOUT, 1200)
         ELSE
             WRITE(NOUT, 1300)
         END IF
         TEST1  = ONE
         TEST2  = ALFA / BETA
         WRITE(NOUT, 1500) ITN, X(1), RNORM, TEST1, TEST2
         WRITE(NOUT, 1600)
      END IF

*     ------------------------------------------------------------------
*     Main iteration loop.
*     ------------------------------------------------------------------
  100 ITN    = ITN + 1
	print*,'iteration:',itn

*     Perform the next step of the bidiagonalization to obtain the
*     next  BETA, U, ALFA, V.  These satisfy the relations
*                BETA*U  =  A*V  -  ALFA*U,
*                ALFA*V  =  A(transpose)*U  -  BETA*V.

      CALL DSCAL ( M, (- ALFA), U, 1 )
      CALL APROD ( 1, M, N, V, U, LENIW, LENRW, IW, RW,
     &       indx,values,mpoin,nonz)

      BETA   =   DNRM2 ( M, U, 1 )
      BBNORM =   BBNORM  +  ALFA**2  +  BETA**2  +  DAMPSQ

      IF (BETA .GT. ZERO) THEN
         CALL DSCAL ( M, (ONE / BETA), U, 1 )
         CALL DSCAL ( N, (- BETA), V, 1 )
         CALL APROD ( 2, M, N, V, U, LENIW, LENRW, IW, RW ,
     &       indx,values,mpoin,nonz)
         ALFA   =   DNRM2 ( N, V, 1 )
         IF (ALFA .GT. ZERO) THEN
            CALL DSCAL ( N, (ONE / ALFA), V, 1 )
         END IF
      END IF

*     Use a plane rotation to eliminate the damping parameter.
*     This alters the diagonal (RHOBAR) of the lower-bidiagonal matrix.

      RHBAR2 = RHOBAR**2  +  DAMPSQ
      RHBAR1 = SQRT( RHBAR2 )
      CS1    = RHOBAR / RHBAR1
      SN1    = DAMP   / RHBAR1
      PSI    = SN1 * PHIBAR
      PHIBAR = CS1 * PHIBAR

*     Use a plane rotation to eliminate the subdiagonal element (BETA)
*     of the lower-bidiagonal matrix, giving an upper-bidiagonal matrix.

      RHO    =   SQRT( RHBAR2  +  BETA**2 )
      CS     =   RHBAR1 / RHO
      SN     =   BETA   / RHO
      THETA  =   SN * ALFA
      RHOBAR = - CS * ALFA
      PHI    =   CS * PHIBAR
      PHIBAR =   SN * PHIBAR
      TAU    =   SN * PHI

*     Update  X, W  and the standard error estimates.

      T1     =   PHI   / RHO
      T2     = - THETA / RHO
      T3     =   ONE   / RHO

      DO 200  I =  1, N
         T      =  W(I)
         X(I)   =  T1*T  +  X(I)
         W(I)   =  T2*T  +  V(I)
         T      = (T3*T)**2
         SE(I)  =  T     +  SE(I)
         DDNORM =  T     +  DDNORM
  200 CONTINUE

*     Use a plane rotation on the right to eliminate the
*     super-diagonal element (THETA) of the upper-bidiagonal matrix.
*     Then use the result to estimate  norm(X).

      DELTA  =   SN2 * RHO
      GAMBAR = - CS2 * RHO
      RHS    =   PHI    - DELTA * Z
      ZBAR   =   RHS    / GAMBAR
      XNORM  =   SQRT( XXNORM    + ZBAR **2 )
      GAMMA  =   SQRT( GAMBAR**2 + THETA**2 )
      CS2    =   GAMBAR / GAMMA
      SN2    =   THETA  / GAMMA
      Z      =   RHS    / GAMMA
      XXNORM =   XXNORM + Z**2

*     Test for convergence.
*     First, estimate the norm and condition of the matrix  Abar,
*     and the norms of  rbar  and  Abar(transpose)*rbar.

      ANORM  =   SQRT( BBNORM )
      ACOND  =   ANORM * SQRT( DDNORM )
      RES1   =   PHIBAR**2
      RES2   =   RES2  +  PSI**2
      RNORM  =   SQRT( RES1 + RES2 )
      ARNORM =   ALFA  * ABS( TAU )

*     Now use these norms to estimate certain other quantities,
*     some of which will be small near a solution.

      TEST1  =   RNORM /  BNORM
      TEST2  =   ZERO
      IF (RNORM .GT. ZERO) TEST2 = ARNORM / (ANORM * RNORM)
      TEST3  =   ONE   /  ACOND
      T1     =   TEST1 / (ONE  +  ANORM * XNORM / BNORM)
      RTOL   =   BTOL  +  ATOL *  ANORM * XNORM / BNORM

*     The following tests guard against extremely small values of
*     ATOL, BTOL  or  CTOL.  (The user may have set any or all of
*     the parameters  ATOL, BTOL, CONLIM  to zero.)
*     The effect is equivalent to the normal tests using
*     ATOL = RELPR,  BTOL = RELPR,  CONLIM = 1/RELPR.

      T3     =   ONE + TEST3
      T2     =   ONE + TEST2
      T1     =   ONE + T1
      IF (ITN .GE. ITNLIM) ISTOP = 7
      IF (T3  .LE. ONE   ) ISTOP = 6
      IF (T2  .LE. ONE   ) ISTOP = 5
      IF (T1  .LE. ONE   ) ISTOP = 4

*     Allow for tolerances set by the user.

      IF (TEST3 .LE. CTOL) ISTOP = 3
      IF (TEST2 .LE. ATOL) ISTOP = 2
      IF (TEST1 .LE. RTOL) ISTOP = 1
*     ==================================================================

*     See if it is time to print something.

      IF (NOUT  .LE.  0       ) GO TO 600
      IF (N     .LE. 40       ) GO TO 400
      IF (ITN   .LE. 10       ) GO TO 400
      IF (ITN   .GE. ITNLIM-10) GO TO 400
      IF (MOD(ITN,10) .EQ. 0  ) GO TO 400
      IF (TEST3 .LE.  2.0*CTOL) GO TO 400
      IF (TEST2 .LE. 10.0*ATOL) GO TO 400
      IF (TEST1 .LE. 10.0*RTOL) GO TO 400
      IF (ISTOP .NE.  0       ) GO TO 400
      GO TO 600

*     Print a line for this iteration.

  400 WRITE(NOUT, 1500) ITN, X(1), RNORM, TEST1, TEST2, ANORM, ACOND
      IF (MOD(ITN,10) .EQ. 0) WRITE(NOUT, 1600)
*     ==================================================================

*     Stop if appropriate.
*     The convergence criteria are required to be met on  NCONV
*     consecutive iterations, where  NCONV  is set below.
*     Suggested value:  NCONV = 1, 2  or  3.

  600 IF (ISTOP .EQ. 0) NSTOP = 0
      IF (ISTOP .EQ. 0) GO TO 100
      NCONV  =   1
      NSTOP  =   NSTOP + 1
      IF (NSTOP .LT. NCONV  .AND.  ITN .LT. ITNLIM) ISTOP = 0
      IF (ISTOP .EQ. 0) GO TO 100
*     ------------------------------------------------------------------
*     End of iteration loop.
*     ------------------------------------------------------------------


*     Finish off the standard error estimates.

      T    =   ONE
      IF (M      .GT.   N )  T = M - N
      IF (DAMPSQ .GT. ZERO)  T = M
      T    =   RNORM / SQRT( T )

      DO 700  I = 1, N
         SE(I)  = T * SQRT( SE(I) )
  700 CONTINUE

*     Print the stopping condition.

  800 IF (NOUT .GT. 0) THEN
         WRITE(NOUT, 2000) EXIT, ISTOP, ITN,
     $                     EXIT, ANORM, ACOND,
     $                     EXIT, RNORM, ARNORM,
     $                     EXIT, BNORM, XNORM
         WRITE(NOUT, 3000) EXIT, MSG(ISTOP)
      END IF

  900 RETURN

*     ------------------------------------------------------------------
 1000 FORMAT(// 1P, A, '  Least-squares solution of  A*x = b'
     $    / ' The matrix  A  has', I7, ' rows   and', I7, ' columns'
     $    / ' The damping parameter is         DAMP   =', E10.2
     $    / ' ATOL   =', E10.2, 15X,        'CONLIM =', E10.2
     $    / ' BTOL   =', E10.2, 15X,        'ITNLIM =', I10)
 1200 FORMAT(// '   Itn       x(1)           Function',
     $   '     Compatible   LS        Norm A    Cond A' /)
 1300 FORMAT(// '   Itn       x(1)           Function',
     $   '     Compatible   LS     Norm Abar Cond Abar' /)
 1500 FORMAT(1P, I6, 2E17.9, 4E10.2)
 1600 FORMAT(1X)
 2000 FORMAT(/ 1P, A, 6X, 'ISTOP =', I3,   16X, 'ITN    =', I9
     $       /     A, 6X, 'ANORM =', E13.5, 6X, 'ACOND  =', E13.5
     $       /     A, 6X, 'RNORM =', E13.5, 6X, 'ARNORM =', E13.5,
     $       /     A, 6X, 'BNORM =', E13.5, 6X, 'XNORM  =', E13.5)
 3000 FORMAT( A, 6X, A )
*     ------------------------------------------------------------------
*     End of LSQR
      END
c=======================================================================

c=======================================================================
      subroutine  dcopy(n,dx,incx,dy,incy)
c
c     copies a vector, x, to a vector, y.
c     uses unrolled loops for increments equal to one.
c     jack dongarra, linpack, 3/11/78.
c     modified 12/3/93, array(1) declarations changed to array(*)
c
c      double precision dx(*),dy(*)
      real dx(*),dy(*)
      integer i,incx,incy,ix,iy,m,mp1,n
c
      if(n.le.0)return
      if(incx.eq.1.and.incy.eq.1)go to 20
c
c        code for unequal increments or equal increments
c          not equal to 1
c
      ix = 1
      iy = 1
      if(incx.lt.0)ix = (-n+1)*incx + 1
      if(incy.lt.0)iy = (-n+1)*incy + 1
      do 10 i = 1,n
        dy(iy) = dx(ix)
        ix = ix + incx
        iy = iy + incy
   10 continue
      return
c
c        code for both increments equal to 1
c
c
c        clean-up loop
c
   20 m = mod(n,7)
      if( m .eq. 0 ) go to 40
      do 30 i = 1,m
        dy(i) = dx(i)
   30 continue
      if( n .lt. 7 ) return
   40 mp1 = m + 1
      do 50 i = mp1,n,7
        dy(i) = dx(i)
        dy(i + 1) = dx(i + 1)
        dy(i + 2) = dx(i + 2)
        dy(i + 3) = dx(i + 3)
        dy(i + 4) = dx(i + 4)
        dy(i + 5) = dx(i + 5)
        dy(i + 6) = dx(i + 6)
   50 continue
      return
      end
c=======================================================================

c=======================================================================
      subroutine  dscal(n,da,dx,incx)
c
c     scales a vector by a constant.
c     uses unrolled loops for increment equal to one.
c     jack dongarra, linpack, 3/11/78.
c     modified 3/93 to return if incx .le. 0.
c     modified 12/3/93, array(1) declarations changed to array(*)
c
c      double precision da,dx(*)
      real*4 da,dx(*)
      integer i,incx,m,mp1,n,nincx
c
      if( n.le.0 .or. incx.le.0 )return
      if(incx.eq.1)go to 20
c
c        code for increment not equal to 1
c
      nincx = n*incx
      do 10 i = 1,nincx,incx
        dx(i) = da*dx(i)
   10 continue
      return
c
c        code for increment equal to 1
c
c
c        clean-up loop
c
   20 m = mod(n,5)
      if( m .eq. 0 ) go to 40
      do 30 i = 1,m
        dx(i) = da*dx(i)
   30 continue
      if( n .lt. 5 ) return
   40 mp1 = m + 1
      do 50 i = mp1,n,5
        dx(i) = da*dx(i)
        dx(i + 1) = da*dx(i + 1)
        dx(i + 2) = da*dx(i + 2)
        dx(i + 3) = da*dx(i + 3)
        dx(i + 4) = da*dx(i + 4)
   50 continue
      return
      end
c=======================================================================

c=======================================================================
c      DOUBLE PRECISION FUNCTION DNRM2 ( N, X, INCX )
      real FUNCTION DNRM2 ( N, X, INCX )
*     .. Scalar Arguments ..
      INTEGER                           INCX, N
*     .. Array Arguments ..
c      DOUBLE PRECISION                  X( * )
      real*4               X( * )
*     ..
*
*  DNRM2 returns the euclidean norm of a vector via the function
*  name, so that
*
*     DNRM2 := sqrt( x'*x )
*
*
*
*  -- This version written on 25-October-1982.
*     Modified on 14-October-1993 to inline the call to DLASSQ.
*     Sven Hammarling, Nag Ltd.
*
*
*     .. Parameters ..
c      DOUBLE PRECISION      ONE         , ZERO
      PARAMETER           ( ONE = 1., ZERO = 0. )
*     .. Local Scalars ..
      INTEGER               IX
c      DOUBLE PRECISION      ABSXI, NORM, SCALE, SSQ
      real*4     ABSXI, NORM, SCALE, SSQ
*     .. Intrinsic Functions ..
      INTRINSIC             ABS, SQRT
*     ..
*     .. Executable Statements ..
      IF( N.LT.1 .OR. INCX.LT.1 )THEN
         NORM  = ZERO
      ELSE IF( N.EQ.1 )THEN
         NORM  = ABS( X( 1 ) )
      ELSE
         SCALE = ZERO
         SSQ   = ONE
*        The following loop is equivalent to this call to the LAPACK
*        auxiliary routine:
*        CALL DLASSQ( N, X, INCX, SCALE, SSQ )
*
         DO 10, IX = 1, 1 + ( N - 1 )*INCX, INCX
            IF( X( IX ).NE.ZERO )THEN
               ABSXI = ABS( X( IX ) )
               IF( SCALE.LT.ABSXI )THEN
                  SSQ   = ONE   + SSQ*( SCALE/ABSXI )**2
                  SCALE = ABSXI
               ELSE
                  SSQ   = SSQ   +     ( ABSXI/SCALE )**2
               END IF
            END IF
   10    CONTINUE
         NORM  = SCALE * SQRT( SSQ )
      END IF
*
      DNRM2 = NORM
      RETURN
*
*     End of DNRM2.
*
      END
c=======================================================================

c=======================================================================
	subroutine aprod(mode,m,n,x,y,lenin,lenva,iw,aty,
     &       indx,values,mpoin,nonz)
c	implicit real*8(a-h,o-z)
	integer iw(lenin)
	real*4 x(n),y(m),aty(n)
	dimension indx(nonz),values(nonz),mpoin(0:m)

	if(mode.eq.1)then
c--compute y=y+A*x
	do k=1,m
	   pro=0.
	   do j=mpoin(k-1)+1,mpoin(k)
	      pro=pro+values(j)*x(indx(j))
	   enddo
	   y(k)=y(k)+pro
	enddo
	return

	elseif(mode.eq.2)then
	do i=1,n
	   aty(i)=0.
	enddo
c--compute x=x+(A^t)*y
	do k=1,m
	   do j=mpoin(k-1)+1,mpoin(k)
	      aty(indx(j))=aty(indx(j))+values(j)*y(k)
	   enddo
	enddo
	do i=1,n
	   x(i)=x(i)+aty(i)
	enddo
	return

	else
	print*,'error: mode=',mode
	stop
	endif
	end
c=======================================================================

c=======================================================================
	function isqre(lat,lon,nsqrs,nsqtot,nlatzones,n,eq_incr)
c----finds the index of the square where (lat,lon) is
	real*4 lat,lon,loc_incr
	dimension nsqrs(nlatzones),nsqtot(nlatzones+1)
c	llat=lat
c	lazone=(90-llat)/eq_incr+1
c	if((90-llat).eq.180)lazone=nlatzones
	lazone=(90.-lat)/eq_incr+1
	if((90.-lat).gt.180.)lazone=nlatzones
	if((90.-lat).gt.181.)stop "problems in function isqre"
	if(lazone.gt.nlatzones)then
	   print*,"problems in function isqre, latitude",lazone,lat
	   stop
	endif
c	llon=lon
	if(lon.lt.0.)lon=360.+lon
	if(lon.eq.360.)lon=0.
	loc_incr=360./float(nsqrs(lazone))
	isqre=(lon/loc_incr)+1
	isqre=isqre+nsqtot(lazone)
	if(isqre.gt.n)then
	   print*,"problems in function isqre, longitude",isqre,n,lon,loc_incr,lazone
	   stop
	endif
	RETURN
	END


        subroutine param(eq_incr,nsqrs,nsqtot,nlatzones,numto,iswit,
     &refgrid,nlatzomax)
c---find vectors nsqrs and nsqtot that define a block parameterization
c---eq_incr,iswit,refgrid are input, the rest is output
c        dimension nsqrs(nlatzones),nsqtot(nlatzones+1)
        dimension nsqrs(nlatzomax),nsqtot(nlatzomax+1)
	parameter(pi=3.1415926536)
	numto=0
	colat=-eq_incr/2.

cTEST
c	print*,nlatzones
c	pause

	do k=1,nlatzones
	   colat=colat+eq_incr
	   theta=(colat/180.)*pi
c--for this latitudinal zone, compute number of blocks (nsqrs)
	   deltalon=eq_incr/(sin(theta))
	   nsqrs(k)=(360./deltalon)+1
           if(mod(nsqrs(k),2).ne.0)nsqrs(k)=nsqrs(k)-1
c--if requested, correct nsqrs(k) so the grid is compatible to reference grid
	   if(iswit.eq.1)then
              if(360./nsqrs(k).ge.refgrid)then
 100             if(mod(360./nsqrs(k),refgrid).ne.0)then
                    nsqrs(k)=nsqrs(k)+1
                    goto100
                 else
                 endif
              elseif(360./nsqrs(k).lt.refgrid)then
 101             if(mod(refgrid,360./nsqrs(k)).ne.0)then
                    nsqrs(k)=nsqrs(k)-1
                    goto101
                 else
                 endif
              endif
	   endif
           if(MOD(NSQRS(K),2).ne.0)stop "nsqrs has to be even"
c---------------------------
	   nsqtot(k)=numto
	   numto=numto+nsqrs(k)
	enddo
	nsqtot(nlatzones+1)=numto
c	print*,nsqrs
	print*,"total number of blocks:",nsqtot(nlatzones+1)
	return
	end
c=======================================================================
	subroutine damp_rough_cmb(dampcmb,nelm,nelrhs,values,indx,
     & mpoin,t,m,n,NONZ,inp,NGRIDCMB,nsqrs,nsurf,nlatzones,nlatzomax)
c--at the present time it is hardwired to 1656 equiareal blocks.
c--the size and number of blocks however can be changed
c--by just modifying the following lines..
c	parameter(eq_incr=5.,Nsurf=1668)
c	PARAMETER(nlatzones=180./EQ_INCR)
c	DIMENSION NSQRS(nlatzones)
	DIMENSION NSQRS(nlatzomax)

	if(nSURF.NE.NGRIDCMB)THEN
	   PRINT*,'problem in damp_rough_cmb'
	   stop
	endif

c	CALL param(EQ_INCR,NSQRS,NSQTOT,NLATZONES,numto,0,eq_incr)
	CALL ROUGHNESS_D(VALUES,INDX,MPOIN,dampcmb,nelrhs,nelm,nonz,
     &m,t,nsqrs,nsqtot,nlatzones,eq_incr,Nsurf,INP,nlatzomax)

	RETURN
	END
c=======================================================================

c=======================================================================
	SUBROUTINE damp_SIZE_cmb(WEIGHT,nnn,nelp,values,indx,
     & mpoin,t,m,nmantle,NGRIDCMB,nonz)
	dimension t(m)
	dimension indx(nonz),values(nonz),mpoin(0:m)

	do k=1,ngridcmb
	   nnn=nnn+1
	   values(nnn)=WEIGHT
	   indx(nnn)=k+nmantle
	   nnn=nnn+1
	   nelp=nelp+1
	   mpoin(nelp)=nnn
	   t(nelp)=0.
	enddo

	RETURN
	END
c=======================================================================

c=======================================================================
	SUBROUTINE damp_SIZE(WEIGHT,nnn,nelp,values,indx,
     & mpoin,t,m,nmantle,nonz,N1)
	dimension t(m)
	dimension indx(nonz),values(nonz),mpoin(0:m)

	do k=1,N1
	   nnn=nnn+1
	   values(nnn)=WEIGHT
	   indx(nnn)=k+nmantle
	   nnn=nnn+1
	   nelp=nelp+1
	   mpoin(nelp)=nnn
	   t(nelp)=0.
	enddo

	RETURN
	END
c=======================================================================

c=======================================================================
	subroutine dampaniso(wisot,nnn,nelp,values,indx,mpoin,rhs,m,n_1,NONZ)
	dimension rhs(m)
	dimension indx(nonz),values(nonz),mpoin(0:m)


	do k=1,n_1
	   nnn=nnn+1
	   values(nnn)=wisot
	   indx(nnn)=k
	   nnn=nnn+1
	   values(nnn)=-1.*wisot
	   indx(nnn)=k+n_1
	   nelp=nelp+1
	   mpoin(nelp)=nnn
	   rhs(nelp)=0.d0
	enddo

	return
	end
c=======================================================================

c=======================================================================
	FUNCTION superISQRE(LAT,LON,nsqrs,nsqtot,nlatzones,n,eq_incr,nlatzomax)
c--FINDS THE NUMBER OF THE SQUARE WHERE (LAT,LON) IS

c	DIMENSION NSQRS(nlatzones),NSQTOT(nlatzones+1)
	DIMENSION NSQRS(nlatzomax),NSQTOT(nlatzomax+1)

	incr=eq_incr*100.

	LAZONE=(9000-LAT)/incr+1
	IF(LAZONE.GT.nlatzones)LAZONE=nlatzones
	LLON=LON
	IF(LLON.LT.0)LLON=36000+LLON
	superISQRE=(LLON*NSQRS(LAZONE))/36000+1
	superISQRE=superISQRE+NSQTOT(LAZONE)
	IF(superISQRE.GT.n)superISQRE=n
	RETURN
	END
c=======================================================================

c=======================================================================
	subroutine coordsuper(nlatzomax,nbloc,blocla,bloclo,nsqrs,nlatzones,eq_incr)
c--given a cell index on the Earth's surface, finds longitude and latitude
c--(NOT colatitude) of its center.
c	DIMENSION NSQRS(nlatzones)
	DIMENSION NSQRS(nlatzomax)

	ntot=0
c--loop(s) over all the blocks
	do 500 ila=1,nlatzones
c--increment latitude
	   rlati=90.-(eq_incr*(ila-1))
c--calculate increment in longitude for this band
	   RINLO=(360./nsqrs(ila))
	   do 400 isq=1,nsqrs(ila)
	      rlong=(360./nsqrs(ila))*(isq-1)
	      ntot=ntot+1
	      if(ntot.eq.nbloc)then
	         bloclo=rlong+(rinlo/2.)
	         blocla=rlati-(eq_incr/2.)
	         goto 600
	      endif
400	   continue
500	continue
600	return
	end
c=======================================================================

c=======================================================================
c	subroutine ROUGHNESS_D(G,indx,mpoin,nblo,weight,kpoin,nnn,
c     &	nonz,m,rhs,nsqrs,nsqtot,nlatzones,EQ_INCR,N)
	subroutine ROUGHNESS_D(G,indx,mpoin,weight,kpoin,nnn,nonz,
     & m,rhs,nsqrs,nsqtot,nlatzones,EQ_INCR,N,INP,nlatzomax)

c--defines the matrix corresponding to gradient damping,
c--accounting for the surface gradient at each BOUNDARY between blocks.
	dimension indx(NONZ),G(NONZ),mpoin(0:M),rhs(m)

c	DIMENSION NSQRS(nlatzones),NSQTOT(nlatzones+1)
	DIMENSION NSQRS(nlatzomax),NSQTOT(nlatzomax+1)

	kpoin0=kpoin
c	print*,'gradamp=',weight

	NNORTH=N/2
c	PRINT*,'NNORTH=',NNORTH
	indexch=(nlatzones/2)+1
	NSOUTH=NNORTH+nsqrs(indexch)+1
c	PRINT*,'NSOUTH=',NSOUTH

	do i1=1,nlatzones
c--rloin_2=half the longitudinal size of the block
	   rloin_2=180./nsqrs(i1)
	   ifirst=nsqtot(i1)+1
	   ilast=nsqtot(i1+1)
	   do i2=ifirst,ilast
c--find iright
	      iright=i2+1
	      if(i2.eq.ilast)iright=ifirst
c--define the row of the damping matrix corresponding to 
c--longitude variation.
	      indx(nnn+1)=i2+INP
	      indx(nnn+2)=iright+INP
	      g(nnn+1)=weight
	      g(nnn+2)=-weight
	      nnn=nnn+2
	      kpoin=kpoin+1
	      mpoin(kpoin)=nnn
	      rhs(kpoin)=0.

c	print*,"lONGITUDE",indx(nnn-1),g(nnn-1),
c     &indx(nnn),g(nnn)
c==============================================
c==============================================
c--find iup and idw
	      IF((1.LE.I2).AND.(I2.LE.NNORTH))THEN
c--if i2 is in the N hemisphere
	         call coordsuper(nlatzomax,i2,rila,rilo,nsqrs,nlatzones,eq_incr)
	         dwla=rila-eq_incr
	         ilol=(rilo-rloin_2)*100.+0.5
	         ilor=(rilo+rloin_2)*100.-0.5
	         idwla=dwla*100.
	         idwl=superISQRE(idwla,ilol,
     &nsqrs,nsqtot,nlatzones,n,eq_incr,nlatzomax)
	         idwr=superISQRE(idwla,ilor,
     &nsqrs,nsqtot,nlatzones,n,eq_incr,nlatzomax)
c--define the row corresponding to
c--variation wrt latitude (N hemisphere)
	         g(nnn+1)=weight
	         indx(nnn+1)=i2+INP
	         nnn=nnn+1

c	PRINT*,'LATITUDE',indx(nnn),g(nnn)

	         peso=1./(idwr-idwl+1)
	         do idw=idwl,idwr
	            g(nnn+1)=-weight*peso
	            indx(nnn+1)=idw+INP
	            nnn=nnn+1
c	PRINT*,indx(nnn),g(nnn)
	         enddo


c	write(99,*)I2,rila,dwla,IDWR,IDWL

	         kpoin=kpoin+1
	         mpoin(kpoin)=nnn
	         rhs(kpoin)=0.

c	print*,mpoin(kpoin)

c==============================================
	      elseIF((NSOUTH.LE.I2).AND.(I2.LE.N))THEN
c--if i2 is in the S hemisphere (exclude the blocks bounded to the N
c--by the equator because the variation across the equator has already
c--been accounted for).
	         call coordsuper(nlatzomax,i2,rila,rilo,nsqrs,nlatzones,eq_incr)
	         upla=rila+eq_incr
	         ilol=(rilo-rloin_2)*100.+0.5
	         ilor=(rilo+rloin_2)*100.-0.5
	         iupla=upla*100.
	         iupl=superISQRE(iupla,ilol,
     &nsqrs,nsqtot,nlatzones,n,eq_incr,nlatzomax)
	         iupr=superISQRE(iupla,ilor,
     &nsqrs,nsqtot,nlatzones,n,eq_incr,nlatzomax)
c--define the row corresponding to
c--variation wrt latitude (S hemisphere)
	         g(nnn+1)=weight
	         indx(nnn+1)=i2+INP
	         nnn=nnn+1
c	PRINT*,'LATITUDE',indx(nnn)

	         peso=1./(iupr-iupl+1)
	         do iup=iupl,iupr
	            g(nnn+1)=-weight*peso
	            indx(nnn+1)=iup+INP
	            nnn=nnn+1
c	PRINT*,indx(nnn)
	         enddo

	         kpoin=kpoin+1
	         mpoin(kpoin)=nnn
	         rhs(kpoin)=0.
c==============================================
c==============================================
	      ENDIF

c	PAUSE

	   enddo
	enddo
c	print*,'number of rows in the CMB g.d.matrix:',kpoin-kpoin0
	return
	END
c*************
c*************
