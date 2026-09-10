// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/controller/Controller.hpp"
#include <cassert>
#include <iostream>
#include <limits>
int main() {
    // Warm-up must leave oxygen AND inventory accounting exactly unchanged.
    double c=.15, supply=0, uptake=0; const double dt=.001, start=.01;
    for(int i=1;i<=20;++i) {
        const double old=c;
        if(oxy::activeStep(i*dt,dt,start)) {
            c=oxy::monodStep(c,dt,.02,.25,.002,.01);
            supply+=dt*.02*(.25-c); uptake+=dt*.002*c/(.01+c);
        }
        if(i<=10)assert(c==.15 && supply==0 && uptake==0);
        assert(std::abs(c-.15-supply+uptake)<1e-14);
        if(i>10)assert(c!=old);
    }
    assert(oxy::activeStep(dt,dt,0));
    assert(!oxy::activeStep(start,dt,start));
    assert(oxy::activeStep(start+dt,dt,start));
    // Check the implicit reaction equation over stiff uptake and supply ranges.
    for(double q:{0.,.002,1.,100.})for(double a:{0.,.02,10.})for(double step:{1e-4,1.,100.}) {
        double x=oxy::monodStep(.15,step,a,.25,q,.01);
        assert(x>=0 && x<=.25);
        double residual=x-.15-step*(a*(.25-x)-q*x/(.01+x));
        assert(std::abs(residual)<1e-9);
    }
    // Pure transfer approaches its exact exponential solution as dt is halved.
    auto error=[](int n){double x=.15;for(int i=0;i<n;++i)x=oxy::monodStep(x,1./n,2.,.25,0.,.01);
        return std::abs(x-(.25-(.25-.15)*std::exp(-2.)));};
    assert(error(200)<.51*error(100));
    assert(oxy::monodStep(0,1,0,.25,1,.01)==0);
    assert(oxy::limited(100,30,0,60,20,.1)==32);
    assert(oxy::limited(-100,30,0,60,20,.1)==28);
    bool failed=false;try{oxy::limited(std::numeric_limits<double>::quiet_NaN(),0,0,1,1,1);}catch(...){failed=true;}assert(failed);
    std::cout<<"PASS: activation, frozen warm-up, reaction balance/positivity/convergence, actuator limits\n";
}
