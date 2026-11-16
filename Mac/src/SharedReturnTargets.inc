// Instantiate shared return chains only for groups which use them. Retain an
// activated chain until recall/kit change so turning a send off preserves tails.
    if(reload || graphEpoch.load()!=state.kitEpoch)activeSharedReturns.clear();
    std::vector<Instrument> groupSharedReturns;
    groupSharedReturns.reserve(kit->groups.size()*6);
    constexpr std::array<double SoundParameters::*,6> sendMembers{&SoundParameters::sendA,&SoundParameters::sendB,&SoundParameters::sendC,&SoundParameters::sendD,&SoundParameters::sendE,&SoundParameters::sendF};
    for(int g=0;g<(int)kit->groups.size();++g){
        const auto& group=kit->groups[(size_t)g];std::array<bool,6> used{};
        for(const auto& instrument:group.instruments)for(int r=0;r<6;++r){
            used[(size_t)r]|=instrument.controls.sound.*sendMembers[(size_t)r]>sendFloor;
            for(const auto& macro:instrument.controls.macros)if(macro.target>3 && macro.target<(int)std::size(macroTargets) && macroTargets[macro.target].member==sendMembers[(size_t)r] && juce::jmax(macro.minimum,macro.maximum)>sendFloor)used[(size_t)r]=true;
        }
        for(int pass=0;pass<6;++pass)for(int a=0;a<6;++a)if(used[(size_t)a])for(int b=0;b<6;++b)if(kit->returnRouting[(size_t)a].sends[(size_t)b]>sendFloor)used[(size_t)b]=true;
        for(int r=0;r<6;++r){
            const auto key=group.id+":"+juce::String(r);
            if(used[(size_t)r])activeSharedReturns.addIfNotAlreadyThere(key);
            auto copy=kit->returns[(size_t)r];copy.note=firstNote+instrumentReturnLane(g,r);
            if(!activeSharedReturns.contains(key))copy.effects.clear();
            for(auto& slot:copy.effects)slot.id="group-shared:"+group.id+":"+slot.id;
            groupSharedReturns.push_back(std::move(copy));
        }
    }
    for(const auto& target:groupSharedReturns)targets.push_back(&target);
