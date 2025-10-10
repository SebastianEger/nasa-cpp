#pragma once

#include "nasa_protocol_types.hpp"

namespace nasacpp
{
    class NasaMsgHandler
    {
    public:
        static bool disable_write_send;
        static bool initial_read_out;

    public:
        NasaMsgHandler(const MessageNumber &message_number, const Address &sa, const DataType &data_type = DataType::Read);

        void getPacket(std::vector<Packet> &pkts);
        void updateValue(const MessageSet &msg);
        void updateValueNew(const int &val);
        const DataType &getDataType() const;
        void resetRetryCounter();
        
        bool hasVal();
        int getVal();

    protected:
        std::atomic<int> val_is_ = INT_MAX;
        std::atomic<int> val_new_ = INT_MAX;

        const MessageNumber message_number_;
        const Address sa_;
        const DataType data_type_;
        const MessageSetType msg_set_type_;

        std::atomic<int> retry_ctr_ = 0;
    };
} // namespace nasacpp
