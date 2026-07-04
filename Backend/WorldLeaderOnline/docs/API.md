# WorldLeader Online API

All authenticated routes use:

```txt
Authorization: Bearer <token>
```

Get a dev token:

```http
POST /v1/auth/guest
Content-Type: application/json

{ "display_name": "Commander" }
```

Core routes:

```txt
GET  /healthz
GET  /readyz
GET  /v1/me
PUT  /v1/profiles/me
GET  /v1/campaigns
POST /v1/campaigns
GET  /v1/campaigns/{campaign_id}
POST /v1/campaigns/{campaign_id}/save
POST /v1/matchmaking/tickets
GET  /v1/matchmaking/tickets/{ticket_id}
POST /v1/lobbies
GET  /v1/lobbies/{lobby_id}
POST /v1/lobbies/{lobby_id}/join
PUT  /v1/lobbies/{lobby_id}/dedicated-server
POST /v1/battle-results
GET  /v1/rankings
POST /v1/diplomacy/events
GET  /v1/campaigns/{campaign_id}/diplomacy
GET  /v1/mods
POST /v1/mods
POST /v1/telemetry
POST /v1/replays
GET  /v1/replays/{replay_id}
```

Battle results are accepted only when the request carries a non-trivial
`validation_hash` and references known winner/loser profiles. The dedicated
server will later sign this hash; the backend already stores and ranks accepted
results through the same intake path.
