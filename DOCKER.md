# Astonia 3 Community Server - Docker Deployment

## Image Variants

| Tag | Size | Description |
|-----|------|-------------|
| `astoniacommunity/astonia_server_3:latest` | ~296MB | Includes default zones - ready to play |
| `astoniacommunity/astonia_server_3:base` | ~180MB | No zones - for custom zone development |
| `astoniacommunity/astonia_server_3:builder` | ~649MB | Build environment - for code customization |

## Quick Start

### Using Docker Compose (Recommended)

```bash
# Clone the repository
git clone https://github.com/AstoniaCommunity/astonia_community_server3
cd astonia_community_server3

# Start the server (builds image if needed)
docker compose up -d

# View logs
docker compose logs -f server

# Create an account and character
docker exec astonia3-server /entrypoint.sh create_account your@email.com yourpassword
docker exec astonia3-server /entrypoint.sh create_character 1 YourCharName MWG
```

### Using Pre-built Image

```bash
# Create a docker-compose.yml:
cat > docker-compose.yml << 'EOF'
services:
  db:
    image: mariadb:10.11
    container_name: astonia3-db
    restart: unless-stopped
    environment:
      MYSQL_ROOT_PASSWORD: astonia
      MYSQL_DATABASE: merc
    volumes:
      - astonia3-db-data:/var/lib/mysql
    healthcheck:
      test: ["CMD", "healthcheck.sh", "--connect", "--innodb_initialized"]
      interval: 10s
      timeout: 5s
      retries: 5
      start_period: 30s

  server:
    image: astoniacommunity/astonia_server_3:latest
    container_name: astonia3-server
    restart: unless-stopped
    depends_on:
      db:
        condition: service_healthy
    environment:
      AS3_DBHOST: db
      AS3_DBUSER: root
      AS3_DBPASS: astonia
      AS3_DBNAME: merc
    ports:
      - "5556-5590:5556-5590"

volumes:
  astonia3-db-data:
EOF

# Start
docker compose up -d
```

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `AS3_DBHOST` | `db` | MySQL/MariaDB hostname |
| `AS3_DBUSER` | `root` | Database username |
| `AS3_DBPASS` | `astonia` | Database password |
| `AS3_DBNAME` | `merc` | Database name |
| `AS3_CHATHOST` | `localhost` | Chat server hostname |

### Ports

| Port Range | Description |
|------------|-------------|
| `5556-5590` | Game area servers |
| `5554` | Chat server (internal, not exposed by default) |

## Commands

```bash
# Start server
docker compose up -d

# Stop server
docker compose down

# View logs
docker compose logs -f server

# Create account
docker exec astonia3-server /entrypoint.sh create_account <email> <password>

# Create character
# Classes: MWG = Male Warrior God, FMG = Female Mage God, etc.
docker exec astonia3-server /entrypoint.sh create_character <account_id> <name> <class>

# Access shell
docker exec -it astonia3-server bash

# Reinitialize database (WARNING: destroys data)
docker compose down -v
docker compose up -d
```

## Connecting with a Client

Use the Astonia client (moac) to connect:

```bash
bin/moac -u<CharacterName> -p<password> -d<server_ip> -v3
```

## Architecture

The Docker container runs multiple processes:
- 1x `chatserver` - Handles in-game chat
- ~30x `server` - One per game area/zone

All processes are managed by the entrypoint script and will be gracefully 
shutdown when the container stops.

## Code Customization (Builder Image)

For modifying the server code without setting up a local build environment:

```bash
# Pull the builder image
docker pull astoniacommunity/astonia_server_3:builder

# Run interactively with your modified source mounted
docker run -it --rm \
  -v $(pwd):/build \
  astoniacommunity/astonia_server_3:builder

# Inside the container:
make clean && make -j$(nproc)

# The compiled binaries are now in your local directory
```

### Building a Custom Image

Create a simple Dockerfile that uses the builder:

```dockerfile
# Build custom server
FROM astoniacommunity/astonia_server_3:builder AS builder
WORKDIR /build
# Source is already there, or mount/copy your modified version
RUN make clean && make -j$(nproc)

# Create runtime image
FROM astoniacommunity/astonia_server_3:base
COPY --from=builder /build/server /server/
COPY --from=builder /build/chatserver /server/
COPY --from=builder /build/runtime/ /server/runtime/
# Add your custom zones
COPY ./my-zones/ /server/zones/
```

Build and run:
```bash
docker build -t my-custom-astonia .
docker compose up -d  # with your custom image
```

## Custom Zone Development

For custom zone development, use the `base` image and mount your zones:

```yaml
# docker-compose.yml for custom zones
services:
  db:
    image: mariadb:10.11
    environment:
      MYSQL_ROOT_PASSWORD: astonia
      MYSQL_DATABASE: merc
    volumes:
      - astonia3-db-data:/var/lib/mysql
    healthcheck:
      test: ["CMD", "healthcheck.sh", "--connect", "--innodb_initialized"]
      interval: 10s
      timeout: 5s
      retries: 5
      start_period: 30s

  server:
    image: astoniacommunity/astonia_server_3:base
    depends_on:
      db:
        condition: service_healthy
    environment:
      AS3_DBHOST: db
      AS3_DBUSER: root
      AS3_DBPASS: astonia
      AS3_DBNAME: merc
    ports:
      - "5556-5590:5556-5590"
    volumes:
      # Mount your custom zones folder
      - ./my-zones:/server/zones

volumes:
  astonia3-db-data:
```

Your `my-zones` folder should have the same structure as the default zones:
```
my-zones/
  generic/
    weapons.itm  (generated, copy from default)
    armor.itm    (generated, copy from default)
    *.itm, *.map, etc.
  1/
  2/
  ...
  37/
```

## Building Locally

```bash
# Build all variants
docker build -t astonia3:latest .                           # With zones (default)
docker build -t astonia3:base --target runtime-base .       # Without zones
docker build -t astonia3:builder --target builder .         # Build environment

# Or with docker compose
docker compose build
```

## Security Notes

- Change the default database password in production
- Configure a firewall to restrict access to game ports
- The chat server port (5554) should not be exposed publicly
- Consider running behind a reverse proxy for additional security
