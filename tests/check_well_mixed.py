"""Verify native CFD output, including flow-only interval and closed-box budget."""
import csv, math, sys
from pathlib import Path
case=Path(sys.argv[1]); rows=list(csv.DictReader((case/'postProcessing/oxyControl/0/oxygenBalance.csv').open()))
assert len(rows)==20, f"Expected 20 steps, found {len(rows)}"
c=.15; dt=.001; volume=.1*.1*.15
for i,row in enumerate(rows,1):
    t=float(row['time_s']); assert abs(t-i*dt)<1e-12
    if i>10:
        A=1+dt*.02; b=A*.01-c-dt*.02*.25+dt*.002
        c=(-b+math.sqrt(b*b+4*A*(c+dt*.02*.25)*.01))/(2*A)
    assert abs(float(row['minC_mol_m3'])-c)<1e-10, (t,row,c)
    assert abs(float(row['maxC_mol_m3'])-c)<1e-10
    assert abs(float(row['inventory_mol'])-volume*c)<1e-12
    assert abs(float(row['balanceResidual_mol']))<1e-12
    if i<=10:
        assert float(row['cumulativeSupply_mol'])==0
        assert float(row['cumulativeUptake_mol'])==0
print('PASS: native warm-up freeze, activation, homogeneous reaction and oxygen conservation')
