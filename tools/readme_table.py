"""Markdown results table for READMEs: python tools/readme_table.py BOT [BOT...] (rows), against v82's own row."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import table

OPPS = table.OPPS


def row(bot):
    cells, w, l = [], 0, 0
    for o, k in OPPS:
        r = (17, 17, 0.0, 0, 0) if (bot == "v82" and o == "v82") else table.read(bot, o, k)
        if r is None:
            cells.append("-")
            continue
        w += r[0]
        l += r[1]
        cells.append("%d-%d (%+.1f%%)" % (r[0], r[1], r[2]))
    return "| %s | %s | **%d-%d** |" % (bot, " | ".join(cells), w, l)


def main():
    names = {"v82": "v82", "spar-brawler": "brawler", "v80-5": "v80-5", "spar-locker": "locker"}
    print("| Bot | " + " | ".join("vs %s %s" % (names[o], "(unlimited)" if k == "unlimited" else "") for o, k in OPPS)
          + " | Total |")
    print("| --- " * (len(OPPS) + 2) + "|")
    for b in sys.argv[1:]:
        print(row(b))


if __name__ == "__main__":
    main()
