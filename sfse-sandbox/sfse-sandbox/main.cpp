#include <SFSE/SFSE.h>

// These headers should be included by RE/Starfield.h but some of the other headers there are broken
#include <RE/B/BSTEvent.h>
#include <RE/B/BGSMovableStatic.h>
#include <RE/N/NiSmartPointer.h>  // This header is missing from TESBoundObject.h
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESObjectLoadedEvent.h>

#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESCellFullyLoadedEvent.h>

#include <Windows.h>

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

            if (form_type == RE::FormType::kMSTT) {
                RE::BGSMovableStatic* stat = obj ? obj->As<RE::BGSMovableStatic>() : nullptr;
                if (stat) {
                    stat->ForEachKeyword([a_event](RE::BGSKeyword *keyword) -> RE::BSContainer::ForEachResult {
                        SFSE::log::info("KW: {:x}->{:x}[{}]", a_event.formID, keyword->formID, keyword->GetFormEditorID());
                        return RE::BSContainer::ForEachResult::kContinue;
                    });
                }
            }
        }

        mutex.lock();
        bool first_seen = types_seen.insert(form_type).second;
        mutex.unlock();
        if (first_seen) {
            SFSE::log::info("path {} seen ({:x})", RE::FormTypeToString(form_type), a_event.formID);
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    void reset() {
        mutex.lock();
        types_seen.clear();
        mutex.unlock();
        SFSE::log::info("alerts reset");
    }

private:
    std::set<RE::FormType> types_seen;
    std::mutex mutex;
};

void AlertResetHotkey(FormAlert *alert) {
    SFSE::log::info("alert thread started");
    while (true) {
        if (GetAsyncKeyState(VK_HOME)) {
            alert->reset();
            while (GetAsyncKeyState(VK_HOME)) {
                Sleep(10);
            }
        }
        Sleep(10);
    }
}

FormAlert g_form_alert;

SFSEPluginLoad(const SFSE::LoadInterface *sfse) {
    Init(sfse);
    spdlog::flush_on(spdlog::level::info);

    RE::TESObjectLoadedEvent::GetEventSource()->RegisterSink(&g_form_alert);
    std::thread resetThread(AlertResetHotkey, &g_form_alert);
    resetThread.detach();
    SFSE::log::info("sink(s) registered");

    return true;
}
