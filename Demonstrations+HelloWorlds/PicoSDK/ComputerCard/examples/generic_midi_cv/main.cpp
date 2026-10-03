// Generic MIDI CV
// Music Thing Modular Workshop Computer
//
// V1:
// USB MIDI host receives CC messages.
// CC20-23 are captured as four controller values.
// CV output will be added after MIDI reception is verified.

#include "ComputerCard.h"

#include "pico/multicore.h"
#include "bsp/board.h"
#include "tusb.h"
#include "usb_midi_host.h"


class GenericMidiCV : public ComputerCard
{
public:
    GenericMidiCV()
    {
        midi_dev_addr = 0;
        device_connected = 0;
        midi_activity = 0;

        for (int i = 0; i < 4; i++)
            cc_value[i] = 0;
    }

    void StartUSBCore()
    {
        instance = this;
        multicore_launch_core1(core1);
    }

    static void core1()
    {
        instance->USBCore();
    }

    void USBCore()
    {
        board_init();
        tusb_init();

        while (1)
        {
            tuh_task();
        }
    }

    // Called at the Computer's audio sample rate.
    virtual void ProcessSample()
    {
        // Bottom-left LED: USB MIDI device connected.
        LedOn(4, device_connected);

        // Bottom-right LED: flashes whenever one of our four CCs is received.
        LedOn(5, midi_activity > 0);

        if (midi_activity > 0)
            midi_activity--;
    }

    static volatile uint8_t device_connected;
    static volatile uint8_t midi_dev_addr;

    // Current MIDI values for our four outputs.
    static volatile uint8_t cc_value[4];

    // Countdown used to flash the MIDI activity LED.
    static volatile uint32_t midi_activity;

private:
    static GenericMidiCV *instance;
};


volatile uint8_t GenericMidiCV::device_connected = 0;
volatile uint8_t GenericMidiCV::midi_dev_addr = 0;
volatile uint8_t GenericMidiCV::cc_value[4] = {0, 0, 0, 0};
volatile uint32_t GenericMidiCV::midi_activity = 0;

GenericMidiCV *GenericMidiCV::instance = nullptr;


// Called when a USB MIDI device is connected.
void tuh_midi_mount_cb(
    uint8_t dev_addr,
    uint8_t in_ep,
    uint8_t out_ep,
    uint8_t num_cables_rx,
    uint16_t num_cables_tx)
{
    (void)in_ep;
    (void)out_ep;
    (void)num_cables_rx;
    (void)num_cables_tx;

    if (GenericMidiCV::midi_dev_addr == 0)
    {
        GenericMidiCV::midi_dev_addr = dev_addr;
        GenericMidiCV::device_connected = 1;
    }
}


// Called when a USB MIDI device is disconnected.
void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    (void)instance;

    if (dev_addr == GenericMidiCV::midi_dev_addr)
    {
        GenericMidiCV::midi_dev_addr = 0;
        GenericMidiCV::device_connected = 0;
    }
}


// Called when MIDI data arrives.
void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets)
{
    if (GenericMidiCV::midi_dev_addr != dev_addr || num_packets == 0)
        return;

    uint8_t cable_num;
    uint8_t buffer[48];

    while (1)
    {
        uint32_t bytes_read =
            tuh_midi_stream_read(dev_addr, &cable_num, buffer, sizeof(buffer));

        if (bytes_read == 0)
            return;

        // MIDI Channel Voice messages are three bytes:
        // status, data1, data2.
        for (uint32_t idx = 0; idx + 2 < bytes_read; idx += 3)
        {
            uint8_t status = buffer[idx];
            uint8_t data1  = buffer[idx + 1];
            uint8_t data2  = buffer[idx + 2];

            // Upper nibble 0xB = Control Change.
            // This accepts CC messages on any MIDI channel.
            if ((status & 0xF0) == 0xB0)
            {
                uint8_t cc = data1;
                uint8_t value = data2 & 0x7F;

                // CC20 -> output 1
                // CC21 -> output 2
                // CC22 -> output 3
                // CC23 -> output 4
                if (cc >= 20 && cc <= 23)
                {
                    GenericMidiCV::cc_value[cc - 20] = value;

                    // About 1/20 second at 48 kHz.
                    GenericMidiCV::midi_activity = 2400;
                }
            }
        }
    }
}


void tuh_midi_tx_cb(uint8_t dev_addr)
{
    (void)dev_addr;
}


int main()
{
    set_sys_clock_khz(144000, true);

    GenericMidiCV card;
    card.StartUSBCore();
    card.Run();
}