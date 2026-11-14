#include "nasa_bridge_runner.h"
#include "nasa_protocol.h"

namespace nasacpp
{
    namespace
    {
        int64_t nowMs()
        {
            using namespace std::chrono;
            return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        }
    } // namespace

    BridgeOptions BridgeOptions::fromConfig(const json &cfg)
    {
        BridgeOptions options;
        options.timeout = cfg.value("timeout", options.timeout);
        options.buffer_limit = cfg.value("buffer_limit", options.buffer_limit);
        options.read_only = cfg.value("read_only", options.read_only);
        return options;
    }

    BridgeRunner::BridgeRunner(ByteTransport &transport, NasaHassBridge &bridge, const BridgeOptions &options)
        : transport_(transport), bridge_(bridge), options_(options)
    {
        tp_last_data_ = std::chrono::steady_clock::now();
        last_data_ms_ = nowMs();
    }

    int64_t BridgeRunner::msSinceLastData() const
    {
        return nowMs() - last_data_ms_;
    }

    void BridgeRunner::step()
    {
        if (buffer_.size() > options_.buffer_limit)
        {
            logger_->warn("Buffer limit reached, clear buffer");
            buffer_.clear();
        }

        // get received data and put it into the buffer
        int n_new_bytes = transport_.read(buffer_);
        if (n_new_bytes > 0)
        {
            logger_->debug("[RECV] {} bytes", n_new_bytes);
            tp_last_data_ = std::chrono::steady_clock::now();
            last_data_ms_ = nowMs();
        }

        // decode packets and publish to mqtt
        auto pkts_rx = NasaProtocol::decode(buffer_);
        size_t pkts_foreign = 0;
        if (!pkts_rx.empty())
        {
            logger_->debug("[DECODE] {} NASA packets", pkts_rx.size());
            packets_rx_ += pkts_rx.size();
            const uint32_t own_address = bridge_.address().to_uint32_t();
            for (auto &pkt_rx : pkts_rx)
            {
                // ignore our own packets, e.g. echoed by an RS485 transceiver: they are no confirmation
                if (pkt_rx.sa.to_uint32_t() == own_address)
                {
                    continue;
                }
                ++pkts_foreign;
                for (auto &msg : pkt_rx.messages)
                {
                    bridge_.publishToHass(msg);
                }
            }
        }

        // only send right after packets were received: the bus is alive and it is our turn
        if (!options_.read_only && pkts_foreign > 0)
        {
            std::vector<Packet> pkts_tx;
            bridge_.getNasaPackets(pkts_tx);
            if (!pkts_tx.empty())
            {
                logger_->info("[SEND] {} NASA packets ...", pkts_tx.size());
                transport_.write(NasaProtocol::encode(pkts_tx));
                packets_tx_ += pkts_tx.size();
            }
        }

        // check connection timeout
        auto since_last = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - tp_last_data_).count();
        if (since_last > options_.timeout && transport_.connected())
        {
            logger_->warn("No data received since {} seconds, reconnecting transport!", options_.timeout);
            transport_.reconnect();
            tp_last_data_ = std::chrono::steady_clock::now();
        }
    }

    void BridgeRunner::run(std::chrono::milliseconds period)
    {
        while (true)
        {
            step();
            std::this_thread::sleep_for(period);
        }
    }
} // namespace nasacpp
