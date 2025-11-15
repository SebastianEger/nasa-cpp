#include <mqtt_client.h>

int main(int argc, char **argv)
{
    nasacpp::MqttClient mqtt_client("localhost", 1883);
    mqtt_client.subscribe("nasacpp/test");
    mqtt_client.subscribe("nasacpp/topic");
    while (true)
    {
        mqtt_client.publish("nasacpp/test", "Hello world");
        mqtt_client.publish("nasacpp/topic", "Hello germany");

        spdlog::info("Recv msg: {0}", mqtt_client.getData().size());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}