#include "core.h"
#include <cassert>
#include <unistd.h>
#include <cstdlib>
using namespace tcli;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
int main(int argc, char** argv) {
    // real DLLs passed on the command line: x64 x86 arm64
    const char* names[3] = {"x64", "x86", "arm64"};
    for (int i = 0; i < 3 && argc > 1 + i; i++) {
        Bytes b; CHECK(readFile(argv[1 + i], b)); CHECK(peArch(b) == names[i]);
        printf("%s: arch=%s version=%s\n", names[i], peArch(b).c_str(), fileVersion(b).c_str());
    }
    CHECK(cmpVersion("1.1.3.0", "1.1.3") == 0); CHECK(cmpVersion("1.1.2.0", "1.1.3.0") < 0); CHECK(cmpVersion("1.2", "1.1.9.9") > 0); CHECK(cmpVersion("10.0", "9.9") > 0);
    fs::path root = fs::temp_directory_path() / "tcli_test"; fs::remove_all(root);
    fs::path npp = root / "Program Files" / "Notepad++"; fs::create_directories(npp);
    Bytes dllA{'M','Z'}; dllA.resize(100, 7);
    // no notepad++.exe: refuse
    Result r = installDll(npp, dllA); CHECK(!r.ok);
    CHECK(!fs::exists(npp / "plugins"));
    { FILE* f = fopen((npp / "notepad++.exe").c_str(), "wb"); fputs("x", f); fclose(f); }
    r = installDll(npp, dllA); CHECK(r.ok); Bytes got; CHECK(readFile(pluginDll(npp), got) && got == dllA);
    CHECK(!fs::exists(pluginDir(npp) / "TextCompareLab.dll.new") && !fs::exists(pluginDir(npp) / "TextCompareLab.dll.bak"));
    // upgrade keeps backup
    Bytes dllB(120, 9); r = installDll(npp, dllB); CHECK(r.ok);
    CHECK(readFile(pluginDll(npp), got) && got == dllB); CHECK(readFile(pluginDir(npp) / "TextCompareLab.dll.bak", got) && got == dllA);
    // other plugins untouched
    fs::create_directories(npp / "plugins" / "Other"); { FILE* f = fopen((npp / "plugins" / "Other" / "Other.dll").c_str(), "wb"); fputs("o", f); fclose(f); }
    // uninstall removes only ours
    r = uninstallDll(npp); CHECK(r.ok); CHECK(!fs::exists(pluginDir(npp))); CHECK(fs::exists(npp / "plugins" / "Other" / "Other.dll")); CHECK(fs::exists(npp / "notepad++.exe"));
    r = uninstallDll(npp); CHECK(r.ok);
    // read-only plugins dir: denied path, nothing half-installed (skipped when running as root)
    fs::path npp2 = root / "ro" / "Notepad++"; fs::create_directories(npp2 / "plugins"); { FILE* f = fopen((npp2 / "notepad++.exe").c_str(), "wb"); fputs("x", f); fclose(f); }
    fs::permissions(npp2 / "plugins", fs::perms::owner_read | fs::perms::owner_exec);
    r = installDll(npp2, dllA); if (geteuid() != 0) { CHECK(!r.ok); CHECK(r.denied); }
    fs::permissions(npp2 / "plugins", fs::perms::owner_all);
    CHECK(!fs::exists(pluginDll(npp2)));
    fs::remove_all(root);
    printf(fails ? "FAILED %d\n" : "core tests OK\n", fails); return fails != 0;
}
