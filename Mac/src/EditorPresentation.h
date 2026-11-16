#pragma once
namespace djl
{
struct EditorPlacement {
    int x=0,y=0,width=0,height=0;bool positioned=false;
    bool validSize() const{return width>=800 && width<=10000 && height>=600 && height<=4000;}
};
struct EditorPresentation {
    juce::CriticalSection lock;
    EditorPlacement hosted,floating;
    bool mixer=false;juce::String group;
    void save(juce::XmlElement& session){
        const juce::ScopedLock guard(lock);auto* xml=session.createNewChildElement("MacEditor");xml->setAttribute("mixer",mixer);xml->setAttribute("group",group);
        for(auto [name,placement]:{std::pair{"Hosted",hosted},std::pair{"Floating",floating}}){auto* item=xml->createNewChildElement(name);item->setAttribute("x",placement.x);item->setAttribute("y",placement.y);item->setAttribute("width",placement.width);item->setAttribute("height",placement.height);item->setAttribute("positioned",placement.positioned);}
    }
    void restore(const juce::XmlElement& session){
        const juce::ScopedLock guard(lock);hosted={};floating={};mixer=false;group.clear();auto* xml=session.getChildByName("MacEditor");if(!xml)return;
        mixer=xml->getBoolAttribute("mixer");group=xml->getStringAttribute("group").substring(0,128);
        for(auto [name,placement]:{std::pair{"Hosted",&hosted},std::pair{"Floating",&floating}})if(auto* item=xml->getChildByName(name)){
            EditorPlacement read{item->getIntAttribute("x"),item->getIntAttribute("y"),item->getIntAttribute("width"),item->getIntAttribute("height"),item->getBoolAttribute("positioned")};
            if(read.validSize() && std::abs((double)read.x)<100000 && std::abs((double)read.y)<100000)*placement=read;
        }
    }
};
bool restoreEditorPlacement(juce::Component&,const EditorPlacement&);
bool captureEditorPlacement(juce::Component&,EditorPlacement&);
}
