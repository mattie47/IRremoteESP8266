// Copyright 2019 David Conran
// Mitsubishi Heavy Industries A/C remote emulation.

// Code to emulate Mitsubishi Heavy Industries A/C IR remote control units,
// which should control at least the following A/C units:
//   Remote Control RLA502A700B:
//     Model SRKxxZM-S
//     Model SRKxxZMXA-S
//   Remote Control RKX502A001C:
//     Model SRKxxZJ-S

// Note: This code was *heavily* influenced by @ToniA's great work & code,
//       but it has been written from scratch.
//       Nothing was copied other than constants and message analysis.

#include "ir_MitsubishiHeavy.h"
#include <algorithm>
#include "IRremoteESP8266.h"
#include "IRutils.h"
#ifndef ARDUINO
#include <string>
#endif

// Ref:
//   https://github.com/crankyoldgit/IRremoteESP8266/issues/660
//   https://github.com/ToniA/Raw-IR-decoder-for-Arduino/blob/master/MitsubishiHeavy.cpp
//   https://github.com/ToniA/arduino-heatpumpir/blob/master/MitsubishiHeavyHeatpumpIR.cpp

// Constants
const uint16_t kMitsubishiHeavyHdrMark = 3140;
const uint16_t kMitsubishiHeavyHdrSpace = 1630;
const uint16_t kMitsubishiHeavyBitMark = 370;
const uint16_t kMitsubishiHeavyOneSpace = 420;
const uint16_t kMitsubishiHeavyZeroSpace = 1220;
const uint32_t kMitsubishiHeavyGap = kDefaultMessageGap;  // Just a guess.

#if SEND_MITSUBISHIHEAVY
// Send a MitsubishiHeavy 88 bit A/C message.
//
// Args:
//   data:   Contents of the message to be sent.
//   nbits:  Nr. of bits of data to be sent. Typically kMitsubishiHeavy88Bits.
//   repeat: Nr. of additional times the message is to be sent.
//
// Status: BETA / Appears to be working. Needs testing against a real device.
void IRsend::sendMitsubishiHeavy88(const unsigned char data[],
                                   const uint16_t nbytes,
                                   const uint16_t repeat) {
  if (nbytes < kMitsubishiHeavy88StateLength)
    return;  // Not enough bytes to send a proper message.
  sendGeneric(kMitsubishiHeavyHdrMark, kMitsubishiHeavyHdrSpace,
              kMitsubishiHeavyBitMark, kMitsubishiHeavyOneSpace,
              kMitsubishiHeavyBitMark, kMitsubishiHeavyZeroSpace,
              kMitsubishiHeavyBitMark, kMitsubishiHeavyGap,
              data, nbytes, 38000, false, repeat, kDutyDefault);
}

// Send a MitsubishiHeavy 152 bit A/C message.
//
// Args:
//   data:   Contents of the message to be sent.
//   nbits:  Nr. of bits of data to be sent. Typically kMitsubishiHeavy152Bits.
//   repeat: Nr. of additional times the message is to be sent.
//
// Status: BETA / Appears to be working. Needs testing against a real device.
void IRsend::sendMitsubishiHeavy152(const unsigned char data[],
                                    const uint16_t nbytes,
                                    const uint16_t repeat) {
  if (nbytes < kMitsubishiHeavy152StateLength)
    return;  // Not enough bytes to send a proper message.
  sendMitsubishiHeavy88(data, nbytes, repeat);
}
#endif  // SEND_MITSUBISHIHEAVY

// Class for decoding and constructing MitsubishiHeavy152 AC messages.
IRMitsubishiHeavy152Ac::IRMitsubishiHeavy152Ac(
    const uint16_t pin) : _irsend(pin) { stateReset(); }

void IRMitsubishiHeavy152Ac::begin(void) { _irsend.begin(); }

#if SEND_MITSUBISHIHEAVY
void IRMitsubishiHeavy152Ac::send(const uint16_t repeat) {
  _irsend.sendMitsubishiHeavy152(this->getRaw(), kMitsubishiHeavy152StateLength,
                                 repeat);
}
#endif  // SEND_MITSUBISHIHEAVY

void IRMitsubishiHeavy152Ac::stateReset(void) {
  uint8_t i = 0;
  for (; i < kMitsubishiHeavySigLength; i++)
    remote_state[i] = kMitsubishiHeavyZmsSig[i];
  for (; i < kMitsubishiHeavy152StateLength - 3; i += 2) remote_state[i] = 0;
  remote_state[17] = 0x80;
}

uint8_t *IRMitsubishiHeavy152Ac::getRaw(void) {
  checksum();
  return remote_state;
}

void IRMitsubishiHeavy152Ac::setRaw(const uint8_t *data) {
  for (uint8_t i = 0; i < kMitsubishiHeavy152StateLength; i++)
    remote_state[i] = data[i];
}

void IRMitsubishiHeavy152Ac::on(void) {
  remote_state[5] |= kMitsubishiHeavyPowerBit;
}

void IRMitsubishiHeavy152Ac::off(void) {
  remote_state[5] &= ~kMitsubishiHeavyPowerBit;
}

void IRMitsubishiHeavy152Ac::setPower(const bool on) {
  if (on)
    this->on();
  else
    this->off();
}

bool IRMitsubishiHeavy152Ac::getPower(void) {
  return remote_state[5] & kMitsubishiHeavyPowerBit;
}

void IRMitsubishiHeavy152Ac::setTemp(const uint8_t temp) {
  uint8_t newtemp = temp;
  newtemp = std::min(newtemp, kMitsubishiHeavyMaxTemp);
  newtemp = std::max(newtemp, kMitsubishiHeavyMinTemp);

  remote_state[7] &= ~kMitsubishiHeavyTempMask;
  remote_state[7] |= newtemp - kMitsubishiHeavyMinTemp;
}

