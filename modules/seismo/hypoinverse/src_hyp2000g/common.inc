C--COMMON BLOCK INCLUDE FILE FOR HYPOINVERSE
      LOGICAL LSUM,LARC,LPRT,LERR,LEJCT,LREP,DONE,LCUSP,LPRALL
      LOGICAL LAPP,LJUNK,LMULT,LTBIG,LMAG,LBSTA,LBCRU,SUBMOD,LP153
      LOGICAL L2000,HYPOFIX,ALLFIX
C--LASK IS A LOGICAL FUNCTION. 
C  THE g77 COMPILER REQUIRES THE NAME BE DECLARED LOGICAL.
C  THE f77 COMPILER DOES NOT NEED THE LOGICAL DECLARATION, AND GIVES
C  WARNING MESSAGES WHEN IT IS DECLARED, WHICH CAN BE IGNORED.
      LOGICAL LASK

C--SHORT INTEGER AND LOGICAL DECLARATIONS
      INCLUDE 'integer.for'	!SUN & VAX VERSION. USES *2 LENGTHS

C--GENERAL PARAMETERS
      CHARACTER CM*3,INST*132,CTEMP*1,IE*1,IS*1,TERM*80
C      CHARACTER CPADD*2
      CHARACTER RUNLAB*1,CP1*1,CP2*1,CP3*1,FIXCHR*1, CDOMAN*2,CPVERS*2
      COMMON /D/ SUBMOD		!F=NORMAL PROGRAM, T=SUBROUTINE VERSION
C      COMMON /C/ CPADD		!DUMMY PADDING IF NEEDED
      COMMON /C/ CM		!CURRENT COMMAND NAME
      COMMON /C/ INST		!CURRENT COMMAND PARAMETERS OR PHASE CARD
      COMMON /C/ CTEMP		!CHARACTER TEMPORARY
      COMMON /C/ TERM		!TERMINATOR (INSTRUCTION) CARD
      COMMON /C/ IS		!CHARACTER TEMPORARY, 'S' FOR SOUTH LAT
      COMMON /C/ IE		!CHARACTER TEMPORARY, 'E' FOR EAST LON
      COMMON /D/ PI		!PI
      COMMON /D/ RDEG		!DEGREES PER RADIAN
      COMMON /D/ INP		!COMMAND INPUT UNIT NUMBER
      COMMON /D/ ISTAT		!A STATUS FLAG SET BY HYCMD FOR HYP
      COMMON /D/ ISTAT2		!A STATUS FLAG SET BY HYOPEN FOR HYP
      COMMON /D/ IRES		!STATUS FLAG FROM HYPOINV & CUSP SUBROUTINES
      COMMON /D/ IPADD		!DUMMY PADDING
      COMMON /D/ LTBIG		!BIG EVENT FLAG (HYPHS TO HYLST)
      COMMON /C/ RUNLAB		!1-LETTER LABEL FOR ENTIRE RUN (LAB)
      COMMON /D/ LP153		!T=PASS COL 153 THRU, F=RUNLAB (LAB)
      COMMON /C/ CP1		!PASS THRU LABEL, IE AUTHORITY
      COMMON /C/ CP2		!PASS THRU LABEL, IE VERSION
      COMMON /C/ CP3		!PASS THRU LABEL, IE REVISION
      COMMON /D/ HYPOFIX	!INDIVIDUAL FLAG TO FIX ONE HYPOCENTER
      COMMON /D/ ALLFIX		!INDIVIDUAL FLAG TO FIX ONE HYPO & ORIGIN
      COMMON /C/ FIXCHR		!CHARACTER USED TO SIGNAL HYPOCENTER FIX
      COMMON /C/ CDOMAN		!CODE FOR PROCESSING DOMAIN (IE NC) (VER)
      COMMON /C/ CPVERS		!CODE FOR PROCESSING VERSION (IE 01) (VER)
      
C--FILES AND OUTPUT CONTROLS
C--DEFINING COMMAND NAMES ARE IN PARENTHESES
      CHARACTER*60 STAFIL,INFILE,PHSFIL,SUMFIL,ARCFIL,PRTFIL,DELFIL
      CHARACTER*60 ATNFIL,FMCFIL,XMCFIL,MAGFIL,BSTAFL,BCRUFL,TERMIN
      CHARACTER*60 CALFIL
      CHARACTER FORID*30, GREETING*70
      COMMON /C/ SUMFIL		!HYPOINVERSE SUMMARY OUTPUT FILENAME (SUM)
      COMMON /C/ ARCFIL		!ARCHIVE OUTPUT FILENAME (ARC)
      COMMON /C/ PRTFIL		!PRINT OUTPUT FILENAME (PRT)
      COMMON /D/ LSUM		!TRUE TO OUTPUT SUMMARY DATA (SUM)
      COMMON /D/ IH71S		!SUMMARY FORMAT 1=HI 2=H71 (H71)
      COMMON /D/ IH71T		!TERMINATOR FORMAT 1=HI 2=H71 3=HDR TRIAL (H71)
      COMMON /D/ LARC		!TRUE TO OUTPUT ARCHIVE DATA (ARC)
      COMMON /D/ LPRT		!TRUE TO OUTPUT A PRINTOUT FILE (PRT)
      COMMON /D/ LMAG		!TRUE TO OUTPUT MAGNITUDE DATA FILE (MFL)
      COMMON /D/ LERR		!TRUE TO OUTPUT ERROR MESSAGES TO TERMINAL (ERF)
      COMMON /D/ LCUSP		!TRUE IF READING FROM CUSP MEM FILES (COP)
      COMMON /C/ FORID		!CUSP ID READING FORMAT (FID)
      COMMON /C/ STAFIL		!STATION INPUT FILENAME (STA)
      COMMON /C/ BSTAFL		!BINARY STATION SNAPSHOT (WST,RST)
      COMMON /D/ LBSTA		!T IF BINARY STATIONS, F IF ASCII (RST,STA,DEL)
      COMMON /C/ BCRUFL		!BINARY CRUST MODELS SNAPSHOT (WCR,RCR)
      COMMON /D/ LBCRU		!T IF BINARY CRUST MODELS, F IF ASCII (RCR,CRT)
      COMMON /C/ DELFIL		!STATION DELAY FILENAME (DEL)
      COMMON /C/ ATNFIL		!STATION ATTENUATION FILENAME (ATE)
      COMMON /C/ CALFIL		!STATION CAL FACTOR FILENAME (CAL)
      COMMON /C/ FMCFIL		!STATION FMAG CORRECTION FILENAME (FMC)
      COMMON /C/ XMCFIL		!STATION XMAG CORRECTION FILENAME (XMC)
      COMMON /C/ PHSFIL		!PHASE DATA INPUT FILENAME (PHS)
      COMMON /C/ MAGFIL		!MAGNITUDE DATA OUTPUT FILE (MFL)
      COMMON /C/ INFILE(0:4)	!COMMAND INPUT FILENAME (@)
      COMMON /D/ ISTFMT		!STATION FORMAT NO. 1=HI 2=H71 3=HI2 (STF)
      COMMON /D/ JCP		!PHASE DATA FORMAT CODE (COP)
      COMMON /D/ JCPO		!1=MODIFY CUSP MEM FILES, 0=NO (COP)
      COMMON /D/ JCA		!ARCHIVE DATA FORMAT CODE (CAR)
