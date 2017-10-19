c---on a 2d square xtot*ytot
c---define a pixel grid x_incr*y_incr
c---and then create a checkerboard map defined by n. of pixels per rectangle side
      print*,"horizontal increment of parameterization grid?"
      read*,x_incr
      print*,"vertical increment of parameterization grid?"
      read*,y_incr
      print*,"horizontal length of gridded region?"
      read*,xtot
      print*,"vertical length of gridded region?"
      read*,ytot
      print*,"how many parameterization pixels per horizontal side of checkerboard rectangle?"
      read*,npxh
      print*,"how many parameterization pixels per vertical side of checkerboard rectangle?"
      read*,npxv
      print*,"amplitude of heterogeneity"
      read*,value
      nxtot=xtot/x_incr
      nytot=ytot/y_incr
      print*,"then ",nxtot," horizontal intervals, and "
     &,nytot," vertical intervals"
      n=nxtot*nytot
      print*,n,"pixels"
      open(1,file="chkbd.px")
      open(2,file="chkbd.xyz")
      isign1=1
c      isign2=1
      do ny=1,nytot
         if(mod(ny+npxv,npxv).eq.0)then
cTEST
c	        print*,ny,npxv," change sign"
	 	isign1=isign1*-1
	 endif
	 isign2=1
         do nx=1,nxtot
            if(mod(nx+npxh,npxh).eq.0)isign2=isign2*-1
            write(1,*)inpx(nx,ny,nxtot,nytot),isign1*isign2*value
c            write(2,*)(nx-1)*x_incr+x_incr/2.,(ny-1)*y_incr+y_incr/2.,isign1*isign2*value
            write(2,*)(nx-1)*x_incr,(ny-1)*y_incr,isign1*isign2*value
         enddo
      enddo
      close(1)
      close(2)
      end
      function inpx(nx,ny,nxtot,nytot)
c-------associate pixel number to nx, ny location
      inpx=(ny-1)*nxtot+nx
      return
      end
