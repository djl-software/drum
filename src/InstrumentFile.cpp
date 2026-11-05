#include "PluginProcessor.h"
#include <set>

namespace {
constexpr juce::int64 maxManifest = 256ll * 1024 * 1024;
constexpr juce::int64 maxSample = 512ll * 1024 * 1024;

juce::String fingerprint(const juce::File& file)
{
    auto stream = file.createInputStream();
    if (!stream) return {};
    uint64_t hash = 14695981039346656037ull;
    char bytes[65536];
    for (;;) {
        const int count = stream->read(bytes, sizeof(bytes));
        if (count <= 0) break;
        for (int n = 0; n < count; ++n) { hash ^= (unsigned char) bytes[n]; hash *= 1099511628211ull; }
    }
    return juce::String::toHexString((juce::int64) hash) + "-" + juce::String(file.getSize());
}

bool readEntry(juce::ZipFile& zip, int index, juce::OutputStream& out, juce::int64 limit)
{
    const auto* entry = zip.getEntry(index);
    if (!entry || entry->isSymbolicLink || entry->uncompressedSize < 0 || entry->uncompressedSize > limit) return false;
    std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(index));
    if (!stream) return false;
    const auto written = out.writeFromInputStream(*stream, entry->uncompressedSize + 1);
    return written == entry->uncompressedSize;
}
}

drum::Result DrumAudioProcessor::exportInstrument(const juce::String& kitId, const juce::String& instrumentId, const juce::File& file)
{
    effects.capture();
    auto snapshot = library.snapshot();
    const drum::Kit* kit = nullptr;
    for (const auto& candidate : snapshot.kits) if (candidate.id == kitId) kit = &candidate;
    const auto* sound = kit ? drum::findInstrument(*kit, instrumentId) : nullptr;
    if (!sound || drum::returnIndex(instrumentId) >= 0) return drum::Result::fail("Select an instrument to export.");
    juce::File sample;
    if (sound->samplePath.isNotEmpty()) {
        sample = juce::File(sound->samplePath);
        if (!sample.existsAsFile()) return drum::Result::fail("Relink the missing sample before exporting this instrument.");
        if (sample.getSize() > maxSample) return drum::Result::fail("Instrument samples must be 512 MiB or smaller.");
        if (sample == file) return drum::Result::fail("Choose a different file; the original sample cannot be overwritten.");
    }
    auto state = library.toXml();
    // Reuse the canonical state serializer, retaining precisely one instrument.
    for (auto* child = state->getFirstChildElement(); child;) {
        auto* next = child->getNextElement();
        if (child->getStringAttribute("id") != kitId) state->removeChildElement(child, true);
        child = next;
    }
    state->setAttribute("activeKit", kitId);
    auto* savedKit = state->getFirstChildElement();
    for (auto* group = savedKit->getFirstChildElement(); group;) {
        auto* next = group->getNextElement();
        bool keep = false;
        if (group->hasTagName("Group")) {
            for (auto* item = group->getFirstChildElement(); item;) {
                auto* nextItem = item->getNextElement();
                if (item->getStringAttribute("id") == instrumentId) keep = true;
                else group->removeChildElement(item, true);
                item = nextItem;
            }
        }
        if (!keep) savedKit->removeChildElement(group, true);
        group = next;
    }
    auto* savedSound = savedKit->getChildByName("Group")->getChildByName("Instrument");
    const auto sampleEntry = sample == juce::File() ? juce::String() : "sample" + sample.getFileExtension().toLowerCase();
    savedSound->setAttribute("samplePath", sampleEntry);
    juce::XmlElement document("DrumInstrument");
    document.setAttribute("version", 1);
    document.setAttribute("sample", sampleEntry);
    document.setAttribute("sampleName", sample.getFileName());
    if (sample != juce::File()) {
        const auto hash = fingerprint(sample);
        if (hash.isEmpty()) return drum::Result::fail("Cannot read the instrument sample.");
        document.setAttribute("sampleHash", hash);
    }
    document.addChildElement(state.release());
    auto* references = document.createNewChildElement("SidechainSources");
    for (const auto& slot : sound->effects) if (slot.sidechainNote >= 0) {
        for (const auto& group : kit->groups) for (const auto& source : group.instruments) if (source.note == slot.sidechainNote) {
            auto* ref = references->createNewChildElement("Source");
            ref->setAttribute("effect", slot.id);
            ref->setAttribute("id", source.id);
            ref->setAttribute("name", source.name);
            ref->setAttribute("self", source.id == sound->id);
            ref->setAttribute("mute",slot.sidechainMute);
        }
    }
    const auto xml = document.toString();
    if (xml.getNumBytesAsUTF8() > maxManifest) return drum::Result::fail("Instrument settings exceed the file size limit.");
    if (!file.getParentDirectory().createDirectory()) return drum::Result::fail("Cannot create the export folder.");
    juce::ZipFile::Builder archive;
    archive.addEntry(new juce::MemoryInputStream(xml.toRawUTF8(), xml.getNumBytesAsUTF8(), true), 6, "Instrument.xml", juce::Time::getCurrentTime());
    if (sample != juce::File()) archive.addFile(sample, 0, sampleEntry);
    juce::TemporaryFile temporary(file);
    {
        juce::FileOutputStream output(temporary.getFile());
        if (!output.openedOk() || !archive.writeToStream(output, nullptr)) return drum::Result::fail("Could not write the instrument file.");
        output.flush();
        if (output.getStatus().failed()) return drum::Result::fail("The export could not be completed.");
    }
    if (!temporary.overwriteTargetFileWithTemporary()) return drum::Result::fail("Cannot replace the export file. The previous file was kept.");
    return drum::Result::success(instrumentId);
}

