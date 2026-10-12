//
//  OriginalTape.h
//  Misja (C++ / SDL3)
//  Gra Misja Atari XE. autor: Janusz Pelc, grafika: Janusz Pelc i Marek Siewior.
//  autor konwersji: Leszek Klich 2026
//  Uwaga - niniejsza gra nie zawiera żadnych danych oryginalnej gry – grafiki, pomieszczeń, dźwięków ani tekstów.
//  Przy pierwszym uruchomieniu trzeba wskazać obraz z oryginalnej kasety jako plik .cas
//  Po wczytaniu plik kasety nie jest wymagany.
//  Licencja darmowa, ale pamiętaj, aby nigdy nie rozpowszechniać ninejszego silnika gry wraz z plikami .cas
/***********************************************************************************************************************************/

//  Odczyt danych z kasety oryginalnej gry Misja (LK Avalon 1990) w formacie .cas.
//
//  Program nie zawiera żadnych danych oryginału. Przy pierwszym uruchomieniu gracz wskazuje plik .cas, a ten moduł odtwarza 
//  z niego obraz pamięci Atari $0000-$9FFF – taki, jaki zostawia program ładujący z kasety.
//  Jak zbudowany jest zapis taśmy magnetofonowej:
//  • 3 rekordy 600 bodów w standardowym formacie Atari (55 55 FC + 128 bajtów + suma):
//    blok startowy ładowany pod $0480 (nagłówek: flagi, 2 rekordy, adres $0480, init),
//    trzeci rekord program startowy kopiuje z bufora kasety $0400 do $0580,
//  • 284 rekordy własnego formatu programu startowego: 55 55 + 128 bajtów + bajt kontrolny,
//    ładowane kolejno od $1200 do $9FFF. Bajt kontrolny to XOR wszystkich dotychczasowych
//    danych z wartością początkową $55 (narastająco przez całą taśmę),
//  • start programu: JMP $6800.
//  Wynik jest weryfikowany sumą SHA-256, więc działa każdy wierny zrzut tej kasety.

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

/// Błąd importu z czytelnym komunikatem dla gracza.
class ImportError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct TapeFile {
    std::string name;            // nazwa pliku (do komunikatów)
    std::vector<uint8_t> bytes;  // zawartość
};

namespace OriginalTape {

/// Rozmiar odtwarzanego obrazu pamięci ($0000-$9FFF; używane $0480-$05FF i $1200-$9FFF).
constexpr int imageSize = 0xA000;
/// SHA-256 obrazu pamięci po wczytaniu oryginalnej kasety.
constexpr const char* expectedSHA256 = "dfda611f75c2f1ce20d91e7a651831a7715741c2b10b4bb9f00b59aa238573fb";

bool isValidImage(const std::vector<uint8_t>& image);

/// Odtwarza obraz pamięci z pliku .cas (albo przyjmuje gotowy obraz). Rzuca ImportError.
std::vector<uint8_t> extract(const std::vector<TapeFile>& files);

}  // namespace OriginalTape