C		THE CODES ARE: 1=FULL 2=CONDENSED 3=CONDENSED PICKER STYLE
      COMMON /D/ JST	!0=PRINT EVENTS ONLY, 1=ADD PARAMS & FILES TO START
C		OF PRINTOUT, 2=ADD STA LIST & CRUST MODELS (LST)
      COMMON /D/ JST2		!STATION LIST QUANTITY CONTROL 0,1,2 (LST)
      COMMON /D/ JST3		!CRUST LIST QUANTITY CONTROL 0,1 (LST)
      COMMON /D/ KPRINT		!CONTROLS AMOUNT OF PRINTOUT (KPR)
      COMMON /D/ LEJCT    !TRUE TO PAGE EJECT PRINTOUT BEFORE EACH EVENT (TOP)
      COMMON /D/ LREP  !TRUE TO REPORT EACH EVENT ON TERMINAL AS LOCATED (REP)
      COMMON /D/ LPRALL		!TRUE TO PRINT UNWEIGHTED STATIONS (REP)
      COMMON /D/ LAPP(3) !INDICATES WHETHER OUTPUT FILES ARE APPENDED TO (APP)
C		1=PRINT FILE 2=SUMMARY 3=ARCHIVE
      COMMON /C/ TERMIN		!THE CURRENT TERMINAL DEVICE CODE
      COMMON /D/ L2000		!T=SUPPORT YR2000 FORMATS, F=OLD FORMATS
      COMMON /D/ ICENT		!DEFAULT CENTURY OLD PHASE FORMAT
      COMMON /D/ IAMPU		!DEFAULT AMP UNITS CODE
      COMMON /C/ GREETING	!GREETING MESSAGE FOR TERMINAL & PRT FILE

C--HYPOINVERSE CRUSTAL MODELS ********************************************
C--DATA PRESENT FOR EACH OF THE LH=LM MODELS
C--LH IS THE MAX NO. OF LAYER MODELS WHICH MUST = LM, THE MAX NO. OF MODELS
      PARAMETER (NLYR=20,LH=36)
      CHARACTER MODNAM*30
      COMMON /MC/ MODNAM(LH)	!MODEL NAME OR LABEL
      COMMON /M/ LAY(LH)	!NUMBER OF LAYERS OR V-D POINTS
      COMMON /M/ D(NLYR,LH)	!DEPTH TO LAYER TOP OR VELOCITY POINT
      COMMON /M/ VEL(NLYR,LH)	!LAYER OR POINT VELOCITY
      COMMON /M/ THK(NLYR,LH)	!THICKNESS OF HOMOGENEOUS LAYER
      COMMON /M/ VSQ(NLYR,LH)	!SQUARED VELOCITY OF HOMOGENEOUS LAYER
      COMMON /M/ MODTYP(LH)	!MDL TYPE (-1=UNDEF, 0=GRAD, 1=HOMO LAYER)

C--DATA USED ONLY FOR LINEAR GRADIENT TRAVEL TIME TABLES
      PARAMETER (LN=36)		!THE NUMBER OF LINEAR-GRAD MODELS ALLOWED
      LOGICAL GD1,GD2,GZ1,GZ2
      COMMON /M/ REDV(LN)	!ONE OVER THE REDUCING VELOCITY
      COMMON /M/ NZ(LN)		!NUMBER OF DEPTH GRID POINTS
      COMMON /M/ NZ1(LN),DZ1(LN),NZ2(LN),DZ2(LN)	!DEPTH GRID PARAMS
      COMMON /M/ ND(LN)		!NUMBER OF DISTANCE GRID POINTS
      COMMON /M/ ND1(LN),DD1(LN),ND2(LN),DD2(LN)	!DIST GRID PARAMS
      COMMON /M/ GD1(LN),GD2(LN),GZ1(LN),GZ2(LN)	!GRID FLAGS

C--DECLARE ARRAY STORAGE FOR TRAVEL TIME TABLE. THE ARRAY KT HOLDS
C  CONDENSED TRAVEL TIMES TO 28 DEPTH & 42 DISTANCE POINTS.
C  KDHR HOLDS THE DISTANCES AT WHICH HORIZONTAL RAYS EMERGE.
      COMMON /M/ KDHR(LN,28), KT(LN,28,42)

C--PARAMETERS FOR MULTIPLE CRUSTAL MODELS
C--LM IS THE MAXIMUM NUMBER OF MODELS OF EITHER GRADIENT OR LAYER TYPE.
C  LM MUST = LH (MAX NO. OF LAYER MODELS). LN (MAX NO. OF GRADIENT MODELS)
C  MAY BE LESS THAN OR = LM.  LH & LN ARE PARAMETERS IN HYPCOMC.
      PARAMETER (LM=36)	!MAX NUMBER OF CRUST MODELS
      CHARACTER CRODE*3, CRUFIL*60
      COMMON /D/ MOD		!CURRENT CRUST MODEL NUMBER
      COMMON /C/ CRUFIL(LM)	!CRUST MODEL INPUT FILENAMES (CRT,CRH)
      COMMON /C/ CRODE(LM)	!CRUST MODEL CODE FROM NAME
      COMMON /D/ MODALT(LM)	!=0 NO ALTERNATE MOD, =N USE MODEL N W/SOME STAS
      COMMON /D/ LMULT		!T TO PROCESS W/ REGION-DEPENDENT MODELS (MUL)
      COMMON /D/ MODDEF		!DEFAULT MODEL FOR UNLISTED REGIONS (MUL)
      COMMON /D/ MAXMOD		!HIGHEST MODEL NUMBER DEFINED (CRT,CRH)
      COMMON /D/ NMOD		!NO. OF POS WEIGHTED MODELS FOR AN EVENT (1-3)
      COMMON /D/ MODS (3)	!MODEL NUMBERS FOR THIS EVENT
      COMMON /D/ WMOD (3)	!WEIGHTS OF EACH MODEL

      PARAMETER (NODMAX=124)	!MAX NO. OF GEOGRAPHIC NODES FOR DEFINING MODELS
      COMMON /D/ NNODE		!NUMBER OF NODES CURRENTLY DEFINED (NOD)
      COMMON /D/ HLAT(NODMAX)	!NODE CIRCLE CENTER LAT (NOD)
      COMMON /D/ HLON(NODMAX)	!NODE CIRCLE CENTER LON (NOD)
      COMMON /D/ RAD1(NODMAX)	!NODE CIRCLE INNER RADIUS (NOD)
      COMMON /D/ RAD2(NODMAX)	!NODE CIRCLE OUTER RADIUS
      COMMON /D/ DRAD(NODMAX)	!NODE CIRCLE TRANSITION WIDTH (NOD)
      COMMON /D/ MODH(NODMAX)	!MODEL NO. FOR THIS NODE (NOD)
      COMMON /D/ HDIS(NODMAX)	!EPICENTRAL DIST FROM NODE

