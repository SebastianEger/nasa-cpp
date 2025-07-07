#include "nasa_msg_handler.h"

#include <spdlog/spdlog.h>

namespace nasacpp
{
    bool NasaMsgHandler::disable_write_send = false;
    bool NasaMsgHandler::initial_read_out = true;

    NasaMsgHandler::NasaMsgHandler(const MessageNumber &message_number, const Address &sa, const DataType &data_type)
        : message_number_(message_number), sa_(sa), data_type_(data_type), msg_set_type_(getMessageSetType(message_number)) {}

    void NasaMsgHandler::getPackets(std::vector<Packet> &pkts)
    {
        // send READ packet if value is not initialized yet
        if (val_is_ == INT_MAX && initial_read_out)
        {
            pkts.push_back(Packet::createToBroadcastSelf(sa_, DataType::Read, message_number_, 1));
            return;
        }

        // no need to send packet
        if (val_new_ == INT_MAX || val_is_ == val_new_ || data_type_ == DataType::Read)
        {
            return;
        }

        // check if send packet is disabled by user config
        if (data_type_ == DataType::Write && disable_write_send)
        {
            spdlog::warn("NasaMsgHandler: [SEND] disabled for [WRITE]!");
            val_new_.store(val_is_);
            resetRetryCounter();
            return;
        }

        // check if val_new is set and retry counter not reached
        if (retry_ctr_ > 10)
        {
            spdlog::warn("NasaMsgHandler: [SEND] retry counter reached!");
            val_new_.store(val_is_);
            resetRetryCounter();
            return;
        }

        // create packet
        auto pkt = Packet::createToBroadcastSelf(sa_, data_type_, message_number_, val_new_);
        pkt.command.retry_count = retry_ctr_++;
        pkts.push_back(pkt);
    }

    void NasaMsgHandler::updateValue(const MessageSet &msg)
    {
        // check message number
        if (msg.message_number != message_number_)
        {
            spdlog::error("NasaMsgHandler: [UPDATE] Message numbers do not match!");
            return;
        }

        // check msg type
        switch (msg_set_type_)
        {
        case Enum:
            val_is_ = msg.value;
            break;
        case Variable:
            val_is_ = (short)msg.value;
            break;
        case LongVariable:
            val_is_ = (int)msg.value;
            break;
        case Structure:
            break;
        default:
            val_is_ = msg.value;
            break;
        }

        // reset value new
        if (val_is_ == val_new_)
        {
            val_new_ = INT_MAX;
        }
    }

    void NasaMsgHandler::updateValueNew(const int &val)
    {
        val_new_ = val;
    }

    const DataType &NasaMsgHandler::getDataType() const
    {
        return data_type_;
    }

    void NasaMsgHandler::resetRetryCounter()
    {
        retry_ctr_ = 0;
    }

    bool NasaMsgHandler::hasVal()
    {
        return val_is_ != INT_MAX;
    }

    int NasaMsgHandler::getVal()
    {
        return val_is_;
    }
} // namespace nasacpp
