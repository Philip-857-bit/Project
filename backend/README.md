# Smart Fish Feeder Backend API

A high-performance Go backend service for the Smart Fish Feeder IoT system, providing automated fish feeding with intelligent scheduling, comprehensive monitoring, and data analytics for aquaculture operations.

## Features

- **Go-Powered Performance**: Built with Go for superior performance and concurrency
- **RESTful API**: Clean REST API design with comprehensive endpoints
- **PostgreSQL Database**: Robust relational database with GORM ORM
- **Redis Caching**: High-performance caching and session management
- **JWT Authentication**: Secure user authentication and authorization
- **Device Management**: Arduino device registration and binding
- **Real-time Monitoring**: Sensor data collection and processing
- **Feed Calculator**: Species-specific feeding recommendations
- **Docker Support**: Containerized deployment with Docker Compose
- **Comprehensive Testing**: Unit tests and property-based testing
- **Structured Logging**: JSON-formatted logging with logrus

## Quick Start

### Prerequisites

- Go 1.21+
- PostgreSQL 15+
- Redis 7+
- Docker and Docker Compose (optional)

### Local Development Setup

1. **Clone and navigate to the project**:
   ```bash
   cd smart-fish-feeder
   ```

2. **Install dependencies**:
   ```bash
   make deps
   ```

3. **Set up environment variables**:
   ```bash
   # Copy example environment file
   cp .env.example .env
   # Edit .env with your settings (see Environment Configuration below)
   
   # Or use config file
   cp configs/config.yaml configs/config.local.yaml
   # Edit configs/config.local.yaml with your settings
   ```

4. **Start dependencies with Docker**:
   ```bash
   docker-compose up -d db redis
   ```

5. **Run the development server**:
   ```bash
   make run
   ```

The API will be available at `http://localhost:8080` with health check at `http://localhost:8080/health`.

### Docker Development Setup

1. **Start all services with Docker Compose**:
   ```bash
   make docker-run
   ```

The API will be available at `http://localhost:8080`, PostgreSQL at `localhost:5432`, Redis at `localhost:6379`, and Adminer (database admin) at `http://localhost:8081`.

## API Documentation

### Health Endpoints

- `GET /health` - Basic health check
- `GET /health/detailed` - Detailed system health with component status

### API Endpoints

All API endpoints are prefixed with `/api/v1`:

#### Authentication
- `POST /api/v1/auth/register` - User registration
- `POST /api/v1/auth/login` - User login
- `POST /api/v1/auth/refresh` - Refresh JWT token
- `POST /api/v1/auth/logout` - User logout

#### Users
- `GET /api/v1/users/profile` - Get user profile
- `PUT /api/v1/users/profile` - Update user profile

#### Devices
- `POST /api/v1/devices/register` - Arduino device registration
- `POST /api/v1/devices/bind` - Bind device to user
- `GET /api/v1/devices` - List user devices
- `GET /api/v1/devices/:id` - Get device details
- `PUT /api/v1/devices/:id` - Update device
- `DELETE /api/v1/devices/:id` - Delete device

#### Feeding
- `GET /api/v1/feeding/schedules` - Get feeding schedules
- `POST /api/v1/feeding/schedules` - Create feeding schedule
- `PUT /api/v1/feeding/schedules/:id` - Update feeding schedule
- `DELETE /api/v1/feeding/schedules/:id` - Delete feeding schedule
- `POST /api/v1/feeding/manual` - Manual feeding
- `GET /api/v1/feeding/history` - Get feeding history
- `GET /api/v1/feeding/analytics` - Get feeding analytics

#### Monitoring
- `GET /api/v1/monitoring/sensors` - Get sensor data
- `POST /api/v1/monitoring/sensors` - Receive sensor data (Arduino)
- `GET /api/v1/monitoring/status` - Get device status
- `GET /api/v1/monitoring/alerts` - Get alerts

#### Calculator
- `POST /api/v1/calculator/recommend` - Calculate feed recommendations
- `GET /api/v1/calculator/species` - Get fish species data

## Configuration

The application uses Viper for configuration management. Configuration can be provided via:

1. **YAML config file** (`configs/config.yaml`)
2. **Environment variables** (prefixed with `SFF_`)
3. **Command line flags** (when implemented)

### Key Configuration Options

