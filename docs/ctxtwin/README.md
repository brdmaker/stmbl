# Gildemeister CTX twin: documentation as text

These are text versions of the machine's scanned manuals, made for the
retrofit: two LinuxCNC + stmbl controllers, one per side, and a sync for the
part transfer. Each file follows its manual's own chapters, keeps every
figure, part number, M-code and message, and ends with a "For the retrofit"
section. Unclear readings are marked [uncertain] or [unreadable].

| folder | source | language |
|---|---|---|
| [manual/](manual/) | Mechanical technical documentation, doc. daaam0g1, Feb 1995: chapters 1-5 (remarks, maintenance, assembly groups with parts lists, hydraulic and lubrication plans, chucks, spindle cooling) | English original |
| [operating/](operating/) | Betriebsanleitung CTX twin, doc. daaab1d1, Feb 1995: chapters 1-10 | German, translated (screen and message texts kept in German) |
| [operating_en/](operating_en/) | Operating instructions CTX twin (English edition, doc. daaab1g1, software release 4.4): chapters 1-10 | English original |
| [turret/](turret/) | Sauter disc-type tool turret 0.5.450.020 with electrical drive, BA-444 e (June 1994), parts a and c | English |

**Gaps:**
- **Mechanical, chapters 3 and 5:** blank in the scans.
- **Mechanical 4.3, the headstock (pages 4-9 to 4-16):** missing.
- **English chapter 7:** pages 7-3 to 7-12 are missing. The German chapter 7 is complete, including the transfer expert programs.
- **Turret part b (pages 21-40):** not supplied. It holds the electrics and connection diagram, coolant, the limit-switch setting and the overload clutch.
- **Electrical documentation:** none yet. It would give drives, I/O and wiring.
- **German vs English editions:** they differ in places. For example, the German chapter 9 has more pages and the German chapter 7 has more content. Where they disagree, both files note it.

## The machine in short (from these documents)

Facts that the retrofit design rests on. Page references are in the files.

**Control**
- Gildemeister ELTROPILOT (DIN PLUS, TURN PLUS) with a Grundig panel shared
  by both sides.
- PLC I/O on Eltromatic modules.
- The sides are the two "aggregate groups" AG 1 (left) and AG 2 (right),
  each with its own work area and operating mode.
- "Betriebsartensplitting" (mode splitting) and the AG selector decide
  whether cycle start/stop and feed stop act on one side or on both.

**Axes** (2 x 2, each side a mirror image; the Z axes face each other)
- **X:** X1/X2 on 45° slant slides, 232 mm.
- **Z1:** 380 mm.
- **Z2:** the right headstock itself travels in Z, 1165 mm. This is how it
  reaches the transfer position.
- **Ball screws:** 32x10 on X and 40x10 on Z, directly coupled through KTR
  ROTEX couplings.
- **Rapids:** 16 m/min (X) and 24 m/min (Z).

**Motors**
- **Feed motors:** the operating manual names Indramat MAC 93 A on X
  (2000 rpm, 9.2 Nm) and MAC 93 B on Z (3000 rpm, 14.5 Nm).
- **The mechanical parts lists say MDD 093 A/B instead:** read the
  nameplates.
- **Overload slip clutches on X/Z:** they trip E-STOP (message 1553).

**Spindles**
- Integrated motors (ISM), 18 kW, 202 Nm, 25-4000 min⁻¹, base speed
  850 min⁻¹.
- Water/glycol cooled, with winding and bearing temperature monitoring
  (AEM module).
- C axis optional (M14/M15).

**Turrets**
- Sauter disc turret, 12 stations VDI 40, 0.12-0.20 s per station, shortest
  way.
- **Indexing:** by an AC servo through a gear and a ratchet overload clutch;
  the motor's encoder gives the position (no coded switches). With the
  retrofit, each turret is another stmbl drive.
- **One 4/2 valve** (`Y1.1a` / `Y1.1b`) does three things at once: it locks
  the disc, engages the driven-tool drive and disengages the indexing drive.
- **Switches:** `S4` disengaged, `S5` engaged, `S1` reference.
- **Homing:** unlock, turn clockwise past `S1`, then on to the encoder's zero
  pulse.
- **Driven tools (option):** 3000 rpm, 5.3 kW / 24 Nm, M103/M104/M105
  (AG 2: M203-M205).

**Chucks**
- Hydraulic.
- **Left:** valve `Y18241` open / `Y18251` close, stroke switches
  `S18121`/`S18111`, pressure switch `F18041`.
- **Right:** `Y24251` open / `Y24241` close, stroke switches
  `S24121`/`S24111`, pressure switch `F24041`.
- 5/10-60 bar. M34/M35 switch between normal and low pressure.
- **Interlocks:**
  - no chuck command while the spindle turns;
  - clamp/release within 20 s;
  - the spindle may not run more than 90 s with the chuck open (M033 says
    180 s: the manual contradicts itself);
  - low pressure or a stroke out of range stops the spindle.
- Foot switches: right clamps (held ≥ 2 s), left releases.

**Hydraulics**
- 70 bar vane pump, 2.2 kW, 100 l tank, 2 l accumulator at 55 bar.
- Pressure switch 6.1 (5-60 bar), filter switches.
- Turret lock pressure 50 bar.

**Lubrication**
- Central lubrication unit with an oil-pressure switch (20 bar) and a level
  switch.
- 26 points, 0.84 cm³ per cycle.
- M48/M49 suppress / give an extra pulse. Pulse interval and suppression
  are set by parameters.

**Safety**
- Hoods per side, interlocked:
  - they open only with the spindles stopped;
  - the spindle runs only with the hood closed;
  - setting-up with the hood open: key switch plus enabling key, spindle
    ≤ 50 min⁻¹, slides ≤ 1 m/min.
- Speed monitoring at 0 / 50 min⁻¹ / max.
- **Automatic hood option:** safety edge, two-hand close.
- **Partition wall** between the work areas (option, hydraulic): M46 opens,
  M47 closes. It moves only with the hoods closed (messages 1950-1953 /
  6950-6953).

**Part transfer** (German chapter 7, chapter 9 M-codes)
- **Expert programs:** `%XTWI1U2.NCS` / `%XTWI1U2A.NCS`, called in both
  channels at the same time.
- **Spindles coupled:** spindle 3 (right) follows spindle H (left), speed- or
  angle-synchronous with an angle offset `S..`.
  - Default 1000 min⁻¹, range 300-2000.
  - Speed limit 2000 min⁻¹ by default.
- **Synchronisation M-codes:** M803 = AG1 leads AG2, M804 = AG2 leads AG1,
  M800 = off. M97 sets sync points between the channels.
- **Approach:** Z2 approaches at 1000 mm/min, or at 500 mm/min with reduced
  torque against a fixed stop (not in single block).
- **Positions:** taught pick-off positions 1-6 and 9.
- **Checks:** part seating M027/M327 (event flag E1.4/E2.4). M50/M350 flush
  the jaws.
- **The original machine transfers with the spindles turning in angle
  sync.** For the retrofit, that is the "synchronous transfer" option: the
  receiving spindle's stmbl drive follows the giving spindle's encoder.

**Program events**
- `E1[n]` / `E2[n]`, 16 per side.
- On the twin: bar end (1), seating check (4), handling inputs X/Y (9/10),
  chuck clamped (12).
- The NC must answer PLC signals within 300 ms.

**This machine's settings**
- The German parameter list (chapter 9a) has handwritten values. They are
  probably this machine's own, including a Finnish note dated 20.6.95.
