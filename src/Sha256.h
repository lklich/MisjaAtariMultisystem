//
//  Sha256.h
//  Misja (C++ / SDL3)
//  Gra Misja Atari XE. autor: Janusz Pelc, grafika: Janusz Pelc i Marek Siewior.
//  autor konwersji: Leszek Klich 2026
//  Uwaga - niniejsza gra nie zawiera żadnych danych oryginalnej gry – grafiki, pomieszczeń, dźwięków ani tekstów.
//  Przy pierwszym uruchomieniu trzeba wskazać obraz z oryginalnej kasety jako plik .cas
//  Po wczytaniu plik kasety nie jest wymagany.
//  Licencja darmowa, ale pamiętaj, aby nigdy nie rozpowszechniać ninejszego silnika gry wraz z plikami .cas
/***********************************************************************************************************************************/
//  Minimalna implementacja SHA-256 (FIPS 180-4) do weryfikacji danych z kasety.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

/// Skrót SHA-256 jako 64 znaki szesnastkowe (małe litery).
std::string sha256Hex(const uint8_t* data, size_t size);
