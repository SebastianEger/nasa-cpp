#include <tcp_client.h>
#include <nasa_protocol.h>

using namespace nasacpp;

int main(int argc, char **argv)
{
    TcpClient tcp_client("192.168.178.25", 26);
    NasaProtocol nasa_protocol;

    Address sa;
    sa.a_class = AddressClass::WiredRemote;
    sa.channel = 0x00;
    sa.address = 0x00;

    Address da;
    da.a_class = AddressClass::BroadcastSelfLayer;
    da.channel = 0xFF;
    da.address = 0x20;

    auto packet_dhw_on = Packet::create(sa, da, DataType::Request, (MessageNumber)0x4065, 1);

    tcp_client.send(packet_dhw_on.encode());
    getchar();
    return 0;
}