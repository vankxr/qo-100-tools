<Qucs Schematic 0.0.19>
<Properties>
  <View=142,9,2351,1324,0.646801,0,0>
  <Grid=10,10,1>
  <DataSet=preamp-bias-simulation.dat>
  <DataDisplay=preamp-bias-simulation.dpl>
  <OpenDisplay=1>
  <Script=preamp-bias-simulation.m>
  <RunScript=0>
  <showFrame=0>
  <FrameText0=Title>
  <FrameText1=Drawn By:>
  <FrameText2=Date:>
  <FrameText3=Revision:>
</Properties>
<Symbol>
</Symbol>
<Components>
  <GND * 1 550 410 0 0 0 0>
  <GND * 1 800 410 0 0 0 0>
  <Pac P3 1 800 350 18 -26 0 1 "3" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0>
  <GND * 1 670 490 0 0 0 0>
  <.SP SP1 1 310 40 0 66 0 0 "lin" 1 "1 MHz" 1 "10 GHz" 1 "5000" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Eqn Gains 1 540 50 -30 16 0 0 "Gain_S21db=20*log10(abs(S[2,1]))" 1 "Gain_S31db=20*log10(abs(S[3,1]))" 1 "Gain_S54db=20*log10(abs(S[5,4]))" 1 "yes" 0>
  <GND * 1 1140 770 0 0 0 0>
  <R R1 1 670 430 15 -26 0 1 "10 Ohm" 1 "26.85" 0 "0.0" 0 "0.0" 0 "26.85" 0 "US" 0>
  <R R11 1 740 300 -35 -48 0 0 "1000" 1 "26.85" 0 "0.0" 0 "0.0" 0 "26.85" 0 "US" 0>
  <Eqn Eqn2 1 860 50 -30 16 0 0 "ZinA=(1+S[1,1])/(1-S[1,1])" 1 "ZinB=(1+S[4,4])/(1-S[4,4])" 1 "RinA=real(ZinA)" 1 "RinB=real(ZinB)" 1 "XinA=imag(ZinA)" 1 "XinB=imag(ZinB)" 1 "yes" 0>
  <GND * 1 290 770 0 0 0 0>
  <Pac P1 1 290 670 18 -26 0 1 "1" 1 "50" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0>
  <Pac P4 1 1140 670 18 -26 0 1 "4" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0>
  <SPfile L_0603HP47N 1 550 300 -40 -41 0 0 "/home/joao/preamp-bias-simulation/06HP47N.s2p" 0 "polar" 0 "linear" 0 "open" 0 "2" 0>
  <GND * 1 650 770 0 0 0 0>
  <Pac P2 1 650 650 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0>
  <C C1 1 540 570 -26 -55 0 2 "12 pF" 1 "" 0 "neutral" 0>
  <R R9 1 450 570 -35 -48 0 0 "0.6" 1 "26.85" 0 "0.0" 0 "0.0" 0 "26.85" 0 "US" 0>
  <C C5 1 1390 570 -26 -55 0 2 "12 pF" 1 "" 0 "neutral" 0>
  <R R10 1 1300 570 -6 11 0 0 "0.6" 1 "26.85" 0 "0.0" 0 "0.0" 0 "26.85" 0 "US" 0>
  <GND * 1 1620 770 0 0 0 0>
  <Pac P5 1 1620 670 18 -26 0 1 "5" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0>
  <C C9 1 670 350 17 -26 0 1 "22 uF" 1 "" 0 "neutral" 0>
</Components>
<Wires>
  <390 300 390 570 "" 0 0 0 "">
  <390 300 520 300 "" 0 0 0 "">
  <550 330 550 410 "" 0 0 0 "">
  <800 380 800 410 "" 0 0 0 "">
  <580 300 670 300 "" 0 0 0 "">
  <670 300 670 320 "" 0 0 0 "">
  <670 380 670 400 "" 0 0 0 "">
  <670 460 670 490 "" 0 0 0 "">
  <800 300 800 320 "" 0 0 0 "">
  <770 300 800 300 "" 0 0 0 "">
  <670 300 710 300 "" 0 0 0 "">
  <290 700 290 770 "" 0 0 0 "">
  <290 570 390 570 "" 0 0 0 "">
  <290 570 290 640 "" 0 0 0 "">
  <1140 700 1140 770 "" 0 0 0 "">
  <390 570 420 570 "" 0 0 0 "">
  <650 680 650 770 "" 0 0 0 "">
  <650 570 650 620 "" 0 0 0 "">
  <570 570 650 570 "" 0 0 0 "">
  <480 570 510 570 "" 0 0 0 "">
  <1140 570 1140 640 "" 0 0 0 "">
  <1140 570 1270 570 "" 0 0 0 "">
  <1420 570 1620 570 "" 0 0 0 "">
  <1330 570 1360 570 "" 0 0 0 "">
  <1620 700 1620 770 "" 0 0 0 "">
  <1620 570 1620 640 "" 0 0 0 "">
</Wires>
<Diagrams>
  <Rect 1139 454 692 405 3 #c0c0c0 1 00 1 0 1e+09 1e+10 1 -10 1 1 1 -1 0.2 1 315 0 225 "" "" "">
	<"Gain_S21db" #0000ff 0 3 0 0 0>
	  <Mkr 2.38924e+09 225 -385 3 0 0>
	<"Gain_S31db" #ff0000 0 3 0 0 0>
	  <Mkr 2.40324e+09 226 -192 3 0 0>
	<"Gain_S54db" #ff00ff 0 3 0 0 0>
  </Rect>
  <Smith 1920 1224 404 404 3 #c0c0c0 1 00 1 0 1 1 1 0 4 1 1 0 1 1 315 0 225 "" "" "">
	<"S[1,1]" #0000ff 0 3 0 0 0>
	  <Mkr 2.40324e+09 -51 -114 3 0 0>
	<"S[4,4]" #ff0000 0 3 0 0 0>
  </Smith>
  <Rect 299 1234 689 389 3 #c0c0c0 1 00 1 0 1e+09 1e+10 1 0 0.1 0.5 1 -1 0.2 1 315 0 225 "" "" "">
	<"RinA" #0000ff 0 3 0 0 0>
	  <Mkr 2.40724e+09 226 -101 3 0 0>
	<"RinB" #ff0000 0 3 0 0 0>
  </Rect>
  <Rect 1139 1234 689 389 3 #c0c0c0 1 00 1 0 1e+09 1e+10 0 -10 1 1 1 -1 0.2 1 315 0 225 "" "" "">
	<"XinA" #0000ff 0 3 0 0 0>
	  <Mkr 2.38924e+09 225 -405 3 0 0>
	<"XinB" #ff0000 0 3 0 0 0>
  </Rect>
</Diagrams>
<Paintings>
</Paintings>
