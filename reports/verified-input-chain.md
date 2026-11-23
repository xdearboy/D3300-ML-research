# D3300 C 1.02 — verified input-chain boundary

Static-only chain for `b860_102.bin` (SHA-256
`c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`):

```text
GPIO debouncer 0x9011DDA8
  -> state helper 0x9011E738
  -> RelayEventEx mask 0x00000008 (0x9011E742)
  -> callback 0x9011DD5C
  -> input message path / queue state 0x1011FFB4
  -> COPNOQueueButton 0x9013DDCC
  -> normalized 16-byte record
  -> CSYSCAppButtonController 0x903276B4
  -> mailbox setter 0x903512A0
```

What is proven: the code addresses, the mask, the 16-byte record boundary,
the queue-button range/TBB, and the application mailbox setter/getter.

What is not proven: which physical camera button is wired to GPIO Pin 8, and
whether an external observer can safely read the live T-Kernel flag or queue.
The host implementation therefore accepts only read-only snapshots and keeps
candidate MENU/OK/INFO codes explicitly unconfirmed.