C--DATA FOR ALL AVAILABLE STATIONS STORED AS LOOKUP TABLE (SUBSCRIPT J)
C  USE 2-BYTE INTEGERS WHERE POSSIBLE
      CHARACTER STANAM*5,STRMK*1,JCOMP1*1,JCOMP3*3,JNET*2,JSLOC*2
      CHARACTER JCOMPA*3,JSLOC2*2
      LOGICAL LLOC2
C     INTEGER*4 JCEXP,JFEXP,JXEXP	!LONGER LENGTH REQUIRED
      PARAMETER (MAXSTA=9000)	!MAX NO. OF STATIONS STORED IN LOOKUP TABLE
      COMMON /D/ JSTA		!NUMBER OF STATIONS ACTUALLY READ INTO TABLE
C				  JSTA <= MAXSTA
      COMMON /C/ STANAM (MAXSTA)	!5-LETTER STATION NAME
      COMMON /C/ JNET   (MAXSTA)	!2-LETTER SEISMIC NET CODE
      COMMON /C/ JCOMP1 (MAXSTA)	!1-LETTER COMPONENT CODE
      COMMON /C/ JCOMP3 (MAXSTA)	!3-LETTER COMPONENT CODE
      COMMON /C/ JCOMPA (MAXSTA)	!ALTERNATIVE 3-LETTER COMPONENT CODE
      COMMON /C/ JSLOC  (MAXSTA)	!2-LETTER LOCATION CODE
      COMMON /C/ JSLOC2 (MAXSTA)	!EQUIVALENT LOCATION CODE ('  '='--')
      COMMON /D/ LLOC2			!T IF 2ND LOCATION CODE MATCHED
      COMMON /D/ JLATD (MAXSTA)		!LATITUDE (DEG)
      COMMON /D/ JLATM (MAXSTA)		!LATITUDE (IN .01 MIN)
      COMMON /D/ JLOND (MAXSTA)		!LONGITUDE (DEG)
      COMMON /D/ JLONM (MAXSTA)		!LONGITUDE (IN .01 MIN)
      COMMON /D/ JPER (MAXSTA) 		!STANDARD PERIOD (PERIOD IN SEC * 10)
      COMMON /D/ JTYPE (MAXSTA)		!RESPONSE TYPE 0=WA 1,3=L4C 4=HS1 ETC
      COMMON /D/ JCAL (MAXSTA)		!STATION CAL FACTOR *1000
      COMMON /D/ JCEXP (MAXSTA)		!CAL EXPIRATION DATE (IN YMDH)
      COMMON /D/ JLMOD (MAXSTA)		!T TO USE ALTERNATE MODEL FOR THIS STA
      COMMON /D/ JFCOR (MAXSTA)		!DUR MAG CORRECTION *100
      COMMON /D/ JFEXP (MAXSTA)		!FCOR EXPIRATION DATE (IN YMDH)
      COMMON /D/ JXCOR (MAXSTA)		!AMP MAG CORRECTION *100
      COMMON /D/ JXEXP (MAXSTA)		!XCOR EXPIRATION DATE (IN YMDH)
      COMMON /D/ JPSWT (MAXSTA)		!P & S STATION WEIGHTS
      COMMON /D/ JXWT (MAXSTA)		!AMPLITUDE MAGNITUDE WEIGHT *10
      COMMON /D/ JFWT (MAXSTA)		!DURATION MAGNITUDE WEIGHT *10
      COMMON /D/ JFGWT (MAXSTA)		!DURATION GAIN CORRECTION WEIGHT *1
      COMMON /D/ JPD (LM,MAXSTA)	!P DELAYS IN .01 SEC
      COMMON /C/ STRMK (MAXSTA)		!1-LETTER STATION REMARK
C--THE FOLLOWING 4 VARIABLES DEPEND ON COMP TYPE 
      COMMON /D/ JFM1 (MAXSTA)		!USE THIS STATION FOR 1ST DUR MAG
      COMMON /D/ JFM2 (MAXSTA)		!USE THIS STATION FOR 2ND DUR MAG
      COMMON /D/ JXM1 (MAXSTA)		!USE THIS STATION FOR 1ST AMP MAG
      COMMON /D/ JXM2 (MAXSTA)		!USE THIS STATION FOR 2ND AMP MAG
      COMMON /D/ JPM1 (MAXSTA)		!USE THIS STATION FOR 1ST P AMP MAG
      COMMON /D/ JPM2 (MAXSTA)		!USE THIS STATION FOR 2ND P AMP MAG

C************** DATA FOR A PARTICULAR EVENT *********************
C--DATA PRESENT FOR EACH STATION REPORTING AN EVENT (SUBSCRIPT K)
C  USE 2-BYTE INTEGERS WHERE POSSIBLE
      PARAMETER (MAXPHS=1500)	!MAX NO. OF STATIONS FOR EACH EVENT
      CHARACTER KPRK*3,KSRK*2,KRMK*1,KSOU*1,KDEV*3,KRMK6*6,PARMK*1
C      CHARACTER KPSWT*1
      CHARACTER KSHAD*104	!A MAX LENGTH OF 104 IS TESTED IN HYPHS	
      COMMON /D/ KSTA		!NO. OF STATIONS (PHASE CARDS) FOR THIS EVENT
