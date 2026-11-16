#pragma once
#include "EffectRack.h"
namespace drum {
inline juce::String sendDestinationText(const Kit& kit,const ReturnRouting& route){
    juce::String text=route.main?"Main":"";
    if(route.group.isNotEmpty())if(auto* group=findGroup(kit,route.group)){if(text.isNotEmpty())text+=" + ";text+="Group: "+group->name;}
    for(int n=0;n<6;++n)if(route.sends[(size_t)n]>sendFloor){if(text.isNotEmpty())text+=" + ";text+="Send "+juce::String::charToString('A'+n);}
    return text.isEmpty()?"None":text;
}
inline void showSendRoutingMenu(juce::Component& owner,KitLibrary& library,EffectRack& effects,const juce::String& kitId,const juce::String& target,std::function<void()> changed={}){
    auto state=library.snapshot();const auto* kit=activeKit(state);if(!kit || kit->id!=kitId)return;
    int source=-1;const auto* routes=routesFor(*kit,target,source);if(!routes)return;const auto route=(*routes)[(size_t)source];juce::PopupMenu menu;
    auto available=[&](bool main,const juce::String& group){auto candidate=*kit;int index=-1;auto* copy=const_cast<std::array<ReturnRouting,6>*>(routesFor(candidate,target,index));(*copy)[(size_t)index].main=main;(*copy)[(size_t)index].group=group;return validProcessingRouting(candidate);};
    menu.addItem(1,"Main",available(true,{}),route.main && route.group.isEmpty());menu.addItem(2,"No direct output",true,!route.main && route.group.isEmpty());
    std::vector<juce::String> groups;
#if JUCE_MAC
    for(const auto& group:kit->groups){groups.push_back(group.id);menu.addItem(100+(int)groups.size()-1,"Group: "+group.name,available(false,group.id),route.group==group.id);}

#endif
    menu.addSeparator();
    for(int n=0;n<6;++n){auto candidate=*kit;int index=-1;auto* copy=const_cast<std::array<ReturnRouting,6>*>(routesFor(candidate,target,index));bool on=route.sends[(size_t)n]>sendFloor;(*copy)[(size_t)index].sends[(size_t)n]=on?sendFloor:0;menu.addItem(10+n,"Send "+juce::String::charToString('A'+n)+(on?" (on)":""),validProcessingRouting(candidate),on);}
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&owner),[safe=juce::Component::SafePointer<juce::Component>(&owner),&library,&effects,kitId,target,groups,route,changed](int n){if(!safe || n==0)return;Result result;
        if(n==1 || n==2)result=library.setSendDestination(kitId,target,n==1,{});
        else if(n>=100 && n<100+(int)groups.size())result=library.setSendDestination(kitId,target,false,groups[(size_t)(n-100)]);
        else if(n>=10 && n<16)result=library.setSendRoute(kitId,target,n-10,route.sends[(size_t)(n-10)]>sendFloor?sendFloor:0);
        if(result.ok){effects.routingChanged();if(changed)changed();}
    });
}
}
