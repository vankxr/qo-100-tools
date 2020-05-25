<Qucs Schematic 0.0.19>
<Properties>
  <View=0,-120,1108,800,1,0,0>
  <Grid=10,10,1>
  <DataSet=pa-lpf-simulation.dat>
  <DataDisplay=pa-lpf-simulation.dpl>
  <OpenDisplay=1>
  <Script=5g_filter.m>
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
  <L L1 1 410 290 -26 10 0 0 "4.81 nH" 1 "" 0>
  <L L2 1 540 290 -26 10 0 0 "9.35 nH" 1 "" 0>
  <L L4 1 700 290 -26 10 0 0 "8.51 nH" 1 "" 0>
  <L L3 1 620 390 -75 -26 0 3 "1.2 nH" 1 "" 0>
  <C C1 1 480 390 -94 -26 0 3 "1.12 pF" 1 "" 0 "neutral" 0>
  <C C2 1 620 480 -94 -26 0 3 "0.86 pF" 1 "" 0 "neutral" 0>
  <Pac P2 1 800 420 18 -26 0 1 "2" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0>
  <Eqn Eqn1 1 590 40 -30 16 0 0 "rloss=-20*log10(abs(S[1,1]))" 1 "gain=20*log10(abs(S[2,1]))" 1 "yes" 0>
  <.SP SP1 1 330 20 0 63 0 0 "lin" 1 "1 GHz" 1 "10 GHz" 1 "5000" 1 "no" 0 "1" 0 "2" 0 "no" 0 "no" 0>
  <Pac P1 1 290 410 18 -26 0 1 "1" 1 "50 Ohm" 1 "0 dBm" 0 "1 GHz" 0 "26.85" 0>
</Components>
<Wires>
  <620 510 620 560 "" 0 0 0 "">
  <480 560 620 560 "" 0 0 0 "">
  <480 420 480 560 "" 0 0 0 "">
  <480 290 480 360 "" 0 0 0 "">
  <440 290 480 290 "" 0 0 0 "">
  <480 290 510 290 "" 0 0 0 "">
  <570 290 620 290 "" 0 0 0 "">
  <620 290 670 290 "" 0 0 0 "">
  <620 290 620 360 "" 0 0 0 "">
  <620 420 620 450 "" 0 0 0 "">
  <620 560 800 560 "" 0 0 0 "">
  <730 290 800 290 "" 0 0 0 "">
  <800 450 800 560 "" 0 0 0 "">
  <800 290 800 390 "" 0 0 0 "">
  <290 560 480 560 "" 0 0 0 "">
  <290 440 290 560 "" 0 0 0 "">
  <290 290 380 290 "" 0 0 0 "">
  <290 290 290 380 "" 0 0 0 "">
</Wires>
<Diagrams>
</Diagrams>
<Paintings>
</Paintings>