C				  KSTA <= MAXPHS
      COMMON /D/ KINDX (MAXPHS)		!POINTER TO STATION IN LOOKUP TABLE
      COMMON /D/ KP (MAXPHS)		!P TIME IN .01"
      COMMON /D/ KS (MAXPHS)		!S TIME IN .01"
      COMMON /D/ DIS (MAXPHS)		!DISTANCE TO EPICENTER IN KM
      COMMON /D/ KAZEM (MAXPHS)		!CONDENSED AZIMUTH & EMERGENCE ANGLE
      COMMON /D/ KWT (MAXPHS)		!CONDENSED ASSIGNED P & S WEIGHT CODES
      COMMON /D/ KFWT (MAXPHS)		!ASSIGNED DUR MAGNITUDE WEIGHT CODE
      COMMON /D/ KXWT (MAXPHS)		!ASSIGNED AMP MAGNITUDE WEIGHT CODE
      COMMON /D/ KPAWT (MAXPHS)		!ASSIGNED P AMP MAG WEIGHT CODE
      COMMON /D/ PAWT (MAXPHS)		!P AMP MAGNITUDE WEIGHT
      COMMON /D/ KFMP (MAXPHS)		!CODA DURATION IN SECONDS
      COMMON /D/ KXMAG (MAXPHS)		!CALCULATED AMP MAG FOR THIS STA *100
      COMMON /D/ KFMAG (MAXPHS)		!CALCULATED DUR MAG FOR THIS STA *100
      COMMON /D/ KPMAG (MAXPHS)		!CALCULATED P MAG FOR THIS STA *100
      COMMON /D/ AMPK (MAXPHS)		!MAX PEAK-TO-PEAK AMP IN MM OR COUNTS
      COMMON /D/ KAMPU (MAXPHS)		!AMP UNITS CODE 0=PP-MM 1=0-P 2=COUNTS
      COMMON /D/ KAMPTYP (MAXPHS)	!AMP TYP 0=? 1=WA 2=VEL 3=ACCEL 4=NOUSE
      COMMON /D/ KIMTYP (MAXPHS)	!INST TYP 0=NOMAG 1=ML 2=MX
      COMMON /D/ KPAMP (MAXPHS)		!PP P AMP IN COUNTS, 1ST 3 PEAKS
      COMMON /D/ KPER (MAXPHS)		!PERIOD AT AMP READING IN .01 SEC.
      COMMON /D/ KCAL (MAXPHS)		!CAL FACTOR FOR THIS AMP ONLY IN .01
      COMMON /C/ KPRK (MAXPHS)		!P REMARK INCL FIRST MOTION
      COMMON /C/ KSRK (MAXPHS)		!S REMARK
      COMMON /C/ PARMK (MAXPHS)		!P AMPLITUDE DESCRIPTOR
      COMMON /D/ PNORM (MAXPHS)		!PMAG L1 NORM FOR STA TB MINIMIZED
      COMMON /D/ PNORM2 (MAXPHS)	!PMAG2 L1 NORM FOR STA TB MINIMIZED
      COMMON /C/ KRMK (MAXPHS)		!ANY 1 LETTER STATION REMARK
      COMMON /C/ KRMK6 (MAXPHS)		!ORIGINAL 6-LETTER REMARK
      COMMON /C/ KSHAD (MAXPHS)		!OPTIONAL SHADOW PHASE RECORD
      COMMON /D/ KLSHA (MAXPHS)		!LENGTH OF KSHAD
      COMMON /C/ KSOU (MAXPHS)		!1-LETTER DATA SOURCE CODE
      COMMON /C/ KDEV (MAXPHS)		!3-LETTER DIGITIZER DEVICE CODE

C--DATA PRESENT FOR EACH PHASE (P OR S) REPORTED FOR THIS EVENT (SUBSCRIPT IM)
      PARAMETER (MMAX=1200)		!MAX NO. OF PHASES (P OR S) PER EVENT
      COMMON /D/ M			!NO. OF ARRIVAL TIMES FOR THIS EVENT
      COMMON /D/ IND (MMAX)		!POINTER TO STATION IN TABLE & S FLAG
      COMMON /D/ A(MMAX,5)		!TRAVEL TIMES & DERIVATIVES
      COMMON /D/ R (MMAX)		!CURRENT TRAVEL TIME RESIDUAL
      COMMON /D/ W (MMAX)		!COMPUTED WEIGHT OF READING
      COMMON /D/ IMPORT (MMAX)		!COMPUTED IMPORTANCE OF READING IN .001
      COMMON /D/ MTCAL (MMAX)		!CALC TT IN .01. MULT S TIMES BY POS
C--THESE VALUES ARE PASSED BY THE ARRAY A.
C 1 AIN	!ANGLE OF EMERGENCE AT THE SOURCE, MEASURED IN DEGREES FROM NADIR.
C 2 T		!TRAVEL TIME
C 3 DTDR	!TT DERIVATIVE WRT DISTANCE
C 4 DTDZ	!TT DERIVATIVE WRT DEPTH

C--HYPOCENTER DATA FOR THIS EVENT
      COMMON /D/ KYEAR			!TWO DIGIT YEAR
      COMMON /D/ KYEAR2			!4 DIGIT YEAR
      COMMON /D/ KMONTH			!MONTH
      COMMON /D/ KDAY			!DAY
      COMMON /D/ KHOUR			!HOUR
      COMMON /D/ KMIN			!MINUTE
      COMMON /D/ T1			!CURRENT (OR TRIAL) ORIGIN TIME, SEC
      COMMON /D/ Z1			!CURRENT (OR TRIAL) DEPTH
      COMMON /D/ CLAT			!CURRENT (OR TRIAL) LAT, DECIMAL DEGREES
      COMMON /D/ CLON			!CURRENT (OR TRIAL) LON, DECIMAL DEG, POS WEST
      COMMON /D/ LAT			!EVENT LAT, DEG
      COMMON /D/ XLTM			!EVENT LAT, MIN
      COMMON /D/ LON			!EVENT LON, DEG
      COMMON /D/ XLNM			!EVENT LON, MIN

C--ERROR DATA FOR THIS EVENT
      COMMON /D/ ERH			!HORIZONTAL LOCATION ERROR
      COMMON /D/ ERZ			!DEPTH ERROR
      COMMON /D/ SERR(3)		!MAGNITUDES OF 3 STD. ERRORS
      COMMON /D/ IAZ(3)			!AZIMUTHS OF 3 STD. ERRORS
      COMMON /D/ IDIP(3)		!DIPS OF 3 STD. ERRORS

C--FIRST MAGNITUDE DATA FOR EVENT
      CHARACTER BMTYP*1,BMTYPX*1
      COMMON /D/ FMAG			!MEDIAN DURATION MAG
      COMMON /D/ XMAG			!MEDIAN AMPLITUDE MAG
C      COMMON /D/ MFMAG			!TOTAL OF DURATION MAG WEIGHTS *100
      COMMON /D/ NFMAG			!NUMBER OF DURATION MAGS W/ WEIGHTS>0
C      COMMON /D/ MXMAG			!TOTAL OF AMPLITUDE MAG WEIGHTS *100
      COMMON /D/ NXMAG			!NUMBER OF AMPLITUDE MAGS W/ WEIGHTS>0
      COMMON /D/ FMMAD			!MEDIAN ABSOL DIFFERENCE OF DURATION MAGS
      COMMON /D/ XMMAD			!MEDIAN ABSOL DIFFERENCE OF AMPLITUDE MAGS
      COMMON /D/ BMAG			!PASS THRU (BERKELEY) MAGNITUDE