uint8_t IRMitsubishiHeavy152Ac::getTemp(void) {
  return (remote_state[7] & kMitsubishiHeavyTempMask) + kMitsubishiHeavyMinTemp;
}

// Set the speed of the fan
void IRMitsubishiHeavy152Ac::setFan(const uint8_t speed) {
  uint8_t newspeed = speed;
  switch (speed) {
    case kMitsubishiHeavy152FanLow:
    case kMitsubishiHeavy152FanMed:
    case kMitsubishiHeavy152FanHigh:
    case kMitsubishiHeavy152FanMax:
    case kMitsubishiHeavy152FanEcono:
    case kMitsubishiHeavy152FanTurbo:
      break;
    default:
      newspeed = kMitsubishiHeavy152FanAuto;
  }
  remote_state[9] &= ~kMitsubishiHeavyFanMask;
  remote_state[9] |= newspeed;
}

uint8_t IRMitsubishiHeavy152Ac::getFan(void) {
  return remote_state[9] & kMitsubishiHeavyFanMask;
}

void IRMitsubishiHeavy152Ac::setMode(const uint8_t mode) {
  uint8_t newmode = mode;
  switch (mode) {
    case kMitsubishiHeavyCool:
    case kMitsubishiHeavyDry:
    case kMitsubishiHeavyFan:
    case kMitsubishiHeavyHeat:
      break;
    default:
      newmode = kMitsubishiHeavyAuto;
  }
  remote_state[5] &= ~kMitsubishiHeavyModeMask;
  remote_state[5] |= newmode;
}

uint8_t IRMitsubishiHeavy152Ac::getMode(void) {
  return remote_state[5] & kMitsubishiHeavyModeMask;
}

void IRMitsubishiHeavy152Ac::setSwingVertical(const uint8_t pos) {
  uint8_t newpos = std::min(pos, kMitsubishiHeavy152SwingVOff);
  remote_state[11] &= ~kMitsubishiHeavy152SwingVMask;
  remote_state[11] |= (newpos << 5);
}

uint8_t IRMitsubishiHeavy152Ac::getSwingVertical(void) {
  return remote_state[11] >> 5;
}

void IRMitsubishiHeavy152Ac::setSwingHorizontal(const uint8_t pos) {
  uint8_t newpos = std::min(pos, kMitsubishiHeavy152SwingHOff);
  remote_state[13] &= ~kMitsubishiHeavy152SwingHMask;
  remote_state[13] |= (newpos & kMitsubishiHeavy152SwingHMask);
}

uint8_t IRMitsubishiHeavy152Ac::getSwingHorizontal(void) {
  return remote_state[13] & kMitsubishiHeavy152SwingHMask;
}

void IRMitsubishiHeavy152Ac::setNight(const bool on) {
  if (on)
    remote_state[15] |= kMitsubishiHeavyNightBit;
  else
    remote_state[15] &= ~kMitsubishiHeavyNightBit;
}

bool IRMitsubishiHeavy152Ac::getNight(void) {
  return remote_state[15] & kMitsubishiHeavyNightBit;
}

void IRMitsubishiHeavy152Ac::set3D(const bool on) {
  if (on)
    remote_state[11] |= kMitsubishiHeavy3DMask;
  else
    remote_state[11] &= ~kMitsubishiHeavy3DMask;
}

bool IRMitsubishiHeavy152Ac::get3D(void) {
  return (remote_state[11] & kMitsubishiHeavy3DMask) == kMitsubishiHeavy3DMask;
}

void IRMitsubishiHeavy152Ac::setSilent(const bool on) {
  if (on)
    remote_state[15] |= kMitsubishiHeavySilentBit;
  else
    remote_state[15] &= ~kMitsubishiHeavySilentBit;
}

bool IRMitsubishiHeavy152Ac::getSilent(void) {
  return remote_state[15] & kMitsubishiHeavySilentBit;
}

void IRMitsubishiHeavy152Ac::setFilter(const bool on) {
  if (on)
    remote_state[5] |= kMitsubishiHeavyFilterBit;
  else
    remote_state[5] &= ~kMitsubishiHeavyFilterBit;
}

bool IRMitsubishiHeavy152Ac::getFilter(void) {
  return remote_state[5] & kMitsubishiHeavyFilterBit;
}

void IRMitsubishiHeavy152Ac::setClean(const bool on) {
  this->setFilter(on);
  if (on)
    remote_state[5] |= kMitsubishiHeavyCleanBit;
  else
    remote_state[5] &= ~kMitsubishiHeavyCleanBit;
}

bool IRMitsubishiHeavy152Ac::getClean(void) {
  return remote_state[5] & kMitsubishiHeavyCleanBit && this->getFilter();
}

void IRMitsubishiHeavy152Ac::setTurbo(const bool on) {
  if (on)
    this->setFan(kMitsubishiHeavy152FanTurbo);
  else if (this->getTurbo()) this->setFan(kMitsubishiHeavy152FanAuto);
}

bool IRMitsubishiHeavy152Ac::getTurbo(void) {
  return this->getFan() == kMitsubishiHeavy152FanTurbo;
}

void IRMitsubishiHeavy152Ac::setEcono(const bool on) {
  if (on)
    this->setFan(kMitsubishiHeavy152FanEcono);
  else if (this->getEcono()) this->setFan(kMitsubishiHeavy152FanAuto);
}

bool IRMitsubishiHeavy152Ac::getEcono(void) {
  return this->getFan() == kMitsubishiHeavy152FanEcono;
}

