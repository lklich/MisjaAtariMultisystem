//
//  App.cpp
//  Misja (C++ / SDL3)
//

#include "App.h"

#include "NativeMenu.h"
#include "OriginalTape.h"
#include "Storage.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <random>

namespace {

#ifdef __APPLE__
const char* kMod = "Cmd";
#else
const char* kMod = "Ctrl";
#endif

const char* kCacheName = "misja_mem.bin";
constexpr int kSampleRate = 44100;
constexpr Uint64 kFrameNS = 20000000;     // 50 Hz jak Atari PAL

const SDL_Color kWhite{255, 255, 255, 255};
const SDL_Color kGray{150, 150, 150, 255};
const SDL_Color kDim{100, 100, 100, 255};
const SDL_Color kGreen{120, 220, 120, 255};
const SDL_Color kAccent{130, 200, 90, 255};
const SDL_Color kRed{255, 90, 80, 255};
const SDL_Color kPanel{16, 20, 16, 235};
const SDL_Color kSelect{50, 80, 50, 255};

std::string shortcut(const char* key) { return std::string(kMod) + "+" + key; }

/// Dekodowanie UTF-8 do znaków Unicode.
std::u32string utf8Decode(const std::string& s) {
    std::u32string out;
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp;
        int extra;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { ++i; continue; }
        if (i + extra >= s.size() + (extra > 0 ? 0 : 1)) break;   // urwana sekwencja na końcu
        ++i;
        for (int k = 0; k < extra && i < s.size(); ++k, ++i) cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
        out.push_back(cp);
    }
    return out;
}

/// Znak ASCII do czcionki SDL (SDL_RenderDebugText) – polskie litery bez ogonków.
char asciiFold(char32_t ch) {
    switch (ch) {
    case U'ą': return 'a'; case U'Ą': return 'A';
    case U'ć': return 'c'; case U'Ć': return 'C';
    case U'ę': return 'e'; case U'Ę': return 'E';
    case U'ł': return 'l'; case U'Ł': return 'L';
    case U'ń': return 'n'; case U'Ń': return 'N';
    case U'ó': return 'o'; case U'Ó': return 'O';
    case U'ś': return 's'; case U'Ś': return 'S';
    case U'ż': case U'ź': return 'z';
    case U'Ż': case U'Ź': return 'Z';
    case U'„': case U'”': return '"';
    case U'–': case U'—': return '-';
    case U'…': return '.';
    default: break;
    }
    if (ch >= 0x20 && ch < 0x7F) return static_cast<char>(ch);
    return '?';
}

std::string formatDate(int64_t t) {
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &tt);
#else
    localtime_r(&tt, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M", &tmv);
    return buf;
}

std::string roomName(int room) {
    char b[8];
    std::snprintf(b, sizeof(b), "%02X", room & 0xFF);
    return b;
}

}  // namespace

App::App() = default;

App::~App() {
    if (audio_) SDL_DestroyAudioStream(audio_);   // zatrzymuje wywołania zwrotne przed usunięciem Pokey
    audio_ = nullptr;
    game_.reset();
    pokey_.reset();
    freeTextures();
    if (screenTex_) SDL_DestroyTexture(screenTex_);
    for (auto* p : pads_) SDL_CloseGamepad(p);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

// MARK: - Start

bool App::init(const Options& options) {
    options_ = options;
    SDL_SetAppMetadata("Misja", "1.0", "pl.lk.misja");
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        // bez dźwięku i gamepada też da się grać
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            SDL_Log("SDL_Init: %s", SDL_GetError());
            return false;
        }
    }
    window_ = SDL_CreateWindow("Misja", Game::width * 3, Game::height * 3,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window_) {
        SDL_Log("SDL_CreateWindow: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(window_, viewW, viewH);
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
        SDL_Log("SDL_CreateRenderer: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(renderer_, 1);
    SDL_SetRenderLogicalPresentation(renderer_, viewW, viewH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    screenTex_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, Game::width,
                                   Game::height);
    SDL_SetTextureScaleMode(screenTex_, SDL_SCALEMODE_NEAREST);

    dialogEvent_ = SDL_RegisterEvents(2);
    menuEvent_ = dialogEvent_ + 1;
    NativeMenu::install(window_, menuEvent_);   // menu „Gra” (macOS, Windows)

    const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, kSampleRate};
    audio_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audioCallback, this);
    if (audio_) {
        SDL_ResumeAudioStreamDevice(audio_);
    } else {
        SDL_Log("Brak dźwięku: %s", SDL_GetError());
    }

    store_ = std::make_unique<SaveStore>(Storage::dataFile("saves.bin"));
    loadCachedData();
    if (game_) show("F1: menu – zapis i wczytanie stanu gry");
    startTicks_ = SDL_GetTicks();
    return true;
}

// MARK: - Dane z oryginalnej kasety

void App::loadCachedData() {
    std::vector<uint8_t> mem;
    if (Storage::readFile(Storage::dataFile(kCacheName), mem) && OriginalTape::isValidImage(mem)) {
        setData(std::move(mem));
    }
}

void App::setData(std::vector<uint8_t> image) {
    closeOverlay();
    game_.reset();
    auto pokey = std::make_unique<Pokey>(std::vector<uint8_t>{}, kSampleRate);
    pokey->setMuted(muted_);
    replacePokey(std::move(pokey));
    image_ = std::move(image);
    game_ = std::make_unique<Game>(image_, *pokey_);
    for (int i = 0; i < 3; ++i) game_->setCheat(static_cast<Game::Cheat>(i), cheats_[i]);
    freeTextures();
    releaseInput();
    importError_.clear();
}

