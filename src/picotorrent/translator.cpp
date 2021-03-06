#include "translator.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "config.hpp"
#include "environment.hpp"
#include "picojson.hpp"
#include "utils.hpp"

namespace fs = std::experimental::filesystem::v1;
namespace pj = picojson;
using pt::Translator;

Translator::Translator(std::map<int, Language> const& languages, int selectedLanguage)
    : m_languages(languages),
    m_selectedLanguage(selectedLanguage)
{
}

std::vector<Translator::Language> Translator::GetAvailableLanguages()
{
    std::vector<Language> result;

    for (auto& p : m_languages)
    {
        result.push_back(p.second);
    }

    return result;
}

wxString Translator::Translate(wxString key)
{
    auto lang = m_languages.find(m_selectedLanguage);
    if (lang == m_languages.end()) { lang = m_languages.find(1033); }
    if (lang == m_languages.end()) { return key; }

    auto translation = lang->second.translations.find(key);

    if (translation == lang->second.translations.end())
    {
        return key;
    }

    return translation->second;
}

std::shared_ptr<Translator> Translator::Load(HINSTANCE hInstance, std::shared_ptr<pt::Configuration> config)
{
    // TODO: Very Win32 specific code, enumerating the embedded resources.
    // Should be split into a platform specific layer when making PicoTorrent
    // cross platform.

    std::map<int, Language> langs;

    EnumResourceNames(
        hInstance,
        TEXT("LANGFILE"),
        LoadTranslationResource,
        reinterpret_cast<LONG_PTR>(&langs));

    fs::path translationsDirectory = config->LanguagesPath();

    for (fs::directory_entry const& entry : fs::directory_iterator(translationsDirectory))
    {
        if (entry.path().extension() != ".json")
        {
            continue;
        }

        std::ifstream jsonStream(entry.path(), std::ios::binary | std::ios::in);
        std::stringstream json;
        json << jsonStream.rdbuf();

        Language lang;

        if (LoadLanguageFromJson(json.str(), lang))
        {
            langs[lang.code] = lang;
        }
    }

    return std::shared_ptr<Translator>(
        new Translator(langs,
            config->CurrentLanguageId()));
}

BOOL Translator::LoadTranslationResource(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam)
{
    std::map<int, Language>* langs = reinterpret_cast<std::map<int, Language>*>(lParam);

    HRSRC rc = FindResource(hModule, lpszName, lpszType);
    DWORD size = SizeofResource(hModule, rc);
    HGLOBAL data = LoadResource(hModule, rc);
    const char* buffer = reinterpret_cast<const char*>(LockResource(data));

    std::string json(buffer, static_cast<size_t>(size));
    Language lang;

    if (LoadLanguageFromJson(json, lang))
    {
        langs->insert({ lang.code, lang });
    }

    return TRUE;
}

bool Translator::LoadLanguageFromJson(std::string const& json, Translator::Language& lang)
{
    pj::value v;
    std::string err = pj::parse(v, json);

    if (!err.empty())
    {
        return false;
    }

    pj::object obj = v.get<pj::object>();

    int langId = static_cast<int>(obj.at("lang_id").get<int64_t>());
    std::string langName = obj.at("lang_name").get<std::string>();

    Language l;
    l.code = langId;
    l.name = Utils::ToWideString(langName.c_str(), static_cast<int>(langName.size()));

    for (auto& p : obj.at("strings").get<pj::object>())
    {
        std::string val = p.second.get<std::string>();
        wxString converted = Utils::ToWideString(val.c_str(), static_cast<int>(val.size()));

        l.translations.insert({ p.first, converted });
    }

    lang = l;

    return true;
}
