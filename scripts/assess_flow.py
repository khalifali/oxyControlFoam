#!/usr/bin/env python3
"""Summarise diagnostic CSVs; no automatic declaration of developed turbulence."""
import argparse, csv, statistics
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('case', type=Path)
parser.add_argument('--after', type=float, required=True, help='Discard startup before this physical time (s)')
a=parser.parse_args()
def rows(name):
    # Later restart segments replace duplicate times. Ignore repeated CSV headers.
    selected={}
    segments=list((a.case/'postProcessing/oxyControl').glob('*'))
    for segment in sorted(segments,key=lambda p:float(p.name)):
        path=segment/name
        if not path.exists(): continue
        with path.open() as f:
            for row in csv.DictReader(f):
                if row['time_s']=='time_s':continue
                if float(row['time_s'])>=a.after:
                    selected[(float(row['time_s']),row.get('patch',''))]=row
    return [selected[k] for k in sorted(selected)]
flow=rows('flowMeasurements.csv'); walls=rows('wallResolution.csv')
if len(flow)<4:raise SystemExit('Need at least four post-startup flow samples.')
print(f"Window: {flow[0]['time_s']} .. {flow[-1]['time_s']} s, {len(flow)} samples")
for key in flow[0]:
    if key=='time_s':continue
    values=[float(r[key]) for r in flow];half=len(values)//2
    print(f'{key}: mean={statistics.mean(values):.6g}, rms fluctuation={statistics.pstdev(values):.6g}, '
          f'first/second-half means={statistics.mean(values[:half]):.6g}/{statistics.mean(values[half:]):.6g}')
if not walls:raise SystemExit('No wall-resolution output found.')
for patch in sorted({r['patch'] for r in walls}):
    patch_rows=[r for r in walls if r['patch']==patch]
    maxima={key:max(float(r[key]) for r in patch_rows) for key in ('maxYPlus','maxStreamwisePlus','maxSpanwisePlus')}
    print(patch, maxima)
print('Interpret statistics with spatial fields, spectra and mesh/time refinement. A nonzero RMS or high Reynolds number alone does not establish turbulence.')
