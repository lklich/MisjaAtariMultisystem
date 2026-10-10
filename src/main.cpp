//  Leszek Klich (2026)
//  main.cpp
//  Misja (C++ / SDL3)
//
//  Gra Misja (Atari XL/XE, LK Avalon 1990, autor: Janusz Pelc) przepisana na C++.
//
//  Uruchomienie bez parametrów otwiera okno gry. Parametry dla testów:
//    --tape plik.cas               dane z pliku kasety zamiast z pamięci podręcznej
//    --headless N plik.png [skrypt]  N ramek bez okna i zapis ekranu, skrypt: "ramka:klawisz:0|1,..."
//    --trace N plik.bin [skrypt]   N ramek bez okna; skrót pamięci RAM po każdej ramce (do porównań
//                                  z oryginałem uruchomionym w emulatorze)
//    --shots katalog               zrzuty kolejnych ekranów interfejsu (z oknem) i wyjście
//

#include "App.h"
#include "OriginalTape.h"
#include "Storage.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool loadMemory(const std::vector<std::string>& tapes, std::vector<uint8_t>& mem) {
    if (tapes.empty()) {
        return Storage::readFile(Storage::dataFile("misja_mem.bin"), mem) && OriginalTape::isValidImage(mem);
    }
    std::vector<TapeFile> files;
    for (const auto& t : tapes) {
        TapeFile f;
        f.name = Storage::baseName(t);
        if (!Storage::readFile(t, f.bytes)) {
            std::fprintf(stderr, "Nie można odczytać %s\n", t.c_str());
            return false;
        }
        files.push_back(std::move(f));
    }
    try {
        mem = OriginalTape::extract(files);
        return true;
    } catch (const ImportError& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return false;
    }
}

struct Event {
    std::string key;
    bool down;
};

/// Skrypt: "ramka:klawisz:1|0,...". Klawisze: up down left right fire start select option
/// space esc break reset.
std::multimap<int, Event> parseScript(const std::string& script) {
    std::multimap<int, Event> events;
    std::stringstream ss(script);
    std::string item;
    while (std::getline(ss, item, ',')) {
        int f = 0, down = 0;
        char name[32] = {};
        if (std::sscanf(item.c_str(), "%d:%31[a-z]:%d", &f, name, &down) == 3) events.insert({f, {name, down != 0}});
    }
    return events;
}

void applyEvent(Game& game, const Event& e) {
    static const std::map<std::string, GameKey> keys = {
        {"up", GameKey::up},       {"down", GameKey::down},     {"left", GameKey::left},
        {"right", GameKey::right}, {"fire", GameKey::fire},     {"start", GameKey::start},
        {"select", GameKey::select}, {"option", GameKey::option}, {"grenade", GameKey::grenade}};
    auto it = keys.find(e.key);
    if (it != keys.end()) game.setKey(it->second, e.down);
    else if (e.key == "space") { if (e.down) game.atariKeyDown(0x21); else game.atariKeyUp(); }
    else if (e.key == "esc") { if (e.down) game.atariKeyDown(0x1C); else game.atariKeyUp(); }
    else if (e.key == "break" && e.down) game.pressBreak();
    else if (e.key == "reset" && e.down) game.reset();
}

int runHeadless(const std::vector<uint8_t>& mem, int frames, const std::string& out, const std::string& script,
                bool trace) {
    Pokey pokey({}, 44100);
    Game game(mem, pokey);
    const auto events = parseScript(script);
    std::vector<float> audio(882);
    FILE* traceFile = nullptr;
    if (trace) {
        traceFile = std::fopen(out.c_str(), "wb");
        if (!traceFile) {
            std::fprintf(stderr, "Nie można zapisać %s\n", out.c_str());
            return 1;
        }
    }
    std::vector<uint8_t> snap;
    for (int i = 0; i < frames; ++i) {
        auto range = events.equal_range(i);
        for (auto it = range.first; it != range.second; ++it) applyEvent(game, it->second);
        game.frame();
        pokey.render(audio.data(), static_cast<int>(audio.size()));   // 1/50 s dźwięku – sprawdza syntezę
        if (traceFile) {
            // skrót FNV-1a pamięci (bez stosu $0100-$01FF i komórki $862F, zależnej od stosu)
            const auto& m = game.memory();
            uint32_t h = 0x811C9DC5u;
            for (int a = 0; a < 0x10000; ++a) {
                if ((a >> 8) == 1 || a == 0x862F) continue;
                h = (h ^ m[a]) * 0x01000193u;
            }
            std::fwrite(&h, 4, 1, traceFile);
        }
    }
    if (traceFile) {
        std::fclose(traceFile);
    } else {
        std::vector<uint32_t> px = game.pixels();
        SDL_Surface* s = SDL_CreateSurfaceFrom(Game::width, Game::height, SDL_PIXELFORMAT_ARGB8888, px.data(),
                                               Game::width * 4);
        if (!s || !SDL_SavePNG(s, out.c_str())) {
            std::fprintf(stderr, "Zapis %s: %s\n", out.c_str(), SDL_GetError());
            return 1;
        }
        SDL_DestroySurface(s);
    }
    const Game::Info info = game.info();
    std::printf("room=%02X score=%s energy=%d%% containers=%d\n", info.room, info.score.c_str(), info.energy,
                info.containers);
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string> tapes;
    std::string headlessOut, script;
    int headlessFrames = -1;
    bool trace = false;
    App::Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--tape") {
            while (i + 1 < argc && argv[i + 1][0] != '-') tapes.emplace_back(argv[++i]);
        } else if ((a == "--headless" || a == "--trace") && i + 2 < argc) {
            trace = a == "--trace";
            headlessFrames = std::atoi(argv[++i]);
            headlessOut = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') script = argv[++i];
        } else if (a == "--shots" && i + 1 < argc) {
            options.shotsDir = argv[++i];
        } else if (!a.empty() && a[0] != '-') {
            tapes.push_back(a);   // plik przeciągnięty na ikonę programu
        }
    }

    if (headlessFrames >= 0) {
        std::vector<uint8_t> mem;
        if (!loadMemory(tapes, mem)) {
            std::fprintf(stderr, "Brak danych gry – podaj --tape Misja.cas\n");
            return 2;
        }
        return runHeadless(mem, headlessFrames, headlessOut, script, trace);
    }

    if (!tapes.empty()) {
        // np. plik .cas przeciągnięty na ikonę programu – zapis do pamięci podręcznej przed startem
        std::vector<uint8_t> mem;
        if (loadMemory(tapes, mem)) Storage::writeFile(Storage::dataFile("misja_mem.bin"), mem);
    }
    App app;
    if (!app.init(options)) return 1;
    app.run();
    return 0;
}
