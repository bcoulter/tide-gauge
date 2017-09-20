* Create CSV for date, time etc from NMEA file
SET TALK OFF
CLOSE ALL
SET DEFAULT TO .
SET date TO british
SET cent on
SET SAFETY off
CLEAR
filename = '                                          '
filename = LOCFILE("*.txt")+'                       '
filecsv5 = left(filename,LEN(ALLTRIM(filename))-4)+"-5.CSV                   "
filecsv6 = left(filename,LEN(ALLTRIM(filename))-4)+"-6.CSV                   "
filexls5 = left(filename,LEN(ALLTRIM(filename))-4)+"-5.XLS                   "
filexls6 = left(filename,LEN(ALLTRIM(filename))-4)+"-6.XLS                   "
filecsv = left(filename,LEN(ALLTRIM(filename))-4)+".CSV                   "
filexls = left(filename,LEN(ALLTRIM(filename))-4)+".XLS                   "
grain = -99 && Dont record anything unless step is greater than this    
offset = 0.10 && Amount to be added to depth for sounder
depthtol = -1 && Depths of this or below will not be included in output
maxvelo = 15
ndepth = 0
m.easting = 0
peasting = 0
m.northing = 0
pnorthing = 0
ptime = '00:00:00'
ii = 0
DIMENSION arg(20)

earliest = DATE() && To get date for SQL tide download
days = 0 && Days to go back for tide records

*@ 5,5 say "Name of NMEA Data File:                 " get filename function 'K' 
*@ 7,5 say "Grain size for recorded data:           " get grain function 'K' 
*@ 9,5 say "Sounder offset to be added to depth (m):" get offset function 'K' 
*@ 11,5 say "Minimum depth for inclusion in output  :" get depthtol function 'K' 
*@ 13,5 say "Maximum velocity for inclusion in output  :" get maxvelo function 'K' 
*@ 15,5 say "Name of CSV Output File:                 " get filecsv function 'K' when precalc()
*READ
resulin = FOPEN(filename,0)
IF resulin<=0 then
   WAIT "Invalid Input Filename or File Already Open "+filename wind
   exit
ENDIF
lin = "Start"
* recline = "Date,Time,Latitude,Longitude,Depth_m"
   USE depths IN 0 EXCLUSIVE
   zap
   mdate = 0
   mhour = ""
   mlatitude = 0
   mlongitude = 0
   mdepth = 0
   nline = 0
   lincount = 0
   validlin = 0
   depthcount = 0
   rmc = .F.
   dbt = .f. 