C      COMMON /D/ MBMAG			!TOTAL OF PASS THRU MAG WEIGHTS *100
      COMMON /D/ NBMAG			!NUMBER OF PASS THRU MAGS W/ WEIGHTS>0
      COMMON /C/ BMTYP			!PASS THRU MAGNITUDE LABEL CODE
      COMMON /D/ BMAGX			!PASS THRU X-MAGNITUDE
C      COMMON /D/ MBMAGX		!NUMBER OF PASS THRU XMAG READINGS *100
      COMMON /D/ NBMAGX			!NUMBER OF PASS THRU XMAG READINGS
      COMMON /C/ BMTYPX			!PASS THRU X-MAGNITUDE LABEL CODE

C--SECOND MAGNITUDE DATA FOR EVENT
      COMMON /D/ FMAG2		!MEDIAN DURATION MAG
      COMMON /D/ XMAG2		!MEDIAN AMPLITUDE MAG
C      COMMON /D/ MFMAG2	!TOTAL OF DURATION MAG WEIGHTS *100
      COMMON /D/ NFMAG2		!NUMBER OF DURATION MAGS W/ WEIGHTS>0
C      COMMON /D/ MXMAG2	!TOTAL OF AMPLITUDE MAG WEIGHTS *100
      COMMON /D/ NXMAG2		!NUMBER OF AMPLITUDE MAGS W/ WEIGHTS>0
      COMMON /D/ FMMAD2		!MEDIAN ABSOL DIFFERENCE OF DURATION MAGS
      COMMON /D/ XMMAD2		!MEDIAN ABSOL DIFFERENCE OF AMPLITUDE MAGS

C--MISCELLANEOUS DATA FOR THIS EVENT
      CHARACTER REMK*3,RMK1*1,RMK2*1,FULNAM*25,SHADO*95,SHAD1*95
      CHARACTER*1 SOUCOD,FMSOU,XMSOU,FMSOU2,XMSOU2
      COMMON /D/ RMS		!RMS RESIDUAL AFTER RESIDUAL WEIGHTING
      COMMON /D/ RMSWT		!RMS RESIDUAL BEFORE RESIDUAL WEIGHTING
      COMMON /C/ REMK		!3-LETTER REMARK BASED ON LOCATION
      COMMON /C/ RMK1		!FIRST 1-LETTER AUXILIARY REMARK
      COMMON /C/ RMK2		!SECOND 1-LETTER AUXILIARY REMARK
      COMMON /C/ FULNAM		!FULL 25-LETTER REGION NAME
      COMMON /D/ DONE		!FLAG SET WHEN EVENT IS LOCATED
      COMMON /D/ V(4,4)		!EIGENVECTOR MATRIX
      COMMON /D/ EIGVAL(4)	!EIGENVALUES
      COMMON /D/ COVAR(4,4)	!COVARIANCE MATRIX
      COMMON /D/ IDNO		!EVENT ID NO. INPUT FOR EVENT
      COMMON /D/ MEMDSK		!0=GET CUSP EVENT FROM MEMORY 1=DISK (LOC)
      COMMON /D/ INUM		!EVENT NO. WITHIN THIS LOCATION RUN
      COMMON /D/ LENSHA		!LENGTH OF OPTIONAL REC AFTER TERMINATOR CARD
      COMMON /C/ SHADO		!OPTIONAL RECORD AFTER TERMINATOR CARD

      PARAMETER (MSHA=4)	!MAX NUMBER OF SHADOW RECS AFTER HEADER
      COMMON /D/ NSHA1		!ACTUAL NUMBER OF SHADOW RECS AFTER HEADER
      COMMON /D/ LSHA1 (MSHA)	!LENGTH OF OPTIONAL REC AFTER ARCHIVE HEADER
      COMMON /C/ SHAD1 (MSHA)	!OPTIONAL RECORD AFTER ARCHIVE HEADER
      COMMON /D/ ITR		!CURRENT ITERATION NUMBER
      COMMON /D/ DMIN		!DISTANCE TO CLOSEST STATION
      COMMON /D/ KEND		!PROGRAM TERMINATION FLAG
      COMMON /D/ MAXGAP		!MAXIMUM AZIMUTHAL GAP
      COMMON /D/ NFRM		!NUMBER OF FIRST MOTIONS
      COMMON /D/ NWR		!NUMBER OF WEIGHTED READINGS (P & S)
      COMMON /D/ NWS		!NUMBER OF WEIGHTED S READINGS
      COMMON /D/ NVR		!NUMBER OF VALID READINGS (>0 ASSIGNED WEIGHTS)
      COMMON /D/ RR		!LENGTH OF ADJUSTMENT VECTOR IN KM
      COMMON /C/ SOUCOD		!MOST COMMON PHASE SOURCE CODE
      COMMON /C/ FMSOU		!MOST COMMON 1ST FMAG SOURCE CODE
      COMMON /C/ XMSOU		!MOST COMMON 1ST XMAG SOURCE CODE
      COMMON /C/ FMSOU2		!MOST COMMON 2ND FMAG SOURCE CODE
      COMMON /C/ XMSOU2		!MOST COMMON 2ND XMAG SOURCE CODE

C***************** PARAMETERS WHICH APPLY TO ALL EVENTS ******************
C--THE DEFINING COMMANDS ARE GIVEN IN PARENTHESES

CC--STATION DELAY PARAMETERS (DLY) [MADE OBSOLETE BY MULTIPLE MODEL OPTION]
C	COMMON /D/ KDLY	!DELAY MODEL CONTROL: 1=MODEL 1, 2=MODEL 2, 3=LINE
CC			 SEPARATES 2 MODELS, 4=CIRCLE SEPS. 2 MODELS.
C	COMMON /D/ DLYBAL	!FACTOR WHICH BALENCES BETWEEN 2 DELAY MODELS
CC			  (0 FOR MOD1, 1 FOR MOD2)
C	COMMON /D/ DLYAZ	!AZIMUTH OF LINE OR RADIUS OF CIRCLE WHICH
CC			  SEPARATES THE 2 DELAY MODELS
C	COMMON /D/ DLYWD	!WIDTH OF TRANSITION ZONE BETWEEN MODELS
C	COMMON /D/ DLYLON	!LON OF CIRCLE CENTER OR POINT ON LINE
C	COMMON /D/ DLYLAT	!LAT OF CIRCLE CENTER OR POINT ON LINE
CC                         BOTH IN DECIMAL DEGREES, POS N & W

C--TERMINATING LOCATION UPON CONVERGENCE (CON)
      COMMON /D/ ITRLIM		!MAXIMUM NUMBER OF ITERATIONS
      COMMON /D/ DQUIT		!STOP WHEN HYPO ADJUSTMENT IS < THIS
      COMMON /D/ DRQT		!STOP WHEN CHANGE IN RMS < THIS
      COMMON /D/ MINSTA		!MIN NO. OF PHASE CARDS TO ATTEMPT LOC (MIN)