void App::forgetOriginal() {
    closeOverlay();
    game_.reset();
    replacePokey(nullptr);
    image_.clear();
    freeTextures();
    Storage::removeFile(Storage::dataFile(kCacheName));
    importError_.clear();
    show("Dane oryginału usunięte");
}

void App::importTape(const std::vector<std::string>& paths) {
    if (paths.empty()) return;
    std::vector<TapeFile> files;
    for (const auto& p : paths) {
        TapeFile f;
        f.name = Storage::baseName(p);
        if (!Storage::readFile(p, f.bytes)) {
            importError_ = "Nie można odczytać pliku „" + f.name + "”.";
            if (game_) show(importError_);
            return;
        }
        files.push_back(std::move(f));
    }
    try {
        auto mem = OriginalTape::extract(files);
        if (!Storage::writeFile(Storage::dataFile(kCacheName), mem)) {
            SDL_Log("Nie udało się zapisać %s", Storage::dataFile(kCacheName).c_str());
        }
        setData(std::move(mem));
        show("Dane gry odczytane z kasety i zapisane");
    } catch (const ImportError& e) {
        importError_ = e.what();
        if (game_) show(importError_);
    }
}

void App::chooseTape() {
    {
        std::lock_guard<std::mutex> g(dialogLock_);
        if (dialogOpen_) return;
        dialogOpen_ = true;
    }
    releaseInput();
    static const SDL_DialogFileFilter filters[] = {
        {"Kaseta Atari (.cas)", "cas;bin"},
        {"Wszystkie pliki", "*"},
    };
    SDL_ShowOpenFileDialog(dialogCallback, this, window_, filters, 2, nullptr, false);
}

void SDLCALL App::dialogCallback(void* userdata, const char* const* filelist, int) {
    App* app = static_cast<App*>(userdata);
    {
        std::lock_guard<std::mutex> g(app->dialogLock_);
        app->dialogFiles_.clear();
        if (filelist) {
            for (const char* const* f = filelist; *f; ++f) app->dialogFiles_.emplace_back(*f);
        } else {
            SDL_Log("Okno wyboru plików: %s", SDL_GetError());
        }
    }
    SDL_Event ev{};
    ev.type = app->dialogEvent_;
    SDL_PushEvent(&ev);
}

void App::handleDialogResult() {
    std::vector<std::string> files;
    {
        std::lock_guard<std::mutex> g(dialogLock_);
        files.swap(dialogFiles_);
        dialogOpen_ = false;
    }
    if (!files.empty()) importTape(files);
}

// MARK: - Menu systemowe „Gra”

void App::handleMenuCommand(int command) {
    switch (command) {
    case NativeMenu::cmdSave: saveCurrent(); break;
    case NativeMenu::cmdSaves: openOverlay(Overlay::saves); break;
    case NativeMenu::cmdReset: resetGame(); break;
    case NativeMenu::cmdGameMenu:
        if (overlay_ == Overlay::menu) closeOverlay(); else openOverlay(Overlay::menu);
        break;
    case NativeMenu::cmdFullscreen: toggleFullscreen(); break;
    case NativeMenu::cmdMute: toggleMute(); break;
    case NativeMenu::cmdLoadTape: closeOverlay(); chooseTape(); break;
    case NativeMenu::cmdForget: confirmForget(); break;
    case NativeMenu::cmdQuit: running_ = false; break;
    default: break;
    }
}

void App::confirmForget() {
    if (!game_) return;
    releaseInput();
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Anuluj"},
        {0, 1, "Usuń"},
    };
    const SDL_MessageBoxData box{SDL_MESSAGEBOX_WARNING, window_, "Misja",
                                 "Usunąć dane oryginału odczytane z kasety? Przy następnym uruchomieniu "
                                 "trzeba będzie znowu wskazać plik .cas. Zapisy gry zostaną.",
                                 2, buttons, nullptr};
    int choice = 0;
    if (SDL_ShowMessageBox(&box, &choice) && choice == 1) forgetOriginal();
}

// MARK: - Dźwięk

void SDLCALL App::audioCallback(void* userdata, SDL_AudioStream* stream, int additional, int) {
    // Wywoływane z wątku audio przy zablokowanym strumieniu (SDL_LockAudioStream chroni pokey_).
    App* app = static_cast<App*>(userdata);
    const int frames = additional / static_cast<int>(sizeof(float));
    if (frames <= 0) return;
    if (static_cast<int>(app->audioBuf_.size()) < frames) app->audioBuf_.resize(frames);
    float* buf = app->audioBuf_.data();
    if (app->pokey_) {
        app->pokey_->render(buf, frames);
    } else {
        std::fill_n(buf, frames, 0.0f);
    }
    SDL_PutAudioStreamData(stream, buf, frames * static_cast<int>(sizeof(float)));
}

void App::replacePokey(std::unique_ptr<Pokey> p) {
    if (audio_) SDL_LockAudioStream(audio_);
    pokey_ = std::move(p);
    if (audio_) SDL_UnlockAudioStream(audio_);
}

// MARK: - Pętla główna