// Verify the given state has a ZM-S signature.
bool IRMitsubishiHeavy152Ac::checkZmsSig(const uint8_t *state) {
  for (uint8_t i = 0; i < kMitsubishiHeavySigLength; i++)
    if (state[i] != kMitsubishiHeavyZmsSig[i]) return false;
  return true;
}

// Protocol technically has no checksum, but does has inverted byte pairs.
void IRMitsubishiHeavy152Ac::checksum(void) {
  for (uint8_t i = kMitsubishiHeavySigLength - 2;
       i < kMitsubishiHeavy152StateLength;
       i += 2) {
    remote_state[i + 1] = ~remote_state[i];
  }
}

// Protocol technically has no checksum, but does has inverted byte pairs.
bool IRMitsubishiHeavy152Ac::validChecksum(const uint8_t *state,
                                           const uint16_t length) {
  // Assume anything too short is fine.
  if (length < kMitsubishiHeavySigLength) return true;
  // Check all the byte pairs.
  for (uint16_t i = kMitsubishiHeavySigLength - 2;
       i < length;
       i += 2) {
    // XOR of a byte and it's self inverted should be 0xFF;
    if ((state[i] ^ state[i + 1]) != 0xFF) return false;
  }
  return true;
}

// Convert a standard A/C mode into its native mode.
uint8_t IRMitsubishiHeavy152Ac::convertMode(const stdAc::opmode_t mode) {
  switch (mode) {
    case stdAc::opmode_t::kCool:
      return kMitsubishiHeavyCool;
    case stdAc::opmode_t::kHeat:
      return kMitsubishiHeavyHeat;
    case stdAc::opmode_t::kDry:
      return kMitsubishiHeavyDry;
    case stdAc::opmode_t::kFan:
      return kMitsubishiHeavyFan;
    default:
      return kMitsubishiHeavyAuto;
  }
}

// Convert a standard A/C Fan speed into its native fan speed.
uint8_t IRMitsubishiHeavy152Ac::convertFan(const stdAc::fanspeed_t speed) {
  switch (speed) {
    case stdAc::fanspeed_t::kMin:
      return kMitsubishiHeavy152FanEcono;  // Assumes Econo is slower than Low.
    case stdAc::fanspeed_t::kLow:
      return kMitsubishiHeavy152FanLow;
    case stdAc::fanspeed_t::kMedium:
      return kMitsubishiHeavy152FanMed;
    case stdAc::fanspeed_t::kHigh:
      return kMitsubishiHeavy152FanHigh;
    case stdAc::fanspeed_t::kMax:
      return kMitsubishiHeavy152FanMax;
    default:
      return kMitsubishiHeavy152FanAuto;
  }
}

// Convert a standard A/C vertical swing into its native setting.
uint8_t IRMitsubishiHeavy152Ac::convertSwingV(const stdAc::swingv_t position) {
  switch (position) {
    case stdAc::swingv_t::kAuto:
      return kMitsubishiHeavy152SwingVAuto;
    case stdAc::swingv_t::kHighest:
      return kMitsubishiHeavy152SwingVHighest;
    case stdAc::swingv_t::kHigh:
      return kMitsubishiHeavy152SwingVHigh;
    case stdAc::swingv_t::kMiddle:
      return kMitsubishiHeavy152SwingVMiddle;
    case stdAc::swingv_t::kLow:
      return kMitsubishiHeavy152SwingVLow;
    case stdAc::swingv_t::kLowest:
      return kMitsubishiHeavy152SwingVLowest;
    default:
      return kMitsubishiHeavy152SwingVOff;
  }
}

// Convert a standard A/C horizontal swing into its native setting.
uint8_t IRMitsubishiHeavy152Ac::convertSwingH(const stdAc::swingh_t position) {
  switch (position) {
    case stdAc::swingh_t::kAuto:
      return kMitsubishiHeavy152SwingHAuto;
    case stdAc::swingh_t::kLeftMax:
      return kMitsubishiHeavy152SwingHLeftMax;
    case stdAc::swingh_t::kLeft:
      return kMitsubishiHeavy152SwingHLeft;
    case stdAc::swingh_t::kMiddle:
      return kMitsubishiHeavy152SwingHMiddle;
    case stdAc::swingh_t::kRight:
    return kMitsubishiHeavy152SwingHRight;
    case stdAc::swingh_t::kRightMax:
      return kMitsubishiHeavy152SwingHRightMax;
    default:
      return kMitsubishiHeavy152SwingHOff;
  }
}

// Convert a native mode to it's common equivalent.
stdAc::opmode_t IRMitsubishiHeavy152Ac::toCommonMode(const uint8_t mode) {
  switch (mode) {
    case kMitsubishiHeavyCool: return stdAc::opmode_t::kCool;
    case kMitsubishiHeavyHeat: return stdAc::opmode_t::kHeat;
    case kMitsubishiHeavyDry: return stdAc::opmode_t::kDry;
    case kMitsubishiHeavyFan: return stdAc::opmode_t::kFan;
    default: return stdAc::opmode_t::kAuto;
  }
}

// Convert a native fan speed to it's common equivalent.
stdAc::fanspeed_t IRMitsubishiHeavy152Ac::toCommonFanSpeed(const uint8_t spd) {
  switch (spd) {
    case kMitsubishiHeavy152FanMax: return stdAc::fanspeed_t::kMax;
    case kMitsubishiHeavy152FanHigh: return stdAc::fanspeed_t::kHigh;
    case kMitsubishiHeavy152FanMed: return stdAc::fanspeed_t::kMedium;
    case kMitsubishiHeavy152FanLow: return stdAc::fanspeed_t::kLow;
    case kMitsubishiHeavy152FanEcono: return stdAc::fanspeed_t::kMin;
    default: return stdAc::fanspeed_t::kAuto;
  }
}

