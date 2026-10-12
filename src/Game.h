//
//  Game.h
//  Misja (C++ / SDL3)
//  Gra Misja Atari XE. autor: Janusz Pelc, grafika: Janusz Pelc i Marek Siewior.
//  autor konwersji: Leszek Klich 2026
//  Uwaga - niniejsza gra nie zawiera żadnych danych oryginalnej gry – grafiki, pomieszczeń, dźwięków ani tekstów.
//  Przy pierwszym uruchomieniu trzeba wskazać obraz z oryginalnej kasety jako plik .cas
//  Po wczytaniu plik kasety nie jest wymagany.
//  Licencja darmowa, ale pamiętaj, aby nigdy nie rozpowszechniać ninejszego silnika gry wraz z plikami .cas
/***********************************************************************************************************************************/
//  Gra dla warstwy okna: sprzęt Atari (Machine), przepisany kod gry (Misja), dźwięk POKEY, joystick/konsola/klawiatura Atari oraz zapis i wczytanie stanu gry.

#pragma once

#include "Machine.h"
#include "Misja.h"
#include "Pokey.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/// grenade = joystick w dół + przycisk (tak granat rzuca się w oryginale)
enum class GameKey { up, down, left, right, fire, start, select, option, grenade, count };

class Game {
public:
    static constexpr int width = Machine::width;
    static constexpr int height = Machine::height;

    /// `image` – obraz pamięci z kasety (OriginalTape::extract).
    Game(const std::vector<uint8_t>& image, Pokey& pokey);
    ~Game();

    /// Jedna ramka 50 Hz.
    void frame();
    const std::vector<uint32_t>& pixels() const { return machine_.pixels(); }
    const std::vector<uint8_t>& memory() const { return machine_.m; }

    // Wejście
    void setKey(GameKey key, bool pressed);
    /// Klawiatura Atari: KBCODE (z bitami SHIFT $40 i CONTROL $80); keyUp – puszczenie.
    void atariKeyDown(int kbcode);
    void atariKeyUp();
    void pressBreak();
    void releaseAllKeys();
    /// RESET: program od początku (ekran tytułowy); gra zostaje w pamięci.
    void reset();

    // Ułatwienia (nie ma ich w oryginale)
    enum class Cheat { energy, ammo, grenades };
    void setCheat(Cheat c, bool on);
    bool cheat(Cheat c) const;

    // Stan
    bool isPlaying() const;   // trwa zwykła gra (nie tytuł, nie pokaz)
    bool canSave() const;     // gra w miejscu, w którym da się zapisać stan
    struct Info {
        int room = 0;
        std::string score;    // 6 cyfr
        int energy = 0;       // 0-100 %
        int containers = 0;   // pojemniki z uranem przy sobie
    };
    Info info() const;
    bool makeSnapshot(std::vector<uint8_t>& out) const;
    bool restore(const std::vector<uint8_t>& snapshot);

private:
    Machine machine_;
    std::unique_ptr<Misja> misja_;
    Pokey& pokey_;
    bool keys_[static_cast<int>(GameKey::count)] = {};
    void applyInput();
    void drawCredit();
};
