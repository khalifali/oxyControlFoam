// SPDX-License-Identifier: GPL-3.0-or-later
// Momentum assembly follows OpenFOAM Foundation incompressibleFluid,
// Copyright (C) 2022-2023 OpenFOAM Foundation; GPL-3.0-or-later.
#include "oxyControlFoam.H"
#include "addToRunTimeSelectionTable.H"
#include "fvmDdt.H"
#include "fvmDiv.H"
#include "fvmLaplacian.H"
#include "fvmSup.H"
#include "fvcGrad.H"
#include "PstreamReduceOps.H"
#include "OSspecific.H"
#include "fvModels.H"
#include "meshSearch.H"
#include "fvConstraints.H"
#include "wallFvPatch.H"
#include "mathematicalConstants.H"
#include <dlfcn.h>
#include <iomanip>
#include <cmath>

namespace Foam { namespace solvers {
defineTypeNameAndDebug(oxyControlFoam,0);
addToRunTimeSelectionTable(solver,oxyControlFoam,fvMesh);

scalar oxyControlFoam::inventory() const {
    scalar total=0;forAll(oxygen_,i)total+=oxygen_[i]*mesh.V()[i];
    reduce(total,sumOp<scalar>());return total;
}
scalar oxyControlFoam::coefficient(const word& key,const dimensionSet& dims) const {
    scalar value=dimensionedScalar(key,dims,cfg_).value();
    if(!std::isfinite(value)) FatalErrorInFunction<<key<<" must be finite"<<exit(FatalError);
    return value;
}
scalarField oxyControlFoam::weights(const dictionary& d) const {
    scalarField result(mesh.nCells(),1);
    if(d.lookupOrDefault<bool>("uniform",false))return result; // Verification only.
    const vector centre=dimensionedVector("centre",dimLength,d).value();
    const scalar radius=dimensionedScalar("radius",dimLength,d).value();
    const scalar height=dimensionedScalar("halfHeight",dimLength,d).value();
    if(!std::isfinite(mag(centre)+radius+height) || radius<=0 || height<=0)
        FatalErrorInFunction<<"Finite, positive source dimensions required"<<exit(FatalError);
    forAll(result,i) {
        const vector r=mesh.C()[i]-centre;
        const scalar q=(sqr(r.x())+sqr(r.y()))/sqr(radius), z=sqr(r.z()/height);
        result[i]=q<1 && z<1 ? sqr(1-q)*sqr(1-z) : 0;
    }
    return result;
}
oxyControlFoam::oxyControlFoam(fvMesh& mesh)
: incompressibleFluid(mesh),
  cfg_(IOobject("oxyProperties",runTime.constant(),mesh,IOobject::MUST_READ,IOobject::NO_WRITE)),
  state_(IOobject("oxyControlState",runTime.name(),"uniform",mesh,
      runTime.value()>0?IOobject::MUST_READ:IOobject::READ_IF_PRESENT,IOobject::AUTO_WRITE)),
  oxygen_(IOobject("oxygen",runTime.name(),mesh,IOobject::MUST_READ,IOobject::AUTO_WRITE),mesh),
  mode_(cfg_.lookup<word>("controller")),
  interval_(coefficient("sampleInterval",dimTime)),
  lastSample_(state_.lookupOrDefault<scalar>("lastSample",runTime.value()-interval_)),
  initialInventory_(state_.lookupOrDefault<scalar>("initialInventory",inventory())),
  cumulativeSupply_(state_.lookupOrDefault<scalar>("cumulativeSupply",0)),
  cumulativeUptake_(state_.lookupOrDefault<scalar>("cumulativeUptake",0)),
  applied_{state_.lookupOrDefault<scalar>("omega",coefficient("initialOmega",dimless/dimTime)),
           state_.lookupOrDefault<scalar>("kla",coefficient("initialKLa",dimless/dimTime))}
{
    if(mesh.dynamic()) FatalErrorInFunction<<"Version 0.1 uses a fixed mesh"<<exit(FatalError);
    if(oxygen_.dimensions()!=dimMoles/dimVolume) FatalErrorInFunction<<"oxygen must be mol/m3"<<exit(FatalError);
    if(interval_<=0 || coefficient("molecularDiffusivity",dimArea/dimTime)<0
       || coefficient("turbulentSchmidt",dimless)<=0
       || coefficient("halfSaturation",dimMoles/dimVolume)<=0
       || coefficient("saturation",dimMoles/dimVolume)<0
       || coefficient("maximumUptake",dimMoles/dimVolume/dimTime)<0)
        FatalErrorInFunction<<"Invalid oxygen/control parameters"<<exit(FatalError);
    label invalidOxygen=0;
    forAll(oxygen_,i)if(!std::isfinite(oxygen_[i]) || oxygen_[i]<0)invalidOxygen=1;
    reduce(invalidOxygen,maxOp<label>());
    if(invalidOxygen)FatalErrorInFunction<<"Initial/restarted oxygen must be finite and nonnegative"<<exit(FatalError);
    if(mode_!="constant" && mode_!="prescribed" && mode_!="student")
        FatalErrorInFunction<<"controller must be constant, prescribed or student"<<exit(FatalError);
    stirWeights_=weights(cfg_.subDict("stirrer"));
    supplyWeights_=weights(cfg_.subDict("supplyRegion"));
    if(coefficient("minKLa",dimless/dimTime)<0)
        FatalErrorInFunction<<"kLa bounds must be nonnegative"<<exit(FatalError);
    for(const word& actuator : {word("Omega"),word("KLa")}) {
        const scalar lower=coefficient(word("min"+actuator),dimless/dimTime);
        const scalar upper=coefficient(word("max"+actuator),dimless/dimTime);
        const scalar initial=actuator=="Omega"?applied_.omega:applied_.kla;
        if(lower>upper || initial<lower || initial>upper)
            FatalErrorInFunction<<"Invalid initial/state command or bounds: "<<actuator<<exit(FatalError);
    }
    const scalar dt=runTime.deltaTValue();
    if(runTime.controlDict().lookupOrDefault<bool>("adjustTimeStep",false))
        FatalErrorInFunction<<"Use fixed deltaT so sampling and activation are exact"<<exit(FatalError);
    for(const word& key : {word("oxygenStartTime"),word("controlStartTime"),word("sampleInterval"),word("demandChangeTime")}) {
        const scalar value=coefficient(key,dimTime);
        if(value<0 || mag(value/dt-round(value/dt))>1e-6)
            FatalErrorInFunction<<key<<" must be nonnegative and a multiple of deltaT"<<exit(FatalError);
    }
    const dictionary& probes=cfg_.subDict("measurements");
    names_=probes.toc(); locations_.setSize(names_.size()); cells_.setSize(names_.size());
    if(names_.empty()) FatalErrorInFunction<<"Specify at least one measurement"<<exit(FatalError);
    if(runTime.value()>0 && (state_.lookup<word>("controller")!=mode_
        || state_.lookup<wordList>("probeNames")!=names_
        || state_.lookup<scalar>("sampleInterval")!=interval_))
        FatalErrorInFunction<<"Keep controller, probes and sample interval unchanged on restart"<<exit(FatalError);
    const meshSearch& search=meshSearch::New(mesh);
    forAll(names_,i) {
        locations_[i]=probes.subDict(names_[i]).lookup<vector>("position");
        label cell=search.findCell(locations_[i]);
        label owner=cell>=0?Pstream::myProcNo():labelMax;
        reduce(owner,minOp<label>());
        if(owner==labelMax) FatalErrorInFunction<<"Probe outside mesh: "<<names_[i]<<exit(FatalError);
        cells_[i]=owner==Pstream::myProcNo()?cell:-1;
    }
    if(runTime.value()>0 && state_.lookup<vectorField>("probePositions")!=locations_)
        FatalErrorInFunction<<"Keep probe positions unchanged on restart"<<exit(FatalError);
    if(Pstream::master()) {
        fileName folder=runTime.globalPath()/"postProcessing"/"oxyControl"/runTime.name();
        mkDir(folder);
        // Append on repeated starts; segment files keep later restarts separate.
        probesLog_.open((folder/"measurements.csv").c_str(),std::ios::app);
        actuatorLog_.open((folder/"actuators.csv").c_str(),std::ios::app);
        budgetLog_.open((folder/"oxygenBalance.csv").c_str(),std::ios::app);
        wallLog_.open((folder/"wallResolution.csv").c_str(),std::ios::app);
        flowLog_.open((folder/"flowMeasurements.csv").c_str(),std::ios::app);
        wallLog_<<std::setprecision(17)<<"time_s,patch,maxYPlus,maxStreamwisePlus,maxSpanwisePlus\n";
        flowLog_<<std::setprecision(17)<<"time_s";
        for(const word& name:names_) flowLog_<<','<<name<<"_Ux_m_s,"<<name<<"_Uy_m_s,"<<name<<"_Uz_m_s";
        flowLog_<<'\n';
        if(!probesLog_||!actuatorLog_||!budgetLog_||!wallLog_||!flowLog_) FatalErrorInFunction<<"Cannot open CSV output"<<exit(FatalError);
        probesLog_<<std::setprecision(17)<<"time_s";
        for(const word& name:names_) probesLog_<<','<<name<<"_mol_m3";
        probesLog_<<'\n';
        actuatorLog_<<std::setprecision(17)<<"time_s,requestedOmega_rad_s,appliedOmega_rad_s,requestedKLa_1_s,appliedKLa_1_s\n";
        budgetLog_<<std::setprecision(17)<<"time_s,inventory_mol,cumulativeSupply_mol,cumulativeUptake_mol,balanceResidual_mol,minC_mol_m3,maxC_mol_m3\n";
        if(mode_=="student") {
            fileName library=cfg_.lookup<fileName>("studentLibrary"); library.expand();
            plugin_=dlopen(library.c_str(),RTLD_NOW|RTLD_LOCAL);
            if(!plugin_) FatalErrorInFunction<<dlerror()<<exit(FatalError);
            auto create=reinterpret_cast<oxy::Controller*(*)()>(dlsym(plugin_,"createOxyController"));
            if(!create) FatalErrorInFunction<<"Missing createOxyController export"<<exit(FatalError);
            controller_.reset(create());
            List<scalar> saved=state_.lookupOrDefault<List<scalar>>("studentState",List<scalar>());
            if(!controller_) FatalErrorInFunction<<"Null student controller"<<exit(FatalError);
            try {controller_->restore(std::vector<double>(saved.begin(),saved.end()));}
            catch(const std::exception& e) {FatalErrorInFunction<<e.what()<<exit(FatalError);}
        }
    }
    if(mode_=="prescribed") {
        const List<vector> schedule(cfg_.lookup("schedule"));
        if(schedule.empty()) FatalErrorInFunction<<"Empty prescribed schedule"<<exit(FatalError);
        forAll(schedule,i) if(!std::isfinite(mag(schedule[i])) || (i && schedule[i].x()<=schedule[i-1].x()))
            FatalErrorInFunction<<"Schedule times must increase and values be finite"<<exit(FatalError);
    }
    if(runTime.value()==0) sampleAndControl();
    persist();
}
oxyControlFoam::~oxyControlFoam() { controller_.reset(); if(plugin_)dlclose(plugin_); }

void oxyControlFoam::sampleAndControl() {
    const scalar t=runTime.value(), elapsed=t-lastSample_;
    oxy::Observation observation{t,elapsed,{},applied_};
    forAll(names_,i) {
        scalar value=cells_[i]>=0?oxygen_[cells_[i]]:0;
        reduce(value,sumOp<scalar>());
        observation.probes.push_back({names_[i].c_str(),value});
    }
    oxy::Command requested=applied_;
    if(Pstream::master()) {
        if(mode_=="student" && t>=coefficient("controlStartTime",dimTime)) {
            try {requested=controller_->update(observation);}
            catch(const std::exception& e) {FatalErrorInFunction<<e.what()<<exit(FatalError);}
        }
        if(mode_=="prescribed") {
            const List<vector> rows(cfg_.lookup("schedule"));
            vector command=rows.first();
            forAll(rows,i) {
                if(t>=rows[i].x()) command=rows[i];
                if(i+1<rows.size() && t>=rows[i].x() && t<rows[i+1].x()) {
                    scalar f=(t-rows[i].x())/(rows[i+1].x()-rows[i].x());
                    command=(1-f)*rows[i]+f*rows[i+1]; break;
                }
            }
            requested={command.y(),command.z()};
        }
    }
    Pstream::scatter(requested.omega); Pstream::scatter(requested.kla);
    applied_.omega=oxy::limited(requested.omega,applied_.omega,coefficient("minOmega",dimless/dimTime),
        coefficient("maxOmega",dimless/dimTime),coefficient("omegaRate",dimless/sqr(dimTime)),elapsed);
    applied_.kla=oxy::limited(requested.kla,applied_.kla,coefficient("minKLa",dimless/dimTime),
        coefficient("maxKLa",dimless/dimTime),coefficient("klaRate",dimless/sqr(dimTime)),elapsed);
    if(Pstream::master()) {
        probesLog_<<t; for(const auto& probe:observation.probes)probesLog_<<','<<probe.concentration;
        probesLog_<<std::endl;
        actuatorLog_<<t<<','<<requested.omega<<','<<applied_.omega<<','<<requested.kla<<','<<applied_.kla<<std::endl;
    }
    if(Pstream::master())flowLog_<<t;
    forAll(names_,i) {
        vector velocity=cells_[i]>=0?U_[cells_[i]]:vector::zero;
        reduce(velocity,sumOp<vector>());
        if(Pstream::master())flowLog_<<','<<velocity.x()<<','<<velocity.y()<<','<<velocity.z();
    }
    if(Pstream::master())flowLog_<<std::endl;
    wallResolution();
    lastSample_=t;
}
void oxyControlFoam::persist() {
    state_.set("lastSample",lastSample_);state_.set("omega",applied_.omega);state_.set("kla",applied_.kla);
    state_.set("probePositions",locations_);state_.set("controller",mode_);state_.set("probeNames",names_);state_.set("sampleInterval",interval_);
    state_.set("initialInventory",initialInventory_);state_.set("cumulativeSupply",cumulativeSupply_);
    state_.set("cumulativeUptake",cumulativeUptake_);
    List<scalar> saved;
    if(Pstream::master() && controller_) {
        std::vector<double> data;
        try {data=controller_->save();}
        catch(const std::exception& e) {FatalErrorInFunction<<e.what()<<exit(FatalError);}
        saved.setSize(data.size());
        forAll(saved,i)saved[i]=data[i];
    }
    Pstream::scatter(saved);state_.set("studentState",saved);
}
void oxyControlFoam::wallResolution() {
    const tmp<volScalarField> tnu=viscosity->nu();
    const volScalarField& nu=tnu();
    forAll(mesh.boundary(),patchi) {
        const fvPatch& patch=mesh.boundary()[patchi];
        if(!isA<wallFvPatch>(patch))continue;
        const vectorField gradient(U_.boundaryField()[patchi].snGrad());
        const vectorField normals(patch.nf());
        scalar ymax=0,alongMax=0,acrossMax=0;
        forAll(patch,fi) {
            const scalar v=nu.boundaryField()[patchi][fi];
            const vector tangent=gradient[fi]-normals[fi]*(gradient[fi]&normals[fi]);
            const scalar speed=mag(tangent), friction=sqrt(v*speed);
            if(v<=0) FatalErrorInFunction<<"Positive viscosity required"<<exit(FatalError);
            ymax=max(ymax,friction/(v*patch.deltaCoeffs()[fi]));
            if(speed<=VSMALL)continue;
            const vector along=tangent/speed, across=normals[fi]^along;
            scalar amin=GREAT,amax=-GREAT,bmin=GREAT,bmax=-GREAT;
            const face& facePoints=mesh.faces()[patch.start()+fi];
            for(const label pointi:facePoints) {
                const point& x=mesh.points()[pointi];
                const scalar a=x&along,b=x&across;
                amin=min(amin,a);amax=max(amax,a);bmin=min(bmin,b);bmax=max(bmax,b);
            }
            alongMax=max(alongMax,(amax-amin)*friction/v);
            acrossMax=max(acrossMax,(bmax-bmin)*friction/v);
        }
        reduce(ymax,maxOp<scalar>());reduce(alongMax,maxOp<scalar>());reduce(acrossMax,maxOp<scalar>());
        if(Pstream::master())wallLog_<<runTime.value()<<','<<patch.name()<<','<<ymax<<','<<alongMax<<','<<acrossMax<<std::endl;
    }
}
void oxyControlFoam::momentumPredictor() {
    const dictionary& drive=cfg_.subDict("stirrer");
    const vector centre=dimensionedVector("centre",dimLength,drive).value();
    scalar tau=dimensionedScalar("tau",dimTime,drive).value();
    if(tau<=0) FatalErrorInFunction<<"stirrer tau must be positive"<<exit(FatalError);
    volScalarField rate(IOobject("stirrerRate",runTime.name(),mesh,IOobject::NO_READ,IOobject::NO_WRITE),
        mesh,dimensionedScalar(dimless/dimTime,0));
    volVectorField target(IOobject("stirrerTarget",runTime.name(),mesh,IOobject::NO_READ,IOobject::NO_WRITE),
        mesh,dimensionedVector(dimVelocity,vector::zero));
    const scalar seedEnd=dimensionedScalar("perturbationDuration",dimTime,drive).value();
    const scalar seed=dimensionedScalar("perturbationVelocity",dimVelocity,drive).value();
    const scalar halfHeight=dimensionedScalar("halfHeight",dimLength,drive).value();
    const scalar sourceRadius=dimensionedScalar("radius",dimLength,drive).value();
    forAll(rate,i) {
        vector r=mesh.C()[i]-centre;
        rate[i]=stirWeights_[i]/tau;
        target[i]=applied_.omega*vector(-r.y(),r.x(),0);
        if(runTime.value()<seedEnd && seedEnd>0) {
            scalar a=atan2(r.y(),r.x()), f=seed*sqr(1-runTime.value()/seedEnd)*min(scalar(1),sqrt(sqr(r.x())+sqr(r.y()))/sourceRadius);
            target[i]+=f*vector(sin(3*a),cos(2*a),sin(a+constant::mathematical::pi*r.z()/halfHeight));
        }
    }
    tUEqn=(fvm::ddt(U_)+fvm::div(phi,U_)+MRF.DDt(U_)+momentumTransport->divDevSigma(U_)
         +fvm::Sp(rate,U_) == rate*target+fvModels().source(U_));
    auto& eqn=tUEqn.ref();eqn.relax();fvConstraints().constrain(eqn);
    if(pimple.momentumPredictor()) {solve(eqn == -fvc::grad(p));fvConstraints().constrain(U_);}
}
void oxyControlFoam::postSolve() {
    const scalar dt=runTime.deltaTValue();
    // Do not advance oxygen across an interval that starts before activation.
    // Choose activation times on the fixed CFD time grid.
    if(oxy::activeStep(runTime.value(),dt,coefficient("oxygenStartTime",dimTime))) {
    const scalar D=coefficient("molecularDiffusivity",dimArea/dimTime);
    const scalar Sc=coefficient("turbulentSchmidt",dimless);
    volScalarField effectiveD("oxygenDiffusivity",dimensionedScalar(dimArea/dimTime,D)+momentumTransport->nut()/Sc);
    // First-order conservative transport/reaction splitting; oxygen uses Euler.
    fvScalarMatrix transport(fvm::ddt(oxygen_)+fvm::div(phi,oxygen_)-fvm::laplacian(effectiveD,oxygen_));
    transport.solve();
    if(gMin(oxygen_.primitiveField())<0) FatalErrorInFunction<<"Negative transported oxygen: refine timestep/mesh"<<exit(FatalError);
    scalar supply=0,uptake=0;
    scalar q=coefficient("maximumUptake",dimMoles/dimVolume/dimTime);
    if(oxy::activeStep(runTime.value(),dt,coefficient("demandChangeTime",dimTime)))q*=coefficient("demandMultiplier",dimless);
    if(q<0) FatalErrorInFunction<<"Negative oxygen demand"<<exit(FatalError);
    scalar sat=coefficient("saturation",dimMoles/dimVolume), half=coefficient("halfSaturation",dimMoles/dimVolume);
    forAll(oxygen_,i) {
        scalar a=applied_.kla*supplyWeights_[i];
        scalar next=oxy::monodStep(oxygen_[i],dt,a,sat,q,half);
        oxygen_[i]=next;
        supply+=dt*a*(sat-next)*mesh.V()[i];
        uptake+=dt*q*next/(half+next)*mesh.V()[i];
    }
    oxygen_.correctBoundaryConditions();
    reduce(supply,sumOp<scalar>());reduce(uptake,sumOp<scalar>());
    cumulativeSupply_+=supply;cumulativeUptake_+=uptake;
    } // Otherwise oxygen and both cumulative transfers remain frozen.
    scalar total=inventory();
    scalar low=gMin(oxygen_.primitiveField()),high=gMax(oxygen_.primitiveField());
    if(Pstream::master())budgetLog_<<runTime.value()<<','<<total<<','<<cumulativeSupply_<<','<<cumulativeUptake_
        <<','<<total-initialInventory_-cumulativeSupply_+cumulativeUptake_<<','<<low<<','<<high<<std::endl;
    if(runTime.value()-lastSample_>=interval_*(1-1e-9)) sampleAndControl();
    persist();
}
} }