// Convert a native vertical swing to it's common equivalent.
stdAc::swingh_t IRMitsubishiHeavy152Ac::toCommonSwingH(const uint8_t pos) {
  switch (pos) {
    case kMitsubishiHeavy152SwingHLeftMax: return stdAc::swingh_t::kLeftMax;
    case kMitsubishiHeavy152SwingHLeft: return stdAc::swingh_t::kLeft;
    case kMitsubishiHeavy152SwingHMiddle: return stdAc::swingh_t::kMiddle;
    case kMitsubishiHeavy152SwingHRight: return stdAc::swingh_t::kRight;
    case kMitsubishiHeavy152SwingHRightMax: return stdAc::swingh_t::kRightMax;
    case kMitsubishiHeavy152SwingHOff: return stdAc::swingh_t::kOff;
    default: return stdAc::swingh_t::kAuto;
  }
}

// Convert a native vertical swing to it's common equivalent.
stdAc::swingv_t IRMitsubishiHeavy152Ac::toCommonSwingV(const uint8_t pos) {
  switch (pos) {
    case kMitsubishiHeavy152SwingVHighest: return stdAc::swingv_t::kHighest;
    case kMitsubishiHeavy152SwingVHigh: return stdAc::swingv_t::kHigh;
    case kMitsubishiHeavy152SwingVMiddle: return stdAc::swingv_t::kMiddle;
    case kMitsubishiHeavy152SwingVLow: return stdAc::swingv_t::kLow;
    case kMitsubishiHeavy152SwingVLowest: return stdAc::swingv_t::kLowest;
    case kMitsubishiHeavy152SwingVOff: return stdAc::swingv_t::kOff;
    default: return stdAc::swingv_t::kAuto;
  }
}

// Convert the A/C state to it's common equivalent.
stdAc::state_t IRMitsubishiHeavy152Ac::toCommon(void) {
  stdAc::state_t result;
  result.protocol = decode_type_t::MITSUBISHI_HEAVY_152;
  result.model = -1;  // No models used.
  result.power = this->getPower();
  result.mode = this->toCommonMode(this->getMode());
  result.celsius = true;
  result.degrees = this->getTemp();
  result.fanspeed = this->toCommonFanSpeed(this->getFan());
  result.swingv = this->toCommonSwingV(this->getSwingVertical());
  result.swingh = this->toCommonSwingH(this->getSwingHorizontal());
  result.turbo = this->getTurbo();
  result.econo = this->getEcono();
  result.clean = this->getClean();
  result.quiet = this->getSilent();
  result.filter = this->getFilter();
  result.sleep = this->getNight() ? 0 : -1;
  // Not supported.
  result.light = false;
  result.beep = false;
  result.clock = -1;
  return result;
}

// Convert the internal state into a human readable string.
String IRMitsubishiHeavy152Ac::toString(void) {
  String result = "";
  result.reserve(180);  // Reserve some heap for the string to reduce fragging.
  result += IRutils::acBoolToString(getPower(), F("Power"), false);
  result += IRutils::acModeToString(getMode(), kMitsubishiHeavyAuto,
                                    kMitsubishiHeavyCool, kMitsubishiHeavyHeat,
                                    kMitsubishiHeavyDry, kMitsubishiHeavyFan);
  result += F(", Temp: ");
  result += uint64ToString(this->getTemp()) + 'C';
  result += F(", Fan: ");
  result += uint64ToString(this->getFan());
  switch (this->getFan()) {
    case kMitsubishiHeavy152FanAuto:
      result += F(" (Auto)");
      break;
    case kMitsubishiHeavy152FanHigh:
      result += F(" (High)");
      break;
    case kMitsubishiHeavy152FanLow:
      result += F(" (Low)");
      break;
    case kMitsubishiHeavy152FanMed:
      result += F(" (Med)");
      break;
    case kMitsubishiHeavy152FanMax:
      result += F(" (Max)");
      break;
    case kMitsubishiHeavy152FanEcono:
      result += F(" (Econo)");
      break;
    case kMitsubishiHeavy152FanTurbo:
      result += F(" (Turbo)");
      break;
    default:
      result += F(" (UNKNOWN)");
  }
  result += F(", Swing (V): ");
  result += uint64ToString(this->getSwingVertical());
  switch (this->getSwingVertical()) {
    case kMitsubishiHeavy152SwingVAuto:
      result += F(" (Auto)");
      break;
    case kMitsubishiHeavy152SwingVHighest:
      result += F(" (Highest)");
      break;
    case kMitsubishiHeavy152SwingVHigh:
      result += F(" (High)");
      break;
    case kMitsubishiHeavy152SwingVMiddle:
      result += F(" (Middle)");
      break;
    case kMitsubishiHeavy152SwingVLow:
      result += F(" (Low)");
      break;
    case kMitsubishiHeavy152SwingVLowest:
      result += F(" (Lowest)");
      break;
    case kMitsubishiHeavy152SwingVOff:
      result += F(" (Off)");
      break;
    default:
      result += F(" (UNKNOWN)");
  }
  result += F(", Swing (H): ");
  result += uint64ToString(this->getSwingHorizontal());
  switch (this->getSwingHorizontal()) {
    case kMitsubishiHeavy152SwingHAuto:
      result += F(" (Auto)");
      break;
    case kMitsubishiHeavy152SwingHLeftMax:
      result += F(" (Max Left)");
      break;
    case kMitsubishiHeavy152SwingHLeft:
      result += F(" (Left)");
      break;
    case kMitsubishiHeavy152SwingHMiddle:
      result += F(" (Middle)");
      break;
    case kMitsubishiHeavy152SwingHRight:
      result += F(" (Right)");
      break;
    case kMitsubishiHeavy152SwingHRightMax:
      result += F(" (Max Right)");
      break;
    case kMitsubishiHeavy152SwingHLeftRight:
      result += F(" (Left Right)");
      break;
    case kMitsubishiHeavy152SwingHRightLeft:
      result += F(" (Right Left)");
      break;
    case kMitsubishiHeavy152SwingHOff:
      result += F(" (Off)");
      break;
    default:
      result += F(" (UNKNOWN)");
  }
  result += IRutils::acBoolToString(getSilent(), F("Silent"));
  result += IRutils::acBoolToString(getTurbo(), F("Turbo"));
  result += IRutils::acBoolToString(getEcono(), F("Econo"));
  result += IRutils::acBoolToString(getNight(), F("Night"));
  result += IRutils::acBoolToString(getFilter(), F("Filter"));
  result += IRutils::acBoolToString(get3D(), F("3D"));
  result += IRutils::acBoolToString(getClean(), F("Clean"));
  return result;
}