void App::run() {
    Uint64 last = SDL_GetTicksNS();
    Uint64 acc = 0;
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) handleEvent(e);
        if (!running_) break;

        const Uint64 now = SDL_GetTicksNS();
        acc += now - last;
        last = now;
        int n = 0;
        while (acc >= kFrameNS && n < 5) {
            if (game_ && overlay_ == Overlay::none) game_->frame();
            acc -= kFrameNS;
            ++n;
        }
        if (acc > 5 * kFrameNS) acc = 0;   // po uśpieniu/przeciąganiu okna nie nadrabiamy

        draw();
        if (!options_.shotsDir.empty()) testShots();
        SDL_RenderPresent(renderer_);
        int vsync = 0;
        if (!SDL_GetRenderVSync(renderer_, &vsync) || vsync == 0) SDL_Delay(2);
    }
}

// MARK: - Zdarzenia

void App::handleEvent(const SDL_Event& ev) {
    SDL_Event e = ev;
    if (e.type == dialogEvent_) {
        handleDialogResult();
        return;
    }
    if (e.type == menuEvent_) {
        handleMenuCommand(e.user.code);
        return;
    }
    switch (e.type) {
    case SDL_EVENT_QUIT:
        running_ = false;
        break;
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        NativeMenu::setFullscreen(true);
        break;
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        NativeMenu::setFullscreen(false);
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        // Okno straciło fokus – puść wszystkie klawisze, żeby postać nie szła sama.
        releaseInput();
        break;
    case SDL_EVENT_KEY_DOWN:
        handleKeyDown(e.key);
        break;
    case SDL_EVENT_KEY_UP:
        handleKeyUp(e.key);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        SDL_ConvertEventToRenderCoordinates(renderer_, &e);
        mouseX_ = e.motion.x;
        mouseY_ = e.motion.y;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        SDL_ConvertEventToRenderCoordinates(renderer_, &e);
        mouseX_ = e.button.x;
        mouseY_ = e.button.y;
        if (e.button.button == SDL_BUTTON_LEFT) {
            std::function<void()> action;
            for (const auto& b : buttons_) {
                if (hovered(b.rect)) action = b.action;
            }
            if (action) action();
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        if (overlay_ == Overlay::saves) savesScroll_ -= static_cast<int>(e.wheel.y);
        break;
    case SDL_EVENT_DROP_BEGIN:
        dropped_.clear();
        break;
    case SDL_EVENT_DROP_FILE:
        if (e.drop.data) dropped_.emplace_back(e.drop.data);
        break;
    case SDL_EVENT_DROP_COMPLETE:
        if (!dropped_.empty()) {
            const auto files = dropped_;
            dropped_.clear();
            importTape(files);
        }
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        if (SDL_Gamepad* p = SDL_OpenGamepad(e.gdevice.which)) pads_.push_back(p);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        for (auto it = pads_.begin(); it != pads_.end(); ++it) {
            if (SDL_GetGamepadID(*it) == e.gdevice.which) {
                SDL_CloseGamepad(*it);
                pads_.erase(it);
                break;
            }
        }
        std::fill(std::begin(pad_), std::end(pad_), false);
        std::fill(std::begin(axis_), std::end(axis_), false);
        syncInput();
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        handleGamepad(e);
        break;
    default:
        break;
    }
}

/// Joystick i konsola Atari: strzałki = joystick, spacja = przycisk joysticka (strzał),
/// Shift = granat (w dół + przycisk), Enter/F4 = START, F3 = SELECT, F2 = OPTION.
int App::gameKeyFor(SDL_Keycode key) {
    switch (key) {
    case SDLK_UP: return static_cast<int>(GameKey::up);
    case SDLK_DOWN: return static_cast<int>(GameKey::down);
    case SDLK_LEFT: return static_cast<int>(GameKey::left);
    case SDLK_RIGHT: return static_cast<int>(GameKey::right);
    case SDLK_SPACE: return static_cast<int>(GameKey::fire);
    case SDLK_LSHIFT: case SDLK_RSHIFT: return static_cast<int>(GameKey::grenade);
    case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_F4: return static_cast<int>(GameKey::start);
    case SDLK_F3: return static_cast<int>(GameKey::select);
    case SDLK_F2: return static_cast<int>(GameKey::option);
    default: return -1;
    }
}

/// Klawiatura Atari (KBCODE): litery i cyfry (hasło na ekranie tytułowym), P – pauza (spacja Atari),
/// Esc – przerwanie gry. -1 = klawisz nie trafia do Atari.
int App::atariKeyCode(SDL_Keycode key) {
    static const int letters[26] = {0x3F, 0x15, 0x12, 0x3A, 0x2A, 0x38, 0x3D, 0x39, 0x0D, 0x01, 0x05, 0x00, 0x25,
                                    0x23, 0x08, 0x0A, 0x2F, 0x28, 0x3E, 0x2D, 0x0B, 0x10, 0x2E, 0x16, 0x2B, 0x17};
    static const int digits[10] = {0x32, 0x1F, 0x1E, 0x1A, 0x18, 0x1D, 0x1B, 0x33, 0x35, 0x30};
    if (key == SDLK_P) return 0x21;   // pauza – w oryginale spacja
    if (key >= SDLK_A && key <= SDLK_Z) return letters[key - SDLK_A];
    if (key >= SDLK_0 && key <= SDLK_9) return digits[key - SDLK_0];
    switch (key) {
    case SDLK_ESCAPE: return 0x1C;
    case SDLK_MINUS: return 0x0E;
    case SDLK_EQUALS: return 0x0F;
    case SDLK_COMMA: return 0x20;
    case SDLK_PERIOD: return 0x22;
    case SDLK_SLASH: return 0x26;
    case SDLK_SEMICOLON: return 0x02;
    default: return -1;
    }
}

bool App::handleShortcut(const SDL_KeyboardEvent& k) {
    const bool mod = (k.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
    if (k.key == SDLK_F11 || (k.key == SDLK_RETURN && (k.mod & SDL_KMOD_ALT)) ||
        (mod && (k.mod & SDL_KMOD_CTRL) && (k.mod & SDL_KMOD_GUI) && k.key == SDLK_F)) {
        if (!k.repeat) toggleFullscreen();
        return true;
    }
    if (k.key == SDLK_F1 || k.key == SDLK_F10) {
        if (k.repeat) return true;
        if (overlay_ == Overlay::menu) closeOverlay(); else openOverlay(Overlay::menu);
        return true;
    }
    if (!mod) return false;
    if (k.repeat) return true;
    switch (k.key) {
    case SDLK_Q: running_ = false; return true;
    case SDLK_O: chooseTape(); return true;
    case SDLK_D: toggleMute(); return true;
    case SDLK_S: if (game_) saveCurrent(); return true;
    case SDLK_L: if (game_) openOverlay(Overlay::saves); return true;
    case SDLK_R: if (game_) resetGame(); return true;
    default: return true;   // inne skróty z Ctrl/Cmd nie trafiają do gry
    }
}

void App::handleKeyDown(const SDL_KeyboardEvent& k) {
    if (handleShortcut(k)) return;
    if (overlay_ != Overlay::none) {
        handleOverlayKey(k);
        return;
    }
    if (!game_) {
        if (!k.repeat && (k.key == SDLK_RETURN || k.key == SDLK_KP_ENTER || k.key == SDLK_SPACE)) chooseTape();
        return;
    }
    if (k.repeat) return;
    if (k.key == SDLK_BACKSPACE) {           // RESET
        resetGame();
        return;
    }
    if (k.key == SDLK_PAUSE || k.key == SDLK_F7) {   // BREAK
        game_->pressBreak();
        return;
    }
    const int gk = gameKeyFor(k.key);
    if (gk >= 0) {
        kb_[gk] = true;
        syncInput();
        return;
    }
    const int code = atariKeyCode(k.key);
    if (code >= 0) {
        atariKey_ = k.key;
        game_->atariKeyDown(code);
    }
}

void App::handleKeyUp(const SDL_KeyboardEvent& k) {
    const int gk = gameKeyFor(k.key);
    if (gk >= 0) {
        kb_[gk] = false;
        syncInput();
        return;
    }
    if (game_ && k.key == atariKey_) {
        atariKey_ = SDLK_UNKNOWN;
        game_->atariKeyUp();
    }
}

void App::handleGamepad(const SDL_Event& e) {
    if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        const int v = e.gaxis.value;
        const int dead = 16000;
        if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
            axis_[static_cast<int>(GameKey::left)] = v < -dead;
            axis_[static_cast<int>(GameKey::right)] = v > dead;
        } else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
            axis_[static_cast<int>(GameKey::up)] = v < -dead;
            axis_[static_cast<int>(GameKey::down)] = v > dead;
        } else {
            return;
        }
        if (overlay_ == Overlay::none) syncInput();
        return;
    }
    const bool down = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    const auto button = static_cast<SDL_GamepadButton>(e.gbutton.button);

    if (button == SDL_GAMEPAD_BUTTON_GUIDE) {
        if (down) { if (overlay_ == Overlay::menu) closeOverlay(); else openOverlay(Overlay::menu); }
        return;
    }
    if (overlay_ != Overlay::none || !game_) {
        // nawigacja po menu gamepadem
        if (!down) return;
        SDL_KeyboardEvent k{};
        switch (button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP: k.key = SDLK_UP; break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: k.key = SDLK_DOWN; break;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: k.key = SDLK_LEFT; break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: k.key = SDLK_RIGHT; break;
        case SDL_GAMEPAD_BUTTON_SOUTH: k.key = SDLK_RETURN; break;
        case SDL_GAMEPAD_BUTTON_EAST: k.key = SDLK_ESCAPE; break;
        default: return;
        }
        if (game_) handleOverlayKey(k); else if (k.key == SDLK_RETURN) chooseTape();
        return;
    }
    if (button == SDL_GAMEPAD_BUTTON_BACK) {          // pauza (spacja Atari)
        if (down) game_->atariKeyDown(0x21); else game_->atariKeyUp();
        return;
    }
    int gk = -1;
    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: gk = static_cast<int>(GameKey::up); break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: gk = static_cast<int>(GameKey::down); break;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: gk = static_cast<int>(GameKey::left); break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: gk = static_cast<int>(GameKey::right); break;
    case SDL_GAMEPAD_BUTTON_SOUTH:
    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_WEST:
    case SDL_GAMEPAD_BUTTON_NORTH: gk = static_cast<int>(GameKey::fire); break;
    case SDL_GAMEPAD_BUTTON_START: gk = static_cast<int>(GameKey::start); break;
    default: return;
    }
    // kilka przycisków strzału naraz – strzał trwa, dopóki którykolwiek jest wciśnięty
    if (gk == static_cast<int>(GameKey::fire)) {
        bool any = false;
        for (auto* p : pads_) {
            any = any || SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_SOUTH) ||
                  SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_EAST) || SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_WEST) ||
                  SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_NORTH);
        }
        pad_[gk] = any || down;
    } else {
        pad_[gk] = down;
    }
    syncInput();
}

