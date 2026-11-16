// Included inside SampleEngine::mixEffects.

    groupAudio.setSize(maxGroups*2,frames,false,false,true);groupAudio.clear();
    for(int note=firstNote;note<firstNote+maxInstruments;++note)
    {
        const auto& c=renderControls[(size_t)note];if(!current->samples[(size_t)note] || !c.audible)continue;
        const int input=(note-firstNote)*2;
        auto write=[&](int bus,float gain){
            const int channel=busChannels[(size_t)bus];if(channel<0 || channel>=output.getNumChannels())return;
            if(bus==0 && mainWidth==1)
            {
                output.addFrom(channel,offset,instrumentAudio,input,0,frames,gain*0.5f);
                output.addFrom(channel,offset,instrumentAudio,input+1,0,frames,gain*0.5f);
            }
            else if(channel+1<output.getNumChannels())
                for(int lane=0;lane<2;++lane)output.addFrom(channel+lane,offset,instrumentAudio,input+lane,0,frames,gain);
        };
        for(int lane=0;lane<2;++lane)groupAudio.addFrom(c.groupIndex*2+lane,0,instrumentAudio,input+lane,0,frames);
        const int destination=c.sound.output>0?(int)c.sound.output:c.groupMixer.output;
        if(!current->returnSolo && c.sound.output>0)write(busChannels[(size_t)destination]>=0?destination:0,1);
    }
    // Group buses receive member dry paths after their chains and send taps,
    // plus any return explicitly routed to this group. Each has its own chain.
    for(int group=0;group<current->groupCount;++group){const int input=(groupBusBase+group*7)*2;const int destination=current->groupMixers[(size_t)group].output;const int channel=busChannels[(size_t)destination]>=0?busChannels[(size_t)destination]:0;
        for(int lane=0;lane<2;++lane){groupAudio.copyFrom(group*2+lane,0,instrumentAudio,input+lane,0,frames);if(!current->returnSolo){if(channel==0 && mainWidth==1)output.addFrom(0,offset,instrumentAudio,input+lane,0,frames,.5f);else if(channel+lane<output.getNumChannels())output.addFrom(channel+lane,offset,instrumentAudio,input+lane,0,frames);}}
        for(int send=0;send<6;++send){const int target=current->groupReturnMixers[(size_t)(group*6+send)].output;const int channel=busChannels[(size_t)target]>=0?busChannels[(size_t)target]:0;for(int lane=0;lane<2;++lane){const int source=input+2+send*2+lane;if(channel==0 && mainWidth==1)output.addFrom(0,offset,instrumentAudio,source,0,frames,.5f);else if(channel+lane<output.getNumChannels())output.addFrom(channel+lane,offset,instrumentAudio,source,0,frames);}}

    }
    // Local returns are already summed and processed by the shared graph.
    for(int send=0;send<6;++send){const int dest=current->returnMixers[(size_t)send].output;const int channel=busChannels[(size_t)dest]>=0?busChannels[(size_t)dest]:0;
        for(int lane=0;lane<2;++lane){for(int group=0;group<current->groupCount;++group){const int source=instrumentReturnLane(group,send)*2+lane;if(channel==0 && mainWidth==1)output.addFrom(0,offset,instrumentAudio,source,0,frames,.5f);else if(output.getNumChannels()>channel+lane)output.addFrom(channel+lane,offset,instrumentAudio,source,0,frames);}}}
    const float decay=std::exp(-(float)frames/((float)hostRate*.3f));
    for(int group=0;group<maxGroups;++group){if(resetGroupHolds[(size_t)group].exchange(false))groupHoldStates[(size_t)group]={};for(int lane=0;lane<2;++lane){float value=groupAudio.getMagnitude(group*2+lane,0,frames);auto& peak=groupMeters[(size_t)group][(size_t)lane];peak.store(juce::jmax(value,peak.load()*decay));retainMaximum(groupMaxima[(size_t)group][(size_t)lane],value);groupHolds[(size_t)group][(size_t)lane].store(groupHoldStates[(size_t)group][(size_t)lane].process(value,frames,hostRate,peakHoldSeconds.load()));}}