// Class for decoding and constructing MitsubishiHeavy88 AC messages.
IRMitsubishiHeavy88Ac::IRMitsubishiHeavy88Ac(
    const uint16_t pin) : _irsend(pin) { stateReset(); }

void IRMitsubishiHeavy88Ac::begin(void) { _irsend.begin(); }

#if SEND_MITSUBISHIHEAVY
void IRMitsubishiHeavy88Ac::send(const uint16_t repeat) {
  _irsend.sendMitsubishiHeavy88(this->getRaw(), kMitsubishiHeavy88StateLength,
                                repeat);
}
#endif  // SEND_MITSUBISHIHEAVY

void IRMitsubishiHeavy88Ac::stateReset(void) {
  uint8_t i = 0;
  for (; i < kMitsubishiHeavySigLength; i++)
    remote_state[i] = kMitsubishiHeavyZjsSig[i];
  for (; i < kMitsubishiHeavy88StateLength; i++) remote_state[i] = 0;
}

uint8_t *IRMitsubishiHeavy88Ac::getRaw(void) {
  checksum();
  return remote_state;
}

void IRMitsubishiHeavy88Ac::setRaw(const uint8_t *data) {
  for (uint8_t i = 0; i < kMitsubishiHeavy88StateLength; i++)
    remote_state[i] = data[i];
}

void IRMitsubishiHeavy88Ac::on(void) {
  remote_state[9] |= kMitsubishiHeavyPowerBit;
}

void IRMitsubishiHeavy88Ac::off(void) {
  remote_state[9] &= ~kMitsubishiHeavyPowerBit;
}

void IRMitsubishiHeavy88Ac::setPower(const bool on) {
  if (on)
    this->on();
  else
    this->off();
}

bool IRMitsubishiHeavy88Ac::getPower(void) {
  return remote_state[9] & kMitsubishiHeavyPowerBit;
}

void IRMitsubishiHeavy88Ac::setTemp(const uint8_t temp) {
  uint8_t newtemp = temp;
  newtemp = std::min(newtemp, kMitsubishiHeavyMaxTemp);
  newtemp = std::max(newtemp, kMitsubishiHeavyMinTemp);

  remote_state[9] &= kMitsubishiHeavyTempMask;
  remote_state[9] |= ((newtemp - kMitsubishiHeavyMinTemp) << 4);
}

uint8_t IRMitsubishiHeavy88Ac::getTemp(void) {
  return (remote_state[9] >> 4) + kMitsubishiHeavyMinTemp;
}

// Set the speed of the fan
void IRMitsubishiHeavy88Ac::setFan(const uint8_t speed) {
  uint8_t newspeed = speed;
  switch (speed) {
    case kMitsubishiHeavy88FanLow:
    case kMitsubishiHeavy88FanMed:
    case kMitsubishiHeavy88FanHigh:
    case kMitsubishiHeavy88FanTurbo:
    case kMitsubishiHeavy88FanEcono:
      break;
    default:
      newspeed = kMitsubishiHeavy88FanAuto;
  }
  remote_state[7] &= ~kMitsubishiHeavy88FanMask;
  remote_state[7] |= (newspeed << 5);
}

uint8_t IRMitsubishiHeavy88Ac::getFan(void) {
  return remote_state[7] >> 5;
}

void IRMitsubishiHeavy88Ac::setMode(const uint8_t mode) {
  uint8_t newmode = mode;
  switch (mode) {
    case kMitsubishiHeavyCool:
    case kMitsubishiHeavyDry:
    case kMitsubishiHeavyFan:
    case kMitsubishiHeavyHeat:
      break;
    default:
      newmode = kMitsubishiHeavyAuto;
  }
  remote_state[9] &= ~kMitsubishiHeavyModeMask;
  remote_state[9] |= newmode;
}

uint8_t IRMitsubishiHeavy88Ac::getMode(void) {
  return remote_state[9] & kMitsubishiHeavyModeMask;
}

void IRMitsubishiHeavy88Ac::setSwingVertical(const uint8_t pos) {
  uint8_t newpos;
  switch (pos) {
    case kMitsubishiHeavy88SwingVAuto:
    case kMitsubishiHeavy88SwingVHighest:
    case kMitsubishiHeavy88SwingVHigh:
    case kMitsubishiHeavy88SwingVMiddle:
    case kMitsubishiHeavy88SwingVLow:
    case kMitsubishiHeavy88SwingVLowest:
      newpos = pos;
      break;
    default:
      newpos = kMitsubishiHeavy88SwingVOff;
  }
  remote_state[5] &= ~kMitsubishiHeavy88SwingVMaskByte5;
  remote_state[5] |= (newpos & kMitsubishiHeavy88SwingVMaskByte5);
  remote_state[7] &= ~kMitsubishiHeavy88SwingVMaskByte7;
  remote_state[7] |= (newpos & kMitsubishiHeavy88SwingVMaskByte7);
}