void App::syncInput() {
    if (!game_) return;
    for (int i = 0; i < keyCount; ++i) {
        const bool want = kb_[i] || pad_[i] || axis_[i];
        if (want != applied_[i]) {
            game_->setKey(static_cast<GameKey>(i), want);
            applied_[i] = want;
        }
    }
}

void App::releaseInput() {
    std::fill(std::begin(kb_), std::end(kb_), false);
    std::fill(std::begin(pad_), std::end(pad_), false);
    std::fill(std::begin(axis_), std::end(axis_), false);
    std::fill(std::begin(applied_), std::end(applied_), false);
    atariKey_ = SDLK_UNKNOWN;
    if (game_) game_->releaseAllKeys();
}

// MARK: - Menu w grze

void App::openOverlay(Overlay o) {
    if (!game_ && o != Overlay::menu) return;
    releaseInput();
    overlay_ = o;
    confirmForget_ = false;
    if (o == Overlay::menu) menuSel_ = 0;
    if (o == Overlay::saves) { savesSel_ = 0; savesScroll_ = 0; }
}

void App::closeOverlay() {
    overlay_ = Overlay::none;
    confirmForget_ = false;
    releaseInput();
}

void App::handleOverlayKey(const SDL_KeyboardEvent& k) {
    const SDL_Keycode key = k.key;
    if (overlay_ == Overlay::menu) {
        // menu rysuje i obsługuje te same pozycje – liczba pozycji ustalana w drawMenu
        if (key == SDLK_ESCAPE) { closeOverlay(); return; }
        if (key == SDLK_UP) { menuSel_ -= 1; confirmForget_ = false; }
        if (key == SDLK_DOWN) { menuSel_ += 1; confirmForget_ = false; }
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
            if (menuSel_ >= 0 && menuSel_ < static_cast<int>(menuActions_.size())) {
                auto action = menuActions_[menuSel_];
                if (action) action();
            }
        }
        return;
    }
    if (overlay_ == Overlay::saves) {
        const auto& list = store_->entries();
        const int n = static_cast<int>(list.size());
        switch (key) {
        case SDLK_ESCAPE: closeOverlay(); break;
        case SDLK_UP: savesSel_ = std::max(0, savesSel_ - 1); break;
        case SDLK_DOWN: savesSel_ = std::min(n - 1, savesSel_ + 1); break;
        case SDLK_RETURN: case SDLK_KP_ENTER:
            if (savesSel_ >= 0 && savesSel_ < n) loadSave(list[savesSel_]);
            break;
        case SDLK_DELETE: case SDLK_BACKSPACE:
            if (savesSel_ >= 0 && savesSel_ < n) deleteSave(list[savesSel_].id);
            break;
        case SDLK_S: saveCurrent(); break;
        default: break;
        }
    }
}