```yaml
server:
  host: "0.0.0.0"
  port: 8080
  debug: true

database:
  host: "localhost"
  port: 5432
  user: "smartfeeder"
  password: "smartfeeder123"
  dbname: "smart_fish_feeder"

redis:
  host: "localhost"
  port: 6379

jwt:
  secret_key: "your-secret-key-change-in-production"
  access_token_duration: "1h"
  refresh_token_duration: "720h"
```

### Environment Variables

All configuration options can be overridden with environment variables:

- `SFF_SERVER_HOST` - Server host
- `SFF_SERVER_PORT` - Server port
- `DATABASE_URL` or `SFF_DATABASE_URL` - Full PostgreSQL connection URI
- `REDIS_URL` or `SFF_REDIS_URL` - Full Redis connection URI
- `SFF_DATABASE_HOST` - Database host
- `SFF_DATABASE_PASSWORD` - Database password
- `SFF_JWT_SECRET_KEY` - JWT secret key
- etc.

## Environment Configuration (.env.example)

The `.env.example` file contains all available environment variables. Copy it to `.env` and customize:

```bash
cp .env.example .env
```

### Where to Get Environment Variable Values

| Variable | Where to Get It |
|----------|-----------------|
| `DATABASE_URL` / `SFF_DATABASE_URL` | PostgreSQL provider connection URI (Railway, Supabase, RDS, etc.) |
| `REDIS_URL` / `SFF_REDIS_URL` | Redis provider connection URI (Railway, Upstash, ElastiCache, etc.) |
| `SFF_DATABASE_*` | Create a PostgreSQL database (local, Railway, AWS RDS, etc.) |
| `SFF_REDIS_*` | Create a Redis instance (local, Railway, Upstash, etc.) |
| `SFF_JWT_SECRET_KEY` | Generate with: `openssl rand -base64 32` |
| `SFF_MQTT_BROKER_URL` | Your MQTT broker (HiveMQ Cloud, EMQX, Mosquitto) |
| `SFF_MQTT_USERNAME/PASSWORD` | Create credentials in your MQTT broker dashboard |

### MQTT Broker Options

1. **HiveMQ Cloud** (Recommended for production)
   - Sign up at https://www.hivemq.com/cloud/
   - Create a free cluster
   - Get broker URL: `ssl://your-cluster.hivemq.cloud:8883`
   - Create credentials in Access Management

2. **EMQX Cloud**
   - Sign up at https://www.emqx.com/en/cloud
   - Create a deployment
   - Get broker URL from dashboard

3. **Self-hosted Mosquitto**
   - Install: `apt install mosquitto mosquitto-clients`
   - Configure TLS and authentication
   - URL: `tcp://your-server:1883` or `ssl://your-server:8883`

### Database Options

1. **Railway** (Recommended for quick deployment)
   - Add PostgreSQL plugin to your project
   - Connection string provided automatically

2. **Supabase**
   - Create project at https://supabase.com
   - Get connection string from Settings > Database

3. **AWS RDS / Google Cloud SQL**
   - Create PostgreSQL instance
   - Configure security groups/firewall
   - Use connection details from console

### Configuration Sections

| Section | Description |
|---------|-------------|
| **Server** | HTTP server host, port, timeouts, debug mode |
| **Database** | PostgreSQL connection settings |
| **Redis** | Redis cache connection settings |
| **JWT** | Authentication token settings and secrets |
| **Logging** | Log level and format (json/text) |
| **Device** | Device binding code expiration and limits |
| **Vision** | Computer vision storage and compression settings |
| **MQTT** | MQTT broker connection for ESP32 communication |
| **BLE** | Bluetooth provisioning session settings |
| **WebSocket** | Real-time data streaming configuration |
| **Offline Sync** | Store-and-forward data synchronization |
| **Cellular** | GSM data usage limits and cost tracking |
| **Power** | Battery and solar power thresholds |

### Storage Paths (Videos & Images)

The backend stores video clips and images from ESP32-CAM devices locally. Configure storage paths:

```yaml
vision:
  storage_path: "./storage/videos"        # Video clips from ESP32-CAM
  image_storage_path: "./storage/images"  # Extracted frames for analysis
  max_storage_mb: 1024                    # Maximum storage size (1GB default)
  compression_on: true                    # Enable video compression
```

**Storage Structure:**
```
# Local Development (fallback)
./storage/
├── videos/
│   └── {device_id}/
│       └── feeding_YYYYMMDD_HHMMSS.mjpeg
└── images/
    └── {device_id}/
        └── frame_YYYYMMDD_HHMMSS.jpg

# Production (Cloudinary)
Cloudinary Cloud Storage:
├── smart-fish-feeder/
│   └── {device_id}/
│       ├── feeding_YYYYMMDD_HHMMSS (video)
│       └── frames/
│           └── frame_YYYYMMDD_HHMMSS (image)
```

