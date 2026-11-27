"""Pomiary czasu programu taio do sekcji Testy sprawozdania.

Uzycie (w katalogu z taio.exe i gen.exe):
    py bench.py                  wszystkie serie T1..T5
    py bench.py --serie T1 T3    wybrane serie
    py bench.py --pilot          szybki przebieg kontrolny (male zakresy)

Dla kazdego punktu pomiarowego skrypt generuje programem gen plik z R zbiorami
tej samej wielkosci, uruchamia na nim taio i mierzy czas calego uruchomienia.
Czas na jeden zbior = (czas uruchomienia - T0) / R, gdzie T0 to czas startu
programu zmierzony na trywialnym pliku. R jest zwiekszane, dopoki uruchomienie
nie trwa co najmniej 0,5 s. Uruchomienie krotsze niz 2 s jest powtarzane:
pierwszy przebieg to rozgrzewka, wynik to mediana trzech kolejnych.
Kazdy plik wynikowy jest sprawdzany (pokrycie zbioru, sumy, porzadek, delta).

Wyniki: katalog wyniki/ (CSV na serie + srodowisko.txt). Pliki robocze:
katalog robocze/, usuwane po kazdym pomiarze. Tylko biblioteka standardowa.
"""
import argparse
import csv
import datetime
import math
import os
import platform
import statistics
import subprocess
import sys
import time
from pathlib import Path

EXE = ".exe" if os.name == "nt" else ""
TARGET_RUN_S = 0.5      # minimalny czas uruchomienia (amortyzacja startu procesu)
REPEAT_BELOW_S = 2.0    # krotsze uruchomienia sa powtarzane (mediana z 3)
FILE_BUDGET = 40_000_000  # przyblizony limit znakow w pliku wejsciowym
R_CAP = 2_000_000
MAX_CHECKED_SETS = 20_000
TIMEOUT_S = 3600

COLUMNS = ["seria", "alg", "rodzina", "porzadek", "n", "k", "bity", "ziarno", "R",
           "powtorzen", "t_uruch_s", "t0_s", "t_na_zbior_s", "sprawdzono",
           "odsetek_brak", "delta_sr", "delta_amax_sr", "odsetek_delta0"]


def parse_line(line):
    return [int(x) for x in line.split()]


def mean_value(values):
    total = sum(values)
    try:
        return total / len(values)
    except OverflowError:  # liczby wieksze niz zakres float
        return total // len(values)


def check_output(path, mode, k, expected_sets):
    """Sprawdza plik wynikowy taio i zwraca statystyki (dla co najwyzej
    MAX_CHECKED_SETS pierwszych zbiorow)."""
    with open(path, "r", encoding="ascii") as f:
        header = f.readline().split()
        if header != [str(expected_sets), str(k)]:
            raise ValueError(f"{path}: zly naglowek {header}")
        checked = brak = zero = 0
        deltas, ratios = [], []
        for i in range(min(expected_sets, MAX_CHECKED_SETS)):
            if i > 0 and f.readline().strip() != "":
                raise ValueError(f"{path}: brak pustej linii przed zbiorem {i + 1}")
            total, *elems = parse_line(f.readline())
            if total != sum(elems):
                raise ValueError(f"{path}: zla suma zbioru {i + 1}")
            line = f.readline()
            checked += 1
            if line.strip() == "BRAK":
                if mode != "D":
                    raise ValueError(f"{path}: BRAK w trybie A")
                brak += 1
                continue
            parts = []
            for _ in range(k):
                s, *members = parse_line(line)
                if not members or s != sum(members) or members != sorted(members, reverse=True):
                    raise ValueError(f"{path}: zly podzbior w zbiorze {i + 1}")
                parts.append((s, members))
                line = f.readline()
            delta = int(line)
            sums = [s for s, _ in parts]
            members = [m for _, ms in parts for m in ms]
            if sorted(members) != sorted(elems):
                raise ValueError(f"{path}: podzbiory nie pokrywaja zbioru {i + 1}")
            if any(parts[j][1] < parts[j + 1][1] for j in range(k - 1)):
                raise ValueError(f"{path}: zly porzadek podzbiorow w zbiorze {i + 1}")
            if delta != max(sums) - min(sums):
                raise ValueError(f"{path}: zla roznica w zbiorze {i + 1}")
            a_max = max(elems)
            if mode == "D" and delta != 0:
                raise ValueError(f"{path}: D zwrocil podzial nieidealny")
            if mode == "A" and delta > a_max:
                raise ValueError(f"{path}: naruszona gwarancja delta <= a_max")
            deltas.append(delta)
            zero += delta == 0
            if a_max > 0:
                ratios.append(delta / a_max)
    found = checked - brak
    return {
        "sprawdzono": checked,
        "odsetek_brak": brak / checked,
        "delta_sr": mean_value(deltas) if deltas else "",
        "delta_amax_sr": statistics.fmean(ratios) if ratios else "",
        "odsetek_delta0": zero / found if found else "",
    }