* =============================================================
* Main NMEA sentence reading section
morefiles = .t.
DO WHILE morefiles
* Keep going as long as there are more daily NMEA files to read
DO WHILE resulin > 0 AND !FEOF(resulin)
   lin = FGETS (resulin,255)
   = csvsplit() && Divide csv into a set of arguments
   lincount = lincount+1
   checkok = .f.
   IF sumcheck()
      validlin = validlin + 1
      checkok = .t.
   endif
   DO CASE 
      CASE arg(1) = "$GPRMC" AND arg(3) = "A" AND checkok
         * === RMC - Recommended Minimum Navigation Information ===
         *                                                           12
         *        1         2 3       4 5        6  7   8   9    10 11|  13
         *        |         | |       | |        |  |   |   |    |  | |   |
         * $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,xxxx,x.x,a,m,*hh e.g.
         * $GPRMC,165422,V,5218.9280,N,00631.9940,W,4.4,180.0,291212,3.8,W*63
         * 1) Time (UTC)
 		 * 2) Status, A is ok, V = Navigation receiver warning
		 * 3) Latitude
		 * 4) N or S
		 * 5) Longitude
		 * 6) E or W
		 * 7) Speed over ground, knots
		 * 8) Track made good, degrees true
		 * 9) Date, ddmmyy
		* 10) Magnetic Variation, degrees
		* 11) E or W
		* 12) Checksum

         rmc = .t.
         gprmc = lin
         nline = nline + 1
         IF arg(2) ='173740' OR arg(2) = '174722' then
            xx = 0 && Breakpoint opportunity
         ENDIF
         ndate = CTOD(SUBSTR(arg(10),1,2)+"/"+SUBSTR(arg(10),3,2)+"/"+SUBSTR(arg(10),5,2))
         earliest = IIF(ndate <> {} and ndate< earliest, ndate, earliest) && Find earliest date of gps/depth records 
         nhour = SUBSTR(arg(2),1,2)+":"+SUBSTR(arg(2),3,2)+":"+SUBSTR(arg(2),5,2)
	     nclatitude = SUBSTR(arg(4),1,2)+" "+SUBSTR(arg(4),3,7)+" "+arg(5)
	     nclongitude = SUBSTR(arg(6),1,3)+" "+SUBSTR(arg(6),4,7)+" "+arg(7)
	     nlatitude = VAL(SUBSTR(arg(4),1,2))+VAL(SUBSTR(arg(4),3,7))/60
	     nlongitude = -VAL(SUBSTR(arg(6),1,3))-VAL(SUBSTR(arg(6),4,7))/60
      CASE arg(1) = "$SDDBT" AND LEN(arg(2)) > 0 AND checkok
         * === DBT - Depth below transducer ===
         *        1   2 3   4 5   6 7
         *        |   | |   | |   | |
         * $SDDBT,x.x,f,x.x,M,x.x,F*hh<CR><LF>
         * 1) Depth, feet
         * 2) f = feet
         * 3) Depth, meters
         * 4) M = meters
         * 5) Depth, Fathoms
         * 6) F = Fathoms
         * 7) Checksum

         dbt = .t.
         IF LEN(arg(2)) = 0 then
            ndepth = -99
         ELSE
            if VAL(arg(2)) < 10 then
				ndepth = VAL(arg(2)) * 0.3048 + offset && Now uses depth feet * 0.3048 as more accurate than meters
			ELSE 	
            	ndepth = VAL(arg(4)) + offset && Now uses depth meters
            ENDIF	
         ENDIF
      OTHERWISE
         lin = ""
    ENDCASE   
    IF rmc AND dbt && Had time/position and depth sentences
       = geonatgrid (nlatitude,nlongitude)
       step = ((m.easting - peasting)^2 + (m.northing - pnorthing)^2)^.5 && Calculate increment in metres
       velocity = step / ((VAL(left(nhour,2))*3600+VAL(subst(ALLTRIM(nhour),4,2))*60+VAL(subst(ALLTRIM(nhour),7,2)))-(VAL(left(ptime,2))*3600+VAL(subst(ALLTRIM(ptime),4,2))*60+VAL(subst(ALLTRIM(ptime),7,2))))
       IF !EMPTY(lin) AND step >= grain AND !EMPTY(ndate) then
          INSERT INTO depths (obsdate,obstime,clatitude,clongitude,latitude,longitude,easting,northing,depth_uncr,depth_m,neg_depth,speed) ;
           VALUES (ndate,nhour,nclatitude,nclongitude,nlatitude,nlongitude,m.easting,m.northing,ndepth,0,0,velocity)
          mlatitude = nlatitude
          mlongitude = nlongitude
          mdepth = ndepth
          peasting = m.easting
          pnorthing = m.northing
          ptime = nhour
       ENDIF
       rmc = .f.
       dbt = .f.
       depthcount = depthcount + 1
    ENDIF 
ENDDO
= FCLOSE(resulin)
WAIT "More files to read (Y/N)?" WINDOW AT 8,40 TO morefiles
IF UPPER(morefiles) = 'Y'
   morefiles = .t.
   filename = LOCFILE("*.txt")+'                       '
   resulin = FOPEN(filename,0)
ELSE 
   morefiles = .f.
ENDIF
ENDDO
* ================================================================= 
 
* Populate date_time rounded to 6 minute intervals
* REPLACE ALL date_time WITH DATETIME(val(RIGHT(DTOC(obsdate),4)),VAL(SUBSTR(DTOC(obsdate),4,2)),VAL(LEFT(DTOC(obsdate),2)),VAL(left(obstime,2)),ROUND(VAL(subst(ALLTRIM(obstime),4,2))*60+VAL(subst(ALLTRIM(obstime),7,2))/360,0)/60)
* Make two Excel files of complete data
COPY TO (filecsv) TYPE csv
COPY TO (filexls) TYPE xl5
GO TOP 
* Get rid of fishy outliers by insisting that no row depth is more than .2m from the one above
pdepth = depth_uncr
SCAN WHILE !EOF()
	IF ABS(depth_uncr - pdepth) < 0.2
		REPLACE NEXT 1 depth_m WITH depth_uncr
		pdepth = depth_uncr
	ELSE
		REPLACE NEXT 1 depth_m WITH pdepth
	ENDIF 
ENDSCAN

