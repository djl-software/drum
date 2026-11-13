#pragma once
#include "KitLibrary.h"
#include <set>
namespace drum {
using MidiSelection=std::set<juce::String>;
struct MidiEditing {
    static bool chosen(const PatternHit& h,const MidiSelection& ids){return ids.empty() || ids.contains(h.id);}
    static double snap(double beat,double grid){return grid>0?std::round(beat/grid)*grid:beat;}
    static void clamp(PatternHit& h,const Pattern& p){h.start=juce::jlimit(0.0,juce::jmax(0.0,p.end()-1.0/960),h.beat());h.duration=juce::jlimit(1.0/960,p.end()-h.start,h.duration);}
    static std::vector<juce::String> rows(const Kit& kit){std::vector<juce::String> ids;for(const auto& g:kit.groups)for(const auto& i:g.instruments)ids.push_back(i.id);return ids;}
    static void apply(Pattern& p,const Kit& kit,const MidiSelection& ids,int operation,double grid,double amount=1){
        auto pitches=rows(kit);double lo=p.end(),hi=0;int lowPitch=127,highPitch=-1;
        for(const auto& h:p.hits)if(chosen(h,ids)){lo=juce::jmin(lo,h.beat());hi=juce::jmax(hi,h.beat()+h.duration);auto it=std::find(pitches.begin(),pitches.end(),h.instrumentId);int row=(int)std::distance(pitches.begin(),it);lowPitch=juce::jmin(lowPitch,row);highPitch=juce::jmax(highPitch,row);}
        if(hi<=lo)return;juce::Random random;
        if(operation==8){auto copy=p.hits;double offset=juce::jmax(grid,std::ceil((hi-lo)/juce::jmax(grid,1.0/960))*juce::jmax(grid,1.0/960));p.steps=juce::jmin(4096,juce::jmax(p.steps,(int)std::ceil((hi+offset)*4)));for(auto h:copy)if(chosen(h,ids)){h.id=juce::Uuid().toString();h.start=h.beat()+offset;if(h.start<p.end()){clamp(h,p);p.hits.push_back(h);}}return;}
        if(operation==9){auto copy=p.hits;p.hits.clear();for(auto h:copy){if(!chosen(h,ids)){p.hits.push_back(h);continue;}double end=h.beat()+h.duration;double start=h.beat();double division=grid>0?grid:.25;while(start<end-1.e-8 && p.hits.size()<8192){auto part=h;part.id=juce::Uuid().toString();part.start=start;part.duration=juce::jmin(division,end-start);p.hits.push_back(part);start+=division;}}return;}
        if(operation==10){std::vector<PatternHit> joined;for(auto h:p.hits){if(!chosen(h,ids)){joined.push_back(h);continue;}auto found=std::find_if(joined.begin(),joined.end(),[&](const auto& n){return chosen(n,ids) && n.instrumentId==h.instrumentId;});if(found==joined.end())joined.push_back(h);else{double end=juce::jmax(found->beat()+found->duration,h.beat()+h.duration);found->start=juce::jmin(found->beat(),h.beat());found->duration=end-found->start;}}p.hits=std::move(joined);return;}
        auto original=p.hits;
        if(operation==6)p.steps=juce::jmin(4096,juce::jmax(p.steps,(int)std::ceil((lo+(hi-lo)*2)*4)));
        for(auto& h:p.hits)if(chosen(h,ids)){
            auto row=(int)std::distance(pitches.begin(),std::find(pitches.begin(),pitches.end(),h.instrumentId));
            switch(operation){
            case 1:h.start=h.beat()+(snap(h.beat(),grid)-h.beat())*amount;break;
            case 2:{double end=h.beat()+h.duration;h.duration=juce::jmax(1.0/960,end+(snap(end,grid)-end)*amount-h.beat());break;}
            case 3:{double next=p.end();for(const auto& n:original)if(n.instrumentId==h.instrumentId && n.beat()>h.beat()+1.e-8)next=juce::jmin(next,n.beat());h.duration=next-h.beat();break;}
            case 4:h.start=lo+hi-(h.beat()+h.duration);break;
            case 5:if(lowPitch+highPitch-row<(int)pitches.size())h.instrumentId=pitches[(size_t)(lowPitch+highPitch-row)];break;
            case 6:h.start=lo+(h.beat()-lo)*2;h.duration*=2;break;
            case 7:h.start=lo+(h.beat()-lo)*.5;h.duration*=.5;break;
            case 11:h.enabled=!h.enabled;break;
            case 12:h.velocity=juce::jlimit(1,127,h.velocity+random.nextInt(2*(int)(amount*63)+1)-(int)(amount*63));break;
            case 13:h.chance=juce::jlimit(0.0,1.0,h.chance+(random.nextDouble()*2-1)*amount);break;
            case 14:h.start=h.beat()+(random.nextDouble()*2-1)*juce::jmax(grid,.0625)*amount*.5;break;
            case 15:if(row+1<(int)pitches.size())h.instrumentId=pitches[(size_t)(row+1)];break;
            case 16:if(row>0)h.instrumentId=pitches[(size_t)(row-1)];break;
            case 17:h.start=h.beat()+juce::jmax(grid,1.0/960);break;
            case 18:h.start=h.beat()-juce::jmax(grid,1.0/960);break;
            case 19:h.duration+=juce::jmax(grid,1.0/960);break;
            case 20:h.duration-=juce::jmax(grid,1.0/960);break;
            default:break;
            }clamp(h,p);
        }
    }
};
}
