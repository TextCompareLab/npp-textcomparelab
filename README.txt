TextCompareLab for Notepad++ 1.1.3 (beta)
Compares two open tabs line by line, with word-level highlights. Runs locally, no network access.
BETA: version 1.1.3 is NOT yet tested inside Notepad++ (new dialogs, menus, export). The comparison and patch engine is tested separately.

Install (manual):
1. Close Notepad++.
2. Copy TextCompareLab.dll to: C:\Program Files\Notepad++\plugins\TextCompareLab\TextCompareLab.dll
   (create the TextCompareLab folder; use the 32-bit zip for 32-bit Notepad++, ARM64 zip for ARM64).
3. Start Notepad++. Menu: Plugins > TextCompareLab.

Use:
- "Choose two tabs to compare...": pick the first and second text from two lists of your open tabs, then Compare.
  (Or: "1. Mark this tab as first" on one tab, "2. Compare this tab with the first" on the other. The menu item
  shows a check mark while a tab is marked.)
- Red line = only in first. Green = only in second. Yellow = changed line, orange boxes = changed words.
- "Next/Previous difference" jumps between changes. "Clear comparison" removes the highlights.
  Marks are a snapshot: after editing either text, compare again.
- "Ignore spaces and tabs" affects the highlighting and the HTML report only.

Export (after a comparison; reads the two tabs as they are now):
- "Export as patch file (.patch)...": standard unified diff, like git diff. Turns the FIRST text into the SECOND.
  Apply to the first file:  git apply file.txt.patch   or   patch -p1 < file.txt.patch
  Contents: changed lines plus 3 lines of context, not the whole files. Always an exact comparison (spaces and
  line endings count), so the patch reproduces the second text byte for byte. File name in the header is the first
  tab's name. Text is written exactly as Notepad++ holds it (UTF-8/Hebrew preserved; a UTF-8 BOM is kept).
  The patch is only exported when both tabs are UTF-8 or plain ASCII. Otherwise nothing is saved and a message says why
  (UTF-16 and non-ASCII ANSI code pages are not supported). Old Mac line endings (a lone CR) are also refused.
- "Export as HTML report...": a single file that opens in any browser, no plugin needed. It contains the FULL text of
  both documents, with all text escaped, no scripts and no external resources. Share it only with people who may
  see both texts.

The DLL is not code-signed, so Windows may warn about it. Check the SHA-256 on the download page matches
before using it. Do not turn off Windows protections.
License: GPL v2 or later. Source: TextCompareLab-Notepad++-source.zip. See THIRD-PARTY-NOTICES.txt.

Toolbar and keyboard (new in 1.1.3, NOT yet tested inside Notepad++):
- Four toolbar buttons: Choose two tabs to compare, Next difference, Previous difference, Export (asks: patch file or HTML report; the two direct export commands stay in the menu). If you do not see them, check the toolbar is visible and use the Plugins menu.
- Default keys: Choose two tabs = Ctrl+Alt+T, Next difference = Ctrl+Alt+Down, Previous difference = Ctrl+Alt+Up, Export = Ctrl+Alt+P.
  They were picked to avoid Notepad++'s built-in keys and the default keys of ComparePlus; another plugin or your own settings may still use them.
  Change them in Settings > Shortcut Mapper > Plugin commands (a conflict is shown there).
- After a successful export, the patch opens in a Notepad++ tab and the HTML report opens in your default browser (for viewing only; nothing is applied or run).
  If the file saved but could not be opened, a message says so.
