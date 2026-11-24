# D3300 C 1.02 — verified input-chain boundary

Static-only chain for `b860_102.bin` (SHA-256
`c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`):

```text
GPIO debouncer 0x9011DDA8
  -> state helper 0x9011E738
  -> RelayEventEx mask 0x00000008 (0x9011E742)
  -> callback 0x9011DD5C
  -> ARM state gate 0x90369364 (reads global byte +3)
  -> continuation 0x9011E756 (message type 2, length 0x10)
  -> snd_mbf veneer 0x9002EC0E / queue state 0x1011FFB4
  -> COPNOQueueButton 0x9013DDCC
  -> normalized 16-byte record
  -> CSYSCAppButtonController 0x903276B4
  -> mailbox setter 0x903512A0
```

What is proven: the code addresses, the mask, the 16-byte record boundary,
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