C--ITERATION & DAMPING CONTROLS (DAM)
      COMMON /D/ DXFIX		!KEEP DEPTH FIXED UNTIL ADJUST IS < THIS
      COMMON /D/ DZMAX		!DAMP DEPTH ADJUST IF > THIS
      COMMON /D/ DXMAX		!DAMP DISTANCE ADJUST IF > THIS
      COMMON /D/ D2FAR		!STOP WHEN STA-DIST(2) > THIS
      COMMON /D/ DZAIR		!FRACTION OF DEPTH ADJ. IF QUAKE GOES INTO AIR
      COMMON /D/ DAMP		!DAMPING FACTOR FOR ALL HYPO ADJUSTMENTS
      COMMON /D/ EIGTOL		!MIN EIGENVALUE REQUIRED FOR ADJUSTMENT
      COMMON /D/ RBACK		!LIMIT ON RMS INCREASE FOR ONE ITERATION
      COMMON /D/ BACFAC		!FRACTION OF STEP TO MOVE BACK IF 
C				  INCREASE IN RMS > RBACK

C--DURATION MAG CONSTANTS 
      LOGICAL LATEN,LCOWT, LNOFMC,LNOXMC
      CHARACTER CLA0*3, CDUG*3
C     INTEGER*4 ICDATE,IFDATE,IXDATE	!LONGER LENGTH REQUIRED
      COMMON /D/ LATEN		!TRUE TO CONVERT STA ATTEN TO CAL FACTOR (ATN)
      COMMON /D/ FMA1,FMB1,FMZ1,FMD1,FMF1	!LOW DUR MAG CONSTANTS (DUR)
      COMMON /D/ FMA2,FMB2,FMZ2,FMD2,FMF2 	!HIGH DUR MAG CONSTANTS (DUR)
      COMMON /D/ FMBRK		!DURATION SEPARATING LO & HI CONSTANTS (DUR)
      COMMON /D/ FMGN		!COEFF OF GAIN CORR TO OLD FMAGS (DUR)
      COMMON /D/ IDUG		!NUMBER OF GAIN CORR COMPS, -1=ALL (DUG)
      COMMON /C/ CDUG(10)	!COMPONENTS TO CORRECT (DUG)
      COMMON /D/ DCOFM1		!COEFF OF ADDL LINEAR DIST TERM (DU2)
      COMMON /D/ DBRKM1		!START DIST OF ADDL LINEAR DIST TERM (DU2)
      COMMON /D/ DCOFM2		!COEFF OF ADDL LINEAR DIST TERM (DU2)
      COMMON /D/ DBRKM2		!START DIST OF ADDL LINEAR DIST TERM (DU2)
      COMMON /D/ ZCOFM		!COEFF OF ADDL LINEAR DEPTH TERM (DU2)
      COMMON /D/ ZBRKM		!START DEPTH OF ADDL LINEAR DEPTH TERM (DU2)
      COMMON /D/ FMA1B,FMB1B,FMZ1B,FMD1B,FMF1B	!LOW 2nd DUR MAG (DUB)
      COMMON /D/ FMA2B,FMB2B,FMZ2B,FMD2B,FMF2B	!HIGH 2nd DUR MAG (DUB)
      COMMON /D/ FMBRKB		!2ND DURATION SEPARATING LO & HI CONSTS (DUB)
      COMMON /D/ FMGNB		!COEFF OF GAIN CORR FOR 2nd FMAGS (DUB)
      COMMON /D/ MAGSEL		!FMAG1: 1=CODA MAGS 2=TAU MAGS 3=2nd CODA (MAG)
      COMMON /D/ MAGSL2		!FMAG2: 1=CODA MAGS 2=TAU MAGS 3=2nd CODA (MAG)
      COMMON /D/ LCOWT		!T=USE CODA WEIGHTS F=IGNORE CODA WEIGHTS (MAG)
      COMMON /D/ MLOGA0		!1=USE EATON LOGA0 RELATION 2=BAKUN & JOYNER(MAG
C				!3=RICHTER 4=BKY'S NORDQUIST (MAG)
      COMMON /D/ NLA0		!NUMBER OF COMPONENTS W/ UNIQUE LOGA0 REL.(LA0)
      COMMON /C/ CLA0(20)	!COMPONENT CODES FOR UNIQUE LOGA0 (LA0)
      COMMON /D/ MLA0(20)	!LOGA0 # (LIKE MLOGA0) FOR UNIQUE LOGA0S (LA0)
      COMMON /D/ DMA0		!CONSTANT IN TAU MAG (TAU)
      COMMON /D/ DMA1		!COEFF. OF LOG(TAU) IN TAU MAG (TAU)
      COMMON /D/ DMA2		!COEFF. OF LOG(TAU)**2 IN TAU MAG (TAU)
      COMMON /D/ DMLI		!COEFF. OF TAU IN TAU MAG (TAU)
      COMMON /D/ DMZ		!COEFF. OF DEPTH IN TAU MAG (TAU)
      COMMON /D/ DMGN		!COEFF. OF GAIN/ATTEN IN TAU MAG (TAU)

C--DURATION & AMPLITUDE MAGNITUDE CORRECTIONS
      CHARACTER CFCM*3,CXCM*3
      COMMON /D/ ICDATE		!INITIAL DATE FOR EXTRACTING ATTENS (ATE)
      COMMON /D/ IFDATE		!INITIAL DATE FOR EXTRACTING FMCORS (FMC)
      COMMON /D/ IXDATE		!INITIAL DATE FOR EXTRACTING XMCORS (XMC)
      COMMON /D/ LNOFMC		!T TO USE STA W/ NO CORR, F TO OMIT (FMC)
      COMMON /D/ LNOXMC		!T TO USE STA W/ NO CORR, F TO OMIT (XMC)
      COMMON /D/ CALSV(0:13)	!CAL FACTORS FOR EACH ATTENUATION SETTING
      COMMON /D/ NFCM		!# OF COMPONENTS TO CORRECT FMAGS (FCM)
      COMMON /C/ CFCM(10)	!3-LETTER COMPONENTS TO CORRECT (FCM)
      COMMON /D/ AFCM(10)	!FMAG CORRECTION FOR COMPONENT (FCM)
      COMMON /D/ NXCM		!# OF COMPONENTS TO CORRECT XMAGS (XCM)
      COMMON /C/ CXCM(10)	!3-LETTER COMPONENTS TO CORRECT (XCM)
      COMMON /D/ AXCM(10)	!XMAG CORRECTION FOR COMPONENT (XCM)

C--SELECTING WHICH COMPONENTS TO USE FOR THE 4 MAGNITUDES
C--ALSO SELECT WHICH INSTRUMENT TYPES TO USE WITH THE 2 AMP MAGS
      CHARACTER*3 COMPF1,COMPF2,COMPX1,COMPX2
      CHARACTER*1 LABF1,LABF2,LABX1,LABX2
      COMMON /D/ NCPF1		!NUMBER OF COMPONENTS TO SELECT FOR FMAG1 (FC1)
      COMMON /C/ COMPF1(20)	!COMPONENTS TO USE FOR FMAG1 (FC1)
      COMMON /C/ LABF1		!LABEL CODE TO IDENTIFY FMAG1 (FC1)
      COMMON /D/ NCPF2		!NUMBER OF COMPONENTS TO SELECT FOR FMAG2 (FC2)
      COMMON /C/ COMPF2(20)	!COMPONENTS TO USE FOR FMAG2 (FC2)
      COMMON /C/ LABF2		!LABEL CODE TO IDENTIFY FMAG2 (FC2)
      COMMON /D/ NCPX1		!NUMBER OF COMPONENTS TO SELECT FOR XMAG1 (XC1)
      COMMON /C/ COMPX1(20)	!COMPONENTS TO USE FOR XMAG1 (XC1)
      COMMON /C/ LABX1		!LABEL CODE TO IDENTIFY XMAG1 (XC1)
      COMMON /D/ NCPX2		!NUMBER OF COMPONENTS TO SELECT FOR XMAG2 (XC2)
      COMMON /C/ COMPX2(20)	!COMPONENTS TO USE FOR XMAG2 (XC2)
      COMMON /C/ LABX2		!LABEL CODE TO IDENTIFY XMAG2 (XC2)
      COMMON /D/ MAG1TYPX	!TYPE OF MAG FOR XMAG1 1=ML 2=MX (XMT)
      COMMON /D/ MAG2TYPX	!TYPE OF MAG FOR XMAG2 1=ML 2=MX (XMT)

C--SELECTING WHICH INSTRUMENT TYPES TO USE FOR THE 2 AMP MAGS
      LOGICAL LXCH
      COMMON /D/ LXCH		!T FOR COMPONENT CHOICE, F INST TYPE CHOICE(XCH)
      COMMON /D/ NXTYP1		!NUMBER OF INST TYPES FOR XMAG1 (XTY)
      COMMON /D/ IXTYP1(3)	!INST TYPES FOR XMAG1 (XTY)
      COMMON /D/ NXTYP2		!NUMBER OF INST TYPES FOR XMAG2 (XTY)
      COMMON /D/ IXTYP2(3)	!INST TYPES FOR XMAG2 (XTY)

C--PREFERRED EVENT MAGNITUDES
C--MAG NUMBER TAGS: 1=FMAG 2=XMAG 3=PASS-THRU-MAG 4=XMAG2 5=FMAG2
      CHARACTER LABPR*1
      COMMON /D/ NMAGS		!NUMBER OF PREFERRED MAGNITUDES (PRE)
      COMMON /D/ MPREF(10)	!PREFERENCE ORDER OF MAG NUMBER TAGS (PRE)
      COMMON /D/ MNPREF(10)	!MINIMUM NUMBER OF READINGS TO USE (PRE)
      COMMON /D/ AMPREF(10)	!MIN MAG TO SELECT PREFERRED MAG (PRE)
      COMMON /D/ AXPREF(10)	!MAX MAG TO SELECT PREFERRED MAG (PRE)
      COMMON /D/ PMAG		!PREFERRED MAG
C      COMMON /D/ MPMAG		!TOTAL WEIGHTS OF PREFERRED MAG *100
      COMMON /D/ NPMAG		!NUMBER OF PREFERRED MAGS W/ WEIGHTS>0
      COMMON /C/ LABPR		!LABEL CODE OF PREFERRED MAG
      COMMON /D/ PMMAD		!MEDIAN-ABS-DIFF OF PREFERRED MAG

C--MISCELLANEOUS PARAMETERS
      LOGICAL LCOMP1, LKEEP
      COMMON /D/ ITRDIS		!ITERATION TO BEGIN DISTANCE WEIGHTING (DIS)
      COMMON /D/ DISCUT,DISW1,DISW2	!DIST. WEIGHT PARAMETERS (DIS)
      COMMON /D/ ITRDI1,DISCU1,DISW11,DISW21	!DIST. WEIGHT PARAMETERS (DI1)
      COMMON /D/ ITRRES		!ITERATION TO BEGIN RESIDUAL WEIGHTING (RMS)
      COMMON /D/ RMSCUT,RMSW1,RMSW2	!RMS WEIGHTING PARAMS. (RMS)
      COMMON /D/ LJUNK		!T TO KILL DIS & RES WEIGHTING WHEN M<4 (JUN)
      COMMON /D/ SWT		!MULTIPLY ALL S WEIGHTS BY THIS (SWT)
      COMMON /D/ POS		!VP/VS VELOCITY RATIO (POS)
      COMMON /D/ ZTR		!TRIAL DEPTH (ZTR)
      COMMON /D/ RDERR		!ESTIMATED READING ERROR IN SECS (ERR)
      COMMON /D/ ERCOF		!WEIGHTING FACTOR OF RMS IN ERROR CALCS (ERC)
      COMMON /D/ NET	!NET FOR ASSIGNING 3-LET. NAMES BASED ON LOCATION (NET)
      COMMON /D/ NSTLET	!NUMBER OF STATION SITE LETTERS TO TEST (LET)
      COMMON /D/ NETLET	!NUMBER OF STATION NET LETTERS TO TEST (LET)
      COMMON /D/ NCOMP	!NUMBER OF COMPONENT LETTERS TO TEST (LET)
      COMMON /D/ NSLOC	!# OF LOCATION CODE LETS TO TEST IN PHASE FILE (LET)
      COMMON /D/ NSLOC2	!# OF LOCATION CODE LETS TO TEST IN OTHER FILES (LET)
      COMMON /D/ LCOMP1	!T= 1 LETTER COMPONENT, F=OTHERWISE (LES)
      COMMON /D/ LKEEP	!T=WRITE UNKNOWN STAS TO ARC FILE (KEP)
      COMMON /D/ WTVALS(4)	!NUMERICAL WEIGHTS FOR CODES 0-3 (WET)

C--PHASE & SHADOW DATA FOR UNKNOWN STATIONS. COMM BETWEEN HYPHS AND HYLST.
      PARAMETER (MAXUNK=40)
      CHARACTER PUNK*132,SUNK*132,LUNK*5
      COMMON /UNK/ NLUNK	!NUMBER OF UNK STAS IN "NO COMPLAIN" LIST (UNK)
      COMMON /UNC/ LUNK(10)	!LIST OF STAS TO NOT COMPLAIN WHEN UNKNOWN (UNK)
      COMMON /UNK/ NUNK	!NUMBER OF UNKNOWN STATIONS THIS EVENT
      COMMON /UNC/ PUNK(MAXUNK)  !PHASE RECORDS OF UNKNOWN STATIONS
      COMMON /UNK/ NPUNK(MAXUNK) !LENGTH OF PHASE RECORDS
      COMMON /UNC/ SUNK(MAXUNK)  !SHADOW RECORDS OF UNKNOWN STATIONS
      COMMON /UNK/ NSUNK(MAXUNK) !LENGTH OF SHADOW RECORDS

C--INTERACTIVE PROCESSING OF SINGLE-EVENT FILES (PRO COMMAND)
      CHARACTER LSTFIL*40,LSTFOR*40
      CHARACTER*8 EXTPHS,EXTARC,EXTSUM,EXTPRT
      COMMON /C/ LSTFIL		!FILE FOR READING BASE EVENT NAMES (BAS)
      COMMON /D/ NCBASE		!NUMBER OF CHARACTERS IN BASE NAME (BAS)
      COMMON /C/ LSTFOR		!FORMAT FOR READING EVENT FILE (BAS)
      COMMON /C/ EXTPHS		!PHASE FILENAME EXTENSION (BAS)
      COMMON /C/ EXTARC		!ARCHIVE FILENAME EXTENSION (BAS)
      COMMON /C/ EXTSUM		!SUMMARY FILENAME EXTENSION (BAS)
      COMMON /C/ EXTPRT		!PRINTOUT FILENAME EXTENSION (BAS)
      COMMON /D/ IEDFLG		!EDITOR FLAG 1=EDT 2=ANY ED (BAS)

C--TRANSLATION ARRAYS FROM 3-LETTER DIGITIZERS TO 1-LETTER DATA SOURCE CODES
      CHARACTER DIG3*3, DIG1*1, DIGDEF*1
      PARAMETER (MAXDIG=50)
      COMMON /C/ DIG3(MAXDIG)	!3-LETTER DIGITIZER CODES IN MEM FILE (DIG)
      COMMON /C/ DIG1(MAXDIG)	!CORRESPONDING 1-LETTER DATA SOURCE CODES (DIG)
      COMMON /C/ DIGDEF		!DEFAULT DATA SOURCE CODE (DID)
      COMMON /D/ NDIG		!NUMBER OF DATA SOURCE CODES (DID)

C--P AMPLITUDE MAGNITUDE PARAMETERS
      LOGICAL LPMAG,LPPRT
      CHARACTER CPWM*3,PSOUR*1,PSOUR2*1, COMPP1*3,COMPP2*3, CCNTMM*1
      CHARACTER LABP1*1,LABP2*1	
      COMMON /D/ LPMAG		!T= COMPUTE PMAG, MUST HAVE SHADOW INPUT (PMA)
      COMMON /D/ LPPRT		!T= ADD PMAG STA. LINES TO PRINT OUTPUT (PMA)
      COMMON /D/ CNT2MD		!DEVEL. MM PER COUNT (P AMPS) RTP=.04 (PMA)
      COMMON /D/ CLPRAT		!RATIO OF PMCLIP/PMUSED FOR CLIPPED MAG (PMA)
      COMMON /D/ LATYPP		!LOGA0 FORMULA # FOR PMAGS (PMA)
      COMMON /D/ NPWM		!NUMBER OF COMPONENTS WITH PAMAG WEIGHTS (PAC)
      COMMON /C/ CPWM(10)	!COMPONENTS WITH DEFINED WEIGHTS (PAC)
      COMMON /D/ WPWM(10)	!WEIGHTS FOR EACH COMPONENT (PAC)

C--P AMP MAGNITUDE COMPONENT AND DATA SOURCE DEPENDENT PARAMETERS
      COMMON /C/ LABP1		!LABEL OF PRIMARY P AMP MAGNITUDE (PC1)
      COMMON /D/ PMA1		!COEFFICIENT IN PMAG = PMA1 +PMB1*LOGA (PC1)
      COMMON /D/ PMB1		!COEFFICIENT IN PMAG = PMA1 +PMB1*LOGA (PC1)
      COMMON /D/ NCPP1		!NUMBER OF COMPONENTS TO USE FOR PAMAG (PC1)
      COMMON /C/ COMPP1(10)	!COMPONENT CODES FOR PRIMARY P MAG (PC1)
      COMMON /C/ LABP2		!LABEL OF SECONDARY P AMP MAGNITUDE (PC2)
      COMMON /D/ PMA2		!COEFFICIENT IN PMAG = PMA2 +PMB2*LOGA (PC1)
      COMMON /D/ PMB2		!COEFFICIENT IN PMAG = PMA2 +PMB2*LOGA (PC1)
      COMMON /D/ NCPP2		!NUMBER OF COMPONENTS TO USE FOR PAMAG2 (PC2)
      COMMON /C/ COMPP2(10)	!COMPONENT CODES FOR SECONDARY P MAG (PC2)
      COMMON /D/ NCNTMM		!NUMBER OF SOURCE SPECIFIC CONV FACTORS (PMC)
      COMMON /C/ CCNTMM(10)	!DATA SOURCE CODES (PMC)
      COMMON /D/ CNT2MM(10)	!COUNTS TO MM CONVERSION FOR SOURCE (PMC)

C--PRIMARY P AMPLITUDE MAGNITUDE
      COMMON /D/ PAMAG		!PRIMARY P AMPLITUDE MAGNITUDE FOR EVENT
      COMMON /D/ PAMAD		!MEDIAN ABSOLUTE DIFFERENCE OF PMAG
      COMMON /D/ PMUSED		!TOTAL OF WEIGHTS OF STATIONS USED IN PMAG
      COMMON /D/ PMCLIP		!TOT OF WEIGHTS OF CLIPPED STAS USED IN PMAG
      COMMON /D/ MINPM		!PMAG CLIP FLAG =1 OF PMAG IS A MIN, =0 OK
      COMMON /D/ PNORMN		!L1 NORM FOR EVENT P MAG
      COMMON /C/ PSOUR		!PRINCIPAL DATA SOURCE FOR P MAGS

C--SECONDARY P AMPLITUDE MAGNITUDE (IE LOW GAIN)
      COMMON /D/ PAMAG2		!SECONDARY P AMPLITUDE MAGNITUDE FOR EVENT
      COMMON /D/ PAMAD2		!MEDIAN ABSOLUTE DIFFERENCE OF PMAG
      COMMON /D/ PMUSD2		!TOTAL OF WEIGHTS OF STATIONS USED IN PMAG
      COMMON /D/ PMCLP2		!TOT OF WEIGHTS OF CLIPPED STAS USED IN PMAG
      COMMON /D/ MINPM2		!PMAG CLIP FLAG =1 OF PMAG IS A MIN, =0 OK
      COMMON /D/ PNRMN2		!L1 NORM FOR EVENT P MAG
      COMMON /C/ PSOUR2		!PRINCIPAL DATA SOURCE FOR P MAGS
