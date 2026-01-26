# Monitoring (ClickHouse + Grafana + Postgres)

## Quickstart

* Start containers and injest data:

```bash
docker compose up -d
./symbolizer processor -f memhawk_<process_name>_<process_pid>_protobuf.binpb --watch
```

Open grafana ui: http://localhost:3000, user - `admin`, password - `admin`

* Stop containers and remove data

```bash
docker compose down -v
```

## Admin panels and credentials

* Grafana UI: http://localhost:3000
  * Admin user: `admin` or GF_SECURITY_ADMIN_USER
  * Admin password: `admin` or GF_SECURITY_ADMIN_PASSWORD

* ClickHouse HTTP endpoint: http://localhost:8123
  * User: `admin` or CLICKHOUSE_USER
  * Password: `admin` or CLICKHOUSE_PASSWORD

* Postgres:
  * Port: `5432` or POSTGRES_PORT
  * User: `admin` or POSTGRES_USER
  * Password: `admin` or POSTGRES_PASSWORD

**Note:** Ports/credentials can be configured via `.env` (or environment variables) before starting compose.
