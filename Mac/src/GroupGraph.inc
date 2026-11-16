// Included inside EffectRack::sync; uses the same graph and latency compensation.
    std::array<Graph::NodeID,processingLanes> entrances{},exits{};
    std::array<double,processingLanes> tails{};
    for(auto& muted:mutedSidechainSources)muted.store(false);
    for(auto& node:groupFaders)graph.removeNode(node,Graph::UpdateKind::none);
    groupFaders.clear();
    const int laneCount=macInstrumentReturnBase+(int)kit->groups.size()*6;
    for(int lane=0;lane<laneCount;++lane){
        Graph::NodeID previous=input->nodeID;int previousChannel=lane*2;double chainTail=0;
        entrances[(size_t)lane]=splitters[(size_t)lane]->nodeID;
        for(const auto& [assigned,slots]:next)if(assigned==firstNote+lane)for(const auto& slot:slots){
            auto& runtime=*nodes.at(slot.id);if(slot.bypassed || !runtime.node)continue;
            if(previous==input->nodeID)entrances[(size_t)lane]=runtime.node->nodeID;
            for(int c=0;c<2;++c)graph.addConnection({{previous,previousChannel+c},{runtime.node->nodeID,c}},Graph::UpdateKind::none);
            if(runtime.processor()->hasSidechain && slot.sidechainNote>=firstNote){for(int c=0;c<2;++c)graph.addConnection({{input->nodeID,(slot.sidechainNote-firstNote)*2+c},{runtime.node->nodeID,2+c}},Graph::UpdateKind::none);if(slot.sidechainMute && slot.sidechainNote!=firstNote+lane)mutedSidechainSources[(size_t)(slot.sidechainNote-firstNote)].store(true);}
            previous=runtime.node->nodeID;previousChannel=0;const double seconds=runtime.processor()->plugin->getTailLengthSeconds();chainTail+=std::isfinite(seconds)?juce::jlimit(0.0,3600.0,seconds):3600;
        }
        auto split=splitters[(size_t)lane];for(int c=0;c<2;++c)graph.addConnection({{previous,previousChannel+c},{split->nodeID,c}},Graph::UpdateKind::none);
        exits[(size_t)lane]=split->nodeID;tails[(size_t)lane]=chainTail;
    }
    std::vector<std::pair<int,int>> edges;
    auto connect=[&](int source,int sourceChannel,int target){edges.emplace_back(source,target);for(int c=0;c<2;++c)graph.addConnection({{exits[(size_t)source],sourceChannel+c},{entrances[(size_t)target],c}},Graph::UpdateKind::none);};
    // Each terminal branch gets its source group's fader after all effects.
    // Returns feeding their own group are scaled only by that group's output.
    auto faded=[&](int source,int owner){
        auto node=graph.addNode(std::make_unique<GroupFader>(mixerLevels[(size_t)(macGroupFaderBase+owner)]),{},Graph::UpdateKind::none);
        groupFaders.push_back(node);
        for(int c=0;c<2;++c)graph.addConnection({{exits[(size_t)source],c},{node->nodeID,c}},Graph::UpdateKind::none);
        return node->nodeID;
    };
    auto write=[&](int source,int owner){auto node=faded(source,owner);for(int c=0;c<2;++c)graph.addConnection({{node,c},{output->nodeID,source*2+c}},Graph::UpdateKind::none);};
    for(int n=0;n<(int)kit->groups.size();++n){const auto& group=kit->groups[(size_t)n];const int base=groupBusBase+n*7;write(base,n);
        for(const auto& instrument:group.instruments){const int lane=instrument.note-firstNote;
            if(instrument.controls.sound.output==0)connect(lane,0,base);else write(lane,n);
            for(int send=0;send<6;++send){connect(lane,2+send*2,instrumentReturnLane(n,send));connect(lane,2+send*2,maxInstruments+send);}
        }
        for(int r=0;r<6;++r)if(group.sends[(size_t)r]>sendFloor)connect(base,2+r*2,base+1+r);
    }
    auto wireReturns=[&](const auto& routes,int base,int owner){for(int a=0;a<6;++a){const auto& route=routes[(size_t)a];if(route.main)write(base+a,owner);
        if(route.group.isNotEmpty())for(int g=0;g<(int)kit->groups.size();++g)if(kit->groups[(size_t)g].id==route.group){
            const int target=groupBusBase+g*7;
            if(g==owner)connect(base+a,0,target);
            else {edges.emplace_back(base+a,target);auto node=faded(base+a,owner);for(int c=0;c<2;++c)graph.addConnection({{node,c},{entrances[(size_t)target],c}},Graph::UpdateKind::none);}
        }
        for(int b=0;b<6;++b)if(route.sends[(size_t)b]>sendFloor)connect(base+a,2+b*2,base+b);
    }};
    for(int g=0;g<(int)kit->groups.size();++g){wireReturns(kit->returnRouting,instrumentReturnLane(g,0),g);wireReturns(kit->groups[(size_t)g].returnRouting,groupBusBase+g*7+1,g);}
    // The editable shared instances monitor the combined send input; only the
    // per-group instances produce routed audio, preserving independent tails.
    for(int a=0;a<6;++a)for(int b=0;b<6;++b)if(kit->returnRouting[(size_t)a].sends[(size_t)b]>sendFloor)connect(maxInstruments+a,2+b*2,maxInstruments+b);
    auto pathTails=tails;std::array<int,processingLanes> degree{};for(auto [source,target]:edges)++degree[(size_t)target];std::vector<int> ready;for(int n=0;n<laneCount;++n)if(!degree[(size_t)n])ready.push_back(n);
    while(!ready.empty()){int source=ready.back();ready.pop_back();for(auto [from,target]:edges)if(from==source){pathTails[(size_t)target]=juce::jmax(pathTails[(size_t)target],pathTails[(size_t)source]+tails[(size_t)target]);if(--degree[(size_t)target]==0)ready.push_back(target);}}
    double longestTail=0;for(double seconds:pathTails)longestTail=juce::jmax(longestTail,seconds);
    graph.rebuild();layout=std::move(next);routingTopology=topology;graphEpoch.store(state.kitEpoch);hasEffects.store(true);tail.store(longestTail);latency(graph.getLatencySamples());
