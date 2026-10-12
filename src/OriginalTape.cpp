//  OriginalTape.cpp
//  Misja (C++ / SDL3)
//  Gra Misja Atari XE. autor: Janusz Pelc, grafika: Janusz Pelc i Marek Siewior.
//  autor konwersji: Leszek Klich 2026
//  Uwaga - niniejsza gra nie zawiera żadnych danych oryginalnej gry – grafiki, pomieszczeń, dźwięków ani tekstów.
//  Przy pierwszym uruchomieniu trzeba wskazać obraz z oryginalnej kasety jako plik .cas
//  Po wczytaniu plik kasety nie jest wymagany.
//  Licencja darmowa, ale pamiętaj, aby nigdy nie rozpowszechniać ninejszego silnika gry wraz z plikami .cas
/***********************************************************************************************************************************/

#include "OriginalTape.h"

#include "Sha256.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr int kBootAddr = 0x0480;
constexpr int kDataStart = 0x1200;
constexpr int kDataEnd = 0xA000;
constexpr int kDataRecords = (kDataEnd - kDataStart) / 128;   // 284

/// Bloki „data” pliku .cas (każdy to jeden rekord z taśmy, razem z bajtami 55 55).
bool blocksFromCAS(const std::vector<uint8_t>& bytes, std::vector<std::vector<uint8_t>>& out) {
    if (bytes.size() < 8 || std::memcmp(bytes.data(), "FUJI", 4) != 0) return false;
    size_t p = 0;
    while (p + 8 <= bytes.size()) {
        const size_t length = size_t(bytes[p + 4]) | (size_t(bytes[p + 5]) << 8);
        const size_t start = p + 8, end = start + length;
        if (end > bytes.size()) break;
        if (std::memcmp(bytes.data() + p, "data", 4) == 0 && length >= 2 && bytes[start] == 0x55 &&
            bytes[start + 1] == 0x55) {
            out.emplace_back(bytes.begin() + start, bytes.begin() + end);
        }
        p = end;
    }
    return true;
}

}  // namespace

namespace OriginalTape {

bool isValidImage(const std::vector<uint8_t>& image) {
    return image.size() == static_cast<size_t>(imageSize) &&
           sha256Hex(image.data(), image.size()) == expectedSHA256;
}

std::vector<uint8_t> extract(const std::vector<TapeFile>& files) {
    const char* notMisja = "To nie jest oryginalna kaseta Misji (LK Avalon) albo plik jest uszkodzony – "
                           "dane się nie zgadzają.";
    for (const auto& f : files) {
        // Gotowy obraz pamięci (np. wcześniej zapisany) – przyjmujemy, jeśli suma się zgadza.
        if (isValidImage(f.bytes)) return f.bytes;
        std::vector<std::vector<uint8_t>> blocks;
        if (!blocksFromCAS(f.bytes, blocks)) {
            throw ImportError("Plik „" + f.name + "” nie jest obrazem kasety Atari (.cas).");
        }
        // Blok startowy: trzy pełne rekordy (55 55 FC), pierwszy z nagłówkiem 2 rekordów pod $0480.
        size_t i = 0;
        while (i < blocks.size() && !(blocks[i].size() == 132 && blocks[i][2] == 0xFC && blocks[i][4] == 0x02 &&
                                      blocks[i][5] == 0x80 && blocks[i][6] == 0x04)) {
            ++i;
        }
        if (i + 3 > blocks.size()) continue;
        std::vector<uint8_t> image(imageSize, 0);
        for (int r = 0; r < 3; ++r) {
            const auto& b = blocks[i + r];
            if (b.size() != 132 || b[2] != 0xFC) throw ImportError(notMisja);
            std::copy_n(b.begin() + 3, 128, image.begin() + kBootAddr + r * 128);
        }
        i += 3;
        // Dane: rekordy 55 55 + 128 bajtów + narastający XOR (start $55).
        int sum = 0x55, loaded = 0;
        for (; i < blocks.size() && loaded < kDataRecords; ++i) {
            const auto& b = blocks[i];
            if (b.size() != 131) continue;
            for (int k = 0; k < 128; ++k) sum ^= b[2 + k];
            if (sum != b[130]) {
                throw ImportError("Błąd sumy kontrolnej w bloku " + std::to_string(loaded + 1) +
                                  " – plik kasety jest uszkodzony.");
            }
            std::copy_n(b.begin() + 2, 128, image.begin() + kDataStart + loaded * 128);
            ++loaded;
        }
        if (loaded < kDataRecords) {
            throw ImportError("Kaseta urywa się po " + std::to_string(loaded) + " z " + std::to_string(kDataRecords) +
                              " bloków danych.");
        }
        if (!isValidImage(image)) throw ImportError(notMisja);
        return image;
    }
    throw ImportError("Nie znaleziono bloku startowego Misji – wskaż plik .cas z grą Misja.");
}

}  // namespace OriginalTape
