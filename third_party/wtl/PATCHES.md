# WTL patches

Source: WTL 10 headers (`Win32-WTL/WTL` mirror / WTL 10 distribution), used under MS-PL.

## atlframe.h

MSVC C++20 two-phase name lookup rejects the unqualified call to a dependent base
member inside `CDynamicUpdateUI`:

```text
error C3861: "UISetText": identifier not found
```

Minimal fix:

```diff
- UISetText(mii.wID, sText);
+ T::UISetText(mii.wID, sText);
```

This is the only source patch applied to the vendored headers.  The project itself
uses `/std:c++20` and `ConformanceMode=false` because WTL 10 predates some of the
stricter C++20 template lookup rules.
