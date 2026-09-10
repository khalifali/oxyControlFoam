// Test-only state probe; the educational controller remains independent.
#include "Controller.hpp"
class StateProbe final : public oxy::Controller {
    double count_=0;
public:
    oxy::Command update(const oxy::Observation& observation) override {
        ++count_;
        return observation.applied;
    }
    std::vector<double> save() const override {return {count_};}
    void restore(const std::vector<double>& state) override {
        if(state.empty()){count_=0;return;}
        if(state.size()!=1)throw std::runtime_error("Bad state probe checkpoint");
        count_=state[0];
    }
};
extern "C" oxy::Controller* createOxyController() {return new StateProbe;}
