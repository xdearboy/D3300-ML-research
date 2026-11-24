# D3300 C 1.02 — verified input-chain boundary

Static-only chain for `b860_102.bin` (SHA-256
`c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`):

```text
GPIO debouncer 0x9011DDA8
  -> state helper 0x9011E738
  -> RelayEventEx mask 0x00000008 (0x9011E742)
  -> callback 0x9011DD5C
  -> ARM state gate 0x90369364 (reads global byte +3)
  -> continuation 0x9011E756 (message source of 0x10 bytes; byte 0 set to type 2)
  -> snd_mbf veneer 0x9002EC0E / queue state 0x1011FFB4
  -> COPNOQueueButton 0x9013DDCC
  -> 16-byte queue payload
  -> CSYSCAppButtonController 0x903276B4
  -> mailbox setter 0x903512A0
```

What is proven: the code addresses, the mask, the 16-byte message boundary,
the queue-button range/TBB, and the application mailbox setter/getter.

`0x90369364` is an ARM A32 helper (`ldr`/`ldrb`/`bx lr`) returning byte
`[0x10125160 + 3]` (the literal pointer is at `0x903695DC`). It does not
construct a button code. The callback's comparison with `1` is therefore a
state gate, not proof of MENU/OK/INFO identity. Its direct ARM setter is
`0x90369358` (`strb r0, [0x10125160 + 3]`). The GPIO debounce path calls that
setter at `0x9011DDF4` with `r0 = 1` and at `0x9011DE08` with `r0 = 0`, each
immediately before forwarding the same 1/0 state to `0x9011E738`. This proves
the state byte follows that GPIO forwarder; it still does not identify the
physical control. The getter is
also called directly from `0x9003D0E6`, `0x9003D13E`, `0x9003D298`,
`0x9003D856`, and `0x9011E1FA`; it is therefore a shared state predicate, not
an input-specific function.

The sender continuation `0x9011E756` first pushes `r0`--`r4` and `lr`, then
writes only `0x02` to byte `[sp]`.  It passes `r1 = sp`, `r2 = 0x10`, and
`r3 = 0x64` to the send veneer `0x9002EC0E`.  Thus its 16-byte source range is
the saved incoming `r0`--`r3` words, with the low byte of saved `r0` replaced
by `0x02`.  The continuation itself does not initialize the other 15 bytes.
The tail branch from `0x9011DD70` and direct `BL` from `0x9011E018` both reach
this same sender.  The later queue/controller path demonstrably copies 16
bytes, but it does not establish a normalized field layout or a button ID for
this source payload.

The `0x9011E018` caller has a separately verified mechanical shape.  Its
enclosing routine begins at `0x9011DFE2`; after `r4` is supplied as a non-null
record pointer, it loads `r0`, `r1`, and `r3` from record offsets `+4`, `+0`,
and `+8`, respectively.  With its local selector equal to `2`, it computes
`r2 = 0x10319D2C + 0x18` and calls the sender only when record `+4` is nonzero.
Consequently this caller preserves source-word provenance, but the record's
type and field meanings remain **UNPROVEN**.  The other caller is the tail
branch from the GPIO callback and is not assumed to share that record shape.

One shared consumer, `0x9003D28C`, is a two-output query called directly from
`0x9011E0AE` and `0x9011E3F6`. Its state base is `0x101219D4` (literal
`0x10121A20` at `0x9003D4BC`, minus `0x4C`). On the proven fast path where
`[base + 0x40] != 0` and the shared gate returns `1`, it copies words
`[base + 0x30]` and `[base + 0x34]` to its two pointer arguments and returns
`0`. The purpose of those words is **UNPROVEN**; they are not assigned a button
or photographic meaning.

What is not proven: which physical camera button is wired to GPIO Pin 8, and
whether an external observer can safely read the live T-Kernel flag or queue.
The host implementation therefore accepts only read-only snapshots and keeps
candidate MENU/OK/INFO codes explicitly unconfirmed.