void App::saveCurrent() {
    if (!game_) return;
    SaveEntry entry;
    if (!game_->makeSnapshot(entry.snapshot)) {
        show("Zapis możliwy tylko w trakcie gry");
        return;
    }
    static std::mt19937_64 rng{std::random_device{}()};
    const Game::Info info = game_->info();
    entry.id = rng();
    entry.date = static_cast<int64_t>(std::time(nullptr));
    entry.room = info.room;
    entry.score = info.score;
    entry.energy = info.energy;
    entry.containers = info.containers;
    entry.thumbnail = makeThumbnail(game_->pixels(), Game::width, Game::height);
    store_->add(std::move(entry));
    savesSel_ = 0;
    show("Zapisano stan gry (pomieszczenie " + roomName(info.room) + ")");
}

void App::loadSave(const SaveEntry& e) {
    if (!game_) return;
    if (!game_->restore(e.snapshot)) {
        show("Ten zapis jest uszkodzony");
        return;
    }
    closeOverlay();
    show("Wczytano zapis – pomieszczenie " + roomName(e.room));
}

void App::deleteSave(uint64_t id) {
    auto it = thumbTex_.find(id);
    if (it != thumbTex_.end()) {
        SDL_DestroyTexture(it->second);
        thumbTex_.erase(it);
    }
    store_->remove(id);
    savesSel_ = std::min(savesSel_, static_cast<int>(store_->entries().size()) - 1);
    if (savesSel_ < 0) savesSel_ = 0;
}

void App::resetGame() {
    if (!game_) return;
    closeOverlay();
    game_->reset();
}

std::string App::cheatLabel(const char* name, Game::Cheat c) const {
    const bool on = game_ && game_->cheat(c);
    return std::string(on ? "[x] " : "[ ] ") + name;
}

void App::toggleCheat(Game::Cheat c) {
    if (!game_) return;
    const bool on = !game_->cheat(c);
    cheats_[static_cast<int>(c)] = on;
    game_->setCheat(c, on);
}

void App::toggleFullscreen() {
    const bool fs = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
    SDL_SetWindowFullscreen(window_, !fs);
}

void App::toggleMute() {
    muted_ = !muted_;
    if (pokey_) pokey_->setMuted(muted_);
    show(muted_ ? "Dźwięk wyłączony" : "Dźwięk włączony");
}

void App::show(const std::string& text) {
    message_ = text;
    messageUntil_ = SDL_GetTicks() + 2500;
}

// MARK: - Rysowanie

