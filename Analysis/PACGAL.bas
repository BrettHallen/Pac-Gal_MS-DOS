10 REM ===================================================================
20 REM  PAC-GAL  by Al J. Jimenez, May 1982  -- RECONSTRUCTED SOURCE
30 REM  Rebuilt from the BASCOM-compiled PAC-GAL.EXE by disassembly.
40 REM  Logic, constants, strings and statement order follow the binary.
50 REM  Line numbers, variable NAMES and the maze-drawing method are new
60 REM  (the original drew the maze with ~900 PRINT items; here it is
70 REM  decoded from DATA but produces the same screen).
80 REM ===================================================================
90 DEFINT A-Z
100 DIM CH(5),CL(5),R(5),C(5),U(5),F(5),DR(5),DC(5),KR(5),KC(5)
110 CLS
120 PRINT "How fast (0-30000)";:INPUT SP
130 CLS
140 E$="t255mbl64o1afgao4d"                     ' eat-a-dot chirp
150 W$="mbl64abceabceebceabceagaa"              ' power-pellet jingle
160 I$="mbt190o2l8bbbl16cecl8bbp8bbbl16cl8edcc" ' intro tune
170 REM --------------------------------------------------- restart point
180 CLS
190 PLAY I$
200 FOR I=60 TO 1 STEP -1                        ' Pac-Gal runs right-to-left
210   LOCATE 14,I:PRINT CHR$(2);" ";
220   FOR J=1 TO 25:NEXT J
230 NEXT I
240 PLAY I$
250 FOR I=1 TO 60                                ' ...then gets chased back
260   LOCATE 14,I:PRINT " ";CHR$(3);"   ";CHR$(4);CHR$(5);CHR$(6);"       ";CHR$(2);
270   FOR J=1 TO 25:NEXT J
280 NEXT I
290 S=0:I=0:J=0:NR=0:NC=0:K=0:Z=0:SG=0:T=0
300 DEF FNT(X)=VAL(LEFT$(TIME$,2))*360+VAL(MID$(TIME$,4,2))*60+VAL(RIGHT$(TIME$,2))
310 REM  ^ sic: hours*360, not *3600 (see analysis)
320 CLS
330 A$=" ":B$=CHR$(219):C$=A$+CHR$(186):D$=CHR$(196)
340 DN$="ba#ag#gf#fed#dc#c":UP$="cc#dd#eff#gg#aa#b"   ' DN$ is never used
350 G$=A$+CHR$(249):H$=A$+CHR$(219):Q$=STRING$(2,205):R$=CHR$(205)
360 COLOR 15,0:KEY OFF
370 LOCATE 25,28:PRINT "    P A C - G A L ";
380 COLOR 7,0:PRINT "        Al J. Jim";CHR$(130);"nez, May 1982";
390 GOSUB 4800                                   ' draw maze rows 1-24
400 COLOR 26,0                                   ' 26 = blinking light green
410 LOCATE 6,2:PRINT G$;:LOCATE 6,76:PRINT G$;:LOCATE 19,2:PRINT G$;
420 LOCATE 2,30:PRINT G$;:LOCATE 2,50:PRINT G$;:LOCATE 23,30:PRINT G$;
430 LOCATE 23,50:PRINT G$;:LOCATE 19,76:PRINT G$;:LOCATE 12,18:PRINT G$;
440 LOCATE 12,60:PRINT G$;
450 COLOR 7,0
460 LV=3:DT=468:P=0
470 T0=FNT(2):RANDOMIZE T0
480 LOCATE 25,1:PRINT "dots";DT;
490 LOCATE 25,15:PRINT STRING$(3,1);
500 CH(1)=1:CL(1)=7:R(1)=19:C(1)=19:U(1)=32:F(1)=0:DR(1)=0:DC(1)=0:KR(1)=0:KC(1)=0
510 FOR K=2 TO 5
520   CH(K)=K+1:CL(K)=7:R(K)=14:C(K)=K+16:U(K)=32
530   SG=RND(2):IF SG=0 THEN SG=-1               ' CINT(RND) -> 0/1 -> -1/+1
540   F(K)=0:DR(K)=RND(2):DC(K)=(1-DR(K))*SG:DR(K)=DR(K)*SG
550   KR(K)=0:KC(K)=0
560 NEXT K
570 KEY(11) ON:KEY(12) ON:KEY(13) ON:KEY(14) ON
580 ON KEY(11) GOSUB 640:ON KEY(12) GOSUB 650:ON KEY(13) GOSUB 660:ON KEY(14) GOSUB 670
590 GOTO 700
640 KR(1)=-1:KC(1)=0:RETURN                      ' Up
650 KR(1)=0:KC(1)=-1:RETURN                      ' Left
660 KR(1)=0:KC(1)=1:RETURN                       ' Right
670 KR(1)=1:KC(1)=0:RETURN                       ' Down
680 REM ======================================================= MAIN LOOP
700 GOSUB 4700                                   ' speed delay
710 GOSUB 1000                                   ' move Pac-Gal
720 EL=FNT(2)-T0:IF EL<=1 THEN EL=1
730 FOR T=1 TO EL STEP 180:GOSUB 2000:NEXT T     ' +1 ghost move per 3 min
740 GOTO 700
990 REM =================================================== MOVE PAC-GAL
1000 NR=KR(1)+R(1):NC=KC(1)+C(1)                 ' try the wanted direction
1010 S=SCREEN(NR,NC*2+1)
1020 IF S=32 THEN 1210
1030 IF S=249 THEN 1210
1040 IF S>2 THEN IF S<7 THEN 3000               ' ran into a ghost
1050 IF NR=12 THEN IF S=196 THEN NC=39-C(1):S=SCREEN(NR,NC*2+1):IF S=32 OR S=249 THEN 1210
1100 NR=DR(1)+R(1):NC=DC(1)+C(1)                 ' else keep going straight
1110 S=SCREEN(NR,NC*2+1)
1120 IF S=32 THEN 1220
1130 IF S=249 THEN 1220
1140 IF S>2 THEN IF S<7 THEN 3000
1150 IF NR=12 THEN IF S=196 THEN NC=39-C(1):S=SCREEN(NR,NC*2+1):IF S=32 OR S=249 THEN 1220
1160 DR(1)=0:DC(1)=0                             ' blocked: stop
1170 SOUND 4000,.2
1180 RETURN
1210 DR(1)=KR(1):DC(1)=KC(1)
1220 IF SCREEN(NR,NC*2+1,1)<=7 THEN 1300         ' attribute > 7 = power pellet
1230 PLAY W$
1240 I=(DT/5+20)/(LV*LV)                         ' fright time, in ghost steps
1250 FOR K=2 TO 5:CL(K)=26:F(K)=I:NEXT K
1300 LOCATE R(1),C(1)*2+1:PRINT CHR$(U(1));
1310 LOCATE NR,NC*2+1:COLOR CL(1),0:PRINT CHR$(CH(1));:COLOR 7,0
1320 U(1)=32:R(1)=NR:C(1)=NC
1330 IF S=249 AND DT>0 THEN DT=DT-1:LOCATE 25,5:PRINT DT;:PLAY E$ ELSE SOUND 150,.1
1340 IF DT<1 THEN 4000
1350 IF DT<50 THEN P=P*.5
1360 RETURN
1990 REM ===================================================== MOVE GHOSTS
2000 FOR K=2 TO 5
2010   F(K)=F(K)-1
2020   NR=R(K):NC=C(K)
2030   IF NC>=18 AND NC<=23 THEN IF NR=11 OR NR=12 THEN 2200
2040   IF NC>=18 AND NC<=23 THEN IF NR>12 AND NR<16 THEN 2300
2050   IF RND(2)<P THEN 2200                     ' P is always 0: never taken
2060   IF DR(K)<>0 OR R(K)=R(1) THEN 2100        ' chase: turn toward row
2070   NR=NR+SGN(R(1)-NR):S=SCREEN(NR,NC*2+1)
2080   IF S=32 OR S=249 THEN 2500
2090   IF S=1 THEN 2900
2100   IF DC(K)<>0 OR C(K)=C(1) THEN 2200        ' chase: turn toward column
2110   NR=R(K):NC=NC+SGN(C(1)-NC):S=SCREEN(NR,NC*2+1)
2120   IF S=32 OR S=249 THEN 2500
2130   IF S=1 THEN 2900
2200   NR=R(K)+DR(K):NC=C(K)+DC(K):S=SCREEN(NR,NC*2+1)   ' straight on
2210   IF S=32 OR S=249 THEN 2500
2220   IF S=CH(1) THEN 2900
2300   SG=RND(2):IF SG=0 THEN SG=-1              ' random 90-degree turn
2310   NR=R(K)+DC(K)*SG:NC=C(K)+DR(K)*SG:S=SCREEN(NR,NC*2+1)
2320   IF S=32 OR S=249 THEN 2500
2330   IF S=CH(1) THEN 2900
2340   NR=R(K)-DC(K)*SG:NC=C(K)-DR(K)*SG:S=SCREEN(NR,NC*2+1)  ' other side
2350   IF S=32 OR S=249 THEN 2500
2360   IF S=CH(1) THEN 2900
2370   NR=R(K)-DR(K):NC=C(K)-DC(K):S=SCREEN(NR,NC*2+1)        ' reverse
2380   IF S=32 OR S=249 THEN 2500
2390   IF S=CH(1) THEN 2900
2400   NR=R(K):NC=C(K):S=U(K):GOTO 2510          ' boxed in: stay put
2500   DR(K)=NR-R(K):DC(K)=NC-C(K)
2510   LOCATE R(K),C(K)*2+1:PRINT CHR$(U(K));
2520   IF F(K)<=0 THEN CL(K)=7
2530   COLOR CL(K),0:LOCATE NR,NC*2+1:PRINT CHR$(CH(K));:COLOR 7,0
2540   U(K)=S:R(K)=NR:C(K)=NC
2550 NEXT K
2560 RETURN
2900 NR=R(K):NC=C(K):S=K+1:GOTO 3000             ' ghost walked into Pac-Gal
2990 REM ======================================================= COLLISION
3000 IF SCREEN(NR,NC*2+1,1)>7 THEN 3500          ' ghost is blinking: eat it
3010 PLAY "mbl8t255o4fego3abcdefgo0l1g-g"
3020 LOCATE R(1),C(1)*2+1:PRINT CHR$(U(1));
3030 COLOR 26,0
3040 FOR K=1 TO 200:LOCATE NR,NC*2+1:PRINT CHR$(168);:NEXT K
3050 IF DT>300 THEN IF P<.1 THEN P=P+P
3060 COLOR 7,0
3070 IF LV<=1 THEN 4100
3080 LV=LV-1
3090 LOCATE 25,15:PRINT STRING$(LV,1);A$;A$;A$;A$,
3100 FOR K=1 TO 5
3110   LOCATE R(K),C(K)*2+1:PRINT CHR$(U(K))
3120   U(K)=32:CL(K)=7:R(K)=14:C(K)=K+17:F(K)=0
3130   IF K=1 THEN DR(K)=0:DC(K)=0
3140   KR(K)=0:KC(K)=0
3150 NEXT K
3160 R(1)=19:C(1)=19
3170 T0=FNT(2)
3180 RETURN
3490 REM ======================================================= EAT GHOST
3500 PLAY "mbl24o2x"+VARPTR$(UP$)+"l32o3x"+VARPTR$(UP$)+"l64o4x"+VARPTR$(UP$)
3510 LOCATE R(1),C(1)*2+1:PRINT CHR$(U(1));
3520 COLOR 26,0
3530 FOR K=1 TO 100:LOCATE NR,NC*2+1:PRINT CHR$(2);:NEXT K
3540 T0=FNT(2)
3550 P=P*.5
3560 COLOR 7,0
3570 IF LV<3 THEN LV=LV+1:LOCATE 25,15:PRINT STRING$(LV,1);A$;A$;A$;A$;
3580 FOR K=2 TO 5
3590   F(K)=F(K)-1
3600   IF R(K)<>NR OR C(K)<>NC THEN 3650
3610   CL(K)=7:R(K)=14:C(K)=K+16
3620   IF U(K)=249 THEN DT=DT-1:LOCATE 25,5:PRINT DT;
3630   U(K)=32
3650 NEXT K
3660 GOTO 1300                                   ' Pac-Gal steps into the spot
3990 REM ======================================================= BOARD CLEAR
4000 LOCATE 25,10:COLOR 26,0:PRINT "You did it!!!";:COLOR 7,0
4010 T0=FNT(2)
4020 IF FNT(2)-T0<15 THEN 4020
4090 REM ======================================================= PLAY AGAIN?
4100 LOCATE 25,10:COLOR 26,0:PRINT "Play again???";:COLOR 7,0
4110 POKE 106,0                                  ' BASICA keyboard-flush idiom
4120 K$=INKEY$:IF K$="" THEN 4120
4130 IF K$="y" OR K$="Y" THEN 180
4140 IF K$<>"n" THEN 4120
4150 CLS:KEY ON
4160 PRINT "...Hope you had a good time"
4170 END
4690 REM ====================================================== SPEED DELAY
4700 D=0
4710 FOR L=1 TO SP:D=D+1:NEXT L
4720 RETURN
4790 REM ========================================================= MAZE DRAW
4800 RESTORE 5000:K1$="#_^=|-r7LJTW.":K2$=CHR$(219)+CHR$(220)+CHR$(223)+CHR$(205)+CHR$(186)+CHR$(196)+CHR$(201)+CHR$(187)+CHR$(200)+CHR$(188)+CHR$(203)+CHR$(202)+CHR$(249)
4810 FOR Y=1 TO 24
4820   READ M$:LOCATE Y,1
4830   FOR X=1 TO LEN(M$)
4840     Z=INSTR(K1$,MID$(M$,X,1))
4850     IF Z=0 THEN PRINT " "; ELSE PRINT MID$(K2$,Z,1);
4860   NEXT X
4880 NEXT Y
4890 Z=0:RETURN
4990 REM  # =219  _ =220  ^ =223  = =205  | =186  - =196 (tunnel)
4995 REM  r =201  7 =187  L =200  J =188  T =203  W =202  . =249 (dot)
5000 DATA "_______________________________________________________________________________"
5010 DATA "# . . . . . . . . | . . . . . . . . . | . . . . . . . . . | . . . . . . . . . #"
5020 DATA "# . | . r====== . | . ========T==== . | . ====T======== . | . ========7 . | . #"
5030 DATA "# . | . | . . . . . . . . . . | . . . | . . . | . . . . . . . . . . . | . | . #"
5040 DATA "# . | . | . =============== . | . ====W==== . | . ================= . | . | . #"
5050 DATA "# . | . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . | . #"
5060 DATA "# . | . ========= . ======7 . r=======T=======T==== . r==== . ========= . | . #"
5070 DATA "# . | . . . . . . . . . r=J . | . . . | . . . | . . . | . . . . . . . . . | . #"
5080 DATA "# . | . | . ========7 . | . . . . | . . . | . . . | . | . r======== . | . | . #"
5090 DATA "# . . . | . . . . . | . | . | . ==W=======W==== . | . | . | . . . . . | . . . #"
5100 DATA "^^^^^^^^^ . | . | . . . | . | . . . . . . . . . . | . | . . . | . | . ^^^^^^^^^"
5110 DATA "--- . . . . | . | . | . | . | . r====     ====7 . | . | . | . | . | . . . . ---"
5120 DATA "_________ . | . | . | . | . . . |             | . . . | . | . | . | . _________"
5130 DATA "# . . . | . . . | . | . | . | . |             | . | . | . | . | . . . | . . . #"
5140 DATA "# . | . | . | . . . | . | . | . |             | . | . | . | . . . | . | . | . #"
5150 DATA "# . | . | . L==== . | . | . | . L=============J . | . | . | . ====J . | . | . #"
5160 DATA "# . | . . . . . . . | . . . . . . . . . . . . . . . . . . | . . . . . . . | . #"
5170 DATA "# . | . ============J . L=============================J . L============ . | . #"
5180 DATA "# . | . . . . . . . . . . . . . . . .   . . . . . . . . . . . . . . . . . | . #"
5190 DATA "# . | . | . =============== . | . ====T==== . | . ================= . | . | . #"
5200 DATA "# . | . | . . . . . . . . . . | . . . | . . . | . . . . . . . . . . . | . | . #"
5210 DATA "# . | . L====== . | . ========W==== . | . ====W======== . | . ========J . | . #"
5220 DATA "# . . . . . . . . | . . . . . . . . . | . . . . . . . . . | . . . . . . . . . #"
5230 DATA "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"
