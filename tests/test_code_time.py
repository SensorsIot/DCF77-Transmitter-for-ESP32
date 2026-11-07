#!/usr/bin/env python3
"""Test the sketch's actual encoder on a host with Python 3 and a C++ compiler.

Run: python3 tests/test_code_time.py
Optionally pass a sketch path to reproduce the failures in an older revision.
No Arduino libraries, Wi-Fi credentials or hardware are needed.
"""
from datetime import datetime, timezone
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


def main():
    sketch = Path(sys.argv[1]) if len(sys.argv) > 1 else (
        Path(__file__).resolve().parents[1] / "DFC77_ESP32/DFC77_ESP32.ino"
    )
    source = sketch.read_text()
    start = source.index("void CodeTime() {")
    # Compile CodeTime and Bin2Bcd directly, excluding the hardware callbacks.
    encoder = source[start:source.index("\nvoid DcfOut()", start)]
    cases = [
        # UTC start of transmitting minute; expected local date/time, ISO weekday, DST.
        ("ordinary minute", "2026-01-15T11:34", (26, 1, 15, 4, 12, 35, 0)),
        ("hour rollover", "2026-01-15T11:59", (26, 1, 15, 4, 13, 0, 0)),
        ("midnight", "2026-01-15T22:59", (26, 1, 16, 5, 0, 0, 0)),
        ("month rollover", "2026-01-31T22:59", (26, 2, 1, 7, 0, 0, 0)),
        ("year rollover", "2026-12-31T22:59", (27, 1, 1, 5, 0, 0, 0)),
        ("enter leap day", "2024-02-28T22:59", (24, 2, 29, 4, 0, 0, 0)),
        ("leave leap day", "2024-02-29T22:59", (24, 3, 1, 5, 0, 0, 0)),
        ("non-leap February", "2025-02-28T22:59", (25, 3, 1, 6, 0, 0, 0)),
        ("Sunday numbering", "2026-01-17T22:59", (26, 1, 18, 7, 0, 0, 0)),
        ("summer time", "2026-07-15T10:34", (26, 7, 15, 3, 12, 35, 1)),
        ("spring DST transition", "2026-03-29T00:59", (26, 3, 29, 7, 3, 0, 1)),
        ("fall DST transition", "2026-10-25T00:59", (26, 10, 25, 7, 2, 0, 0)),
        ("first repeated hour", "2026-10-25T00:29", (26, 10, 25, 7, 2, 30, 1)),
        ("second repeated hour", "2026-10-25T01:29", (26, 10, 25, 7, 2, 30, 0)),
        ("end repeated hour", "2026-10-25T01:59", (26, 10, 25, 7, 3, 0, 0)),
    ]
    calls = []
    for name, instant, expected in cases:
        epoch = int(datetime.fromisoformat(instant).replace(tzinfo=timezone.utc).timestamp())
        calls.append('  check("%s", %d, {%s});' % (name, epoch, ", ".join(map(str, expected))))

    harness = r'''
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>

int impulseArray[60];
int actualHours, actualMinutes, actualSecond, actualDay, actualMonth, actualYear, DayOfW;
struct tm timeinfo;
int Bin2Bcd(int dato);
''' + encoder + r'''
int failures = 0;
int decode(int first, int count) {
  int bcd = 0;
  for (int i = 0; i < count; ++i)
    if (impulseArray[first + i] == 2) bcd |= 1 << i;
  return (bcd >> 4) * 10 + (bcd & 15);
}
bool evenParity(int first, int last) {
  int ones = 0;
  for (int i = first; i <= last; ++i) ones += impulseArray[i] == 2;
  return ones % 2 == 0;
}
std::array<int, 9> fields(const tm& t) {
  return {t.tm_year, t.tm_mon, t.tm_mday, t.tm_wday, t.tm_yday,
          t.tm_hour, t.tm_min, t.tm_sec, t.tm_isdst};
}
void check(const char* name, time_t start, const std::array<int, 7>& expected) {
  for (int second = 0; second < 60; ++second) {
    const time_t now = start + second;
    localtime_r(&now, &timeinfo);
    const auto original = fields(timeinfo);
    CodeTime();
    const std::array<int, 7> actual = {
      decode(50, 8), decode(45, 5), decode(36, 6), decode(42, 3),
      decode(29, 6), decode(21, 7), impulseArray[17] == 2 ? 1 : 0
    };
    const bool valid = actual == expected && fields(timeinfo) == original &&
      actualSecond == second && impulseArray[18] == (expected[6] ? 1 : 2) &&
      impulseArray[20] == 2 && impulseArray[59] == 0 &&
      evenParity(21, 28) && evenParity(29, 35) && evenParity(36, 58);
    if (!valid) {
      std::fprintf(stderr, "FAIL: %s at second %d: encoded", name, second);
      for (int field : actual) std::fprintf(stderr, " %d", field);
      std::fprintf(stderr, "; expected");
      for (int field : expected) std::fprintf(stderr, " %d", field);
      std::fprintf(stderr, "\n");
      ++failures;
      return;
    }
  }
  std::printf("PASS: %s (all 60 seconds)\n", name);
}
int main() {
  setenv("TZ", "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", 1);
  tzset();
''' + "\n".join(calls) + r'''
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
'''
    with tempfile.TemporaryDirectory(prefix="dcf77-code-time-") as directory:
        test = Path(directory) / "test.cpp"
        binary = Path(directory) / "test"
        test.write_text(harness)
        subprocess.run(shlex.split(os.environ.get("CXX", "c++")) +
                       ["-std=c++11", "-Wall", "-Wextra", "-Werror", str(test), "-o", str(binary)], check=True)
        return subprocess.run([str(binary)]).returncode


if __name__ == "__main__":
    sys.exit(main())
