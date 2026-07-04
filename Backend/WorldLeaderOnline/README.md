# WorldLeader Online Backend

Rust/Axum service for the online backend described in `ROADMAP.md`.

It owns online-only concerns:

- guest/dev auth and player profiles
- cloud campaign records and save snapshots
- matchmaking tickets and lobbies
- PvP battle result validation intake
- rankings
- diplomacy history for online campaigns
- mod manifests
- telemetry intake
- replay metadata

The Unreal local campaign backend remains in C++. This service is the online layer.

## Run Locally

```powershell
cd Backend/WorldLeaderOnline
cargo run
```

Default bind: `127.0.0.1:8787`.

Useful checks:

```powershell
Invoke-RestMethod http://127.0.0.1:8787/healthz
Invoke-RestMethod http://127.0.0.1:8787/readyz
```

## Environment

```txt
WL_ONLINE_BIND=127.0.0.1:8787
WL_ONLINE_CORS_ORIGIN=*
DATABASE_URL=postgres://worldleader:worldleader@localhost:5432/worldleader
REDIS_URL=redis://localhost:6379
WL_ONLINE_RUN_MIGRATIONS=true
```

Without `DATABASE_URL` and `REDIS_URL`, the service runs in in-memory dev mode so
tests and local API work do not require external services.

When PostgreSQL is configured, the service runs `migrations/0001_online_backend.sql`
and persists API documents into `online_documents`. The migration also creates
the canonical tables named in the roadmap for later normalization.

When Redis is configured, matchmaking tickets and lobbies are cached as ephemeral
JSON keys.

## Minimal API Flow

```powershell
$auth = Invoke-RestMethod `
  -Method Post `
  -Uri http://127.0.0.1:8787/v1/auth/guest `
  -ContentType 'application/json' `
  -Body '{"display_name":"Tester"}'

$headers = @{ Authorization = "Bearer $($auth.token)" }

Invoke-RestMethod `
  -Method Post `
  -Uri http://127.0.0.1:8787/v1/campaigns `
  -Headers $headers `
  -ContentType 'application/json' `
  -Body '{"name":"America Test","mode":"online_campaign","initial_state":{"nation":"CO"}}'
```

## Verification

```powershell
cd Backend/WorldLeaderOnline
cargo fmt --check
cargo test
cargo clippy --all-targets -- -D warnings
```
