# Specyfikacja projektu: ray tracer w C++26

## 1. Cel i zakres

Program wiersza poleceń renderujący statyczne obrazy metodą **śledzenia ścieżek** (_path tracing_, Monte Carlo).
Scena jest opisana w prostym, własnym języku tekstowym. Wynikiem jest plik **PPM w wariancie P3 (ASCII)**.

Projekt ma charakter dydaktyczny: kod ma w sposób naturalny (a nie sztuczny) wykorzystywać możliwie wiele elementów
paradygmatu obiektowego — zob. [§ 12](#12-mapa-wymagań-obiektowych).

### 1.1. W zakresie

- Jedyny kształt: **kula**. Podłoże modeluje się bardzo dużą kulą.
- Materiały: **lambertowski** (matowy), **metal** (z rozmyciem), **szkło** (dielektryk), **plastik** (podłoże
  lambertowskie z błyszczącą powłoką).
- Jedynym źródłem światła jest **niebo** (gradient tła).
- Kamera: położenie, punkt docelowy, wektor „góry”, pionowe pole widzenia, **rozmycie głębi ostrości** (przysłona
  i odległość ogniskowania).
- Renderowanie wielowątkowe (`std::jthread`), deterministyczne dla danego ziarna, niezależnie od liczby wątków.
- Logowanie z poziomami i obsługa błędów oparta na wyjątkach.
- Testy oparte na własnościach (rapidcheck).

### 1.2. Poza zakresem

Inne kształty niż kule, siatki trójkątów, tekstury, emisyjne materiały i światła punktowe, struktury przyspieszające
(BVH itp.), formaty wyjściowe inne niż PPM P3, renderowanie interaktywne, animacja, moduły C++ (`import std`),
zbieranie wielu błędów składni naraz.

## 2. Konwencje

- **Dokumentacja** (ten plik, `docs/`): język polski.
- **Kod** (identyfikatory, nazwy plików i katalogów, komentarze, komunikaty programu, komunikaty commitów): język
  angielski w **odmianie brytyjskiej z końcówkami „-ise”**: `Colour`, `normalise`, `normalised()`, `centre`,
  `initialise`, `serialise`, `quantise`, `Tokeniser`, `behaviour`, `optimise`, `maths/`, `MathsError`. Wyjątkiem są
  wyłącznie nazwy narzucone przez bibliotekę standardową, biblioteki zewnętrzne i narzędzia (np. `std::numeric_limits`,
  `-fsanitize`, nazwy reguł clang-tidy), gdy wymaga tego współpraca z nimi.
- Przestrzeń nazw: `rt` (testy: `rt::test`).
- Nazewnictwo (wymuszane przez `readability-identifier-naming` w `.clang-tidy`):
  - `PascalCase`: klasy, struktury, wyliczenia, aliasy typów, parametry szablonów (`Vector<T, N>`, `Vec3`);
  - `snake_case`: przestrzenie nazw, funkcje, metody, zmienne, parametry, koncepty, wartości wyliczeń
    (`LogLevel::debug`), pola publiczne struktur oraz stałe, także `constexpr` (bez przedrostka `k_`);
  - składowe prywatne i chronione: `snake_case` z sufiksem `_` (`components_`);
  - makra: `UPPER_CASE`.
- Formatowanie: clang-format, wcięcia tabulatorami (zgodnie z `.editorconfig`).
- Każdy plik z kodem (źródła, nagłówki, testy, skrypty CMake) i z dokumentacją przeznaczoną dla ludzi kończy się
  blokiem zmiennych lokalnych Emacsa (w składni komentarza danego języka), ustawiającym co najmniej
  `ispell-local-dictionary`: `"en-GB"` dla kodu i tekstów angielskich, `"pl-PL"` dla dokumentacji po polsku. Pliki
  konfiguracyjne (`.clang-format`, `.clang-tidy`, `.editorconfig`, `.gitignore`, `CMakePresets.json` itp.) nie mają
  tego bloku. Przykład dla C++:

  ```cpp
  // Local Variables:
  // ispell-local-dictionary: "en-GB"
  // End:
  ```
- Układ współrzędnych: prawoskrętny, oś Y w górę. Piksel `(0, 0)` to lewy górny róg obrazu.
- Typ zmiennoprzecinkowy: `double` (szablony dopuszczają dowolny `std::floating_point`). Uzasadnienie:
  - Przejście na `float` nie przyniosłoby istotnego przyspieszenia dzięki SIMD. Projekt nie stosuje układu
    „struktury tablic” (_structure of arrays_, SoA): promienie przetwarzane są pojedynczo, a `Vec3` przechowuje
    trzy składowe w jednej tablicy (układ „tablicy struktur”). Wektoryzacja obejmuje więc co najwyżej składowe
    jednego wektora (3 z 4 pól rejestru niezależnie od typu), a czas renderowania zdominowany jest przez wywołania
    wirtualne, rozgałęzienia materiałów i generator liczb losowych.
  - `float` pogarsza dokładność tam, gdzie jest ona istotna: równanie kwadratowe dla bardzo dużej kuli podłoża
    (znoszenie się składników rzędu 10⁶), stały próg `t ≥ 0.001` przy odległych trafieniach, zakres `[0, 1)`
    generatora (zaokrąglenie do `1.0f`) oraz sumowanie wielu próbek na piksel.

## 3. Stos technologiczny i budowanie

| Element          | Wybór                                                                  |
|------------------|------------------------------------------------------------------------|
| Język            | C++26 (`CMAKE_CXX_STANDARD 26`, `CMAKE_CXX_EXTENSIONS OFF`)            |
| Kompilatory      | GCC 16.2, Clang 23.1 (Linux); kod musi budować się oboma               |
| Budowanie        | CMake ≥ 3.30, generator Ninja, `CMakePresets.json`                     |
| Linter           | clang-tidy (`.clang-tidy` w repozytorium, uruchamiany z CMake)         |
| Formatowanie     | clang-format (`.clang-format` w repozytorium)                          |
| Testy            | rapidcheck (samodzielnie, bez GoogleTest/Catch2) + CTest               |
| Generator losowy | pcg-cpp (`pcg32`)                                                      |
| Zależności       | `FetchContent` z przypiętymi commitami; poza tym tylko biblioteka std. |

Szczegóły:

- **pcg-cpp** nie dostarcza `CMakeLists.txt` — po pobraniu przez `FetchContent` tworzymy bibliotekę `INTERFACE`
  `pcg::pcg` z katalogiem nagłówków oznaczonym jako `SYSTEM` (by ostrzeżenia z jej kodu nie łamały `-Werror`).
- **rapidcheck** budowany przez własny `CMakeLists.txt`, linkowany wyłącznie do celów testowych.
- Flagi ostrzeżeń (oba kompilatory): `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
  -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual`; `-Werror` w presetach CI.
- Presety: `gcc-debug`, `gcc-release`, `clang-debug`, `clang-release`, `clang-asan` (AddressSanitizer +
  UndefinedBehaviorSanitizer), `clang-tsan` (ThreadSanitizer).
- Cele CMake: biblioteka statyczna `rt_core` (cała logika), plik wykonywalny `raytracer` (tylko `main.cpp`), pliki
  wykonywalne testów `test_*` linkowane z `rt_core`.

### 3.1. Funkcje C++26

Bazowo wolno używać tylko tych elementów C++23/26, które działają w **obu** kompilatorach
z [§ 3](#3-stos-technologiczny-i-budowanie). Przewidywane do użycia (jeśli obsługiwane):

- `= delete("powód");` (P2573) — np. zakaz kopiowania `Logger`/`WorkerPool` z komunikatem,
- zmienne zastępcze `_` (P2169) — np. w pętlach po próbkach,
- indeksowanie paczek parametrów (P2662) — w szablonie `Vector`,
- wielowymiarowy `operator[]` (C++23) — `image[x, y]`,
- `std::print`/`std::println` (C++23), `std::format` z własnymi `std::formatter` dla typów `rt`,
- `std::expected` (C++23) tam, gdzie błąd jest oczekiwany i lokalny (np. pojedynczy token liczbowy w parserze);
  błędy przekraczające granice modułów sygnalizujemy wyjątkami.

Kontrakty (P2900) i statyczna refleksja (P2996) są **opcjonalne**: można ich użyć tylko wtedy, gdy oba kompilatory je
obsługują, i tylko w sposób, który nie zmienia zachowania programu.

## 4. Interfejs wiersza poleceń

```
raytracer <scene-file> <WIDTHxHEIGHT> <output.ppm> [options]
```

| Opcja                 | Domyślnie                                      | Opis                                        |
|-----------------------|------------------------------------------------|---------------------------------------------|
| `--samples N`         | `100`                                          | Liczba próbek na piksel, `N ≥ 1`            |
| `--depth N`           | `50`                                           | Maksymalna liczba odbić promienia, `N ≥ 0`  |
| `--seed N`            | `0`                                            | Ziarno generatora (`uint64`)                |
| `--threads N`         | `std::thread::hardware_concurrency()` (min. 1) | Liczba wątków roboczych, `N ≥ 1`            |
| `--log-level L`       | `info`                                         | `debug`, `info`, `warning`, `error`         |
| `--log-file PATH`     | brak                                           | Dodatkowe logowanie do pliku                |
| `--help`              | —                                              | Wypisuje pomoc na stdout i kończy z kodem 0 |

- Rozdzielczość: `WIDTHxHEIGHT`, obie liczby całkowite z przedziału `[1, 16384]`. Proporcje obrazu wynikają
  z rozdzielczości.
- Opcje mogą wystąpić w dowolnym miejscu po nazwie programu, argumenty pozycyjne w podanej kolejności.
- Nieznana opcja, brak wartości, wartość spoza zakresu, zła liczba argumentów pozycyjnych → `CliError`.

### 4.1. Kody wyjścia

| Kod | Znaczenie                                         |
|-----|---------------------------------------------------|
| 0   | Sukces (również `--help`)                         |
| 1   | Błąd wiersza poleceń (`CliError`)                 |
| 2   | Błąd opisu sceny (`SceneError` i pochodne)        |
| 3   | Błąd wejścia-wyjścia (`IoError`)                  |
| 4   | Inny, nieoczekiwany wyjątek                       |

`main` łapie wyjątki wyłącznie w jednym miejscu, loguje je na poziomie `error` i zwraca odpowiedni kod.

## 5. Język opisu sceny

### 5.1. Leksyka

- Plik tekstowy ASCII, przetwarzany wierszami. Jeden wiersz = jedna dyrektywa. Dozwolone są wyłącznie znaki
  drukowalne (`0x20`–`0x7E`), tabulator i znak nowego wiersza (LF; para CR LF jest traktowana jak LF); jedynym
  wyjątkiem są komentarze (niżej). Każdy inny bajt jest błędem (`SceneParseError` wskazujący ten bajt). Poza
  komentarzami kolumna to więc po prostu numer bajtu w wierszu (od 1).
- Komentarz od `#` do końca wiersza. Poza znakami dozwolonymi w całym pliku może zawierać dowolne bajty `≥ 0x80`
  (np. znaki UTF-8 spoza ASCII), które **nie są walidowane** — parser je pomija, nie sprawdzając poprawności
  kodowania. Znaki sterujące (poza tabulatorem) są błędem także w komentarzu. Bajty UTF-8 spoza ASCII nigdy nie są
  równe `#` ani LF, więc komentarz można odciąć wyszukiwaniem bajtu `#` bez dekodowania.
- Puste wiersze są ignorowane.
- Tokeny rozdzielone dowolnymi białymi znakami (spacja, tabulator).
- **Liczba**: zapis dziesiętny akceptowany przez `std::from_chars` dla `double` (np. `-1`, `0.5`, `1e-3`);
  wartości nieskończone i NaN są błędem.
- **Identyfikator**: `[A-Za-z][A-Za-z0-9_-]*`, od 1 do 20 znaków — zaczyna się literą ASCII, dalej litery, cyfry,
  `_` i `-`. Identyfikator nigdy nie jest więc poprawną liczbą (`inf` i `nan` są identyfikatorami, a jako liczby
  i tak są błędem).
- Słowa kluczowe rozróżniają wielkość liter (wyłącznie małe).

### 5.2. Dyrektywy

Notacja: `<v3>` = trzy liczby, `<rgb>` = trzy liczby z przedziału `[0, 1]`, `[…]` = opcjonalne.

```
camera from <v3> to <v3> [up <v3>] [fov <deg>] [aperture <a>] [focus <d>]
sky top <rgb> bottom <rgb>
material <name> lambertian <rgb>
material <name> metal <rgb> [fuzz <f>]
material <name> glass <ior>
material <name> plastic <rgb> [ior <n>]
sphere <v3> <radius> <material-name>
```

Pary klucz–wartość w `camera` mogą występować w dowolnej kolejności, każdy klucz najwyżej raz.

| Parametr        | Domyślnie                      | Ograniczenia                                       |
|-----------------|--------------------------------|----------------------------------------------------|
| `up`            | `0 1 0`                        | niezerowy, nierównoległy do `to − from`            |
| `fov`           | `90`                           | `(0, 180)` stopni, pionowe pole widzenia           |
| `aperture`      | `0`                            | `≥ 0` (średnica przysłony; `0` = kamera otworkowa) |
| `focus`         | `‖to − from‖`                  | `> 0`                                              |
| `from`, `to`    | wymagane                       | `from ≠ to`                                        |
| `sky`           | `top 0.5 0.7 1.0 bottom 1 1 1` | `<rgb>`                                            |
| `fuzz`          | `0`                            | `[0, 1]`                                           |
| `glass <ior>`   | wymagane                       | `> 0`                                              |
| `plastic … ior` | `1.5`                          | `> 0`                                              |
| `radius`        | wymagane                       | `> 0`                                              |

#### Współczynnik załamania (`ior`)

`ior` (_index of refraction_) to **współczynnik załamania** materiału, `n = c / v` — stosunek prędkości światła
w próżni do prędkości w materiale. Zakładamy, że otoczeniem wszystkich kul jest powietrze (`n = 1`). Typowe wartości:
woda 1.33, szkło 1.5, diament 2.42.

Parametr pełni dwie role:

1. **Załamanie** (tylko `glass`). Kierunek promienia przechodzącego przez granicę ośrodków wyznacza prawo Snella,
   `n₁ sin θ₁ = n₂ sin θ₂`: przy wejściu do kuli stosunek współczynników wynosi `1/ior`, przy wyjściu — `ior`. Gdy
   promień wychodzi pod zbyt dużym kątem (`ior · sin θ > 1`), następuje całkowite wewnętrzne odbicie. Dla `ior = 1`
   promień przechodzi bez zmiany kierunku — kula jest niewidoczna.
2. **Odbicie zależne od kąta** (`glass` i `plastic`). Prawdopodobieństwo odbicia lustrzanego wyznacza przybliżenie
   Schlicka równań Fresnela:

   `R₀ = ((1 − n) / (1 + n))²`, `R(θ) = R₀ + (1 − R₀) · (1 − cos θ)⁵`.

   Dla `n = 1.5` powierzchnia oglądana prostopadle odbija ok. 4% światła, a przy kątach ślizgowych — prawie 100%.
   Wyjątkiem jest `n = 1`: granicy ośrodków wtedy nie ma, więc `R(θ) = 0` dla każdego kąta (wzór Schlicka dawałby
   `(1 − cos θ)⁵`). Odpowiada to dokładnym równaniom Fresnela, według których odbicie przy kącie ślizgowym dąży do 1
   dla każdego `n ≠ 1`, a dla `n = 1` znika — nieciągłość jest fizyczna.
   W `plastic` jest to jedyna rola `ior`: opisuje on przezroczystą powłokę, od której promień odbija się lustrzanie
   z prawdopodobieństwem `R(θ)`, a w pozostałych przypadkach rozprasza się jak na podłożu lambertowskim. Większy
   `ior` daje bardziej błyszczący plastik; domyślne `1.5` odpowiada typowym tworzywom i lakierom.

Wartości `ior < 1` są fizycznie nietypowe, ale dobrze określone — zachowują się jak pęcherzyk powietrza w gęstszym
ośrodku.

### 5.3. Reguły semantyczne

- Dokładnie jedna dyrektywa `camera`; najwyżej jedna `sky`.
- Nazwy materiałów są unikalne; materiał musi być zdefiniowany **przed** użyciem w `sphere`.
- Wiele kul może współdzielić jeden materiał.
- Scena bez kul jest poprawna (renderuje się samo niebo).

### 5.4. Błędy

Parser kończy działanie przy **pierwszym** błędzie, rzucając `SceneParseError` z nazwą pliku, numerem wiersza
i kolumny (od 1) oraz opisem, np.:

```
scenes/demo.txt:7:12: error: expected number, got 'abc'
```

Błędy semantyczne (np. nieznany materiał, brak kamery) także rzucają `SceneParseError` ze wskazaniem wiersza
(dla braku kamery: wiersz 0, kolumna 0 i odpowiedni komunikat).

### 5.5. Przykład

```
# scenes/demo.txt
camera from -2 2 1 to 0 0 -1 up 0 1 0 fov 40 aperture 0.1 focus 3.4
sky top 0.5 0.7 1.0 bottom 1 1 1

material ground lambertian 0.8 0.8 0.0
material red    plastic    0.7 0.1 0.1 ior 1.5
material glass  glass      1.5
material gold   metal      0.8 0.6 0.2 fuzz 0.1

sphere  0 -100.5 -1  100   ground
sphere  0  0     -1  0.5   red
sphere -1  0     -1  0.5   glass
sphere  1  0     -1  0.5   gold
```

## 6. Algorytm renderowania

### 6.1. Kamera

Z `from`, `to`, `up`, `fov`, `focus` i proporcji obrazu kamera wyznacza ortonormalną bazę `(u, v, w)` oraz płaszczyznę
ogniskowania w odległości `focus`. Dla piksela `(x, y)` i próbki losowany jest punkt wewnątrz piksela (przesunięcie
jednostajne w `[−0.5, 0.5)²`) i punkt na dysku przysłony o średnicy `aperture`; promień biegnie z punktu na dysku do
punktu na płaszczyźnie ogniskowania.

### 6.2. Śledzenie ścieżki

Iteracyjnie (bez rekurencji), z wagą przepływu `throughput`, początkowo `(1, 1, 1)`:

1. Jeżeli wyczerpano `depth` odbić → wkład czarny.
2. Znajdź najbliższe trafienie wśród wszystkich kul dla `t ∈ [0.001, ∞)` (przegląd liniowy).
3. Brak trafienia → wkład `throughput × sky(direction)`, gdzie
   `sky(d) = lerp(bottom, top, 0.5 · (d̂.y + 1))`.
4. Trafienie → materiał zwraca rozproszony promień i tłumienie (`attenuation`) albo pochłonięcie (wkład czarny);
   `throughput ×= attenuation`, kontynuuj z nowym promieniem.

### 6.3. Materiały

- **Lambertian**: kierunek `n + losowy wektor jednostkowy` (przy zdegenerowanym wyniku — `n`); tłumienie = albedo.
- **Metal**: odbicie lustrzane + `fuzz × losowy wektor jednostkowy`; pochłonięcie, gdy wynik skierowany jest pod
  powierzchnię; tłumienie = albedo.
- **Glass**: załamanie wg prawa Snella ze współczynnikiem `ior` (od strony wewnętrznej `1/ior`); całkowite wewnętrzne
  odbicie, gdy `sin θ_t > 1`; inaczej odbicie z prawdopodobieństwem Fresnela (przybliżenie Schlicka); tłumienie
  `(1, 1, 1)`.
- **Plastic**: z prawdopodobieństwem Fresnela–Schlicka (współczynnik `ior` powłoki) — odbicie lustrzane z tłumieniem
  `(1, 1, 1)`; w przeciwnym razie zachowanie jak Lambertian z danym albedo.

### 6.4. Piksel i obraz

- Kolor piksela = średnia z `samples` próbek.
- Korekcja gamma 2 (`sqrt` składowych), przycięcie do `[0, 1)`, tj. do największej liczby mniejszej od 1
  (`std::nextafter(1.0, 0.0)`), kwantyzacja `⌊256 · c⌋` → `[0, 255]`.
- Plik wyjściowy: `P3`, `width height`, `255`, następnie piksele wierszami od góry, jeden piksel na wiersz
  (`r g b`).

### 6.5. Losowość i determinizm

- Generator: `pcg32` opakowany w klasę `Sampler`.
- Każdy piksel ma **własny strumień**: `pcg32(seed, y · width + x)`. Wynik renderowania zależy więc wyłącznie od
  sceny, rozdzielczości, `samples`, `depth` i `seed` — **nie** od liczby wątków ani kolejności ich pracy.

### 6.6. Wielowątkowość

`WorkerPool` tworzy `threads` obiektów `std::jthread`; wiersze obrazu przydzielane są dynamicznie przez
`std::atomic<std::size_t>`. Każdy wątek zapisuje tylko własne wiersze obrazu (brak współdzielonych zapisów). Wyjątek
z wątku roboczego jest przechwytywany (`std::exception_ptr`), pozostałe wątki są zatrzymywane (`std::stop_token`),
a wyjątek jest ponownie rzucany w wątku wywołującym. Postęp (liczba ukończonych wierszy) logowany jest na poziomie
`info` co ok. 10%.

## 7. Architektura

### 7.1. Układ katalogów

```
CMakeLists.txt  CMakePresets.json  .clang-tidy  .clang-format
cmake/                 # Dependencies.cmake (FetchContent), Warnings.cmake
include/rt/
  maths/               vector.hpp interval.hpp ray.hpp
  colour.hpp
  random/              sampler.hpp
  geometry/            hittable.hpp hit_record.hpp sphere.hpp
  materials/           material.hpp lambertian.hpp metal.hpp glass.hpp plastic.hpp specular_coat.hpp
  scene/               scene.hpp camera.hpp registry.hpp
  parsing/             tokeniser.hpp scene_parser.hpp
  render/              renderer.hpp render_settings.hpp worker_pool.hpp image.hpp
  io/                  image_writer.hpp ppm_writer.hpp
  logging/             log_level.hpp log_sink.hpp logger.hpp
  cli/                 command_line.hpp options.hpp
  errors.hpp
src/                   # odpowiadające pliki .cpp + main.cpp
tests/                 # harness.hpp, generators.hpp, test_*.cpp
scenes/                # przykładowe sceny
docs/
```

### 7.2. Komponenty

**Matematyka**

- `template <std::floating_point T, std::size_t N> class Vector` — prywatna tablica `std::array<T, N>`; operatory
  `+ − * /` (wektor–wektor i wektor–skalar), `+= −= *= /=`, unarny `−`, `==`, `operator[]` (tylko odczyt) i `set(i,
  x)`; funkcje `dot`, `cross` (tylko `N == 3`, przez `requires`), `length`, `length_squared`, `normalised()` (rzuca
  `MathsError` dla wektora zerowego); statyczne fabryki `zero()`, `unit_x()` itd.; `operator<<` i `std::formatter`.
  Niezmiennik: wszystkie składowe są skończone (bez nieskończoności i NaN). Sprawdzają go konstruktor, `set` oraz
  każda operacja, której wynik mógłby go złamać (przepełnienie, dzielenie przez zero, skalar nieskończony lub NaN);
  naruszenie rzuca `MathsError`, a operacje złożone (`+=` itd.) nie zmieniają wtedy wektora. `length` i `normalised`
  są odporne na przepełnienie i niedomiar (poprawne także dla składowych rzędu `1e300` i `1e-300`); `dot`
  i `length_squared` zwracają skalar, więc mogą dać nieskończoność.
  Aliasy: `using Vec3 = Vector<double, 3>; using Point3 = Vec3;`.
- `template <std::floating_point T> class Interval` — `min`, `max`, `contains`, `surrounds`, `clamp`; statyczne stałe
  `empty()` i `universe()`.
- `class Ray` — `origin`, `direction` (niezmiennik: znormalizowany, ustalany w konstruktorze), `at(t)`. Początek
  i kierunek są skończone na mocy niezmiennika `Vector`; `at(t)` rzuca `MathsError` dla nieskończonego lub NaN `t`
  (oraz gdy wynik się przepełnia).
- `class Colour` — składowe `r, g, b ≥ 0` (niezmiennik sprawdzany w konstruktorze); kompozycja `Vec3`; operatory
  `+ * /` (kolor–kolor po składowych i kolor–skalar), `==`; `gamma_corrected()`, `quantised()` →
  `std::array<std::uint8_t, 3>`; stałe statyczne `black()`, `white()`.

**Losowość**

- `class Sampler` — opakowuje `pcg32` (hermetyzacja: biblioteka nie wycieka do interfejsu poza nagłówkiem
  implementacji); `uniform()` ∈ `[0, 1)`, `template <std::floating_point T> T uniform(T lo, T hi)`,
  `unit_vector()`, `in_unit_disk()`; statyczna fabryka `for_pixel(seed, x, y, width)`.
  `uniform` wykorzystuje pełną precyzję typu (dla `double` 53 bity z dwóch wyjść `pcg32`, dla `float` 24 bity
  z jednego), a wynik `uniform(lo, hi)` zawsze leży w `[lo, hi)`, także dla skrajnych granic; granice nieskończone,
  NaN lub `lo ≥ hi` → `MathsError`. `pcg32` uwzględnia tylko 63 bity numeru strumienia, więc numer większy niż
  `2⁶³ − 1` oraz `x ≥ width` → `RangeError`. `in_unit_disk` losuje metodą odrzucania (wynik nigdy nie trafia na
  brzeg koła mimo zaokrągleń), `unit_vector` — z twierdzenia Archimedesa (`z` jednostajne, azymut jednostajny).

**Geometria**

- Interfejs `IHittable` — `virtual std::optional<HitRecord> hit(const Ray&, Interval<double>) const = 0;`
  wirtualny destruktor.
- `struct HitRecord` — `point`, `normal` (zawsze przeciwny do promienia), `t`, `front_face`,
  `const Material*`. Tworzony przez statyczną fabrykę `from_outward_normal(ray, t, outward_normal, material)`,
  która normalizuje normalną i odwraca ją, gdy promień nadchodzi od wewnątrz (promień styczny liczy się jako
  wewnętrzny); `Material` jest tu jedynie zadeklarowany zawczasu.
- `class Sphere final : public IHittable` — `centre`, `radius` (> 0, sprawdzane w konstruktorze), materiał
  przechowywany jako `std::shared_ptr<const Material>` (**agregacja** — materiał żyje niezależnie od kuli i może być
  współdzielony; pusty wskaźnik → `Error`); statyczny licznik utworzonych kul (`Sphere::instance_count()`),
  używany w logach. Promień nieskończony, NaN lub `≤ 0` → `MathsError`; licznik zlicza konstrukcje z parametrów
  (bez kopii) i jest atomowy. `hit` zwraca bliższy pierwiastek z przedziału, a gdy ten wypada poza przedział —
  dalszy; obie różnice kwadratów liczone są jako iloczyny `(a − b)(a + b)`, a bliższy pierwiastek z iloczynu
  pierwiastków, co chroni dokładność dla dużych kul (np. podłoża).

**Materiały**

- Interfejs `IScatterer` — `virtual std::optional<Scatter> scatter(const Ray&, const HitRecord&, Sampler&) const = 0;`
  (`Scatter` = rozproszony promień + tłumienie).
- Interfejs `INamed` — `virtual std::string_view name() const = 0;`
- Klasa abstrakcyjna `Material : public IScatterer, public INamed` (**wielodziedziczenie interfejsów**) —
  przechowuje nazwę (prywatnie), implementuje `name()`, `scatter` pozostaje czysto wirtualne. Pusta nazwa → `Error`
  (wyłącznie zabezpieczenie: parser nie powinien jej przepuścić, bo identyfikator musi zaczynać się literą).
  Parametry materiałów są sprawdzane w konstruktorach i rzucają `MathsError`: albedo o składowej `> 1`, `fuzz`
  spoza `[0, 1]`, `ior` nieskończony, NaN lub `≤ 0`.
- `Lambertian : public Material`, `Metal final : public Material`, `Glass final : public Material` — nadpisują
  `scatter` (`override`).
- `class SpecularCoat` — klasa domieszkowa (nie jest materiałem): przechowuje `ior` powłoki, udostępnia
  `reflectance(cos θ)` (Schlick) i `reflect(…)`; statyczna funkcja `schlick(cos θ, ior)` wykorzystywana również przez
  `Glass`. `reflect` jest statyczne i służy także `Metal`.
- `Plastic final : public Lambertian, private SpecularCoat` (**wielodziedziczenie implementacji**) — nadpisuje
  `scatter`: odbicie z powłoki albo wywołanie `Lambertian::scatter`.

**Scena**

- `template <typename T> class Registry` — odwzorowanie nazwa → `std::shared_ptr<T>`; `add` (rzuca
  `DuplicateNameError`), `get` (rzuca `UnknownNameError`), `contains`, `size`; ograniczenie przez koncept
  (`T` musi spełniać `std::derived_from<T, INamed>`). Kluczem jest `name()` obiektu; `add` pustego wskaźnika rzuca
  `Error`, a nieudane `add` nie zmienia rejestru. Wyjątki o nazwach udostępniają ją przez `name()`.
- `struct CameraSettings` — `from`, `to`, `up` (domyślnie `0 1 0`), `fov` (90), `aperture` (0),
  `std::optional<double> focus` (brak = `‖to − from‖`); porównywalna (`==`), bez walidacji.
- `struct Resolution` — `width`, `height`. Rozdzielczość pochodzi z wiersza poleceń, nie z pliku sceny, więc kamera
  (tworzona przez parser) dostaje ją dopiero przy generowaniu promieni.
- `class Camera` — konstruowana z `CameraSettings` (walidacja parametrów, rzuca `SceneError`); `ray_for(x, y,
  Resolution, Sampler&) const`, piksele liczone od lewego górnego rogu, `(x, y)` poza obrazem → `RangeError`.
  Oprócz warunków z [§ 5.2](#52-dyrektywy) odrzuca `up` prawie równoległy do kierunku patrzenia (sinus kąta
  `< 10⁻⁹`, gdy orientację obrazu wyznaczałyby błędy zaokrągleń) i ustawienia, których geometria wykracza poza zakres
  `double` (zamiast `MathsError` — `SceneError`). Kamera otworkowa (`aperture = 0`) pobiera z `Sampler` tylko dwa
  przesunięcia w obrębie piksela; inna — dodatkowo punkt na przysłonie.
- `struct Sky` — `top`, `bottom` (domyślnie `0.5 0.7 1.0` i `1 1 1`), `colour(direction)` wg
  [§ 6.2](#62-śledzenie-ścieżki); zerowy kierunek → `MathsError`.
- `class Scene` — **kompozycja**: posiada `Camera`, `Sky` oraz `std::vector<std::unique_ptr<IHittable>>`;
  materiały w `Registry<Material>`. Metoda `hit(ray, range)` zwraca najbliższe trafienie; `sky_colour(direction)`.
  Uzupełniana przez `add_material` (rzuca `DuplicateNameError`) i `add_object` (pusty wskaźnik → `Error`);
  `materials()`, `object_count()`. Nie da się jej skopiować (`= delete("...")`), można przenieść (`noexcept`).

**Parsowanie**

- `class Tokeniser` — dzieli wiersz na tokeny z pozycjami (wiersz, kolumna).
- `class SceneParser` — tabela dyrektyw `std::unordered_map<std::string_view, std::function<void(Arguments&)>>`
  wypełniona **wyrażeniami lambda** w konstruktorze; `Scene parse(std::istream&, std::string_view file_name)` oraz
  `Scene parse_file(const std::filesystem::path&)` (rzuca `IoError`, gdy pliku nie da się otworzyć).
- `std::string serialise(const SceneDescription&)` — zapis sceny z powrotem do języka opisu (używany w testach
  „tam i z powrotem”). `SceneDescription` to prosta, porównywalna (`operator==`) struktura danych opisu, z której
  powstaje `Scene`.

**Renderowanie**

- `struct RenderSettings` — `width`, `height`, `samples`, `max_depth`, `seed`, `threads`; walidacja w konstruktorze.
- `class Image` — `width`, `height`, prywatny `std::vector<Colour>`; `operator[](x, y)` (wersja `const`
  i nie-`const`), rzuca `RangeError` poza zakresem.
- `class WorkerPool` — patrz [§ 6.6](#66-wielowątkowość); `void run(std::size_t task_count,
  std::function<void(std::size_t)> task)`; niekopiowalny.
- `class Renderer` — **kompozycja** `WorkerPool`, **agregacja** `const Scene&` i `Logger&`; `Image render()`.

**Wejście-wyjście**

- Interfejs `IImageWriter` — `virtual void write(const Image&, std::ostream&) const = 0;` oraz niewirtualna metoda
  `write_file(const Image&, const std::filesystem::path&) const` (wzorzec metody szablonowej; rzuca `IoError`).
- `class PpmWriter final : public IImageWriter` — format P3.
- `Image read_ppm(std::istream&)` — minimalny czytnik P3 na potrzeby testów (część `rt_core`).

**Logowanie**

- `enum class LogLevel { debug, info, warning, error };` — `operator<<`, `parse_log_level(std::string_view)`
  (rzuca `CliError`), porównania przez `<=>`.
- Interfejs `ILogSink` — `virtual void write(LogLevel, std::string_view message) = 0;`
- Klasa abstrakcyjna `StreamSink : public ILogSink` — wspólna obsługa formatowania i `std::mutex`; czysto wirtualna
  `std::ostream& stream()`. Pochodne: `StderrSink`, `FileSink` (posiada `std::ofstream`, rzuca `IoError`).
- `NullSink`, `MemorySink` (zapamiętuje komunikaty; do testów) — bezpośrednio z `ILogSink`.
- `class Logger` — **kompozycja** `std::vector<std::unique_ptr<ILogSink>>`, minimalny poziom; metody
  `debug/info/warning/error` przyjmujące `std::format_string<Args...>` (szablony wariadyczne); bezpieczny wątkowo.
  Bez singletona — instancja tworzona w `main` i przekazywana przez referencję.
- Format wpisu: `[2026-10-31 12:00:00.123] [info] message`.

**Wiersz poleceń**

- `struct Options` — wynik parsowania.
- `class CommandLine` — `static Options parse(std::span<const std::string_view>)` (rzuca `CliError`),
  `static std::string usage()`; funkcja pomocnicza `parse_resolution(std::string_view)`.

**Błędy** (`errors.hpp`)

```
std::runtime_error
└── rt::Error                      (klasa bazowa wszystkich wyjątków projektu)
    ├── rt::CliError
    ├── rt::IoError                (ścieżka pliku)
    ├── rt::MathsError
    ├── rt::RangeError
    └── rt::SceneError
        ├── rt::SceneParseError    (plik, wiersz, kolumna)
        ├── rt::DuplicateNameError
        └── rt::UnknownNameError
```

Każdy wyjątek ma konstruktor przyjmujący opis i (gdzie ma to sens) dane kontekstowe dostępne przez gettery.

### 7.3. Przepływ w `main`

1. Utwórz `Logger` z `StderrSink`.
2. `CommandLine::parse` → `Options`; ustaw poziom logowania, ewentualnie dodaj `FileSink`.
3. `SceneParser::parse_file` → `Scene`.
4. `Renderer{scene, settings, logger}.render()` → `Image`.
5. `PpmWriter{}.write_file(image, output)`.
6. Obsługa wyjątków i kody wyjścia wg [§ 4.1](#41-kody-wyjścia).

## 8. Testy

### 8.1. Organizacja

- Każdy plik `tests/test_<obszar>.cpp` to osobny plik wykonywalny rejestrowany w CTest (`add_test`).
- `tests/harness.hpp` — minimalne narzędzie: `rt::test::Suite` zbiera wyniki `rc::check(name, property)`
  i zwraca z `main` kod różny od 0, jeśli którakolwiek własność nie przeszła. Testy przykładowe (konkretne
  wejście → konkretne wyjście) zapisujemy jako `rc::check` bez parametrów lub zwykłe asercje w harnessie.
- `tests/generators.hpp` — specjalizacje `rc::Arbitrary` i własne generatory: wektory skończone, wektory
  niezerowe, wektory jednostkowe, kolory w `[0, 1]`, przedziały, promienie, kule, materiały, ustawienia kamery,
  poprawne `SceneDescription`, identyfikatory.
- Liczbę przypadków i ziarno rapidchecka kontroluje zmienna `RC_PARAMS`; CTest domyślnie używa
  `max_success=200`.
- Porównania zmiennoprzecinkowe przez funkcję `approx_equal(a, b, eps)` z tolerancją względną i bezwzględną.
- Testy muszą przechodzić w presetach `clang-asan` i `clang-tsan`.

### 8.2. Wymagane własności

**Vector / Interval / Ray / Colour**

- Dodawanie jest przemienne i (z dokładnością `eps`) łączne; `v − v == 0`; `(a + b) · s ≈ a·s + b·s`.
- `dot` symetryczny; `cross(a, b)` prostopadły do `a` i `b`; `cross(a, b) ≈ −cross(b, a)`.
- `‖normalised(v)‖ ≈ 1` dla niezerowego `v`, także bardzo małego i bardzo dużego; `normalised(0)` rzuca `MathsError`.
- Wektor z nieskończoną lub NaN składową (z konstruktora albo `set`) rzuca `MathsError`; operacja o nieskończonym
  lub NaN wyniku (przepełnienie, dzielenie przez zero, skalar nieskończony) rzuca `MathsError` i nie zmienia
  argumentu.
- `Interval::clamp(x)` zawsze w przedziale; `contains(clamp(x))`.
- `‖ray.at(t) − origin‖ ≈ |t|`; `Ray` z zerowym kierunkiem rzuca wyjątek; `ray.at(t)` dla nieskończonego lub NaN
  `t` rzuca `MathsError`.
- `quantised` jest monotoniczne po składowych i zawsze w `[0, 255]`; `gamma_corrected` monotoniczne.
- Kolor z ujemną składową rzuca wyjątek.

**Sampler**

- `uniform() ∈ [0, 1)`; `uniform(lo, hi) ∈ [lo, hi)`.
- `‖unit_vector()‖ ≈ 1`; `‖in_unit_disk()‖ < 1` i składowa `z == 0`.
- Ten sam `(seed, piksel)` → identyczna sekwencja; różne piksele → różne sekwencje (w praktyce: różne pierwsze
  10 wartości).
- Rozkłady są równomierne: średnie (i średnie kwadratów) próbek zgodne z wartościami oczekiwanymi z dokładnością
  do 6 błędów standardowych; pierwsze wartości dla `pcg32(42, 54)` zgodne z wzorcowym wyjściem pcg-cpp.

**Sphere**

- Promień z punktu zewnętrznego skierowany w środek trafia przy `t ≈ ‖c − o‖ − r`.
- Punkt trafienia leży na sferze (`‖p − c‖ ≈ r`); normalna jest jednostkowa i `dot(normal, direction) ≤ 0`;
  `front_face` zgodne z położeniem początku promienia (na zewnątrz/wewnątrz).
- Promień skierowany „od” kuli (z punktu zewnętrznego) nie trafia.
- Zwrócone `t` zawsze należy do zadanego przedziału.
- Konstruktor z `radius ≤ 0` rzuca wyjątek.

**Materiały**

- Lambertian: rozproszony kierunek leży w półprzestrzeni normalnej; tłumienie = albedo.
- Metal (`fuzz = 0`): kąt odbicia = kąt padania, kierunek odbity jednostkowy; dla dowolnego `fuzz` wynik albo
  pochłonięcie, albo kierunek nad powierzchnią.
- Glass: dla `ior = 1` promień przechodzi bez zmiany kierunku; przy kącie powyżej granicznego (od środka) zawsze
  odbicie; tłumienie zawsze `(1, 1, 1)`.
- Plastic: wynik jest albo odbiciem lustrzanym z tłumieniem 1, albo rozproszeniem lambertowskim z tłumieniem
  albedo.
- Wszystkie materiały: składowe tłumienia w `[0, 1]`; kierunek wyniku jednostkowy.
- `schlick(cos θ, n) ∈ [0, 1]`, `schlick(1, 1) ≈ 0`, rośnie wraz z malejącym `cos θ`.
- Rozkłady: kierunki lambertowskie spełniają prawo cosinusa (średnia `cos θ` równa 2/3); częstość odbić w `Glass`
  i `Plastic` zgodna z `schlick` (z dokładnością do 6 błędów standardowych).

**Camera**

- Dla `aperture = 0` promień środkowego piksela biegnie (w przybliżeniu) od `from` w kierunku `to`.
- Początki promieni leżą w dysku o średnicy `aperture` wokół `from`, w płaszczyźnie prostopadłej do kierunku
  patrzenia.
- Wszystkie promienie przechodzące przez punkt płaszczyzny ogniskowania danego piksela (bez jittera) zbiegają się
  w nim niezależnie od punktu na przysłonie.
- Niepoprawne ustawienia (`fov ∉ (0, 180)`, `from == to`, `up ∥ kierunek`) rzucają `SceneError`.
- Promień piksela `(x, y)` przecina płaszczyznę ogniskowania w obrębie tego piksela (lewy górny piksel — w lewym
  górnym rogu), niezależnie od punktu na przysłonie; bazę kamery test wyznacza niezależnie od `Camera`.
- Domyślne `focus` równe `‖to − from‖`; piksel poza obrazem → `RangeError`; ten sam stan `Sampler` → ten sam promień.

**Scene**

- `hit` zwraca to samo trafienie co najbliższe z trafień pojedynczych kul; pusta scena nie jest trafiana.
- Niebo: `top` dokładnie w górę, `bottom` dokładnie w dół, pośrednie składowe między nimi; kolor zależy tylko od
  kierunku, nie od długości wektora.
- Przeniesiona scena zachowuje obiekty i materiały.

**Registry**

- `add` potem `get` zwraca ten sam obiekt; ponowne `add` tej samej nazwy rzuca `DuplicateNameError`;
  `get` nieistniejącej nazwy rzuca `UnknownNameError`; `size` równe liczbie unikalnych dodanych nazw.

**Parser i Tokeniser**

- „Tam i z powrotem”: dla dowolnego poprawnego `SceneDescription d`: `parse(serialise(d)) == d`.
- Dodanie komentarzy, pustych wierszy i dodatkowych białych znaków nie zmienia wyniku.
- Dowolne losowe wejście (losowe bajty / losowe tokeny) nigdy nie powoduje awarii — wyłącznie poprawny wynik albo
  `SceneParseError`.
- Wstawienie błędnego wiersza na pozycji `k` do poprawnej sceny daje `SceneParseError` z numerem wiersza `k`.
- Użycie niezdefiniowanego materiału, duplikat nazwy, brak kamery, wartości spoza zakresu → `SceneParseError`.
- Bajt spoza dozwolonych (`≥ 0x80` poza komentarzem lub znak sterujący inny niż tabulator w dowolnym miejscu
  wiersza) → `SceneParseError` z kolumną tego bajtu.
- Dowolne bajty `≥ 0x80` w komentarzu (także niepoprawne UTF-8) nie zmieniają wyniku.
- Identyfikator: każdy napis z `[A-Za-z][A-Za-z0-9_-]{0,19}` jest akceptowany; napis dłuższy niż 20 znaków,
  zaczynający się czymś innym niż litera (np. `1`, `_`, `-`) lub z innym znakiem (np. `.`, `$`) → `SceneParseError`.
- Tokeniser: konkatenacja tokenów z odstępami odtwarza wiersz bez komentarza, z dokładnością do białych znaków;
  kolumny tokenów są rosnące.

**Image / PPM**

- „Tam i z powrotem”: `read_ppm(write(img))` równa się `img` po kwantyzacji.
- Nagłówek to `P3`, `width height`, `255`; liczba pikseli = `width · height`.
- Dostęp poza zakresem rzuca wyjątek.

**Renderer i WorkerPool**

- Determinizm: dwa renderowania z tymi samymi parametrami dają identyczny obraz.
- Niezależność od wątków: render z `threads = 1` jest identyczny z renderem z `threads = N` (`N ∈ [2, 8]`).
- `max_depth = 0` → obraz całkowicie czarny.
- Scena bez kul, kamera otworkowa → każdy piksel ma kolor nieba wyznaczony analitycznie dla kierunku promienia
  (z tolerancją uwzględniającą jitter w obrębie piksela).
- `WorkerPool::run` wywołuje zadanie dokładnie raz dla każdego indeksu; wyjątek z zadania jest ponownie rzucany
  w wątku wywołującym.
- Testy renderera używają małych rozdzielczości (≤ 16×16) i małej liczby próbek, by były szybkie.

**Logger i CLI**

- Komunikaty poniżej poziomu minimalnego nie trafiają do ujścia; pozostałe trafiają w kolejności wysłania.
- `parse_log_level(to_string(l)) == l` dla każdego poziomu.
- `parse_resolution(format("{}x{}", w, h)) == {w, h}` dla `w, h ∈ [1, 16384]`; wartości spoza zakresu
  i niepoprawny zapis rzucają `CliError`.
- Dowolna permutacja opcji daje te same `Options`; brakujące argumenty pozycyjne → `CliError`.

## 9. Logowanie — co logujemy

| Poziom    | Zdarzenia                                                                                             |
|-----------|-------------------------------------------------------------------------------------------------------|
| `debug`   | Szczegóły parsowania (każda dyrektywa), parametry kamery, liczba wątków                               |
| `info`    | Start/koniec etapów, liczba kul i materiałów, postęp renderowania, czas trwania, ścieżka wyniku       |
| `warning` | Nietypowe, ale poprawne dane (np. `samples = 1`, kula w całości za kamerą, `threads` > liczba rdzeni) |
| `error`   | Każdy wyjątek przechwycony w `main`                                                                   |

## 10. Wymagania jakościowe

- Kod kompiluje się bez ostrzeżeń w GCC 16.2 i Clang 23.1 z flagami z [§ 3](#3-stos-technologiczny-i-budowanie).
- clang-tidy bez zgłoszeń dla zestawu: `bugprone-*`, `cppcoreguidelines-*`, `modernize-*`, `performance-*`,
  `readability-*`, `misc-*` (wyłączenia uzasadnione komentarzem w `.clang-tidy`).
- Kod sformatowany clang-format (sprawdzane celem CMake `format-check`).
- Brak surowych `new`/`delete`; właścicielstwo wyrażone przez `std::unique_ptr`/`std::shared_ptr`/wartości.
- Każda klasa polimorficzna ma wirtualny destruktor (lub jest `final` bez dziedziczenia dalej); klasy bazowe
  interfejsów mają chronione/usunięte kopiowanie tam, gdzie grozi to „cięciem” obiektu.
- Niezmienniki klas sprawdzane w konstruktorach; obiekty po konstrukcji są zawsze poprawne.
- `const`-poprawność, `[[nodiscard]]` dla funkcji zwracających wartości, `noexcept` tam, gdzie gwarantowane.
- Brak zmiennych globalnych z modyfikowalnym stanem (poza statycznymi licznikami opartymi na `std::atomic`).

## 11. Kryteria akceptacji

1. `cmake --preset gcc-release && cmake --build --preset gcc-release` oraz to samo dla Clang kończy się sukcesem.
2. `ctest --preset <każdy preset>` — wszystkie testy przechodzą, również z ASan/UBSan i TSan.
3. `raytracer scenes/demo.txt 800x450 out.ppm` tworzy poprawny plik PPM P3 z widocznymi czterema typami materiałów
   i rozmyciem głębi ostrości.
4. Dwa uruchomienia z tym samym `--seed` i różnym `--threads` dają identyczne pliki.
5. Każdy rodzaj błędu z [§ 4.1](#41-kody-wyjścia) daje czytelny komunikat i właściwy kod wyjścia.
6. Każdy element z [§ 12](#12-mapa-wymagań-obiektowych) jest obecny w kodzie we wskazanym miejscu.

## 12. Mapa wymagań obiektowych

| Wymaganie                        | Gdzie w projekcie                                                                 |
|----------------------------------|-----------------------------------------------------------------------------------|
| Hermetyzacja (ukrywanie danych)  | Prywatne składowe wszystkich klas z niezmiennikami (`Ray`, `Colour`, `Sphere`,    |
|                                  | `Image`, `Camera`); `Sampler` ukrywa `pcg32`                                      |
| Enkapsulacja (dane + operacje)   | `Vector`, `Image`, `Camera`, `Scene`, `Logger` — operacje na danych tylko przez   |
|                                  | metody                                                                            |
| Konstruktory                     | Walidujące (`Vector`, `Ray`, `Sphere`, `Colour`, `Camera`, `RenderSettings`),     |
|                                  | domyślne, kopiujące/przenoszące (`Vector`, `Image`), `explicit`, `constexpr`,     |
|                                  | delegujące, usunięte z powodem (`Scene`, `Logger`, `WorkerPool`)                  |
| Dziedziczenie pojedyncze         | `Sphere : IHittable`, `Metal/Glass/Lambertian : Material`,                        |
|                                  | `StderrSink/FileSink : StreamSink`, hierarchia wyjątków                           |
| Dziedziczenie wielokrotne        | `Material : IScatterer, INamed` (interfejsy);                                     |
|                                  | `Plastic : Lambertian, private SpecularCoat` (implementacja)                      |
| Interfejsy                       | `IHittable`, `IScatterer`, `INamed`, `ILogSink`, `IImageWriter`                   |
| Klasy i metody abstrakcyjne      | `Material` (abstrakcyjna, `scatter` czysto wirtualne), `StreamSink` (`stream()`   |
|                                  | czysto wirtualne)                                                                 |
| Polimorfizm i nadpisywanie metod | `scatter` w materiałach (z wywołaniem `Lambertian::scatter` w `Plastic`), `hit` w |
|                                  | `Sphere`, `write` w ujściach logów i `PpmWriter`                                  |
| Kompozycja                       | `Scene` posiada `Camera` i kule; `Logger` posiada ujścia; `Renderer` posiada      |
|                                  | `WorkerPool`; `Colour` zawiera `Vec3`                                             |
| Agregacja                        | `Sphere` współdzieli `std::shared_ptr<const Material>`; `Renderer` odwołuje się   |
|                                  | do `Scene` i `Logger`                                                             |
| Funkcje anonimowe                | Tabela dyrektyw w `SceneParser`, zadania `WorkerPool`, algorytmy `std::ranges`,   |
|                                  | generatory rapidchecka, własności testowe                                         |
| Wyjątki                          | Hierarchia `rt::Error`, przenoszenie wyjątków między wątkami, kody wyjścia w      |
|                                  | `main`                                                                            |
| Przeciążanie operatorów          | Arytmetyka `Vector`/`Colour`, `==`, `<=>` (`LogLevel`), `operator[]`              |
|                                  | (wielowymiarowy w `Image`), `operator<<`, `std::formatter`                        |
| Elementy statyczne               | `Vector::zero()`, `Colour::black()`, `Interval::universe()`,                      |
|                                  | `Sampler::for_pixel`, `SpecularCoat::schlick`, `CommandLine::parse`,              |
|                                  | `Sphere::instance_count()`                                                        |
| Typy generyczne i szablony       | `Vector<T, N>`, `Interval<T>`, `Registry<T>` z konceptem, `Sampler::uniform<T>`,  |
|                                  | wariadyczne metody `Logger`, `rc::Arbitrary` w testach                            |

## 13. Podział prac

Zgodnie z `docs/opis.tex`:

- **Kamil J. Hantszel** — matematyka, losowość, geometria, materiały, kamera, scena, renderer, `WorkerPool`;
  testy tych elementów; dokumentacja.
- **Aleksander Kała** — CMake (presety, zależności, clang-tidy/format), `Tokeniser`, `SceneParser`, `serialise`,
  `PpmWriter`/`read_ppm`, logowanie, wyjątki, CLI, `main`; testy tych elementów.

Kolejność prac: (1) szkielet CMake + harness testów → (2) matematyka + `Sampler` → (3) `Sphere` + materiały →
(4) kamera + `Scene` + renderer jednowątkowy + PPM → (5) parser + CLI + logowanie → (6) `WorkerPool` →
(7) testy determinizmu, sanitizery, dopracowanie.

<!--
Local Variables:
ispell-local-dictionary: "pl-PL"
End:
-->
