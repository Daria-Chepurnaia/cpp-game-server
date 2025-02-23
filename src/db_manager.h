#pragma once 

#include <memory>
#include <mutex>
#include <cstdlib>
#include <pqxx/pqxx>
#include <pqxx/zview.hxx>
#include <condition_variable>
#include "data_structures.h"
#include <vector>

constexpr const char DB_URL_ENV_NAME[]{"GAME_DB_URL"};

struct AppConfig {
    std::string db_url;
};

AppConfig GetConfigFromEnv();

class ConnectionPool {
    using PoolType = ConnectionPool;
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

public:
    class ConnectionWrapper {
    public:
        ConnectionWrapper(std::shared_ptr<pqxx::connection>&& conn, PoolType& pool) noexcept
            : conn_{std::move(conn)}
            , pool_{&pool} {
        }

        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;

        ConnectionWrapper(ConnectionWrapper&&) = default;
        ConnectionWrapper& operator=(ConnectionWrapper&&) = default;

        pqxx::connection& operator*() const& noexcept {
            return *conn_;
        }
        pqxx::connection& operator*() const&& = delete;

        pqxx::connection* operator->() const& noexcept {
            return conn_.get();
        }

        ~ConnectionWrapper() {
            if (conn_) {
                pool_->ReturnConnection(std::move(conn_));
            }
        }

    private:
        std::shared_ptr<pqxx::connection> conn_;
        PoolType* pool_;
    };

    // ConnectionFactory is a functional object returning std::shared_ptr<pqxx::connection>
    template <typename ConnectionFactory>
    ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory) {
        pool_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            pool_.emplace_back(connection_factory());
        }
    }

    ConnectionWrapper GetConnection() {
        std::unique_lock lock{mutex_};
        cond_var_.wait(lock, [this] {
            return used_connections_ < pool_.size();
        });
        return {std::move(pool_[used_connections_++]), *this};
    }

private:
    void ReturnConnection(ConnectionPtr&& conn) {
        {
            std::lock_guard lock{mutex_};
            assert(used_connections_ != 0);
            pool_[--used_connections_] = std::move(conn);
        }
        cond_var_.notify_one();
    }

    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::vector<ConnectionPtr> pool_;
    size_t used_connections_ = 0;
};

namespace postgres {

class ConnectionFactory {
public:
    ConnectionFactory(const AppConfig& config) : config_(config){}
    std::shared_ptr<pqxx::connection> operator()() {
        return std::make_shared<pqxx::connection>(config_.db_url);
    }
private:
    AppConfig config_;
};

class DBManager {
public:
    explicit DBManager(ConnectionPool& pool);
    void SavePlayer(const std::string& name, double total_time, int score);
    std::vector<RetiredPlayerInfo> GetPlayers(std::optional<int> start_element, std::optional<int> maxItems);
private:
    ConnectionPool& pool_;
};
}// namespace postgres