void App::draw() {
    NativeMenu::State st;
    st.ready = game_ != nullptr;
    st.canSave = game_ && game_->canSave() && overlay_ == Overlay::none;
    st.muted = muted_;
    NativeMenu::update(st);

    buttons_.clear();
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    if (!game_) {
        drawImportScreen();
    } else {
        const auto& px = game_->pixels();
        SDL_UpdateTexture(screenTex_, nullptr, px.data(), Game::width * 4);
        const SDL_FRect dst{0, 0, static_cast<float>(viewW), static_cast<float>(viewH)};
        SDL_RenderTexture(renderer_, screenTex_, nullptr, &dst);
        switch (overlay_) {
        case Overlay::menu: drawMenu(); break;
        case Overlay::saves: drawSaves(); break;
        case Overlay::none: break;
        }
    }
    if (!game_ && overlay_ == Overlay::menu) drawMenu();
    drawToast();
}

/// Pierwsze uruchomienie: program nie zawiera danych oryginału, więc prosi o kasetę.
void App::drawImportScreen() {
    const float cx = viewW / 2.0f;
    drawTextCentered(cx, 40, "MISJA", 5, kAccent);
    drawTextCentered(cx, 110, "Potrzebne są dane z oryginalnej kasety", 2, kWhite);
    drawWrapped(cx, 145, 600,
                "Ze względu na prawa autorskie (LK Avalon, Janusz Pelc) program nie zawiera grafiki, "
                "pomieszczeń ani dźwięków oryginału. Wskaż obraz kasety oryginalnej gry Misja w formacie .cas. "
                "Dane zostaną odczytane i zapisane na tym komputerze; później plik kasety nie będzie już "
                "potrzebny.",
                1, kGray, 13);

    const SDL_FRect drop{cx - 190, 245, 380, 90};
    const bool hot = hovered(drop);
    frameRect(drop.x, drop.y, drop.w, drop.h, hot ? kAccent : kDim);
    drawTextCentered(cx, 270, "Upuść tutaj plik .cas", 2, hot ? kAccent : kGray);
    drawTextCentered(cx, 300, "albo kliknij, żeby wybrać", 1, hot ? kAccent : kGray);
    buttons_.push_back({drop, [this] { chooseTape(); }});

    button(cx - textWidth("Wybierz plik .cas (Enter)", 2) / 2 - 8, 355, "Wybierz plik .cas (Enter)", 2, kWhite,
           [this] { chooseTape(); });

    if (!importError_.empty()) drawWrapped(cx, 405, 620, importError_, 1, kRed, 13);
    drawTextCentered(cx, viewH - 18, "Folder danych: " + Storage::dataDir(), 1, kDim);
}

void App::drawMenu() {
    fillRect(0, 0, viewW, viewH, {0, 0, 0, 150});
    const float w = 500, h = 466, x = (viewW - w) / 2, y = (viewH - h) / 2;
    fillRect(x, y, w, h, kPanel);
    frameRect(x, y, w, h, kDim);
    drawTextCentered(viewW / 2.0f, y + 14, "Misja – menu", 2, kAccent);

    struct Item {
        std::string label;
        std::string key;
        std::function<void()> action;
        bool enabled;
    };
    const bool ready = game_ != nullptr;
    std::vector<Item> items = {
        {"Wróć do gry", "F1", [this] { closeOverlay(); }, true},
        {"Zapisz stan gry", shortcut("S"), [this] { saveCurrent(); closeOverlay(); },
         ready && game_->canSave()},
        {"Lista zapisów", shortcut("L"), [this] { openOverlay(Overlay::saves); }, ready},
        {"RESET – ekran tytułowy", "Bksp", [this] { resetGame(); }, ready},
        {cheatLabel("Nieograniczona energia", Game::Cheat::energy), "",
         [this] { toggleCheat(Game::Cheat::energy); }, ready},
        {cheatLabel("Nieograniczona amunicja", Game::Cheat::ammo), "",
         [this] { toggleCheat(Game::Cheat::ammo); }, ready},
        {cheatLabel("Nieograniczone granaty", Game::Cheat::grenades), "",
         [this] { toggleCheat(Game::Cheat::grenades); }, ready},
        {"Pełny ekran", "F11", [this] { toggleFullscreen(); }, true},
        {muted_ ? "Włącz dźwięk" : "Wyłącz dźwięk", shortcut("D"), [this] { toggleMute(); }, true},
        {"Wczytaj dane z kasety", shortcut("O"), [this] { closeOverlay(); chooseTape(); }, true},
        {confirmForget_ ? "Na pewno usunąć? Enter: tak" : "Usuń dane oryginału", "",
         [this] {
             if (confirmForget_) forgetOriginal(); else confirmForget_ = true;
         },
         ready},
        {"Zakończ", shortcut("Q"), [this] { running_ = false; }, true},
    };
    const int n = static_cast<int>(items.size());
    menuActions_.clear();
    if (menuSel_ < 0) menuSel_ = n - 1;
    if (menuSel_ >= n) menuSel_ = 0;
    float iy = y + 48;
    for (int i = 0; i < n; ++i) {
        const SDL_FRect r{x + 12, iy - 4, w - 24, 26};
        const bool hot = hovered(r);
        if (i == menuSel_) fillRect(r.x, r.y, r.w, r.h, kSelect);
        else if (hot) fillRect(r.x, r.y, r.w, r.h, {40, 50, 40, 255});
        const SDL_Color c = items[i].enabled ? kWhite : kDim;
        drawText(x + 24, iy + 1, items[i].label, 2, items[i].label.rfind("Na pewno", 0) == 0 ? kRed : c);
        if (!items[i].key.empty()) {
            drawText(x + w - 24 - textWidth(items[i].key, 1), iy + 5, items[i].key, 1, kGray);
        }
        auto action = items[i].action;
        const bool enabled = items[i].enabled;
        auto activate = [this, i, action, enabled] {
            menuSel_ = i;
            if (enabled) action();
        };
        buttons_.push_back({r, activate});
        menuActions_.push_back(activate);
        iy += 28;
    }
    // pomoc – sterowanie
    float hy = iy;
    const char* help[] = {
        "Strzałki: ruch  Spacja: strzał  Shift: granat  P: pauza",
        "Enter: START  F3: SELECT  F2: OPTION",
        "Esc: przerwanie gry  Pause/F7: BREAK  Backspace: RESET",
    };
    for (const char* h : help) {
        drawTextCentered(viewW / 2.0f, hy, h, 1, kGray);
        hy += 12;
    }
}