* Now prepare 5 min averages and 6 minute averages
GO TOP 
SCAN WHILE !EOF()
 dattim = DATETIME(val(RIGHT(DTOC(obsdate),4)),VAL(SUBSTR(DTOC(obsdate),4,2)),VAL(LEFT(DTOC(obsdate),2)),VAL(left(obstime,2)),VAL(subst(ALLTRIM(obstime),4,2)),VAL(subst(ALLTRIM(obstime),7,2)))
 dattim = dattim + IIF(MOD(MINUTE(dattim)*60+SEC(dattim),300) >= 150,300-MOD(MINUTE(dattim)*60+SEC(dattim),300),-MOD(MINUTE(dattim)*60+SEC(dattim),300))
 REPLACE NEXT 1 date_time WITH DATTIM
ENDSCAN
COPY TO (filecsv5) TYPE csv
SELECT date_time,avg(depth_m) as depth_5m FROM depths GROUP BY date_time 
COPY TO (filexls5) TYPE xl5

SELECT depths
GO TOP 
SCAN WHILE !EOF()
 dattim = DATETIME(val(RIGHT(DTOC(obsdate),4)),VAL(SUBSTR(DTOC(obsdate),4,2)),VAL(LEFT(DTOC(obsdate),2)),VAL(left(obstime,2)),VAL(subst(ALLTRIM(obstime),4,2)),VAL(subst(ALLTRIM(obstime),7,2)))
 dattim = dattim + IIF(MOD(MINUTE(dattim)*60+SEC(dattim),360) >= 180,360-MOD(MINUTE(dattim)*60+SEC(dattim),360),-MOD(MINUTE(dattim)*60+SEC(dattim),360))
 REPLACE NEXT 1 date_time WITH DATTIM
ENDSCAN
COPY TO (filecsv6) TYPE csv
SELECT date_time,avg(depth_m) as depth_6m FROM depths GROUP BY date_time
COPY TO (filexls6) TYPE xl5

*


FUNCTION precalc
filename = UPPER(ALLTRIM(filename))
IF RIGHT(filename,4) = ".TXT" then
   filecsv = left(filename,LEN(filename)-3)+"CSV          "
ELSE
   filecsv = filename+".csv              "
ENDIF
RETURN

FUNCTION geonatgrid (phi,lamda)
* Convert geographic position (latitude=phi, longitude=lamda decimal degrees) to national grid coordinates

phi0 = 53.5 && True origin 53° 30' N
lamda0 = -8 && True origin 8° W
phi1    = phi - 50
p       = 0.36 * (lamda + 8)
m.easting = p * (199135.366 - 4130.362 * phi1 - 30.68 * phi1^2 + 0.22 * phi1^3) ;
	  - p^3 * (13.4378 + 2.4304 * phi1 - 0.07304 * phi1^2) ;
	  - p^5 * 0.026 + 200000 ;
	  + 64 && correct for map projection

m.northing = (-139384.421 + 111219.253 * phi1 + 9.666 * phi1^2 - 0.032 * phi1^3) ;
     +p^2 * (3697.809 -22.5126 * phi1 - 2.27085 * phi1^2 + 0.007633 * phi1^3) ;
     +p^4 * (1.0807 - 0.0839 * phi1 + 0.00067 * phi1^2) ;
     - 39 && correct for map projection
RETURN 

FUNCTION speedclass(speed)
* Convert speed into numeric rank
 DO case
 	CASE speed <= 2
 		RETURN 1
 	CASE speed > 2 AND speed <= 4
 		RETURN 2
 	CASE speed > 4 AND speed <= 6
 		RETURN 3
 	CASE speed > 6
 		RETURN 4
 	OTHERWISE
 		RETURN -1
 ENDCASE 			
 
 FUNCTION csvsplit()
* split csv sentence into up to 10 arguments
FOR l = 1 TO 20
   arg(l) = ""
   cnt = 1
ENDFOR   
sent = lin
arg(1) = LEFT(sent,AT(',',sent+',')-1)
sent = SUBSTR(sent,AT(',',sent+',')+1,100)
FOR l = 1 TO 10
   IF LEN(sent) = 0 then
      RETURN
   ENDIF
   cnt = cnt + 1
   arg(cnt) = LEFT(sent,AT(',',sent+',')-1)
   sent = SUBSTR(sent,AT(',',sent+',')+1,100)
ENDFOR
RETURN

FUNCTION sumcheck()
* split lin into bytes and bitwise exclusive OR to check validity 
sumchk = 0
FOR l = 2 TO LEN(lin)-3
   sumchk = BITXOR(sumchk,ASC(SUBSTR(lin,l,1)))
ENDFOR   
d1 = INT(sumchk / 16)
d2 = MOD(sumchk,16)
d1 = IIF (d1 > 9,d1+55,d1+48)
d2 = IIF (d2 > 9,d2+55,d2+48)
return CHR(d1)+CHR(d2) = SUBSTR(lin,LEN(lin)-1,2)


