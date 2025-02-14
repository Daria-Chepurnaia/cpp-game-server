
# Game Server - C++ Backend

This is the backend server for a multiplayer game where players control a dog that collects lost items on the road and delivers them to a lost and found office. The goal is to collect more items than the competitors to earn more points.

## Features

- **Player Authentication:** Players are able to log in and join a game.
- **Collision Detection:** Algorithms ensure players and objects interact correctly.
- **Asynchronous Operations & Multithreading:** The server handles multiple players efficiently
- **State Persistence:** The server periodically saves its state to disk, ensuring that it can recover in case of a restart or failure.
- **Leaderboard:** Players' results are stored in a PostgreSQL database and displayed at the end of each game.
- **Loot Generation:** Dynamic item generation on the map during gameplay.
- **Logging:** Comprehensive logging of game activities and errors.
- **Database Integration:** Player scores and game results are stored in PostgreSQL.

## Game Description

In the game, players control a dog, navigating through a map to collect lost items and return them to the lost and found office. Multiple players can be in the game at the same time, and each player can only control their own dog while being able to observe the actions of other players. Players earn points for delivered items, and the objective is to have the most points at the end of the game.

The game ends if a dog remains stationary for too long, at which point a leaderboard of the best players is displayed.

## Setup

A Dockerfile is provided for building and running the game server in a container. The Docker setup uses two stages:

1. **Build stage:** Compiles the server and installs dependencies using Conan and CMake.
2. **Run stage:** Deploys the server in an Ubuntu-based container, with necessary assets and configurations.

To build and run the Docker container:
```
docker build -t game-server .
docker run -d -p 8080:8080 game-server
```

## Key Technologies

- **C++20:** Core game logic and server management are written in C++20, ensuring high performance and modern language features.
- **Boost:** Utilized for various utilities, including threading, filesystem, and JSON handling.
- **PostgreSQL:** For storing player scores and results in a persistent database.
- **Conan:** Used for managing dependencies, ensuring consistent and reproducible builds.
- **Docker:** Simplifies deployment and setup with a multi-stage Dockerfile.

## Architecture Overview

The server consists of several components:

- **GameModel:** The core game logic, including loot generation and collision detection.
- **Logger:** Provides comprehensive logging functionality for debugging and monitoring.
- **Database Manager:** Manages communication with the PostgreSQL database for storing results.
- **HTTP Server:** Provides API endpoints for client-server communication.
- **Player Manager:** Handles player authentication and interactions during gameplay.
- **State Recovery:** Periodically saves the server's state to ensure persistence.

## License

MIT License. See [LICENSE](https://github.com/Daria-Chepurnaia/cpp-game-server/blob/main/LICENSE) for more details.

