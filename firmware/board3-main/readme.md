# Board 3. Main

The main board keeps track of all internal states, it receives messages from other boards (e.g. buttons) or polls (~200ms) boards (e.g. RTC). This board also is equipped with 64x256 display where in the normal mode it only shows 3 items: level of fresh water, level of fuel, charge of the battery (acid 12V battery, measured by voltage). This board controls navigational lights (bow, stern, steaming, anchoring, tricolor).

### Communications

RB1 SDA
RB2 SCL

Other
RA7 CONFIG_MODE (changes the main board mode into configuration disabling normal functionality)

## Navigational lights

The board also shows the state of each navigational light. This state corresponds to the state of the corresponding channel on the switching board.
The lights are connected in the followign order:

- Anchoring
- Tricolor
- Steaming
- Bow
- Stern

The lights operate in following modes:

- Off
- Anchoring. Combination of anchoring or steaming+stern.
- Steaming. Combination of stern+bow+steaming or bow+anchoring.
- Running. Combintation of stern+bow or just tricolor.

In the config mode any light can be disabled. The system then should find a way to implement the required light pattern using available lights. If not possible it should flash red to indicate error.

The leds used LTST-E563C.

### Pinout

RB5 LED (series of 5)

## Buttons

Communication with buttons is done throught I2C specified in protocol.md.

Left button panel has buttons connected in following order:

- Power. Toggles the functionality of the whole pannel. When off none of the relays are activated.
- Instruments. Controls instruments relay.
- Autopilot. Controls autopilot relay.
- Steaming lights. Enables Steaming mode of navigational lights. When mode is already active it switches the mode to Off. Contols some of the navigational lights relays.
- Runnning lights. Enables Running mode of navigational lights. When mode is already active it switches the mode to Off. Contols some of the navigational lights relays.
- Anchoring lights. Enables Anchoring mode of navigational lights. When mode is already active it switches the mode to Off. Contols some of the navigational lights relays.
- Inverter. Controls inverter relay.

Left button panel has buttons connected in following order:

- Fresh water pump.
- Fridge.
- Deck lights.
- Cabin lights.
- USB outlets.
- Aux 1.
- Aux 2.

The button once pressed should be put into pulsating mode, then trigger corresponding channel on the switching board. Once the channel status on the switching board is set the button effect should be changed to enabled.
If the button state is not changed after 100ms the channel state should be changed to error, which should be displayed as red flashing light. Toggling the button again should clear error state and turn the channel off.

## Relays

Communication with relays is done throught I2C specified in protocol.md.

## Display

The display is SSD1322 OLED monochrome display 64x256 operating in 8080 mode. nCS connected permanently to GND, EnRD pulled up.

The graphics library u8g2, the parts of the library that are used need to be copied to the source folder.

### Pinout

RB0 DnC
RB3 nRST
RB4 RnW
RC0 D0
RC1 D1
RC2 D2
RC3 D3
RC4 D4
RC5 D5
RC6 D6
RC7 D7

