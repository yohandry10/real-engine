use std::sync::Arc;

use anyhow::Context;
use redis::AsyncCommands;
use serde::Serialize;
use sqlx::{PgPool, Row};
use tokio::sync::RwLock;
use tracing::{info, warn};

use crate::{Config, error::ApiError, store::MemoryStore};

#[derive(Clone)]
pub struct AppState {
    pub config: Config,
    pub store: Arc<RwLock<MemoryStore>>,
    pg_pool: Option<PgPool>,
    redis_client: Option<redis::Client>,
}

impl AppState {
    pub async fn bootstrap(config: Config) -> anyhow::Result<Self> {
        let pg_pool = if let Some(database_url) = &config.database_url {
            let pool = PgPool::connect(database_url)
                .await
                .context("connecting to PostgreSQL")?;
            if config.run_migrations {
                run_migrations(&pool).await?;
            }
            Some(pool)
        } else {
            None
        };

        let redis_client = if let Some(redis_url) = &config.redis_url {
            Some(redis::Client::open(redis_url.as_str()).context("creating Redis client")?)
        } else {
            None
        };

        let state = Self {
            config,
            store: Arc::new(RwLock::new(MemoryStore::default())),
            pg_pool,
            redis_client,
        };
        state.load_documents_from_postgres().await?;
        Ok(state)
    }

    pub async fn postgres_ready(&self) -> bool {
        if let Some(pool) = &self.pg_pool {
            sqlx::query("SELECT 1").execute(pool).await.is_ok()
        } else {
            true
        }
    }

    pub async fn redis_ready(&self) -> bool {
        if let Some(client) = &self.redis_client {
            match client.get_multiplexed_async_connection().await {
                Ok(mut connection) => {
                    let pong: redis::RedisResult<String> =
                        redis::cmd("PING").query_async(&mut connection).await;
                    pong.map(|value| value == "PONG").unwrap_or(false)
                }
                Err(_) => false,
            }
        } else {
            true
        }
    }

    pub async fn persist_document<T>(&self, kind: &'static str, id: String, value: &T)
    where
        T: Serialize + ?Sized,
    {
        let Some(pool) = &self.pg_pool else {
            return;
        };
        let body = match serde_json::to_value(value) {
            Ok(value) => value,
            Err(err) => {
                warn!(kind, id, error = %err, "failed to serialize online document");
                return;
            }
        };
        if let Err(err) = sqlx::query(
            r#"
            INSERT INTO online_documents(kind, document_id, body, updated_at)
            VALUES ($1, $2, $3, now())
            ON CONFLICT (kind, document_id)
            DO UPDATE SET body = EXCLUDED.body, updated_at = now()
            "#,
        )
        .bind(kind)
        .bind(&id)
        .bind(body)
        .execute(pool)
        .await
        {
            warn!(kind, id, error = %err, "failed to persist online document");
        }
    }

    pub async fn cache_ephemeral<T>(&self, key: String, value: &T, ttl_seconds: u64)
    where
        T: Serialize + ?Sized,
    {
        let Some(client) = &self.redis_client else {
            return;
        };
        let payload = match serde_json::to_string(value) {
            Ok(payload) => payload,
            Err(err) => {
                warn!(key, error = %err, "failed to serialize redis payload");
                return;
            }
        };
        match client.get_multiplexed_async_connection().await {
            Ok(mut connection) => {
                let result: redis::RedisResult<()> =
                    connection.set_ex(key.clone(), payload, ttl_seconds).await;
                if let Err(err) = result {
                    warn!(key, error = %err, "failed to cache redis payload");
                }
            }
            Err(err) => warn!(key, error = %err, "failed to connect to Redis"),
        }
    }

    async fn load_documents_from_postgres(&self) -> anyhow::Result<()> {
        let Some(pool) = &self.pg_pool else {
            return Ok(());
        };
        let rows = sqlx::query("SELECT kind, body FROM online_documents ORDER BY updated_at ASC")
            .fetch_all(pool)
            .await
            .context("loading online documents")?;
        let mut store = self.store.write().await;
        let mut loaded = 0usize;
        for row in rows {
            let kind: String = row.try_get("kind")?;
            let body: serde_json::Value = row.try_get("body")?;
            if let Err(err) = store.upsert_document(&kind, body) {
                warn!(kind, error = %err, "skipped invalid online document");
                continue;
            }
            loaded += 1;
        }
        info!(loaded, "loaded persisted online documents");
        Ok(())
    }
}

async fn run_migrations(pool: &PgPool) -> anyhow::Result<()> {
    let statements = include_str!("../migrations/0001_online_backend.sql");
    for statement in statements.split(";") {
        let trimmed = statement.trim();
        if trimmed.is_empty() {
            continue;
        }
        sqlx::query(trimmed)
            .execute(pool)
            .await
            .map_err(|err| ApiError::Storage(err.to_string()))
            .context("running online backend migration")?;
    }
    Ok(())
}