class Bench:
    def __init__(self, args):
        self.taio = str(Path(args.taio).resolve())
        self.gen = str(Path(args.gen).resolve())
        self.work = Path(args.robocze)
        self.work.mkdir(parents=True, exist_ok=True)
        self.keep = args.zostaw
        self.t0 = 0.0

    def run_taio(self, mode, path, k):
        start = time.perf_counter()
        proc = subprocess.run([self.taio, mode, str(path), str(k)],
                              stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                              timeout=TIMEOUT_S)
        elapsed = time.perf_counter() - start
        if proc.returncode != 0:
            raise RuntimeError(f"taio {mode} {path} {k}: {proc.stderr.decode(errors='replace')}")
        return elapsed

    def measure_run(self, mode, path, k):
        first = self.run_taio(mode, path, k)
        if first >= REPEAT_BELOW_S:
            return first, 1
        return statistics.median(self.run_taio(mode, path, k) for _ in range(3)), 3

    def measure_t0(self):
        path = self.work / "t0.txt"
        path.write_bytes(b"1\n1 1\n")
        for _ in range(3):
            self.run_taio("A", path, 2)
        self.t0 = statistics.median(self.run_taio("A", path, 2) for _ in range(21))
        self.remove(path)
        return self.t0

    def remove(self, path):
        if not self.keep:
            for p in (path, path.with_name(path.stem + "_output.txt")):
                p.unlink(missing_ok=True)

    def point(self, series, mode, family, n, k, bits, order, seed, r_min, r_max=None):
        if r_max is None:
            # limit rozmiaru pliku; dla duzych n co najmniej 3 zbiory
            digits = len(str(k * (n + 1))) if family == "pes" else int(bits * 0.302) + 2
            r_max = min(R_CAP, max(3, FILE_BUDGET // (n * digits)))
            r_min = min(r_min, r_max)
        path = self.work / f"{series}_{family}_{order}_n{n}_k{k}_b{bits}.txt"
        R = r_min
        while True:
            subprocess.run([self.gen, family, str(n), str(k), str(R), str(bits), order,
                            str(seed), str(path)], check=True)
            t_run, reps = self.measure_run(mode, path, k)
            if t_run >= TARGET_RUN_S or R >= r_max:
                break
            per_set = max(t_run - self.t0, 1e-4) / R
            R = min(r_max, max(2 * R, math.ceil(1.2 * TARGET_RUN_S / per_set)))
        stats = check_output(path.with_name(path.stem + "_output.txt"), mode, k, R)
        self.remove(path)
        row = {"seria": series, "alg": mode, "rodzina": family, "porzadek": order,
               "n": n, "k": k, "bity": bits, "ziarno": seed, "R": R, "powtorzen": reps,
               "t_uruch_s": f"{t_run:.6f}", "t0_s": f"{self.t0:.6f}",
               "t_na_zbior_s": f"{max(t_run - self.t0, 0.0) / R:.6e}"}
        row.update(stats)
        for key in ("odsetek_brak", "delta_sr", "delta_amax_sr", "odsetek_delta0"):
            if isinstance(row[key], float):
                row[key] = f"{row[key]:.6g}"
        print(f"  {series} {mode} {family:7} {order:4} n={n:<8} k={k:<3} b={bits:<5} "
              f"R={R:<8} t/zbior={float(row['t_na_zbior_s']):.3e} s  brak={row['odsetek_brak']}",
              flush=True)
        return row


class CsvOut:
    def __init__(self, path):
        self.file = open(path, "w", newline="", encoding="ascii")
        self.writer = csv.DictWriter(self.file, fieldnames=COLUMNS)
        self.writer.writeheader()

    def write(self, row):
        self.writer.writerow(row)
        self.file.flush()

    def close(self):
        self.file.close()


def per_set(row):
    return float(row["t_na_zbior_s"])


def series_t1(b, out, cfg):
    """D, rodzina pesymistyczna: pelny przeglad k^n kandydatow."""
    for k in (2, 3, 4, 5):
        for n in range(max(4, k), cfg["n_cap_d"] + 1):
            row = b.point("T1", "D", "pes", n, k, 1, "los", 0, r_min=1)
            out.write(row)
            if per_set(row) > cfg["tmax"]:
                break


def series_t2(b, out, cfg):
    """D, dane losowe (S podzielna przez k) w trzech porzadkach."""
    for k in (2, 3):
        for n in range(4, cfg["n_cap_d"] + 1):
            rows = [b.point("T2", "D", "los", n, k, 20, order, 2000 + n, r_min=10)
                    for order in ("los", "nier", "niem")]
            for row in rows:
                out.write(row)
            if max(per_set(r) for r in rows) > cfg["tmax"]:
                break


def series_t3(b, out, cfg):
    """A, losowe liczby 64-bitowe: zakres wymagany i duze n."""
    for k, order in ((2, "los"), (3, "los"), (10, "los"), (3, "nier")):
        for n in cfg["small_n"]:
            if n >= k:
                out.write(b.point("T3", "A", "los", n, k, 64, order, 3000 + n, r_min=20))
    for order in ("los", "nier"):
        for n in cfg["large_n"]:
            out.write(b.point("T3", "A", "los", n, 3, 64, order, 3000 + n, r_min=20))


def series_t4(b, out, cfg):
    """A, wplyw dlugosci liczb b przy stalym n."""
    for bits in (16, 32, 64, 128, 256, 512, 1024):
        out.write(b.point("T4", "A", "los", cfg["t4_n"], 3, bits, "los", 4000 + bits, r_min=5))


def series_t5(b, out, cfg):
    """A, jakosc na danych z podzialem idealnym (delta* = 0)."""
    for k in (2, 3):
        for n in (10, 20, 50, 100, 200):
            out.write(b.point("T5", "A", "idealny", n, k, 30, "los", 5000 + n,
                              r_min=cfg["t5_r"], r_max=cfg["t5_r"]))


SERIES = {"T1": series_t1, "T2": series_t2, "T3": series_t3, "T4": series_t4, "T5": series_t5}


def cpu_name():
    if os.name == "nt":
        try:
            import winreg
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                                 r"HARDWARE\DESCRIPTION\System\CentralProcessor\0")
            return winreg.QueryValueEx(key, "ProcessorNameString")[0].strip()
        except OSError:
            pass
    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or "nieznany"


def gcc_version():
    try:
        out = subprocess.run(["gcc", "--version"], capture_output=True, text=True)
        return out.stdout.splitlines()[0]
    except (OSError, IndexError):
        return "nieznana (gcc niedostepny w PATH)"


def main():
    parser = argparse.ArgumentParser(description="Pomiary czasu programu taio.")
    parser.add_argument("--serie", nargs="+", choices=sorted(SERIES), default=sorted(SERIES))
    parser.add_argument("--pilot", action="store_true", help="szybki przebieg kontrolny")
    parser.add_argument("--tmax", type=float, default=10.0,
                        help="seria D konczy sie, gdy czas na zbior przekroczy tmax [s]")
    parser.add_argument("--taio", default="taio" + EXE)
    parser.add_argument("--gen", default="gen" + EXE)
    parser.add_argument("--wyniki", default="wyniki")
    parser.add_argument("--robocze", default="robocze")
    parser.add_argument("--zostaw", action="store_true", help="nie usuwaj plikow roboczych")
    args = parser.parse_args()

    cfg = {"tmax": args.tmax, "n_cap_d": 40, "t4_n": 100_000, "t5_r": 100,
           "small_n": list(range(5, 201, 5)),
           "large_n": [500, 1_000, 2_000, 5_000, 10_000, 20_000, 50_000,
                       100_000, 200_000, 500_000, 1_000_000]}
    if args.pilot:
        cfg.update(tmax=min(args.tmax, 0.3), t4_n=5_000, t5_r=20,
                   small_n=[5, 50, 200], large_n=[1_000, 10_000])

    results = Path(args.wyniki)
    results.mkdir(parents=True, exist_ok=True)
    bench = Bench(args)
    t0 = bench.measure_t0()
    print(f"T0 (start programu) = {t0 * 1000:.2f} ms", flush=True)
    env = [
        f"data: {datetime.datetime.now().isoformat(timespec='seconds')}",
        f"system: {platform.platform()}",
        f"procesor: {cpu_name()}",
        f"python: {sys.version.split()[0]}",
        f"gcc: {gcc_version()}",
        f"T0_s: {t0:.6f}",
        f"tmax_s: {cfg['tmax']}",
        f"pilot: {args.pilot}",
        f"serie: {' '.join(args.serie)}",
    ]
    (results / "srodowisko.txt").write_text("\n".join(env) + "\n", encoding="utf-8")
    for name in args.serie:
        print(f"Seria {name}", flush=True)
        out = CsvOut(results / f"{name}.csv")
        try:
            SERIES[name](bench, out, cfg)
        finally:
            out.close()


if __name__ == "__main__":
    main()
