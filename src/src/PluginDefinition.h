// TextCompareLab for Notepad++. Copyright (C) 2026 the TextCompareLab project. GPL v2 or later.
#ifndef PLUGINDEFINITION_H
#define PLUGINDEFINITION_H
#include "PluginInterface.h"
const TCHAR NPP_PLUGIN_NAME[] = TEXT("TextCompareLab");
const int nbFunc = 12;
void pluginInit(HANDLE hModule);
void pluginCleanUp();
void commandMenuInit();
void commandMenuCleanUp();
bool setCommand(size_t index, const TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk = NULL, bool check0nInit = false);
void markThisTab();
void compareWithMarked();
void clearComparison();
void nextDifference();
void prevDifference();
void openWebsite();
void aboutBox();
void ignoreWhitespaceToggle();
void chooseTabsDialog();
void exportPatch();
void exportHtml();
void toolbarInit();
void exportChoose();
#endif
