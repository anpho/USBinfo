#include "pch.h"
#include "Language.h"

#include <array>
#include <unordered_map>

namespace
{
using TranslationRow = std::array<std::wstring, 8>;

const LanguageEntry kEntries[] =
{
    // Keep this sentinel so the include files may be empty during bring-up.
    { L"__USBINFO_LANGUAGE_SENTINEL__", L"", L"", L"", L"", L"", L"", L"" },
#include "LanguageCore.inc"
#include "LanguageUi.inc"
#include "LanguageData.inc"
};

std::unordered_map<std::wstring, TranslationRow> g_table;
AppLanguage g_configured = AppLanguage::Auto;
AppLanguage g_current = AppLanguage::EnUS;

int LanguageIndex(AppLanguage language)
{
    switch (language)
    {
    case AppLanguage::ZhCN: return 0;
    case AppLanguage::EnUS: return 1;
    case AppLanguage::FrFR: return 2;
    case AppLanguage::DeDE: return 3;
    case AppLanguage::EsES: return 4;
    case AppLanguage::JaJP: return 5;
    case AppLanguage::KoKR: return 6;
    case AppLanguage::RuRU: return 7;
    default: return 1;
    }
}

AppLanguage DetectSystemLanguage()
{
    LANGID languageId = ::GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(languageId))
    {
    case LANG_CHINESE: return AppLanguage::ZhCN;
    case LANG_FRENCH: return AppLanguage::FrFR;
    case LANG_GERMAN: return AppLanguage::DeDE;
    case LANG_SPANISH: return AppLanguage::EsES;
    case LANG_JAPANESE: return AppLanguage::JaJP;
    case LANG_KOREAN: return AppLanguage::KoKR;
    case LANG_RUSSIAN: return AppLanguage::RuRU;
    case LANG_ENGLISH: return AppLanguage::EnUS;
    default: return AppLanguage::EnUS;
    }
}

void BuildTable()
{
    g_table.clear();
    for (const auto& entry : kEntries)
    {
        if (entry.zh == nullptr || *entry.zh == L'\0')
            continue;
        if (wcscmp(entry.zh, L"__USBINFO_LANGUAGE_SENTINEL__") == 0)
            continue;
        g_table.emplace(entry.zh, TranslationRow{
            entry.zh ? entry.zh : L"",
            entry.en ? entry.en : L"",
            entry.fr ? entry.fr : L"",
            entry.de ? entry.de : L"",
            entry.es ? entry.es : L"",
            entry.ja ? entry.ja : L"",
            entry.ko ? entry.ko : L"",
            entry.ru ? entry.ru : L"" });
    }
}

void LoadConfiguredLanguage()
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\USBInfo", 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        ::RegGetValueW(key, nullptr, L"Language", RRF_RT_REG_DWORD, nullptr, &value, &size);
        ::RegCloseKey(key);
    }
    if (value >= static_cast<DWORD>(AppLanguage::Auto) && value <= static_cast<DWORD>(AppLanguage::RuRU))
        g_configured = static_cast<AppLanguage>(value);
    else
        g_configured = AppLanguage::Auto;
    g_current = LanguageResolve(g_configured);
}

void SaveConfiguredLanguage(AppLanguage language)
{
    HKEY key = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\USBInfo", 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return;
    DWORD value = static_cast<DWORD>(language);
    ::RegSetValueExW(key, L"Language", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    ::RegCloseKey(key);
}
}

void LanguageInitialize()
{
    BuildTable();
    LoadConfiguredLanguage();
}

AppLanguage LanguageGet()
{
    return g_current;
}

AppLanguage LanguageGetConfigured()
{
    return g_configured;
}

AppLanguage LanguageResolve(AppLanguage language)
{
    if (language == AppLanguage::Auto)
        return DetectSystemLanguage();
    return language;
}

void LanguageSet(AppLanguage language, bool persist)
{
    g_configured = language;
    g_current = LanguageResolve(language);
    if (persist)
        SaveConfiguredLanguage(language);
}

bool LanguageIsAuto()
{
    return g_configured == AppLanguage::Auto;
}

const wchar_t* LanguageName(AppLanguage language)
{
    switch (language)
    {
    case AppLanguage::Auto: return Tr(L"自动（系统）");
    case AppLanguage::ZhCN: return L"简体中文";
    case AppLanguage::EnUS: return L"English";
    case AppLanguage::FrFR: return L"Français";
    case AppLanguage::DeDE: return L"Deutsch";
    case AppLanguage::EsES: return L"Español";
    case AppLanguage::JaJP: return L"日本語";
    case AppLanguage::KoKR: return L"한국어";
    case AppLanguage::RuRU: return L"Русский";
    default: return L"English";
    }
}

const wchar_t* LanguageCode(AppLanguage language)
{
    switch (language)
    {
    case AppLanguage::ZhCN: return L"zh-CN";
    case AppLanguage::FrFR: return L"fr-FR";
    case AppLanguage::DeDE: return L"de-DE";
    case AppLanguage::EsES: return L"es-ES";
    case AppLanguage::JaJP: return L"ja-JP";
    case AppLanguage::KoKR: return L"ko-KR";
    case AppLanguage::RuRU: return L"ru-RU";
    case AppLanguage::EnUS:
    default: return L"en-US";
    }
}

const wchar_t* Tr(const wchar_t* source)
{
    if (source == nullptr || *source == L'\0')
        return source != nullptr ? source : L"";
    auto it = g_table.find(source);
    if (it == g_table.end())
        return source;

    int index = LanguageIndex(g_current);
    const std::wstring& translated = it->second[static_cast<size_t>(index)];
    if (!translated.empty())
        return translated.c_str();

    // Fallback to English, then Chinese.
    if (!it->second[1].empty())
        return it->second[1].c_str();
    return source;
}

std::wstring Tr(const std::wstring& source)
{
    return Tr(source.c_str());
}