**Cloudinary Setup (Production):**

1. Sign up at https://cloudinary.com (free tier: 25GB storage, 25GB bandwidth)
2. Get credentials from Dashboard → Account Details
3. Configure environment variables:

```bash
SFF_CLOUDINARY_CLOUD_NAME=your_cloud_name
SFF_CLOUDINARY_API_KEY=your_api_key
SFF_CLOUDINARY_API_SECRET=your_api_secret
SFF_CLOUDINARY_FOLDER=smart-fish-feeder
SFF_CLOUDINARY_ENABLED=true
```

**Benefits of Cloudinary:**
- Auto video compression (saves cellular data)
- Auto-generated thumbnails for mobile app
- CDN delivery for fast streaming
- No Railway storage limits
- URLs stored in PostgreSQL database

**Production Recommendations:**
- Use a dedicated volume or cloud storage (S3, GCS) for production
- Set appropriate `max_storage_mb` based on available disk space
- Enable compression to reduce storage usage
- Implement periodic cleanup of old files (see `cleanup_old_video_clips` function)

**Environment Variables:**
```bash
SFF_VISION_STORAGE_PATH=./storage/videos
SFF_VISION_IMAGE_STORAGE_PATH=./storage/images
SFF_VISION_MAX_STORAGE_MB=1024
SFF_VISION_COMPRESSION_ON=true
```

### Critical Settings for Production

```bash
# MUST change these in production:
SFF_JWT_SECRET_KEY=generate-a-strong-random-key-here
SFF_DATABASE_PASSWORD=use-a-secure-password
SFF_SERVER_DEBUG=false

# Recommended for production:
SFF_MQTT_TLS_ENABLED=true
SFF_LOGGING_LEVEL=info
```

### MQTT Configuration

The backend communicates with ESP32 devices via MQTT. See [MQTT Communication Guide](docs/MQTT_COMMUNICATION.md) for details.

```bash
# Development (no TLS)
SFF_MQTT_BROKER_URL=tcp://localhost:1883

# Production (with TLS)
SFF_MQTT_BROKER_URL=ssl://mqtt.yourserver.com:8883
SFF_MQTT_TLS_ENABLED=true
```

## Development

### Available Make Commands

```bash
make build         # Build the application
make run           # Build and run the application
make dev           # Run with live reload (requires air)
make test          # Run tests
make test-coverage # Run tests with coverage report
make clean         # Clean build artifacts
make deps          # Download and tidy dependencies
make fmt           # Format code
make lint          # Lint code (requires golangci-lint)
make docker-build  # Build Docker image
make docker-run    # Run with Docker Compose
make docker-stop   # Stop Docker Compose
make install-tools # Install development tools
```

### Project Structure

```
smart-fish-feeder/
├── cmd/
│   └── server/          # Application entrypoint
├── internal/
│   ├── algorithms/      # Scientific algorithms (Q10, Kalman, DDPG, etc.)
│   ├── app/             # Application setup and routing
│   ├── config/          # Configuration management
│   ├── database/        # Database connection and migrations
│   ├── handlers/        # HTTP request handlers
│   ├── middleware/      # HTTP middleware
│   ├── models/          # Data models and DTOs
│   ├── mqtt/            # MQTT client and device communication
│   ├── redis/           # Redis client and utilities
│   ├── repository/      # Data access layer
│   └── services/        # Business logic layer
├── configs/             # Configuration files
├── docs/                # Documentation
│   └── MQTT_COMMUNICATION.md  # Hardware communication guide
├── scripts/             # Database and deployment scripts
├── docker-compose.yml   # Docker Compose configuration
├── Dockerfile          # Docker image definition
├── Makefile           # Build and development commands (Linux/Mac)
├── build.ps1          # Build and development commands (Windows)
├── .env.example       # Environment variables template
└── README.md          # This file
```

### Code Style and Guidelines

- Follow Go best practices and idioms
- Use `gofmt` for code formatting
- Write comprehensive tests for all functionality
- Use structured logging with appropriate log levels
- Follow the repository pattern for data access
- Implement proper error handling and validation
- Use dependency injection for testability

### Testing

```bash
# Run all tests
make test

# Run tests with coverage
make test-coverage

# Run specific package tests
go test ./internal/services/...

# Run tests with verbose output
go test -v ./...
```