uint8_t IRMitsubishiHeavy88Ac::getSwingVertical(void) {
  return (remote_state[5] & kMitsubishiHeavy88SwingVMaskByte5) |
         (remote_state[7] & kMitsubishiHeavy88SwingVMaskByte7);
}

void IRMitsubishiHeavy88Ac::setSwingHorizontal(const uint8_t pos) {
  uint8_t newpos;
  switch (pos) {
    case kMitsubishiHeavy88SwingHAuto:
    case kMitsubishiHeavy88SwingHLeftMax:
    case kMitsubishiHeavy88SwingHLeft:
    case kMitsubishiHeavy88SwingHMiddle:
    case kMitsubishiHeavy88SwingHRight:
    case kMitsubishiHeavy88SwingHRightMax:
    case kMitsubishiHeavy88SwingHLeftRight:
    case kMitsubishiHeavy88SwingHRightLeft:
    case kMitsubishiHeavy88SwingH3D:
      newpos = pos;
      break;
    default:
      newpos = kMitsubishiHeavy88SwingHOff;
  }
  remote_state[5] &= ~kMitsubishiHeavy88SwingHMask;
  remote_state[5] |= newpos;
}

uint8_t IRMitsubishiHeavy88Ac::getSwingHorizontal(void) {
  return remote_state[5] & kMitsubishiHeavy88SwingHMask;
}

void IRMitsubishiHeavy88Ac::setTurbo(const bool on) {
  if (on)
    this->setFan(kMitsubishiHeavy88FanTurbo);
  else if (this->getTurbo()) this->setFan(kMitsubishiHeavy88FanAuto);
}

bool IRMitsubishiHeavy88Ac::getTurbo(void) {
  return this->getFan() == kMitsubishiHeavy88FanTurbo;
}

void IRMitsubishiHeavy88Ac::setEcono(const bool on) {
  if (on)
    this->setFan(kMitsubishiHeavy88FanEcono);
  else if (this->getEcono()) this->setFan(kMitsubishiHeavy88FanAuto);
}

bool IRMitsubishiHeavy88Ac::getEcono(void) {
  return this->getFan() == kMitsubishiHeavy88FanEcono;
}

void IRMitsubishiHeavy88Ac::set3D(const bool on) {
  if (on)
    this->setSwingHorizontal(kMitsubishiHeavy88SwingH3D);
  else if (this->get3D())
    this->setSwingHorizontal(kMitsubishiHeavy88SwingHOff);
}

bool IRMitsubishiHeavy88Ac::get3D(void) {
  return this->getSwingHorizontal() == kMitsubishiHeavy88SwingH3D;
}

void IRMitsubishiHeavy88Ac::setClean(const bool on) {
  if (on)
    remote_state[5] |= kMitsubishiHeavy88CleanBit;
  else
    remote_state[5] &= ~kMitsubishiHeavy88CleanBit;
}

bool IRMitsubishiHeavy88Ac::getClean(void) {
  return remote_state[5] & kMitsubishiHeavy88CleanBit;
}

// Verify the given state has a ZJ-S signature.
bool IRMitsubishiHeavy88Ac::checkZjsSig(const uint8_t *state) {
  for (uint8_t i = 0; i < kMitsubishiHeavySigLength; i++)
    if (state[i] != kMitsubishiHeavyZjsSig[i]) return false;
  return true;
}

// Protocol technically has no checksum, but does has inverted byte pairs.
void IRMitsubishiHeavy88Ac::checksum(void) {
  for (uint8_t i = kMitsubishiHeavySigLength - 2;
       i < kMitsubishiHeavy88StateLength;
       i += 2) {
    remote_state[i + 1] = ~remote_state[i];
  }
}

// Protocol technically has no checksum, but does has inverted byte pairs.
bool IRMitsubishiHeavy88Ac::validChecksum(const uint8_t *state,
                                           const uint16_t length) {
  return IRMitsubishiHeavy152Ac::validChecksum(state, length);
}

// Convert a standard A/C mode into its native mode.
uint8_t IRMitsubishiHeavy88Ac::convertMode(const stdAc::opmode_t mode) {
  return IRMitsubishiHeavy152Ac::convertMode(mode);
}

// Convert a standard A/C Fan speed into its native fan speed.
uint8_t IRMitsubishiHeavy88Ac::convertFan(const stdAc::fanspeed_t speed) {
  switch (speed) {
    case stdAc::fanspeed_t::kMin:
      return kMitsubishiHeavy88FanEcono;  // Assumes Econo is slower than Low.
    case stdAc::fanspeed_t::kLow:
      return kMitsubishiHeavy88FanLow;
    case stdAc::fanspeed_t::kMedium:
      return kMitsubishiHeavy88FanMed;
    case stdAc::fanspeed_t::kHigh:
      return kMitsubishiHeavy88FanHigh;
    case stdAc::fanspeed_t::kMax:
      return kMitsubishiHeavy88FanTurbo;
    default:
      return kMitsubishiHeavy88FanAuto;
  }
}

