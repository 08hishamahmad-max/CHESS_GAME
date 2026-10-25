// main.cpp - entry point: finds the assets folder and starts the App.
#include "gui/App.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// The assets folder is normally copied next to the executable by CMake.
// When the program is started from an IDE the working directory may be
// different, so a few other places are tried as well.
std::string findAssetDir(const fs::path& exeDir) {
    std::vector<fs::path> candidates = {exeDir / "assets", fs::current_path() / "assets", exeDir / ".." / "assets",
                                        fs::current_path() / ".." / "assets"};
#ifdef CHESS_SOURCE_ASSETS
    candidates.emplace_back(CHESS_SOURCE_ASSETS);
#endif
    std::error_code ec;
    for (const fs::path& p : candidates)
        if (fs::exists(p / "fonts" / "Inter-Regular.ttf", ec)) return p.lexically_normal().string();
    return "assets";
}

}  // namespace

int main(int argc, char* argv[]) {
    fs::path exeDir = fs::current_path();
    std::error_code ec;
    if (argc > 0 && argv[0] && argv[0][0] != '\0') {
        fs::path exe = fs::absolute(fs::path(argv[0]), ec);
        if (!ec && exe.has_parent_path()) exeDir = exe.parent_path();
    }
    const std::string assetDir = findAssetDir(exeDir);

    try {
        gui::App app(assetDir, exeDir.string());
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
}
