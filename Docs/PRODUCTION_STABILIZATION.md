# Production Stabilization Contract

## Decisions

- Save versions 16 and 17 are internal pre-production formats. They remain migratable so QA fixtures and long-lived developer campaigns do not silently disappear.
- Campaign months are uniform and configurable. `DaysPerMonth = 30` is the authoritative default; gameplay must not embed the literal value elsewhere.
- Simulation state must be deterministic on every supported target. Stable hashes sort identifiers, use UTF-8 input, and quantize persisted decimal remainders.

## Authoritative turn order

1. Decisions
2. Daily economy
3. Recruitment
4. Calendar
5. Fiscal close
6. Province transition
7. Economic snapshot
8. Market
9. Economic AI
10. Politics
11. Military
12. Battles
13. Consequences
14. Validation
15. Snapshot

`FWLCampaignTurnCoordinator` owns this order. New monthly systems must be added as an explicit phase or as work inside the owning phase, never as a second caller after `AdvanceDay`.

## Release gate

Run from the repository root:

```powershell
.\Scripts\run_production_gate.ps1 -Package
```

A release is blocked by a compile failure, a failed or empty automation group, a fatal Standalone log, or a failed Win64 Shipping package. The gate intentionally runs expensive government and politics tests separately so their runtime is visible.

## Save policy

- Current schema: v19.
- Oldest accepted schema: v16.
- Every schema increment requires one sequential and idempotent migration plus a fixture test.
- Future schemas and versions older than v16 fail with a user-facing reason.

## Determinism policy

- Random behavior must receive an explicit campaign seed.
- Iteration over maps/sets must be sorted before affecting outcomes or hashes.
- Floating-point values persisted in the campaign hash must be quantized to a documented precision.
- The 12, 60 and 120 month replay tests must produce identical hashes for identical inputs.
