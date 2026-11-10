#pragma once

// Private window messages.
#define WM_APP_NODE_SELECTED      (WM_APP + 1)
#define WM_APP_SCAN_DONE          (WM_APP + 2)
#define WM_APP_BENCH_UPDATE       (WM_APP + 3)
#define WM_APP_BENCH_DONE         (WM_APP + 4)
#define WM_APP_REQUEST_REFRESH    (WM_APP + 5)
#define WM_APP_REQUEST_EXPORT     (WM_APP + 6)

// Control identifiers.
#define IDC_TREE_USB              1001
#define IDC_TABS                  1100
#define IDC_INFO_LIST             1101
#define IDC_SMART_LIST            1102
#define IDC_BENCH_PANE            1103
#define IDC_BTN_COPY              1110
#define IDC_BTN_BENCH             1111
#define IDC_BTN_EXPORT            1112
#define IDC_BTN_REFRESH           1113
#define IDC_BENCH_VOLUME          1120
#define IDC_BENCH_SIZE            1121
#define IDC_BENCH_START           1122
#define IDC_BENCH_CANCEL          1123
#define IDC_BENCH_PROGRESS        1124
#define IDC_BENCH_STATUS          1125
#define IDC_BENCH_RESULTS         1126
#define IDC_BENCH_WARNING         1127
#define IDC_BENCH_CHART           1128
#define IDC_BENCH_TIMEOUT         1129

// Context-menu commands for the detail lists.
#define IDM_CONTEXT_COPY_VALUE    1201
#define IDM_CONTEXT_COPY_ROW      1202
#define IDM_CONTEXT_COPY_ALL      1203
#define IDM_CONTEXT_LOCATE_DRIVER 1204
#define IDM_CONTEXT_OPEN_DRIVER_INF 1205

// Menu commands.
#define ID_FILE_REFRESH           40001
#define ID_FILE_EXPORT            40002
#define ID_FILE_EXIT              40003
#define ID_ACTION_BENCH           40004
#define ID_ACTION_COPY            40005
#define ID_VIEW_EXPAND_ALL        40010
#define ID_VIEW_COLLAPSE_ALL      40011
#define ID_VIEW_ELEVATE           40012
#define ID_VIEW_LANGUAGE_AUTO     40020
#define ID_VIEW_LANGUAGE_ZH       40021
#define ID_VIEW_LANGUAGE_EN       40022
#define ID_VIEW_LANGUAGE_FR       40023
#define ID_VIEW_LANGUAGE_DE       40024
#define ID_VIEW_LANGUAGE_ES       40025
#define ID_VIEW_LANGUAGE_JA       40026
#define ID_VIEW_LANGUAGE_KO       40027
#define ID_VIEW_LANGUAGE_RU       40028
#define ID_HELP_ABOUT             40031
