      SUBROUTINE HYCOUT
C--OUTPUTS HYPOCENTER AND STATION DATA TO CUSP MEM FILE AFTER LOCATION

      INCLUDE 'common.inc'
      COMMON /CUSPPICK/ KPICK(MAXPHS)

C--DECLARE DATA STRUCTURES USED TO STORE MEM FILE DATA
      INCLUDE 'mem_structure.inc'
      INCLUDE 'mem_record.inc'
      LOGICAL MEMWRT

C--LOAD ORIGIN TIME
      HHY.T.YR = KYEAR2
      HHY.T.MO = KMONTH
      HHY.T.DY = KDAY
      HHY.T.HR = KHOUR
      HHY.T.MN = KMIN
      HHY.T.SEC = T1

C--LOAD ORIGIN TIME IN CUSP SECONDS
C--HHY.T IS THE INPUT DATE & TIME STRUCTURE FROM THE SOLUTION
C--HHY.T0 IS THE OUTPUT ORIGIN TIME IN CUSP SECONDS
      CALL CONVERT_TIME (6,'ENCODE',HHY.T0,HHY.T,IRES)

C--LOAD HYPOCENTER
      HHY.LAT = CLAT
      HHY.LON = -CLON            !HI LONGITUDES ARE POSITIVE WEST
      HHY.Z = -Z1            !HI DEPTHS ARE POSITIVE

C--LOAD ERROR DATA
      HHY.RMS = RMS
      HHY.NP = NWR
      HHY.GAP = MAXGAP
      HHY.DMN = DMIN
      HHY.ELT = ERH            !HI CALCS MAX HORIZ ERROR FROM
      HHY.ELN = ERH            ! PRINCIPAL COORDINATES
      HHY.EZ = ERZ
      HHY.ET = 0.            !HI DOES NOT CALC ORIGIN TIME ERROR

C--LOAD MAGNITUDE INFO
C--CODA DURATION MAGNITUDE (FROM MCD TUPLE)
      IF (FMAG.GT.0.) THEN
        HMG.MD.M = FMAG      !HI CALCS MEDIAN MAGNITUDE
C        HMG.MD.NP = NINT(MFMAG *.01) !HI STORES TOTAL OF WEIGHTS X100
        HMG.MD.NP = NFMAG	!NUMBER OF CODAS
        HMG.MD.RMS = FMMAD      !HI CALCS MEDIAN-ABSOLUTE-DIFFERENCE
      END IF

C--AMPLITUDE MAGNITUDE (FROM AMF TUPLE)
      IF (XMAG.GT.0.) THEN
        HMG.MC.M = XMAG      !HI CALCS MEDIAN MAGNITUDE
C        HMG.MC.NP = NINT(MXMAG *.01) !HI STORES TOTAL OF WEIGHTS X100
        HMG.MC.NP = NXMAG 	!NUMBER OF AMPS
        HMG.MC.RMS = XMMAD      !HI CALCS MEDIAN-ABSOLUTE-DIFFERENCE
      END IF

C--WOOD-ANDERSON LOCAL MAGNITUDE (FROM AMP TUPLE)
      IF (MXMAG2.GT.0. .AND. LABX2.EQ.'L') THEN
        HMG.ML.M = XMAG2      !HI CALCS MEDIAN MAGNITUDE
C        HMG.ML.NP = NINT(MXMAG2 *.01) !HI STORES TOTAL OF WEIGHTS X100
        HMG.ML.NP = NXMAG2 	!NUMBER OF LOCAL MAGS
        HMG.ML.RMS = XMMAD2     !HI CALCS MEDIAN-ABSOLUTE-DIFFERENCE
      END IF

C--LOAD STATION DATA------------------------------------------------------
C--LOOP OVER STATION PHASES.
C--IN HYPOINVERSE, A STATION (K) COULD HAVE BOTH A P AND S PICK,
C  BUT HYCIN USED A SEPARATE STATION ENTRY (K) FOR EACH PICK.
C--IM IS THE PHASE INDEX.
      DO 30 IM=1,M

C--DETERMINE STATION INDEX & WHETHER IT IS P OR S
        K=IND(IM)
        KPS=K/10000
        K=K-10000*KPS

C--POINTER TO PICK STRUCTURE. SOME STATIONS MAY NOT HAVE PICKS.
        IX=KPICK(K)
        IF (IX.EQ.0) GOTO 30

C--DECODE AZIMUTH, EMERGENCE ANGLE, AND DISTANCE
        KAZ=KAZEM(K)/180
        HPX(IX).IA = ABS(KAZEM(K)-180*KAZ)
        IF (KAZ.LT.0) KAZ=KAZ+360
        HPX(IX).AZ = KAZ
        HPX(IX).X = DIS(K)

C--INDEX TO STATION TABLE
        J=KINDX(K)

C--STATION DELAY
C--GET P DELAY FROM MODEL(S) USED IN SOLUTION
        IF (LMULT) THEN
          TEMP=0.
          DO I=1,NMOD
            IT=MODS(I)
            TEMP=TEMP+WMOD(I)*.01*JPD(IT,J)
          END DO
        ELSE
          TEMP=.01*JPD(MOD,J)
        END IF

C--CALCULATED TRAVEL TIME
        TCAL=.01*MTCAL(IM)

C--DECODE ASSIGNED WEIGHTS
        LSWT=KWT(K)/10
        LPWT=KWT(K)-LSWT*10

C--S DELAY AND TRAVEL TIME ARE LARGER THAN FOR P
        IF (KPS.EQ.0) THEN
          DLY=TEMP
          SEC=KP(K)*.01
          LWT=LPWT
        ELSE
          DLY=POS*TEMP
          TCAL=TCAL*POS
          SEC=KS(K)*.01
          LWT=LSWT
        END IF
        HPX(IX).TC = DLY

C--TRAVEL TIME RESIDUAL
        TOBS=SEC-T1
        RES=TOBS-TCAL-DLY
        HPX(IX).TTR = RES

C--THE TIMING ERROR IS NOT CALCULATED BY HYPOINVERSE, BUT CAN BE
C  ESTIMATED FROM THE STANDARD ERROR AND ASSIGNED WEIGHT
        ERR=RDERR
        IF (LWT.EQ.0) THEN
          CONTINUE
        ELSE IF (LWT.EQ.1) THEN
          ERR=ERR*1.3
        ELSE IF (LWT.EQ.2) THEN
          ERR=ERR*2.
        ELSE IF (LWT.EQ.3) THEN
          ERR=ERR*4.
        ELSE IF (LWT.GT.3) THEN
          ERR=10.
        END IF
        HPX(IX).ERR = ERR
30      CONTINUE

C--WRITE INFO IN STRUCTURES TO MEM FILE
C--JCPO CONTROLS TO WHAT EXTENT RESULTS ARE WRITTEN OUT TO CUSP
C  =0 NOTHING WRITTEN ANYWHERE
C  =1 STRUTURES UPDATED
C  =2 ABOVE PLUS SHARED MEMORY UPDATED
C  =3 ABOVE PLUS MEM FILE RE-WRITTEN
      MEMWRT = JCPO.GE.3
      IF (JCPO.GE.2) CALL MEM_EQ_UPDATE (MEMWRT,IRESM)
      IF (IRESM.LE.0) THEN
        WRITE (6,*) 
     2  ' *** COULD NOT WRITE TO MEM FILE FOR EVENT NUMBER ',IDNO
        WRITE (6,*) ' *** IRESM ERROR CODE ',IRESM
        IRES=IRESM-100
        RETURN
      END IF
      RETURN
      END