void App::drawSaves() {
    fillRect(0, 0, viewW, viewH, {0, 0, 0, 170});
    fillRect(16, 12, viewW - 32, viewH - 24, kPanel);
    frameRect(16, 12, viewW - 32, viewH - 24, kDim);
    drawText(32, 26, "Zapisy gry", 2, kAccent);
    const bool canSave = game_ && game_->canSave();
    button(viewW - 32 - textWidth("Zapisz bieżący stan (S)", 1) - 16, 24, "Zapisz bieżący stan (S)", 1,
           canSave ? kWhite : kDim, [this] { saveCurrent(); });

    const auto& list = store_->entries();
    const int n = static_cast<int>(list.size());
    if (n == 0) {
        drawTextCentered(viewW / 2.0f, 200, "Brak zapisów.", 2, kGray);
        drawTextCentered(viewW / 2.0f, 230, "Podczas gry naciśnij " + shortcut("S") + ", aby zapisać stan.", 1,
                         kGray);
    } else {
        const int visible = 3;
        savesSel_ = std::clamp(savesSel_, 0, n - 1);
        if (savesSel_ < savesScroll_) savesScroll_ = savesSel_;
        if (savesSel_ >= savesScroll_ + visible) savesScroll_ = savesSel_ - visible + 1;
        savesScroll_ = std::clamp(savesScroll_, 0, std::max(0, n - visible));
        float y = 56;
        for (int i = savesScroll_; i < std::min(n, savesScroll_ + visible); ++i) {
            const SaveEntry& e = list[i];
            const SDL_FRect row{28, y, viewW - 56.0f, 128};
            if (i == savesSel_) fillRect(row.x, row.y, row.w, row.h, kSelect);
            else if (hovered(row)) fillRect(row.x, row.y, row.w, row.h, {40, 50, 40, 255});
            buttons_.push_back({row, [this, i] { savesSel_ = i; }});
            if (SDL_Texture* t = thumbTexture(e)) {
                const SDL_FRect dst{row.x + 6, row.y + 4, 168, 120};
                SDL_RenderTexture(renderer_, t, nullptr, &dst);
            }
            const float tx = row.x + 190;
            drawText(tx, row.y + 12, "Pomieszczenie " + roomName(e.room), 2, kWhite);
            drawText(tx, row.y + 40,
                     "Wynik " + e.score + "   Energia " + std::to_string(e.energy) + "%   Pojemniki " +
                         std::to_string(e.containers),
                     1, kGray);
            drawText(tx, row.y + 56, formatDate(e.date), 1, kGray);
            const uint64_t id = e.id;
            button(tx, row.y + 86, "Wczytaj", 2, kGreen, [this, i] {
                const auto& l = store_->entries();
                if (i < static_cast<int>(l.size())) loadSave(l[i]);
            });
            button(tx + 150, row.y + 86, "Usuń", 2, kRed, [this, id] { deleteSave(id); });
            y += 132;
        }
        if (n > visible) {
            drawText(viewW - 120, viewH - 40, std::to_string(savesSel_ + 1) + "/" + std::to_string(n), 1, kGray);
        }
    }
    drawTextCentered(viewW / 2.0f, viewH - 28, "Enter: wczytaj  Delete: usuń  S: zapisz  Esc: zamknij", 1, kGray);
}

void App::drawToast() {
    if (message_.empty()) return;
    if (SDL_GetTicks() > messageUntil_) {
        message_.clear();
        return;
    }
    // długie komunikaty skracamy do szerokości okna
    std::string text = message_;
    float scale = 2;
    if (textWidth(text, scale) > viewW - 60) scale = 1;
    const float w = std::min(textWidth(text, scale), viewW - 60.0f) + 28, h = 8 * scale + 16;
    const float x = (viewW - w) / 2, y = 12;
    fillRect(x, y, w, h, {0, 0, 0, 200});
    drawText(x + 14, y + 8, text, scale, kWhite);
}

SDL_Texture* App::thumbTexture(const SaveEntry& e) {
    auto it = thumbTex_.find(e.id);
    if (it != thumbTex_.end()) return it->second;
    if (e.thumbnail.size() != size_t(SaveEntry::thumbWidth) * SaveEntry::thumbHeight) return nullptr;
    SDL_Texture* t = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
                                       SaveEntry::thumbWidth, SaveEntry::thumbHeight);
    if (!t) return nullptr;
    SDL_UpdateTexture(t, nullptr, e.thumbnail.data(), SaveEntry::thumbWidth * 4);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    thumbTex_[e.id] = t;
    return t;
}

