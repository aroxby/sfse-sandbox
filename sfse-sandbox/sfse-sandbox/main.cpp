#include <SFSE/SFSE.h>

// These headers should be included by RE/Starfield.h but some of the other headers there are broken
#include <RE/B/BSTEvent.h>
#include <RE/N/NiSmartPointer.h>  // This header is missing from TESBoundObject.h
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESObjectLoadedEvent.h>

#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESCellFullyLoadedEvent.h>

// Removes CommonLibSF's `inline` so that the symbol will export
#define My_SFSEPluginVersion extern "C" [[maybe_unused]] __declspec(dllexport) constinit SFSE::PluginVersionData SFSEPlugin_Version

namespace Plugin
{
    using namespace std::string_view_literals;

    static constexpr auto Name{ "sfse-sandbox"sv };
    static constexpr auto Author{ "aroxby"sv };
    static constexpr auto Version{
        REL::Version{0, 0, 0, 0}
    };
}

My_SFSEPluginVersion = []() noexcept {
    SFSE::PluginVersionData data{};

    data.PluginVersion(Plugin::Version.pack());
    data.PluginName(Plugin::Name);
    data.AuthorName(Plugin::Author);
    data.UsesAddressLibrary(true);
    data.IsLayoutDependent(true);
    data.CompatibleVersions({ SFSE::RUNTIME_LATEST });

    return data;
}();

struct OBJ_REFR
{
public:
    uint32_t unk;
    struct {
        float x, y, z;
    } pos;
    RE::NiPointer<RE::TESBoundObject> objectReference;
};

class FormAlert : public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
public:
    typedef RE::TESObjectLoadedEvent Event;

    virtual RE::BSEventNotifyControl ProcessEvent(const Event& a_event, RE::BSTEventSource<Event>* a_source) {
        RE::TESForm* form = RE::TESForm::LookupByID(a_event.formID);
        RE::FormType form_type = form ? form->GetFormType() : RE::FormType::kNONE;
        if (form_type == RE::FormType::kREFR) {
            RE::TESObjectREFR* refr = form->As<RE::TESObjectREFR>();
            OBJ_REFR* refr_data = (OBJ_REFR*)&refr->data;
            RE::TESBoundObject* obj = refr_data->objectReference.get();
            form_type = obj ? obj->GetFormType() : RE::FormType::kNONE;
        }

        mutex.lock();
        bool first_seen = types_seen.insert(form_type).second;
        if (first_seen) {
            SFSE::log::info("path {} seen ({:x})", RE::FormTypeToString(form_type), a_event.formID);
        }
        mutex.unlock();

        return RE::BSEventNotifyControl::kContinue;
    }

private:
    std::set<RE::FormType> types_seen;
    std::mutex mutex;
};

class CellAlert : public RE::BSTEventSink<RE::TESCellFullyLoadedEvent> {
public:
    typedef RE::TESCellFullyLoadedEvent Event;

    virtual RE::BSEventNotifyControl ProcessEvent(const Event& a_event, RE::BSTEventSource<Event>* a_source) {
        RE::TESObjectCELL* cell = a_event.cell;

        SFSE::log::info("cell {:x} loaded", cell->formID);

        // FIXME: TESObjectCELL::references seems to point to invalid memory
        /*
        cell->ForEachReference(
            [this](const RE::NiPointer<RE::TESObjectREFR>& ref) -> RE::BSContainer::ForEachResult {
                RE::FormType form_type = ref->GetFormType();

                mutex.lock();
                bool first_seen = types_seen.insert(form_type).second;
                if (first_seen) {
                    SFSE::log::info("type {} seen", form_type);
                }
                mutex.unlock();

                return RE::BSContainer::ForEachResult::kContinue;
            }
        );
        */

        return RE::BSEventNotifyControl::kContinue;
    }

private:
    std::set<RE::FormType> types_seen;
    std::mutex mutex;
};

FormAlert g_form_alert;
CellAlert g_cell_alert;

SFSEPluginLoad(const SFSE::LoadInterface *sfse) {
    Init(sfse);
    spdlog::flush_on(spdlog::level::info);

    RE::TESObjectLoadedEvent::GetEventSource()->RegisterSink(&g_form_alert);
    RE::TESCellFullyLoadedEvent::GetEventSource()->RegisterSink(&g_cell_alert);

    SFSE::log::info("sink(s) registered");

    return true;
}
