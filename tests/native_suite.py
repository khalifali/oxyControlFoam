#!/usr/bin/env python3
"""Small native integration suite. Requires built OF13/module and cylinder-ogrid."""
import argparse, csv, re, shutil, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
out=a.output.resolve()
if out.exists():p.error('Choose a new output directory; results are never overwritten.')
out.mkdir(parents=True)
plugin=out/'libStateProbe.so'
subprocess.run(['g++','-std=c++17','-shared','-fPIC','-I'+str(ROOT/'src/controller'),
                str(ROOT/'tests/StateProbeController.C'),'-o',str(plugin)],check=True)
def run(case,*cmd,tag=None):
    log=case/('log.'+(tag or cmd[0]));print(case,cmd,flush=True)
    with log.open('w') as f:
        result=subprocess.run(cmd,cwd=case,stdout=f,stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text()[-16000:],flush=True)
        raise RuntimeError(f'Failed command: {cmd}; see {log}')
def setting(case,file,key,value):
    run(case,'foamDictionary',file,'-entry',key,'-set',str(value),tag='setting')
def copy(name,template):
    def ignore(folder,names):
        return [n for n in names if n in {'0','polyMesh','postProcessing','__pycache__','mesh.foam','quality.json','config.resolved.json'}
                or n.startswith(('processor','log.')) or re.fullmatch(r'\d+(?:\.\d+)?',n)]
    case=out/name;shutil.copytree(ROOT/'cases'/template,case,ignore=ignore)
    return case
def uniform(name,parallel=False,end=.02):
    case=copy(name,'wellMixed')
    setting(case,'system/controlDict','endTime',end)
    # Exercise loading the student's controller without imposing a control law.
    setting(case,'constant/oxyProperties','controller','student')
    setting(case,'constant/oxyProperties','studentLibrary','"'+str(plugin)+'"')
    run(case,'blockMesh');shutil.copytree(case/'0.backup',case/'0')
    if parallel:
        setting(case,'system/decomposeParDict','numberOfSubdomains',2)
        setting(case,'system/decomposeParDict','method','simple')
        setting(case,'system/decomposeParDict','simpleCoeffs','{ n (2 1 1); delta 0.001; }')
        run(case,'decomposePar');run(case,'mpirun','-np','2','oxyControlFoam','-parallel')
    else:run(case,'oxyControlFoam')
    return case
def balance(case,segment='0'):
    with (case/f'postProcessing/oxyControl/{segment}/oxygenBalance.csv').open() as f:return list(csv.DictReader(f))
serial=uniform('serial');subprocess.run([sys.executable,str(ROOT/'tests/check_well_mixed.py'),str(serial)],check=True)
mpi=uniform('mpi',True);subprocess.run([sys.executable,str(ROOT/'tests/check_well_mixed.py'),str(mpi)],check=True)
for left,right in zip(balance(serial),balance(mpi)):
    for key in left:assert abs(float(left[key])-float(right[key]))<1e-11,(key,left,right)
restart=uniform('restart',True,.01)
setting(restart,'system/controlDict','startTime',.01);setting(restart,'system/controlDict','endTime',.02)
run(restart,'mpirun','-np','2','oxyControlFoam','-parallel',tag='restart')
for case in (mpi,restart):
    for rank in range(2):
        state=(case/f'processor{rank}/0.02/uniform/oxyControlState').read_text()
        assert re.search(r'studentState\s+(?:1\s*)?\(\s*2\s*\)',state),state
reference=balance(mpi)[10:];continued=balance(restart,'0.01')
assert len(reference)==len(continued)==10
for left,right in zip(reference,continued):
    for key in left:assert abs(float(left[key])-float(right[key]))<1e-11,(key,left,right)
tank=copy('les-smoke','stirredTank')
setting(tank,'system/controlDict','endTime',.02)
setting(tank,'system/controlDict','writeInterval',100)
setting(tank,'constant/oxyProperties','oxygenStartTime','[0 0 1 0 0 0 0] 0.01')
setting(tank,'constant/oxyProperties','controlStartTime','[0 0 1 0 0 0 0] 0.01')
setting(tank,'constant/oxyProperties','controller','student')
setting(tank,'system/decomposeParDict','numberOfSubdomains',2)
run(tank,'bash','./Allmesh','--smoke')
run(tank,'bash','./Allrun')
rows=balance(tank);assert len(rows)==200
assert float(rows[-1]['cumulativeUptake_mol'])>0
assert float(rows[-1]['cumulativeSupply_mol'])>0
assert max(abs(float(r['balanceResidual_mol'])) for r in rows)<1e-10
for r in rows[:100]:assert float(r['cumulativeSupply_mol'])==float(r['cumulativeUptake_mol'])==0
for name in ['measurements.csv','actuators.csv','flowMeasurements.csv','wallResolution.csv']:
    assert (tank/'postProcessing/oxyControl/0'/name).stat().st_size>100
# Short LES checkpoint continuation checks that flm/fmm and control state are available.
setting(tank,'system/controlDict','startTime',.02);setting(tank,'system/controlDict','endTime',.021)
run(tank,'bash','./Allrestart',tag='Allrestart')
assert len(balance(tank,'0.02'))==10
print('PASS: serial/MPI uniform chemistry, warm-up restart equivalence and short LES activation')
print('LES smoke is not evidence of developed turbulence or wall resolution.')
