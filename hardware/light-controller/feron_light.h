#include "esphome.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_base/nec_protocol.h"

using namespace esphome;
using namespace esphome::light;
using namespace esphome::remote_base;
using namespace esphome::switch_;

#define FERON_ADDRESS_DIM_BH  0xB721
#define FERON_ADDRESS_SET_BH  0xB781
#define FERON_ADDRESS_NATIVE  0xB7A0
#define FERON_POWER_OFF       0xEC

constexpr float kelvin_to_mireds(unsigned k) { return 1e6 / k; }

class FeronLightOutput : public LightOutput {
 public:
  LightTraits get_traits() override {
    auto traits = LightTraits();
    traits.set_supported_color_modes({ColorMode::COLOR_TEMPERATURE});
    traits.set_min_mireds(kelvin_to_mireds(6500)); // 153
    traits.set_max_mireds(kelvin_to_mireds(2700)); // 371
    return traits;
  }
  static void transmit(RemoteTransmitterBase *remote, NECData data) {
    data.command |= ~data.command * 256;
    auto xmit = remote->transmit();
    NECProtocol().encode(xmit.get_data(), data);
    xmit.perform();
  }
  void write_state(LightState *state) override {
    float color, brightness;
    uint16_t address = FERON_ADDRESS_NATIVE, command = FERON_POWER_OFF;
    state->current_values_as_ct(&color, &brightness);
    if (brightness > 0.0f) {
      address = this->fade_on() ? FERON_ADDRESS_DIM_BH : FERON_ADDRESS_SET_BH;
      command = static_cast<uint16_t>(0.5f + brightness * 15.0f) * 16 + static_cast<uint16_t>(15.5f - color * 15.0f);
    }
    ESP_LOGD("feron_light", "Set color: %f, brightness: %f. NEC command: 0x%02X", color, brightness, command);
    transmit(this->remote_, {address, command});
  }
  static void preset_load(RemoteTransmitterBase *remote, int preset, bool force = false) {
    preset &= 15;
    ESP_LOGD("feron_light", "Loading state from EEPROM preset: %d", preset);
    transmit(remote, {FERON_ADDRESS_NATIVE, static_cast<uint16_t>(preset + force * 16)});
  }
  static void preset_save(RemoteTransmitterBase *remote, int preset) {
    preset &= 15;
    ESP_LOGD("feron_light", "Saving current state to EEPROM preset: %d", preset);
    transmit(remote, {FERON_ADDRESS_NATIVE, static_cast<uint16_t>(preset + 32)});
  }
  void set_remote_transmitter(RemoteTransmitterBase *remote) { this->remote_ = remote; }
  void set_fade_switch(Switch *sw) { this->fade_ = sw; }
  bool fade_on() const { return (this->fade_ == nullptr) || this->fade_->state; }

 protected:
  RemoteTransmitterBase *remote_{};
  Switch *fade_{};
};
