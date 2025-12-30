# Railway Deployment Guide

This guide walks you through deploying the Smart Fish Feeder backend to Railway.

## Prerequisites

- GitHub account with your code pushed
- Railway account (sign up at [railway.app](https://railway.app))

## Services Overview

You'll deploy 3 services on Railway:

| Service | Type | Purpose |
|---------|------|---------|
| **Backend API** | Docker | Go REST API server |
| **PostgreSQL** | Template | Database |
| **Redis** | Template | Cache & sessions |

**Note:** For MQTT, you'll use an external service (HiveMQ Cloud free tier or self-hosted).

## Step-by-Step Deployment

### Step 1: Create Railway Project

1. Go to [railway.app](https://railway.app) and sign in
2. Click **"New Project"**
3. Select **"Deploy from GitHub repo"**
4. Connect your GitHub account and select your repository
5. Select the `backend` folder as the root directory

### Step 2: Add PostgreSQL Database

1. In your project, click **"+ New"**
2. Select **"Database"** → **"Add PostgreSQL"**
3. Railway automatically provisions the database
4. The `DATABASE_URL` variable is automatically added

### Step 3: Add Redis Cache

1. Click **"+ New"** again
2. Select **"Database"** → **"Add Redis"**
3. Railway provisions Redis
4. The `REDIS_URL` variable is automatically added

### Step 4: Configure Environment Variables

Click on your backend service, go to **"Variables"** tab, and add:

```bash
# Required - Generate a strong secret key
SFF_JWT_SECRET_KEY=your-super-secret-key-minimum-32-characters-long

# Server settings (Railway sets PORT automatically)
SFF_SERVER_DEBUG=false
SFF_LOGGING_LEVEL=info
SFF_LOGGING_FORMAT=json

# MQTT Broker (use HiveMQ Cloud or your own)
SFF_MQTT_BROKER_URL=tcp://your-mqtt-broker.hivemq.cloud:1883
SFF_MQTT_USERNAME=your-mqtt-username
SFF_MQTT_PASSWORD=your-mqtt-password
SFF_MQTT_TLS_ENABLED=true
```

**Note:** `DATABASE_URL`, `REDIS_URL`, and `PORT` are automatically set by Railway.

### Step 5: Deploy

1. Railway automatically deploys when you push to GitHub
2. Or click **"Deploy"** to trigger manually
3. Watch the build logs for any errors
4. Once deployed, click **"Generate Domain"** to get your public URL

### Step 6: Verify Deployment

Test your deployment:

```bash
# Health check
curl https://your-app.railway.app/health

# Detailed health (shows database/redis status)
curl https://your-app.railway.app/health/detailed
```

Expected response:
```json
{
  "status": "healthy",
  "timestamp": "2024-01-15T10:30:00Z",
  "version": "1.0.0"
}
```

## Environment Variables Reference

### Automatically Set by Railway

| Variable | Description |
|----------|-------------|
| `PORT` | HTTP port (Railway assigns this) |
| `DATABASE_URL` | PostgreSQL connection string |
| `REDIS_URL` | Redis connection string |
| `RAILWAY_ENVIRONMENT` | "production" or "staging" |

### Required Variables (You Must Set)

| Variable | Example | Description |
|----------|---------|-------------|
| `SFF_JWT_SECRET_KEY` | `a1b2c3d4...` | JWT signing key (min 32 chars) |

### Optional Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `SFF_SERVER_DEBUG` | `false` | Enable debug mode |
| `SFF_LOGGING_LEVEL` | `info` | Log level (debug/info/warn/error) |
| `SFF_MQTT_BROKER_URL` | - | MQTT broker URL |
| `SFF_MQTT_USERNAME` | - | MQTT username |
| `SFF_MQTT_PASSWORD` | - | MQTT password |

## Setting Up MQTT (HiveMQ Cloud)

Since Railway doesn't have a native MQTT service, use HiveMQ Cloud (free tier):

1. Go to [hivemq.com/cloud](https://www.hivemq.com/mqtt-cloud-broker/)
2. Sign up for free tier (100 connections, 10GB/month)
3. Create a cluster
4. Create credentials (username/password)
5. Add to Railway environment variables:

```bash
SFF_MQTT_BROKER_URL=ssl://your-cluster.hivemq.cloud:8883
SFF_MQTT_USERNAME=your-username
SFF_MQTT_PASSWORD=your-password
SFF_MQTT_TLS_ENABLED=true
```

## Project Structure on Railway

```
┌─────────────────────────────────────────────────────┐
│                 Railway Project                      │
├─────────────────────────────────────────────────────┤
│                                                      │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────┐ │
│  │   Backend    │  │  PostgreSQL  │  │   Redis   │ │
│  │   (Docker)   │  │  (Template)  │  │ (Template)│ │
│  │              │  │              │  │           │ │
│  │ Port: $PORT  │  │ Port: 5432   │  │ Port:6379 │ │
│  └──────┬───────┘  └──────┬───────┘  └─────┬─────┘ │
│         │                 │                │       │
│         └────────┬────────┴────────────────┘       │
│                  │                                  │
│           Internal Network                          │
│                                                     │
└─────────────────────────────────────────────────────┘
                    │
                    ▼
            Public Domain
      https://your-app.railway.app
```

## Monitoring & Logs

### View Logs

1. Click on your backend service
2. Go to **"Deployments"** tab
3. Click on a deployment to see logs

### Metrics

Railway provides basic metrics:
- CPU usage
- Memory usage
- Network traffic

### Health Checks

The app has built-in health endpoints:
- `/health` - Basic health check
- `/health/detailed` - Database and Redis status

## Scaling

### Vertical Scaling

Upgrade your Railway plan for more resources:
- **Hobby**: $5/month, 512MB RAM
- **Pro**: $20/month, 8GB RAM, multiple instances

### Horizontal Scaling

On Pro plan, you can run multiple instances:
1. Go to service settings
2. Set **"Replicas"** to desired count
3. Railway load balances automatically

## Troubleshooting

### Build Fails

Check the build logs for errors:
- Missing dependencies: Ensure `go.mod` is up to date
- Dockerfile issues: Test locally with `docker build .`

### Database Connection Errors

1. Verify `DATABASE_URL` is set
2. Check PostgreSQL service is running
3. Look for SSL errors (Railway requires SSL)

### Redis Connection Errors

1. Verify `REDIS_URL` is set
2. Check Redis service is running

### App Crashes on Start

1. Check logs for panic messages
2. Verify all required env vars are set
3. Test locally with same env vars

## Cost Estimation

| Service | Hobby Plan | Pro Plan |
|---------|------------|----------|
| Backend | $5/month | $20/month |
| PostgreSQL | $5/month | $15/month |
| Redis | $5/month | $10/month |
| **Total** | **~$15/month** | **~$45/month** |

*Prices may vary based on usage*

## CI/CD Pipeline

Railway automatically deploys on push to your default branch. For more control:

### Branch Deployments

1. Go to project settings
2. Enable **"PR Deployments"**
3. Each PR gets a preview environment

### Manual Deployments

```bash
# Install Railway CLI
npm install -g @railway/cli

# Login
railway login

# Deploy
railway up
```

## Security Checklist

Before going to production:

- [ ] Set strong `SFF_JWT_SECRET_KEY` (32+ characters)
- [ ] Set `SFF_SERVER_DEBUG=false`
- [ ] Enable MQTT TLS (`SFF_MQTT_TLS_ENABLED=true`)
- [ ] Review database access (Railway restricts by default)
- [ ] Set up monitoring alerts
- [ ] Enable Railway's DDoS protection

## Next Steps

1. Set up custom domain (Railway settings → Domains)
2. Configure monitoring/alerting
3. Set up staging environment
4. Implement CI/CD tests before deploy
