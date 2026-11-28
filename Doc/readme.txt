Program TAIO wymaga kompilatora GCC z obsługą standardu C17, dostępnego
pod poleceniem gcc.

Otwórz PowerShell i przejdź do katalogu z plikami programu.
Polecenia uruchamiania poniżej wykonuj z katalogu głównego TAIO.

Skompiluj program poleceniem GCC. Najpierw przejdź do katalogu Source.

cd .\Source

gcc -std=c17 -Wall -Wextra -O2 main.c big_uint.c number_sort.c min_heap.c partition.c partition_exact.c -o ..\Exe\taio.exe
cd ..

Polecenie tworzy plik Exe/taio.exe. Pliki nagłówkowe .h powinny znajdować
się w tym samym katalogu co odpowiadające im pliki .c.

Instrukcja użycia programu:

Podaj kolejno tryb, plik wejściowy i liczbę podzbiorów k.

.\Exe\taio.exe A .\Examples\dane_przyklad.txt 3
.\Exe\taio.exe D .\Examples\dane_przyklad.txt 3

A przydziela liczby malejąco do części o najmniejszej sumie. (algorytm aproksymacyjny)
D szuka pierwszego podziału na k niepustych części o równych sumach. (algorytm dokładny)


k jest liczbą całkowitą co najmniej 2 i obowiązuje dla wszystkich zbiorów
w pliku. W trybie A każdy zbiór musi zawierać co najmniej k elementów.

Plik wejściowy umieść w bieżącym katalogu lub podaj ścieżkę do niego.
Ścieżkę zawierającą spacje ujmij w cudzysłowy.

.\Exe\taio.exe A ".\Examples\dane testowe.txt" 3

Pierwszy wiersz pliku wejściowego zawiera liczbę zbiorów. Każdy następny
wiersz zawiera jeden zbiór nieujemnych liczb całkowitych zapisanych
dziesiętnie i rozdzielonych pojedynczą spacją. Przykładowa zawartość
pliku dla dwóch zbiorów jest następująca.

2
9 8 7 6 5
0 0 0 0 0

Wynik zostanie zapisany w katalogu pliku wejściowego. Rozszerzenie .txt
zostanie zastąpione przyrostkiem _output.txt Przykładowo wywołanie

.\Exe\taio.exe A .\Examples\dane_przyklad.txt 3

zapisze wynik w pliku Examples/dane_przyklad_output.txt. Po poprawnym zakończeniu
obliczeń program zastępuje wcześniejszy plik wynikowy o tej nazwie.

Plik wynikowy zawiera liczbę zbiorów i k, a dla każdego zbioru jego sumę
i elementy, następnie k wierszy podzbiorów oraz różnicę największej
i najmniejszej sumy. Elementy zbioru zachowują kolejność wejściową.
Elementy podzbiorów są uporządkowane nierosnąco, a wiersze podzbiorów
są uporządkowane leksykograficznie nierosnąco według ich elementów.
Gdy w trybie D podział idealny nie istnieje (również przy n < k),
po wierszu z sumą i elementami zbioru pojawia się jedna linia BRAK.
Następne zbiory są dalej przetwarzane. Oba tryby zapisują ten sam
plik wynikowy dla danego pliku wejściowego.

Błędy wejścia, odczytu lub zapisu są zgłaszane w terminalu. Po błędzie
wcześniejszy poprawny plik wynikowy pozostaje zachowany.

Układ katalogów

Doc — instrukcje kompilacji, uruchomienia i opis testów.
Source — źródła programu.
Exe — pliki wykonywalne dla Windows x64.
Examples — przykłady wejścia, testy i wzorcowe wyniki.

Przykładowe testy uruchamiane z głównego katalogu TAIO

.\Exe\taio.exe A .\Examples\testy_poprawnosci\aproksymacja_k3.txt 3
.\Exe\taio.exe D .\Examples\testy_poprawnosci\dokladny_k2.txt 2

Szczegółowy opis testów znajduje się w Doc/testy.txt.
Do zamknięcia dokumentacji należy dołączyć do Doc raport w PDF
oraz PDF-y publikacji wykorzystanych w raporcie, jeśli takie występują.

POMIARY CZASU (sekcja Testy sprawozdania)

Generator danych testowych kompilujemy tak samo jak program:

gcc -std=c17 -Wall -Wextra -O2 -I Source gen.c Source\big_uint.c -o gen.exe

.\gen.exe rodzina n k R bity porzadek ziarno plik_wyjsciowy

Zapisuje R zbiorów po n liczb we wspólnym formacie wejściowym.
Rodziny: pes (podział idealny nie istnieje, algorytm D przegląda wszystkie
k^n kandydatów), los (liczby losowe z [1, 2^bity - 1], suma podzielna
przez k), idealny (podział idealny istnieje z konstrukcji). Porządek:
los, nier (nierosnąco), niem (niemalejąco). Te same argumenty dają zawsze
identyczny plik, np.

.\gen.exe los 20 2 10 20 los 2020 dane_los.txt

Pomiary uruchamia skrypt bench.py (Python 3, tylko biblioteka
standardowa) z głównego katalogu TAIO; ścieżkę do programu podajemy
opcją --taio:

py bench.py --taio .\Exe\taio.exe                wszystkie serie T1..T5
py bench.py --taio .\Exe\taio.exe --serie T1 T3  wybrane serie
py bench.py --taio .\Exe\taio.exe --pilot        szybki przebieg kontrolny

Wyniki trafiają do katalogu wyniki (jeden plik CSV na serię oraz
srodowisko.txt), pliki robocze do katalogu robocze i są usuwane po
każdym pomiarze. Wykresy w sprawozdaniu są rysowane bezpośrednio
z plików CSV.
