"""The SPI driver of the bench's Mesa card, from hwtest/burnin/stmbl.ini
[HOSTMOT2] DRIVER and DRIVER_PARAMS (one place for every HAL session)."""
import configparser
import os

INI = os.path.join(os.path.dirname(os.path.abspath(__file__)), "burnin", "stmbl.ini")


def driver():
    """(name, params): e.g. ("hm2_spix", "spiclk_rate=12500")."""
    c = configparser.ConfigParser(strict=False, interpolation=None, inline_comment_prefixes=("#",))
    c.read(INI)
    h = c["HOSTMOT2"] if c.has_section("HOSTMOT2") else {}
    return h.get("DRIVER", "hm2_rpspi").strip().strip('"'), h.get("DRIVER_PARAMS", "").strip().strip('"')


def loadrt():
    """The loadrt line for a halrun session (no config string)."""
    name, params = driver()
    return ("loadrt %s %s" % (name, params)).strip()


if __name__ == "__main__":
    print(loadrt())
