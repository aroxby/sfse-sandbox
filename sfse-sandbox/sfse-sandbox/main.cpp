#include <SFSE/SFSE.h>

// These headers should be included by RE/Starfield.h but some of the other headers there are broken
#include <RE/A/ActorValueInfo.h>
#include <RE/B/BSTEvent.h>
#include <RE/B/BGSGenericBaseForm.h>
#include <RE/B/BGSGenericBaseFormTemplate.h>
#include <RE/N/NiSmartPointer.h>  // This header is missing from TESBoundObject.h
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESFullName.h>
#include <RE/T/TESObjectREFR.h>
#include <RE/T/TESObjectLoadedEvent.h>

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

RE::TESBoundObject* refrBaseObject(RE::TESObjectREFR* refr) {
    RE::TESBoundObject* base = nullptr;
    if (refr) {
        OBJ_REFR* refr_data = (OBJ_REFR*)&refr->data;
        base = refr_data->objectReference.get();
    }
    return base;
}

class FormAlert : public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
public:
    typedef RE::TESObjectLoadedEvent Event;

    virtual RE::BSEventNotifyControl ProcessEvent(const Event& a_event, RE::BSTEventSource<Event>* a_source) {
        RE::TESForm* form = RE::TESForm::LookupByID(a_event.formID);
        RE::FormType form_type = form ? form->GetFormType() : RE::FormType::kNONE;
        if (form_type == RE::FormType::kREFR) {
            RE::TESBoundObject* obj = refrBaseObject(form->As<RE::TESObjectREFR>());
            form_type = obj ? obj->GetFormType() : RE::FormType::kNONE;

            if (form_type == RE::FormType::kGBFM) {
                RE::BGSGenericBaseForm* gbfm = obj ? obj->As<RE::BGSGenericBaseForm>() : nullptr;
                if (gbfm) {
                    const char* id1 = gbfm->formEditorID.c_str();

                    RE::BGSGenericBaseFormTemplate* gbft = gbfm->genericBaseFormTemplate;
                    const char* id2 = gbft->formEditorID.c_str();

                    id1 = id1 ? id1 : "--";
                    id2 = id2 ? id2 : "--";

                    SFSE::log::info("GBFM {:x} ids ({}, {})", uint32_t(gbfm->formID), id1, id2);

                    SFSE::log::info("{:x}[{}]", uint32_t(gbfm->formID), gbft->components.size());
                    for (const auto& component : gbft->components) {
                        SFSE::log::info("{:x} -> {}", uint32_t(gbfm->formID), component.c_str());
                    }

                    class TemplateItem : public RE::TESFullName
                    {
                    };

                    RE::BGSMod::Template::Items* item = &gbfm->templateItems;
                    if (item) {
                        SFSE::log::info("{:x} ({})", uint32_t(gbfm->formID), item->unk18.c_str());
                        for(void *vpi : item->unk08) {
                            // const type_info& info = typeid(*(RE::TBO_InstanceData*)vpi);  // class BGSMod::Template::Item
                            TemplateItem* item = (TemplateItem*)vpi;
                            const char* fullname = item->fullName.c_str();
                            SFSE::log::info("{:x} [{}]", uint32_t(gbfm->formID), fullname ? fullname : "--");
                        }
                    }
                }
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

FormAlert g_form_alert;

SFSEPluginLoad(const SFSE::LoadInterface *sfse) {
    Init(sfse);
    spdlog::flush_on(spdlog::level::info);

    RE::TESObjectLoadedEvent::GetEventSource()->RegisterSink(&g_form_alert);
    SFSE::log::info("sink(s) registered");

    return true;
}
