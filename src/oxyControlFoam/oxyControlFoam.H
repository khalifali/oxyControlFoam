// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "incompressibleFluid.H"
#include "IOdictionary.H"
#include "localIOdictionary.H"
#include "Controller.hpp"
#include <fstream>
#include <memory>

namespace Foam { namespace solvers {
class oxyControlFoam : public incompressibleFluid {
    IOdictionary cfg_;
    localIOdictionary state_;
    volScalarField oxygen_;
    word mode_;
    wordList names_;
    vectorField locations_;
    labelList cells_;
    scalarField stirWeights_, supplyWeights_;
    scalar interval_, lastSample_, initialInventory_, cumulativeSupply_, cumulativeUptake_;
    oxy::Command applied_;
    std::unique_ptr<oxy::Controller> controller_;
    void* plugin_=nullptr;
    std::ofstream probesLog_, actuatorLog_, budgetLog_, wallLog_, flowLog_;
    scalar inventory() const;
    scalar coefficient(const word&,const dimensionSet&) const;
    scalarField weights(const dictionary&) const;
    void sampleAndControl();
    void persist();
    void wallResolution();
public:
    TypeName("oxyControlFoam");
    explicit oxyControlFoam(fvMesh&);
    ~oxyControlFoam() override;
    void momentumPredictor() override;
    void postSolve() override;
};
} }
