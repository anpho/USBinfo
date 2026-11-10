#pragma once

#include "pch.h"

enum class AppLanguage : int
{
    Auto = 0,
    ZhCN = 1,
    EnUS = 2,
    FrFR = 3,
    DeDE = 4,
    EsES = 5,
    JaJP = 6,
    KoKR = 7,
    RuRU = 8
};

// Translation table row. Chinese source text is the stable lookup key.
// Columns: zh, en, fr, de, es, ja, ko, ru.
struct LanguageEntry
{
    const wchar_t* zh;
    const wchar_t* en;
    const wchar_t* fr;
    const wchar_t* de;
    const wchar_t* es;
    const wchar_t* ja;
    const wchar_t* ko;
    const wchar_t* ru;
};

void LanguageInitialize();
AppLanguage LanguageGet();
AppLanguage LanguageGetConfigured();
AppLanguage LanguageResolve(AppLanguage language);
void LanguageSet(AppLanguage language, bool persist = true);
const wchar_t* LanguageName(AppLanguage language);
const wchar_t* LanguageCode(AppLanguage language);
bool LanguageIsAuto();

const wchar_t* Tr(const wchar_t* source);
std::wstring Tr(const std::wstring& source);
