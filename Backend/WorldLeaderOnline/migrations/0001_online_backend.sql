CREATE TABLE IF NOT EXISTS online_documents (
    kind TEXT NOT NULL,
    document_id TEXT NOT NULL,
    body JSONB NOT NULL,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (kind, document_id)
);

CREATE INDEX IF NOT EXISTS idx_online_documents_kind_updated
    ON online_documents(kind, updated_at DESC);

CREATE TABLE IF NOT EXISTS audit_logs (
    id UUID PRIMARY KEY,
    actor_user_id UUID NULL,
    action TEXT NOT NULL,
    payload JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS users (
    id UUID PRIMARY KEY,
    provider TEXT NOT NULL,
    provider_subject TEXT NOT NULL,
    display_name TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE(provider, provider_subject)
);

CREATE TABLE IF NOT EXISTS profiles (
    user_id UUID PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    handle TEXT NOT NULL,
    preferred_nation TEXT NULL,
    rating INTEGER NOT NULL DEFAULT 1000,
    wins INTEGER NOT NULL DEFAULT 0,
    losses INTEGER NOT NULL DEFAULT 0,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS campaigns (
    id UUID PRIMARY KEY,
    owner_user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    mode TEXT NOT NULL,
    status TEXT NOT NULL,
    state_version INTEGER NOT NULL DEFAULT 1,
    cloud_save JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS lobbies (
    id UUID PRIMARY KEY,
    owner_user_id UUID NOT NULL,
    mode TEXT NOT NULL,
    status TEXT NOT NULL,
    max_players SMALLINT NOT NULL,
    body JSONB NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS match_tickets (
    id UUID PRIMARY KEY,
    user_id UUID NOT NULL,
    mode TEXT NOT NULL,
    preferred_nation TEXT NULL,
    status TEXT NOT NULL,
    lobby_id UUID NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS battle_results (
    id UUID PRIMARY KEY,
    campaign_id UUID NULL,
    lobby_id UUID NULL,
    winner_user_id UUID NULL,
    loser_user_id UUID NULL,
    validation_hash TEXT NOT NULL,
    summary JSONB NOT NULL,
    accepted BOOLEAN NOT NULL DEFAULT true,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS rankings (
    user_id UUID PRIMARY KEY,
    rating INTEGER NOT NULL DEFAULT 1000,
    wins INTEGER NOT NULL DEFAULT 0,
    losses INTEGER NOT NULL DEFAULT 0,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS diplomacy_relations (
    id UUID PRIMARY KEY,
    campaign_id UUID NOT NULL,
    actor_nation TEXT NOT NULL,
    target_nation TEXT NOT NULL,
    event_type TEXT NOT NULL,
    payload JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS mods (
    id UUID PRIMARY KEY,
    owner_user_id UUID NOT NULL,
    slug TEXT NOT NULL,
    name TEXT NOT NULL,
    version TEXT NOT NULL,
    checksum TEXT NOT NULL,
    download_url TEXT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE(slug, version)
);

CREATE TABLE IF NOT EXISTS telemetry_events (
    id UUID PRIMARY KEY,
    user_id UUID NULL,
    event_name TEXT NOT NULL,
    payload JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS replays (
    id UUID PRIMARY KEY,
    owner_user_id UUID NOT NULL,
    battle_result_id UUID NULL,
    storage_url TEXT NOT NULL,
    checksum TEXT NOT NULL,
    metadata JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