drum::Result DrumAudioProcessor::importInstrumentFile(const juce::String& kitId, const juce::String& groupId, const juce::File& file)
{
    auto targetState = library.snapshot();
    const drum::Kit* target = nullptr;
    for (const auto& candidate : targetState.kits) if (candidate.id == kitId) target = &candidate;
    if (trackInstance && !hasOpenKit()) return drum::Result::fail("Create or open a kit before importing an instrument.");
    if (!target || !drum::findGroup(*target, groupId)) return drum::Result::fail("Choose a destination group first.");
    if (drum::instrumentCount(*target) >= drum::maxInstruments) return drum::Result::fail("This collection already has 88 instruments.");
    if (!file.existsAsFile() || file.getSize() > maxManifest + maxSample + 1024 * 1024) return drum::Result::fail("Instrument file is missing or too large.");
    juce::ZipFile archive(file);
    if (archive.getNumEntries() < 1 || archive.getNumEntries() > 2) return drum::Result::fail("Not a supported Drum instrument file.");
    std::set<juce::String> names;
    for (int n = 0; n < archive.getNumEntries(); ++n) {
        const auto* entry = archive.getEntry(n);
        if (!entry || entry->isSymbolicLink || !names.insert(entry->filename).second) return drum::Result::fail("Invalid instrument archive.");
    }
    juce::MemoryOutputStream manifest;
    if (!readEntry(archive, archive.getIndexOfFileName("Instrument.xml"), manifest, maxManifest)) return drum::Result::fail("Instrument settings are missing or damaged.");
    auto document = juce::parseXML(manifest.toUTF8());
    if (!document || !document->hasTagName("DrumInstrument") || document->getIntAttribute("version") != 1) return drum::Result::fail("Unsupported instrument file version.");
    auto* state = document->getChildByName("DrumState");
    if (!state || state->getNumChildElements() != 1) return drum::Result::fail("Invalid instrument settings.");
    auto* savedKit = state->getChildByName("Kit");
    if (!savedKit || savedKit->getNumChildElements() != 1) return drum::Result::fail("Instrument file must contain one instrument group.");
    auto* savedGroup = savedKit->getChildByName("Group");
    auto* savedSound = savedGroup ? savedGroup->getChildByName("Instrument") : nullptr;
    if (!savedSound || savedGroup->getNumChildElements() != 1) return drum::Result::fail("Instrument file must contain one instrument.");
    const auto entryName = document->getStringAttribute("sample");
    if (savedSound->getStringAttribute("samplePath") != entryName) return drum::Result::fail("Invalid sample reference.");
    const bool hasSample = entryName.isNotEmpty();
    if (archive.getNumEntries() != (hasSample ? 2 : 1) || (hasSample && (!entryName.startsWith("sample.") || entryName.containsAnyOf("/\\:") || entryName.contains("..") || archive.getIndexOfFileName(entryName) < 0))) return drum::Result::fail("Invalid sample entry.");
    auto sampleName=juce::File::createLegalFileName(document->getStringAttribute("sampleName"));
    if(sampleName.isEmpty() || sampleName=="." || sampleName=="..")sampleName=hasSample?entryName:"sample.wav";
    const auto sampleFile = drum::PortableFiles::root().getChildFile("Samples").getChildFile("Imported-" + juce::Uuid().toString()).getChildFile(sampleName);
    savedSound->setAttribute("samplePath", hasSample ? sampleFile.getFullPathName() : juce::String());
    drum::KitLibrary decoded;
    if (!decoded.restore(*state)) return drum::Result::fail("Instrument settings are invalid. Nothing was imported.");
    auto decodedState = decoded.snapshot();
    auto sound = decodedState.kits[0].groups[0].instruments[0];
    // Preserve sidechain controls, reconnecting sources by identity/name rather
    // than accidentally selecting another sound at the same MIDI note.
    int freeNote = drum::firstNote;
    std::set<int> used;
    for (const auto& group : target->groups) for (const auto& item : group.instruments) used.insert(item.note);
    while (used.contains(freeNote)) ++freeNote;
    int unresolved = 0;
    auto* references = document->getChildByName("SidechainSources");
    for (auto& slot : sound.effects) {
        const juce::XmlElement* reference = nullptr;
        if (references) for (auto* ref : references->getChildIterator()) if (ref->getStringAttribute("effect") == slot.id) reference = ref;
        if(!reference && slot.sidechainNote<0)continue;
        int resolved = -1;
        if (reference && reference->getBoolAttribute("self")) resolved = freeNote;
        else if (reference) {
            int matches = 0, named = -1;
            for (const auto& group : target->groups) for (const auto& item : group.instruments) {
                if (item.id == reference->getStringAttribute("id")) resolved = item.note;
                if (item.name == reference->getStringAttribute("name")) { named = item.note; ++matches; }
            }
            if (resolved < 0 && matches == 1) resolved = named;
        }
        slot.sidechainNote = resolved;
        if(reference && resolved>=0)slot.sidechainMute=reference->getBoolAttribute("mute",slot.sidechainMute);
        if (resolved < 0) { slot.sidechainMute = false; ++unresolved; }
    }
    drum::KitLibrary candidate;
    if (!candidate.restore(*library.toXml()) || !candidate.importInstrument(kitId, groupId, sound, true).ok) return drum::Result::fail("This instrument would exceed the collection's storage or keyboard limits.");
    if (hasSample) {
        if (!sampleFile.getParentDirectory().createDirectory()) return drum::Result::fail("Cannot create the imported sample folder.");
        juce::TemporaryFile temporary(sampleFile);
        {
            juce::FileOutputStream output(temporary.getFile());
            if (!output.openedOk() || !readEntry(archive, archive.getIndexOfFileName(entryName), output, maxSample)) return drum::Result::fail("The instrument sample is damaged.");
            output.flush();
            if (output.getStatus().failed()) return drum::Result::fail("Cannot save the imported sample.");
        }
        if (document->getStringAttribute("sampleHash").isEmpty() || fingerprint(temporary.getFile()) != document->getStringAttribute("sampleHash")) return drum::Result::fail("The instrument sample failed its integrity check.");
        if (!temporary.overwriteTargetFileWithTemporary()) return drum::Result::fail("Cannot store the imported sample.");
    }
    auto result = library.importInstrument(kitId, groupId, sound, true);
    if (!result.ok) {
        if (hasSample) { sampleFile.deleteFile(); sampleFile.getParentDirectory().deleteFile(); }
        return result;
    }
    if (juce::MessageManager::getInstance()->isThisTheMessageThread()) effects.sync();
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
    if (unresolved) result.message = "Imported. Reconnect " + juce::String(unresolved) + " unavailable sidechain source(s) in Chain.";
    return result;
}