// Convert a standard A/C vertical swing into its native setting.
uint8_t IRMitsubishiHeavy88Ac::convertSwingV(const stdAc::swingv_t position) {
  switch (position) {
    case stdAc::swingv_t::kAuto:
      return kMitsubishiHeavy88SwingVAuto;
    case stdAc::swingv_t::kHighest:
      return kMitsubishiHeavy88SwingVHighest;
    case stdAc::swingv_t::kHigh:
      return kMitsubishiHeavy88SwingVHigh;
    case stdAc::swingv_t::kMiddle:
      return kMitsubishiHeavy88SwingVMiddle;
    case stdAc::swingv_t::kLow:
      return kMitsubishiHeavy88SwingVLow;
    case stdAc::swingv_t::kLowest:
      return kMitsubishiHeavy88SwingVLowest;
    default:
      return kMitsubishiHeavy88SwingVOff;
  }
}

// Convert a standard A/C horizontal swing into its native setting.
uint8_t IRMitsubishiHeavy88Ac::convertSwingH(const stdAc::swingh_t position) {
  switch (position) {
    case stdAc::swingh_t::kAuto:
      return kMitsubishiHeavy88SwingHAuto;
    case stdAc::swingh_t::kLeftMax:
      return kMitsubishiHeavy88SwingHLeftMax;
    case stdAc::swingh_t::kLeft:
      return kMitsubishiHeavy88SwingHLeft;
    case stdAc::swingh_t::kMiddle:
      return kMitsubishiHeavy88SwingHMiddle;
    case stdAc::swingh_t::kRight:
    return kMitsubishiHeavy88SwingHRight;
    case stdAc::swingh_t::kRightMax:
      return kMitsubishiHeavy88SwingHRightMax;
    default:
      return kMitsubishiHeavy88SwingHOff;
  }
}

// Convert a native fan speed to it's common equivalent.
stdAc::fanspeed_t IRMitsubishiHeavy88Ac::toCommonFanSpeed(const uint8_t speed) {
  switch (speed) {
    case kMitsubishiHeavy88FanTurbo: return stdAc::fanspeed_t::kMax;
    case kMitsubishiHeavy88FanHigh: return stdAc::fanspeed_t::kHigh;
    case kMitsubishiHeavy88FanMed: return stdAc::fanspeed_t::kMedium;
    case kMitsubishiHeavy88FanLow: return stdAc::fanspeed_t::kLow;
    case kMitsubishiHeavy88FanEcono: return stdAc::fanspeed_t::kMin;
    default: return stdAc::fanspeed_t::kAuto;
  }
}

// Convert a native vertical swing to it's common equivalent.
stdAc::swingh_t IRMitsubishiHeavy88Ac::toCommonSwingH(const uint8_t pos) {
  switch (pos) {
    case kMitsubishiHeavy88SwingHLeftMax: return stdAc::swingh_t::kLeftMax;
    case kMitsubishiHeavy88SwingHLeft: return stdAc::swingh_t::kLeft;
    case kMitsubishiHeavy88SwingHMiddle: return stdAc::swingh_t::kMiddle;
    case kMitsubishiHeavy88SwingHRight: return stdAc::swingh_t::kRight;
    case kMitsubishiHeavy88SwingHRightMax: return stdAc::swingh_t::kRightMax;
    case kMitsubishiHeavy88SwingHOff: return stdAc::swingh_t::kOff;
    default: return stdAc::swingh_t::kAuto;
  }
}

// Convert a native vertical swing to it's common equivalent.
stdAc::swingv_t IRMitsubishiHeavy88Ac::toCommonSwingV(const uint8_t pos) {
  switch (pos) {
    case kMitsubishiHeavy88SwingVHighest: return stdAc::swingv_t::kHighest;
    case kMitsubishiHeavy88SwingVHigh: return stdAc::swingv_t::kHigh;
    case kMitsubishiHeavy88SwingVMiddle: return stdAc::swingv_t::kMiddle;
    case kMitsubishiHeavy88SwingVLow: return stdAc::swingv_t::kLow;
    case kMitsubishiHeavy88SwingVLowest: return stdAc::swingv_t::kLowest;
    case kMitsubishiHeavy88SwingVOff: return stdAc::swingv_t::kOff;
    default: return stdAc::swingv_t::kAuto;
  }
}

// Convert the A/C state to it's common equivalent.
stdAc::state_t IRMitsubishiHeavy88Ac::toCommon(void) {
  stdAc::state_t result;
  result.protocol = decode_type_t::MITSUBISHI_HEAVY_88;
  result.model = -1;  // No models used.
  result.power = this->getPower();
  result.mode = IRMitsubishiHeavy152Ac::toCommonMode(this->getMode());
  result.celsius = true;
  result.degrees = this->getTemp();
  result.fanspeed = this->toCommonFanSpeed(this->getFan());
  result.swingv = this->toCommonSwingV(this->getSwingVertical());
  result.swingh = this->toCommonSwingH(this->getSwingHorizontal());
  result.turbo = this->getTurbo();
  result.econo = this->getEcono();
  result.clean = this->getClean();
  // Not supported.
  result.quiet = false;
  result.filter = false;
  result.light = false;
  result.beep = false;
  result.sleep = -1;
  result.clock = -1;
  return result;
}