## Architecture

### System Overview

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Flutter App    │◄───►│   Go Backend    │◄───►│  ESP32 Device   │
│  (Mobile/Web)   │REST │    Server       │MQTT │  (Fish Feeder)  │
└─────────────────┘     └─────────────────┘     └─────────────────┘
                               │
                    ┌──────────┴──────────┐
                    │                     │
              ┌─────▼─────┐        ┌──────▼──────┐
              │PostgreSQL │        │    Redis    │
              │ Database  │        │    Cache    │
              └───────────┘        └─────────────┘
```

### Hardware Communication

The ESP32 fish feeder devices communicate with the backend using **MQTT** (Message Queuing Telemetry Transport), a lightweight IoT messaging protocol.

**Why MQTT?**
- Low bandwidth (ideal for cellular/GSM connections)
- Bi-directional communication
- Offline message queuing
- Real-time data delivery

**Data Flow:**
1. **Device → Backend**: Sensor data, feeding events, alerts
2. **Backend → Device**: Commands, configuration updates, schedules

For detailed information, see the [MQTT Communication Guide](docs/MQTT_COMMUNICATION.md).

### Clean Architecture

The project follows clean architecture principles:

- **Handlers**: HTTP request/response handling
- **Services**: Business logic and use cases
- **Repository**: Data access abstraction
- **Models**: Data structures and DTOs

### Database Schema

The application uses PostgreSQL with GORM for ORM. Key entities:

- **Users**: System users with authentication
- **Devices**: Arduino fish feeder devices
- **DeviceBinding**: Temporary device binding codes
- **FeedingSchedule**: Automated feeding schedules
- **FeedingEvent**: Historical feeding records
- **SensorData**: Sensor readings from devices
- **FishSpecies**: Fish species feeding parameters

### Concurrency

Go's goroutines are used for:
- Concurrent request handling
- Background data processing
- Real-time sensor data ingestion
- Scheduled task execution

## Deployment

### Railway Deployment (Recommended)

The easiest way to deploy is using Railway:

1. Push your code to GitHub
2. Create a new project on [railway.app](https://railway.app)
3. Add PostgreSQL and Redis from templates
4. Set environment variables (see [Railway Deployment Guide](docs/RAILWAY_DEPLOYMENT.md))
5. Deploy!

[![Deploy on Railway](https://railway.app/button.svg)](https://railway.app/template/smart-fish-feeder)

For detailed instructions, see [docs/RAILWAY_DEPLOYMENT.md](docs/RAILWAY_DEPLOYMENT.md).

### Production Deployment

1. **Build Docker image**:
   ```bash
   make docker-build
   ```

2. **Deploy with Docker Compose**:
   ```bash
   docker-compose -f docker-compose.prod.yml up -d
   ```

3. **Or deploy to Kubernetes** (configuration to be added)

### Environment Variables for Production

Ensure these environment variables are set in production:
- `SFF_DATABASE_HOST` - Production database host
- `SFF_DATABASE_PASSWORD` - Secure database password
- `SFF_REDIS_HOST` - Production Redis host
- `SFF_JWT_SECRET_KEY` - Strong JWT secret key
- `SFF_SERVER_DEBUG=false` - Disable debug mode

## Monitoring and Observability

### Health Checks

- `/health` - Basic application health
- `/health/detailed` - Component health (database, Redis)

### Logging

Structured JSON logging with fields:
- Request ID for tracing
- User ID for audit trails
- Device ID for IoT operations
- Performance metrics

### Metrics

- Request duration and count
- Database query performance
- Redis operation metrics
- Device connectivity status

## Security

### Authentication & Authorization
- JWT-based authentication
- Secure password hashing with bcrypt
- Token refresh mechanism
- Device-specific authentication

### API Security
- Input validation with go-playground/validator
- SQL injection prevention with GORM
- CORS configuration
- Rate limiting (to be implemented)

### Device Security
- Device binding with temporary codes
- Device ownership verification
- Secure device-to-backend communication
- Hardware serial validation

## Contributing

1. Follow the existing code style and patterns
2. Write tests for new functionality
3. Update documentation for changes
4. Ensure all tests pass before submitting
5. Use meaningful commit messages

## License

This project is part of the Smart Fish Feeder System. See the main project for license information.

## Support

For issues and questions:
1. Check the existing documentation
2. Review the API endpoints and examples
3. Check the logs for error details
4. Create an issue with detailed information