void App::freeTextures() {
    for (auto& kv : thumbTex_) SDL_DestroyTexture(kv.second);
    thumbTex_.clear();
}

void App::fillRect(float x, float y, float w, float h, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    const SDL_FRect r{x, y, w, h};
    SDL_RenderFillRect(renderer_, &r);
}

void App::frameRect(float x, float y, float w, float h, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    const SDL_FRect r{x, y, w, h};
    SDL_RenderRect(renderer_, &r);
}

float App::textWidth(const std::string& s, float scale) const {
    return static_cast<float>(utf8Decode(s).size()) * 8.0f * scale;
}

/// Tekst wbudowaną czcionką SDL (8×8, polskie litery bez ogonków).
void App::drawText(float x, float y, const std::string& s, float scale, SDL_Color c) {
    std::string ascii;
    for (char32_t ch : utf8Decode(s)) ascii += asciiFold(ch);
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_SetRenderScale(renderer_, scale, scale);
    SDL_RenderDebugText(renderer_, x / scale, y / scale, ascii.c_str());
    SDL_SetRenderScale(renderer_, 1, 1);
}

void App::drawTextCentered(float cx, float y, const std::string& s, float scale, SDL_Color c) {
    drawText(cx - textWidth(s, scale) / 2, y, s, scale, c);
}

void App::drawWrapped(float cx, float y, float maxW, const std::string& s, float scale, SDL_Color c, float lineH) {
    std::vector<std::string> words;
    std::string cur;
    for (char ch : s) {
        if (ch == ' ') { if (!cur.empty()) words.push_back(cur); cur.clear(); }
        else cur += ch;
    }
    if (!cur.empty()) words.push_back(cur);
    std::string line;
    for (const auto& w : words) {
        const std::string test = line.empty() ? w : line + " " + w;
        if (textWidth(test, scale) > maxW && !line.empty()) {
            drawTextCentered(cx, y, line, scale, c);
            y += lineH;
            line = w;
        } else {
            line = test;
        }
    }
    if (!line.empty()) drawTextCentered(cx, y, line, scale, c);
}

bool App::button(float x, float y, const std::string& label, float scale, SDL_Color c, std::function<void()> action) {
    const SDL_FRect r{x, y - 5, textWidth(label, scale) + 16, 8 * scale + 10};
    const bool hot = hovered(r);
    fillRect(r.x, r.y, r.w, r.h, hot ? SDL_Color{60, 80, 60, 255} : SDL_Color{32, 40, 32, 255});
    frameRect(r.x, r.y, r.w, r.h, hot ? kAccent : kDim);
    drawText(x + 8, y, label, scale, c);
    buttons_.push_back({r, std::move(action)});
    return hot;
}

bool App::hovered(const SDL_FRect& r) const {
    return mouseX_ >= r.x && mouseX_ < r.x + r.w && mouseY_ >= r.y && mouseY_ < r.y + r.h;
}

// MARK: - Tryb testowy (--shots): zrzuty kolejnych ekranów interfejsu

void App::saveWindowShot(const std::string& path) {
    SDL_Surface* s = SDL_RenderReadPixels(renderer_, nullptr);
    if (!s) return;
    SDL_SavePNG(s, path.c_str());
    SDL_DestroySurface(s);
}

void App::testShots() {
    const Uint64 t = SDL_GetTicks() - startTicks_;
    const std::string dir = options_.shotsDir + "/";
    auto key = [this](SDL_Keycode k, bool down) {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.key = k;
        e.key.down = down;
        handleEvent(e);
    };
    struct Step { Uint64 at; std::function<void()> fn; };
    const std::vector<Step> steps = {
        {1500, [&] { saveWindowShot(dir + "01_start.png"); }},
        {7000, [&] { saveWindowShot(dir + "02_title.png"); }},
        {7100, [&] { openOverlay(Overlay::menu); }},
        {7300, [&] { saveWindowShot(dir + "03_menu.png"); }},
        {7400, [&] { closeOverlay(); key(SDLK_RETURN, true); }},
        {7600, [&] { key(SDLK_RETURN, false); }},
        {9500, [&] { saveWindowShot(dir + "04_game.png"); key(SDLK_RIGHT, true); }},
        {11000, [&] { key(SDLK_RIGHT, false); key(SDLK_UP, true); }},
        {11600, [&] { key(SDLK_UP, false); saveWindowShot(dir + "05_moved.png"); }},
        {11700, [&] { saveCurrent(); openOverlay(Overlay::saves); }},
        {11900, [&] { saveWindowShot(dir + "06_saves.png"); }},
        {12000, [&] { const auto& l = store_->entries(); if (!l.empty()) loadSave(l.front()); }},
        {13000, [&] { saveWindowShot(dir + "07_loaded.png"); key(SDLK_P, true); }},
        {13100, [&] { key(SDLK_P, false); }},
        {13600, [&] { saveWindowShot(dir + "08_pause.png"); key(SDLK_P, true); }},
        {13700, [&] { key(SDLK_P, false); key(SDLK_BACKSPACE, true); }},
        {13800, [&] { key(SDLK_BACKSPACE, false); }},
        {16000, [&] { saveWindowShot(dir + "09_reset.png"); }},
        {16100, [&] { running_ = false; }},
    };
    while (shotStep_ < static_cast<int>(steps.size()) && t >= steps[shotStep_].at) {
        steps[shotStep_].fn();
        ++shotStep_;
    }
}