// Convert the internal state into a human readable string.
String IRMitsubishiHeavy88Ac::toString(void) {
  String result = "";
  result.reserve(140);  // Reserve some heap for the string to reduce fragging.
  result += IRutils::acBoolToString(getPower(), F("Power"), false);
  result += IRutils::acModeToString(getMode(), kMitsubishiHeavyAuto,
                                    kMitsubishiHeavyCool, kMitsubishiHeavyHeat,
                                    kMitsubishiHeavyDry, kMitsubishiHeavyFan);
  result += F(", Temp: ");
  result += uint64ToString(this->getTemp()) + 'C';
  result += F(", Fan: ");
  result += uint64ToString(this->getFan());
  switch (this->getFan()) {
    case kMitsubishiHeavy88FanAuto:
      result += F(" (Auto)");
      break;
    case kMitsubishiHeavy88FanHigh:
      result += F(" (High)");
      break;
    case kMitsubishiHeavy88FanLow:
      result += F(" (Low)");
      break;
    case kMitsubishiHeavy88FanMed:
      result += F(" (Med)");
      break;
    case kMitsubishiHeavy88FanEcono:
      result += F(" (Econo)");
      break;
    case kMitsubishiHeavy88FanTurbo:
      result += F(" (Turbo)");
      break;
    default:
      result += F(" (UNKNOWN)");
  }
  result += F(", Swing (V): ");
  result += uint64ToString(this->getSwingVertical());
  switch (this->getSwingVertical()) {
    case kMitsubishiHeavy88SwingVAuto:
      result += F(" (Auto)");
      break;
    case kMitsubishiHeavy88SwingVHighest:
      result += F(" (Highest)");
      break;
    case kMitsubishiHeavy88SwingVHigh:
      result += F(" (High)");
      break;
    case kMitsubishiHeavy88SwingVMiddle:
      result += F(" (Middle)");
      break;
    case kMitsubishiHeavy88SwingVLow:
      result += F(" (Low)");
      break;
    case kMitsubishiHeavy88SwingVLowest:
      result += F(" (Lowest)");
      break;
    case kMitsubishiHeavy88SwingVOff:
      result += F(" (Off)");
      break;
    default:
      result += F(" (UNKNOWN)");
  }
  result += F(", Swing (H): ");
  result += uint64ToString(this->getSwingHorizontal());
  switch (this->getSwingHorizontal()) {
    case kMitsubishiHeavy88SwingHAuto:
      result += F(" (Auto)");
      break;
    case kMitsubishiHeavy88SwingHLeftMax:
      result += F(" (Max Left)");
      break;
    case kMitsubishiHeavy88SwingHLeft:
      result += F(" (Left)");
      break;
    case kMitsubishiHeavy88SwingHMiddle:
      result += F(" (Middle)");
      break;
    case kMitsubishiHeavy88SwingHRight:
      result += F(" (Right)");
      break;
    case kMitsubishiHeavy88SwingHRightMax:
      result += F(" (Max Right)");
      break;
    case kMitsubishiHeavy88SwingHLeftRight:
      result += F(" (Left Right)");
      break;
    case kMitsubishiHeavy88SwingHRightLeft:
      result += F(" (Right Left)");
      break;
    case kMitsubishiHeavy88SwingH3D:
      result += F(" (3D)");
      break;
    case kMitsubishiHeavy88SwingHOff:
      result += F(" (Off)");
      break;
    default:
      result += F(" (UNKNOWN)");
  }
  result += IRutils::acBoolToString(getTurbo(), F("Turbo"));
  result += IRutils::acBoolToString(getEcono(), F("Econo"));
  result += IRutils::acBoolToString(get3D(), F("3D"));
  result += IRutils::acBoolToString(getClean(), F("Clean"));
  return result;
}

#if DECODE_MITSUBISHIHEAVY
// Decode the supplied MitsubishiHeavy message.
//
// Args:
//   results: Ptr to the data to decode and where to store the decode result.
//   nbits:   The number of data bits to expect.
//            Typically kMitsubishiHeavy88Bits or kMitsubishiHeavy152Bits.
//   strict:  Flag indicating if we should perform strict matching.
// Returns:
//   boolean: True if it can decode it, false if it can't.
//
// Status: BETA / Appears to be working. Needs testing against a real device.
bool IRrecv::decodeMitsubishiHeavy(decode_results* results,
                                   const uint16_t nbits, const bool strict) {
  // Check if can possibly be a valid MitsubishiHeavy message.
  if (results->rawlen < 2 * nbits + kHeader + kFooter - 1) return false;
  if (strict) {
    switch (nbits) {
      case kMitsubishiHeavy88Bits:
      case kMitsubishiHeavy152Bits:
        break;
      default:
        return false;  // Not what is expected
    }
  }

  uint16_t offset = kStartOffset;
  uint16_t used;
  used = matchGeneric(results->rawbuf + offset, results->state,
                      results->rawlen - offset, nbits,
                      kMitsubishiHeavyHdrMark, kMitsubishiHeavyHdrSpace,
                      kMitsubishiHeavyBitMark, kMitsubishiHeavyOneSpace,
                      kMitsubishiHeavyBitMark, kMitsubishiHeavyZeroSpace,
                      kMitsubishiHeavyBitMark, kMitsubishiHeavyGap, true,
                      kTolerance, 0, false);
  if (used == 0) return false;
  offset += used;
  // Compliance
  switch (nbits) {
    case kMitsubishiHeavy88Bits:
      if (strict && !(IRMitsubishiHeavy88Ac::checkZjsSig(results->state) &&
                      IRMitsubishiHeavy88Ac::validChecksum(results->state)))
        return false;
      results->decode_type = MITSUBISHI_HEAVY_88;
      break;
    case kMitsubishiHeavy152Bits:
      if (strict && !(IRMitsubishiHeavy152Ac::checkZmsSig(results->state) &&
                      IRMitsubishiHeavy152Ac::validChecksum(results->state)))
        return false;
      results->decode_type = MITSUBISHI_HEAVY_152;
      break;
    default:
      return false;
  }

  // Success
  results->bits = nbits;
  // No need to record the state as we stored it as we decoded it.
  // As we use result->state, we don't record value, address, or command as it
  // is a union data type.
  return true;
}
#endif  // DECODE_MITSUBISHIHEAVY
