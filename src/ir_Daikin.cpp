/*
An Arduino sketch to emulate IR Daikin ARC433** & ARC477A1 remote control unit
Read more at:
http://harizanov.com/2012/02/control-daikin-air-conditioner-over-the-internet/

Copyright 2016 sillyfrog
Copyright 2017 sillyfrog, crankyoldgit
Copyright 2018-2019 crankyoldgit
Copyright 2019 pasna (IRDaikin160 class)
*/

#include "ir_Daikin.h"
#include <algorithm>
#ifndef ARDUINO
#include <string>
#endif
#include "IRrecv.h"
#include "IRremoteESP8266.h"
#include "IRsend.h"
#ifdef UNIT_TEST
#include "IRsend_test.h"
#endif
#include "IRutils.h"

// Constants
// Ref:
//   https://github.com/mharizanov/Daikin-AC-remote-control-over-the-Internet/tree/master/IRremote
//   http://rdlab.cdmt.vn/project-2013/daikin-ir-protocol
//   https://github.com/crankyoldgit/IRremoteESP8266/issues/582

#if SEND_DAIKIN
// Send a Daikin A/C message.
//
// Args:
//   data: An array of kDaikinStateLength bytes containing the IR command.
//
// Status: STABLE
//
// Ref:
//   IRDaikinESP.cpp
//   https://github.com/mharizanov/Daikin-AC-remote-control-over-the-Internet/tree/master/IRremote
//   https://github.com/blafois/Daikin-IR-Reverse
void IRsend::sendDaikin(const unsigned char data[], const uint16_t nbytes,
                        const uint16_t repeat) {
  if (nbytes < kDaikinStateLengthShort)
    return;  // Not enough bytes to send a proper message.

  for (uint16_t r = 0; r <= repeat; r++) {
    uint16_t offset = 0;
    // Send the header, 0b00000
    sendGeneric(0, 0,  // No header for the header
                kDaikinBitMark, kDaikinOneSpace, kDaikinBitMark,
                kDaikinZeroSpace, kDaikinBitMark, kDaikinZeroSpace + kDaikinGap,
                (uint64_t)0b00000, kDaikinHeaderLength, 38, false, 0, 50);
    // Data #1
    if (nbytes < kDaikinStateLength) {  // Are we using the legacy size?
      // Do this as a constant to save RAM and keep in flash memory
      sendGeneric(kDaikinHdrMark, kDaikinHdrSpace, kDaikinBitMark,
                  kDaikinOneSpace, kDaikinBitMark, kDaikinZeroSpace,
                  kDaikinBitMark, kDaikinZeroSpace + kDaikinGap,
                  kDaikinFirstHeader64, 64, 38, false, 0, 50);
    } else {  // We are using the newer/more correct size.
      sendGeneric(kDaikinHdrMark, kDaikinHdrSpace, kDaikinBitMark,
                  kDaikinOneSpace, kDaikinBitMark, kDaikinZeroSpace,
                  kDaikinBitMark, kDaikinZeroSpace + kDaikinGap,
                  data, kDaikinSection1Length, 38, false, 0, 50);
      offset += kDaikinSection1Length;
    }
    // Data #2
    sendGeneric(kDaikinHdrMark, kDaikinHdrSpace, kDaikinBitMark,
                kDaikinOneSpace, kDaikinBitMark, kDaikinZeroSpace,
                kDaikinBitMark, kDaikinZeroSpace + kDaikinGap,
                data + offset, kDaikinSection2Length, 38, false, 0, 50);
    offset += kDaikinSection2Length;
    // Data #3
    sendGeneric(kDaikinHdrMark, kDaikinHdrSpace, kDaikinBitMark,
                kDaikinOneSpace, kDaikinBitMark, kDaikinZeroSpace,
                kDaikinBitMark, kDaikinZeroSpace + kDaikinGap,
                data + offset, nbytes - offset, 38, false, 0, 50);
  }
}
#endif  // SEND_DAIKIN

IRDaikinESP::IRDaikinESP(uint16_t pin) : _irsend(pin) { stateReset(); }

void IRDaikinESP::begin(void) { _irsend.begin(); }

#if SEND_DAIKIN
void IRDaikinESP::send(const uint16_t repeat) {
  this->checksum();
  _irsend.sendDaikin(remote, kDaikinStateLength, repeat);
}
#endif  // SEND_DAIKIN

// Verify the checksums are valid for a given state.
// Args:
//   state:  The array to verify the checksums of.
//   length: The size of the state.
// Returns:
//   A boolean.
bool IRDaikinESP::validChecksum(uint8_t state[], const uint16_t length) {
  // Data #1
  if (length < kDaikinSection1Length ||
      state[kDaikinByteChecksum1] != sumBytes(state, kDaikinSection1Length - 1))
    return false;
  // Data #2
  if (length < kDaikinSection1Length + kDaikinSection2Length ||
      state[kDaikinByteChecksum2] != sumBytes(state + kDaikinSection1Length,
                                              kDaikinSection2Length - 1))
    return false;
  // Data #3
  if (length < kDaikinSection1Length + kDaikinSection2Length + 2 ||
      state[length - 1] != sumBytes(state + kDaikinSection1Length +
                                    kDaikinSection2Length,
                                    length - (kDaikinSection1Length +
                                              kDaikinSection2Length) - 1))
    return false;
  return true;
}

// Calculate and set the checksum values for the internal state.
void IRDaikinESP::checksum(void) {
  remote[kDaikinByteChecksum1] = sumBytes(remote, kDaikinSection1Length - 1);
  remote[kDaikinByteChecksum2] = sumBytes(remote + kDaikinSection1Length,
                                          kDaikinSection2Length - 1);
  remote[kDaikinByteChecksum3] = sumBytes(remote + kDaikinSection1Length +
                                          kDaikinSection2Length,
                                          kDaikinSection3Length - 1);
}

void IRDaikinESP::stateReset(void) {
  for (uint8_t i = 0; i < kDaikinStateLength; i++) remote[i] = 0x0;

  remote[0] = 0x11;
  remote[1] = 0xDA;
  remote[2] = 0x27;
  remote[4] = 0xC5;
  // remote[7] is a checksum byte, it will be set by checksum().

  remote[8] = 0x11;
  remote[9] = 0xDA;
  remote[10] = 0x27;
  remote[12] = 0x42;
  // remote[15] is a checksum byte, it will be set by checksum().
  remote[16] = 0x11;
  remote[17] = 0xDA;
  remote[18] = 0x27;
  remote[21] = 0x49;
  remote[22] = 0x1E;
  remote[24] = 0xB0;
  remote[27] = 0x06;
  remote[28] = 0x60;
  remote[31] = 0xC0;
  // remote[34] is a checksum byte, it will be set by checksum().
  this->checksum();
}

uint8_t *IRDaikinESP::getRaw(void) {
  this->checksum();  // Ensure correct settings before sending.
  return remote;
}

void IRDaikinESP::setRaw(const uint8_t new_code[], const uint16_t length) {
  uint8_t offset = 0;
  if (length == kDaikinStateLengthShort) {  // Handle the "short" length case.
    offset = kDaikinStateLength - kDaikinStateLengthShort;
    this->stateReset();
  }
  for (uint8_t i = 0; i < length && i < kDaikinStateLength; i++)
    remote[i + offset] = new_code[i];
}

void IRDaikinESP::on(void) { remote[kDaikinBytePower] |= kDaikinBitPower; }

void IRDaikinESP::off(void) { remote[kDaikinBytePower] &= ~kDaikinBitPower; }

void IRDaikinESP::setPower(const bool on) {
  if (on)
    this->on();
  else
    this->off();
}

bool IRDaikinESP::getPower(void) {
  return remote[kDaikinBytePower] & kDaikinBitPower;
}

// Set the temp in deg C
void IRDaikinESP::setTemp(const uint8_t temp) {
  uint8_t degrees = std::max(temp, kDaikinMinTemp);
  degrees = std::min(degrees, kDaikinMaxTemp);
  remote[kDaikinByteTemp] = degrees << 1;
}

uint8_t IRDaikinESP::getTemp(void) { return remote[kDaikinByteTemp] >> 1; }

// Set the speed of the fan, 1-5 or kDaikinFanAuto or kDaikinFanQuiet
void IRDaikinESP::setFan(const uint8_t fan) {
  // Set the fan speed bits, leave low 4 bits alone
  uint8_t fanset;
  if (fan == kDaikinFanQuiet || fan == kDaikinFanAuto)
    fanset = fan;
  else if (fan < kDaikinFanMin || fan > kDaikinFanMax)
    fanset = kDaikinFanAuto;
  else
    fanset = 2 + fan;
  remote[kDaikinByteFan] &= 0x0F;
  remote[kDaikinByteFan] |= (fanset << 4);
}

uint8_t IRDaikinESP::getFan(void) {
  uint8_t fan = remote[kDaikinByteFan] >> 4;
  if (fan != kDaikinFanQuiet && fan != kDaikinFanAuto) fan -= 2;
  return fan;
}

uint8_t IRDaikinESP::getMode(void) { return remote[kDaikinBytePower] >> 4; }

void IRDaikinESP::setMode(const uint8_t mode) {
  switch (mode) {
    case kDaikinAuto:
    case kDaikinCool:
    case kDaikinHeat:
    case kDaikinFan:
    case kDaikinDry:
      remote[kDaikinBytePower] &= 0b10001111;
      remote[kDaikinBytePower] |= (mode << 4);
      break;
    default:
      this->setMode(kDaikinAuto);
  }
}

void IRDaikinESP::setSwingVertical(const bool on) {
  if (on)
    remote[kDaikinByteFan] |= 0x0F;
  else
    remote[kDaikinByteFan] &= 0xF0;
}

bool IRDaikinESP::getSwingVertical(void) {
  return remote[kDaikinByteFan] & 0x0F;
}

void IRDaikinESP::setSwingHorizontal(const bool on) {
  if (on)
    remote[kDaikinByteSwingH] |= 0x0F;
  else
    remote[kDaikinByteSwingH] &= 0xF0;
}

bool IRDaikinESP::getSwingHorizontal(void) {
  return remote[kDaikinByteSwingH] & 0x0F;
}

void IRDaikinESP::setQuiet(const bool on) {
  if (on) {
    remote[kDaikinByteSilent] |= kDaikinBitSilent;
    // Powerful & Quiet mode being on are mutually exclusive.
    this->setPowerful(false);
  } else {
    remote[kDaikinByteSilent] &= ~kDaikinBitSilent;
  }
}

bool IRDaikinESP::getQuiet(void) {
  return remote[kDaikinByteSilent] & kDaikinBitSilent;
}

void IRDaikinESP::setPowerful(const bool on) {
  if (on) {
    remote[kDaikinBytePowerful] |= kDaikinBitPowerful;
    // Powerful, Quiet, & Econo mode being on are mutually exclusive.
    this->setQuiet(false);
    this->setEcono(false);
  } else {
    remote[kDaikinBytePowerful] &= ~kDaikinBitPowerful;
  }
}

bool IRDaikinESP::getPowerful(void) {
  return remote[kDaikinBytePowerful] & kDaikinBitPowerful;
}

void IRDaikinESP::setSensor(const bool on) {
  if (on)
    remote[kDaikinByteSensor] |= kDaikinBitSensor;
  else
    remote[kDaikinByteSensor] &= ~kDaikinBitSensor;
}

bool IRDaikinESP::getSensor(void) {
  return remote[kDaikinByteSensor] & kDaikinBitSensor;
}

void IRDaikinESP::setEcono(const bool on) {
  if (on) {
    remote[kDaikinByteEcono] |= kDaikinBitEcono;
    // Powerful & Econo mode being on are mutually exclusive.
    this->setPowerful(false);
  } else {
    remote[kDaikinByteEcono] &= ~kDaikinBitEcono;
  }
}

bool IRDaikinESP::getEcono(void) {
  return remote[kDaikinByteEcono] & kDaikinBitEcono;
}

void IRDaikinESP::setMold(const bool on) {
  if (on)
    remote[kDaikinByteMold] |= kDaikinBitMold;
  else
    remote[kDaikinByteMold] &= ~kDaikinBitMold;
}

bool IRDaikinESP::getMold(void) {
  return remote[kDaikinByteMold] & kDaikinBitMold;
}

void IRDaikinESP::setComfort(const bool on) {
  if (on)
    remote[kDaikinByteComfort] |= kDaikinBitComfort;
  else
    remote[kDaikinByteComfort] &= ~kDaikinBitComfort;
}

bool IRDaikinESP::getComfort(void) {
  return remote[kDaikinByteComfort] & kDaikinBitComfort;
}

// starttime: Number of minutes after midnight.
void IRDaikinESP::enableOnTimer(const uint16_t starttime) {
  remote[kDaikinByteOnTimer] |= kDaikinBitOnTimer;
  remote[kDaikinByteOnTimerMinsLow] = starttime;
  // only keep 4 bits
  remote[kDaikinByteOnTimerMinsHigh] &= 0xF0;
  remote[kDaikinByteOnTimerMinsHigh] |= ((starttime >> 8) & 0x0F);
}

void IRDaikinESP::disableOnTimer(void) {
  this->enableOnTimer(kDaikinUnusedTime);
  remote[kDaikinByteOnTimer] &= ~kDaikinBitOnTimer;
}

uint16_t IRDaikinESP::getOnTime(void) {
  return ((remote[kDaikinByteOnTimerMinsHigh] & 0x0F) << 8) +
      remote[kDaikinByteOnTimerMinsLow];
}

bool IRDaikinESP::getOnTimerEnabled(void) {
  return remote[kDaikinByteOnTimer] & kDaikinBitOnTimer;
}

// endtime: Number of minutes after midnight.
void IRDaikinESP::enableOffTimer(const uint16_t endtime) {
  remote[kDaikinByteOffTimer] |= kDaikinBitOffTimer;
  remote[kDaikinByteOffTimerMinsHigh] = endtime >> 4;
  remote[kDaikinByteOffTimerMinsLow] &= 0x0F;
  remote[kDaikinByteOffTimerMinsLow] |= ((endtime & 0x0F) << 4);
}

void IRDaikinESP::disableOffTimer(void) {
  this->enableOffTimer(kDaikinUnusedTime);
  remote[kDaikinByteOffTimer] &= ~kDaikinBitOffTimer;
}

uint16_t IRDaikinESP::getOffTime(void) {
  return (remote[kDaikinByteOffTimerMinsHigh] << 4) +
      ((remote[kDaikinByteOffTimerMinsLow] & 0xF0) >> 4);
}

bool IRDaikinESP::getOffTimerEnabled(void) {
  return remote[kDaikinByteOffTimer] & kDaikinBitOffTimer;
}

void IRDaikinESP::setCurrentTime(const uint16_t mins_since_midnight) {
  uint16_t mins = mins_since_midnight;
  if (mins > 24 * 60) mins = 0;  // If > 23:59, set to 00:00
  remote[kDaikinByteClockMinsLow] = mins;
  // only keep 3 bits
  remote[kDaikinByteClockMinsHigh] &= 0xF8;
  remote[kDaikinByteClockMinsHigh] |= ((mins >> 8) & 0x07);
}

uint16_t IRDaikinESP::getCurrentTime(void) {
  return ((remote[kDaikinByteClockMinsHigh] & 0x07) << 8) +
      remote[kDaikinByteClockMinsLow];
}

void IRDaikinESP::setCurrentDay(const uint8_t day_of_week) {
  // 1 is SUN, 2 is MON, ..., 7 is SAT
  uint8_t days = day_of_week;
  if (days > 7) days = 0;  // Enforce the limit
  // Update bits 5-3
  remote[kDaikinByteClockMinsHigh] &= 0xc7;
  remote[kDaikinByteClockMinsHigh] |= days << 3;
}

uint8_t IRDaikinESP::getCurrentDay(void) {
  return ((remote[kDaikinByteClockMinsHigh] & 0x38) >> 3);
}

void IRDaikinESP::setWeeklyTimerEnable(const bool on) {
  if (on)
    remote[kDaikinByteWeeklyTimer] &= ~kDaikinBitWeeklyTimer;  // Clear the bit.
  else
    remote[kDaikinByteWeeklyTimer] |= kDaikinBitWeeklyTimer;  // Set the bit.
}

bool IRDaikinESP::getWeeklyTimerEnable(void) {
  return !(remote[kDaikinByteWeeklyTimer] & kDaikinBitWeeklyTimer);
}

// Convert a standard A/C mode into its native mode.
uint8_t IRDaikinESP::convertMode(const stdAc::opmode_t mode) {
  switch (mode) {
    case stdAc::opmode_t::kCool:
      return kDaikinCool;
    case stdAc::opmode_t::kHeat:
      return kDaikinHeat;
    case stdAc::opmode_t::kDry:
      return kDaikinDry;
    case stdAc::opmode_t::kFan:
      return kDaikinFan;
    default:
      return kDaikinAuto;
  }
}

// Convert a standard A/C Fan speed into its native fan speed.
uint8_t IRDaikinESP::convertFan(const stdAc::fanspeed_t speed) {
  switch (speed) {
    case stdAc::fanspeed_t::kMin:
      return kDaikinFanQuiet;
    case stdAc::fanspeed_t::kLow:
      return kDaikinFanMin;
    case stdAc::fanspeed_t::kMedium:
      return kDaikinFanMin + 1;
    case stdAc::fanspeed_t::kHigh:
      return kDaikinFanMax - 1;
    case stdAc::fanspeed_t::kMax:
      return kDaikinFanMax;
    default:
      return kDaikinFanAuto;
  }
}

// Convert a native mode to it's common equivalent.
stdAc::opmode_t IRDaikinESP::toCommonMode(const uint8_t mode) {
  switch (mode) {
    case kDaikinCool: return stdAc::opmode_t::kCool;
    case kDaikinHeat: return stdAc::opmode_t::kHeat;
    case kDaikinDry: return stdAc::opmode_t::kDry;
    case kDaikinFan: return stdAc::opmode_t::kFan;
    default: return stdAc::opmode_t::kAuto;
  }
}

// Convert a native fan speed to it's common equivalent.
stdAc::fanspeed_t IRDaikinESP::toCommonFanSpeed(const uint8_t speed) {
  switch (speed) {
    case kDaikinFanMax: return stdAc::fanspeed_t::kMax;
    case kDaikinFanMax - 1: return stdAc::fanspeed_t::kHigh;
    case kDaikinFanMin + 1: return stdAc::fanspeed_t::kMedium;
    case kDaikinFanMin: return stdAc::fanspeed_t::kLow;
    case kDaikinFanQuiet: return stdAc::fanspeed_t::kMin;
    default: return stdAc::fanspeed_t::kAuto;
  }
}

// Convert the A/C state to it's common equivalent.
stdAc::state_t IRDaikinESP::toCommon(void) {
  stdAc::state_t result;
  result.protocol = decode_type_t::DAIKIN;
  result.model = -1;  // No models used.
  result.power = this->getPower();
  result.mode = this->toCommonMode(this->getMode());
  result.celsius = true;
  result.degrees = this->getTemp();
  result.fanspeed = this->toCommonFanSpeed(this->getFan());
  result.swingv = this->getSwingVertical() ? stdAc::swingv_t::kAuto :
                                             stdAc::swingv_t::kOff;
  result.swingh = this->getSwingHorizontal() ? stdAc::swingh_t::kAuto :
                                               stdAc::swingh_t::kOff;
  result.quiet = this->getQuiet();
  result.turbo = this->getPowerful();
  result.clean = this->getMold();
  result.econo = this->getEcono();
  // Not supported.
  result.filter = false;
  result.light = false;
  result.beep = false;
  result.sleep = -1;
  result.clock = -1;
  return result;
}

String IRDaikinESP::renderTime(const uint16_t timemins) {
  String ret;
  ret = uint64ToString(timemins / 60) + ':';
  uint8_t mins = timemins % 60;
  if (mins < 10) ret += '0';
  ret += uint64ToString(mins);
  return ret;
}

// Convert the internal state into a human readable string.
String IRDaikinESP::toString(void) {
  String result = "";
  result.reserve(230);  // Reserve some heap for the string to reduce fragging.
  result += IRutils::acBoolToString(getPower(), F("Power"), false);
  result += IRutils::acModeToString(getMode(), kDaikinAuto,
                                    kDaikinCool, kDaikinHeat,
                                    kDaikinDry, kDaikinFan);
  result += F(", Temp: ");
  result += uint64ToString(this->getTemp());
  result += F("C, Fan: ");
  result += uint64ToString(this->getFan());
  switch (this->getFan()) {
    case kDaikinFanAuto:
      result += F(" (AUTO)");
      break;
    case kDaikinFanQuiet:
      result += F(" (QUIET)");
      break;
    case kDaikinFanMin:
      result += F(" (MIN)");
      break;
    case kDaikinFanMax:
      result += F(" (MAX)");
      break;
  }
  result += IRutils::acBoolToString(getPowerful(), F("Powerful"));
  result += IRutils::acBoolToString(getQuiet(), F("Quiet"));
  result += IRutils::acBoolToString(getSensor(), F("Sensor"));
  result += IRutils::acBoolToString(getMold(), F("Mold"));
  result += IRutils::acBoolToString(getComfort(), F("Comfort"));
  result += IRutils::acBoolToString(getSwingHorizontal(),
                                    F("Swing (Horizontal)"));
  result += IRutils::acBoolToString(getSwingVertical(),
                                    F("Swing (Vertical)"));
  result += F(", Current Time: ");
  result += this->renderTime(this->getCurrentTime());
  result += F(", Current Day: ");
  switch (this->getCurrentDay()) {
  case 1:
    result +=F("SUN"); break;
  case 2:
    result +=F("MON"); break;
  case 3:
    result +=F("TUE"); break;
  case 4:
    result +=F("WED"); break;
  case 5:
    result +=F("THU"); break;
  case 6:
    result +=F("FRI"); break;
  case 7:
    result +=F("SAT"); break;
  default:
    result +=F("(UNKNOWN)"); break;
  }
  result += F(", On Time: ");
  if (this->getOnTimerEnabled())
    result += this->renderTime(this->getOnTime());
  else
    result += F("Off");
  result += F(", Off Time: ");
  if (this->getOffTimerEnabled())
    result += this->renderTime(this->getOffTime());
  else
    result += F("Off");
  result += F(", Weekly Timer: ");
  result += this->getWeeklyTimerEnable() ? F("On") : F("Off");
  return result;
}

#if DECODE_DAIKIN
// Decode the supplied Daikin A/C message.
// Args:
//   results: Ptr to the data to decode and where to store the decode result.
//   nbits:   Nr. of bits to expect in the data portion. (kDaikinBits)
//   strict:  Flag to indicate if we strictly adhere to the specification.
// Returns:
//   boolean: True if it can decode it, false if it can't.
//
// Status: BETA / Should be working.
//
// Ref:
//   https://github.com/mharizanov/Daikin-AC-remote-control-over-the-Internet/tree/master/IRremote
bool IRrecv::decodeDaikin(decode_results *results, const uint16_t nbits,
                          const bool strict) {
  // Is there enough data to match successfully?
  if (results->rawlen < (2 * (nbits + kDaikinHeaderLength) +
                         kDaikinSections * (kHeader + kFooter) + kFooter - 1))
    return false;

  // Compliance
  if (strict && nbits != kDaikinBits) return false;

  uint16_t offset = kStartOffset;
  match_result_t data_result;

  // Header #1 - Doesn't count as data.
  data_result = matchData(&(results->rawbuf[offset]), kDaikinHeaderLength,
                          kDaikinBitMark, kDaikinOneSpace,
                          kDaikinBitMark, kDaikinZeroSpace,
                          kDaikinTolerance, kDaikinMarkExcess, false);
  offset += data_result.used;
  if (data_result.success == false) return false;  // Fail
  if (data_result.data) return false;  // The header bits should be zero.
  // Footer
  if (!matchMark(results->rawbuf[offset++], kDaikinBitMark,
                 kDaikinTolerance, kDaikinMarkExcess)) return false;
  if (!matchSpace(results->rawbuf[offset++], kDaikinZeroSpace + kDaikinGap,
                  kDaikinTolerance, kDaikinMarkExcess)) return false;
  // Sections
  const uint8_t ksectionSize[kDaikinSections] = {
      kDaikinSection1Length, kDaikinSection2Length, kDaikinSection3Length};
  uint16_t pos = 0;
  for (uint8_t section = 0; section < kDaikinSections; section++) {
    uint16_t used;
    // Section Header + Section Data (7 bytes) + Section Footer
    used = matchGeneric(results->rawbuf + offset, results->state + pos,
                        results->rawlen - offset, ksectionSize[section] * 8,
                        kDaikinHdrMark, kDaikinHdrSpace,
                        kDaikinBitMark, kDaikinOneSpace,
                        kDaikinBitMark, kDaikinZeroSpace,
                        kDaikinBitMark, kDaikinZeroSpace + kDaikinGap,
                        section >= kDaikinSections - 1,
                        kDaikinTolerance, kDaikinMarkExcess, false);
    if (used == 0) return false;
    offset += used;
    pos += ksectionSize[section];
  }
  // Compliance
  if (strict) {
    // Re-check we got the correct size/length due to the way we read the data.
    if (pos * 8 != kDaikinBits) return false;
    // Validate the checksum.
    if (!IRDaikinESP::validChecksum(results->state)) return false;
  }

  // Success
  results->decode_type = DAIKIN;
  results->bits = nbits;
  // No need to record the state as we stored it as we decoded it.
  // As we use result->state, we don't record value, address, or command as it
  // is a union data type.
  return true;
}
#endif  // DECODE_DAIKIN

#if SEND_DAIKIN2
// Send a Daikin2 A/C message.
//
// Args:
//   data: An array of kDaikin2StateLength bytes containing the IR command.
//
// Status: BETA/Appears to work.
//
// Ref:
//   https://github.com/crankyoldgit/IRremoteESP8266/issues/582
void IRsend::sendDaikin2(const unsigned char data[], const uint16_t nbytes,
                         const uint16_t repeat) {
  if (nbytes < kDaikin2Section1Length)
    return;  // Not enough bytes to send a partial message.

  for (uint16_t r = 0; r <= repeat; r++) {
    // Leader
    sendGeneric(kDaikin2LeaderMark, kDaikin2LeaderSpace,
                0, 0, 0, 0, 0, 0, (uint64_t) 0,  // No data payload.
                0, kDaikin2Freq, false, 0, 50);
    // Section #1
    sendGeneric(kDaikin2HdrMark, kDaikin2HdrSpace, kDaikin2BitMark,
                kDaikin2OneSpace, kDaikin2BitMark, kDaikin2ZeroSpace,
                kDaikin2BitMark, kDaikin2Gap, data, kDaikin2Section1Length,
                kDaikin2Freq, false, 0, 50);
    // Section #2
    sendGeneric(kDaikin2HdrMark, kDaikin2HdrSpace, kDaikin2BitMark,
                kDaikin2OneSpace, kDaikin2BitMark, kDaikin2ZeroSpace,
                kDaikin2BitMark, kDaikin2Gap, data + kDaikin2Section1Length,
                nbytes - kDaikin2Section1Length,
                kDaikin2Freq, false, 0, 50);
  }
}
#endif  // SEND_DAIKIN2

// Class for handling Daikin2 A/C messages.
//
// Code by crankyoldgit, Reverse engineering analysis by sheppy99
//
// Supported Remotes: Daikin ARC477A1 remote
//
// Ref:
//   https://github.com/crankyoldgit/IRremoteESP8266/issues/582
//   https://docs.google.com/spreadsheets/d/1f8EGfIbBUo2B-CzUFdrgKQprWakoYNKM80IKZN4KXQE/edit?usp=sharing
//   https://www.daikin.co.nz/sites/default/files/daikin-split-system-US7-FTXZ25-50NV1B.pdf
IRDaikin2::IRDaikin2(uint16_t pin) : _irsend(pin) { stateReset(); }

void IRDaikin2::begin() { _irsend.begin(); }

#if SEND_DAIKIN2
void IRDaikin2::send(const uint16_t repeat) {
  checksum();
  _irsend.sendDaikin2(remote_state, kDaikin2StateLength, repeat);
}
#endif  // SEND_DAIKIN2

// Verify the checksum is valid for a given state.
// Args:
//   state:  The array to verify the checksum of.
//   length: The size of the state.
// Returns:
//   A boolean.
bool IRDaikin2::validChecksum(uint8_t state[], const uint16_t length) {
  // Validate the checksum of section #1.
  if (length <= kDaikin2Section1Length - 1 ||
      state[kDaikin2Section1Length - 1] != sumBytes(state,
                                                    kDaikin2Section1Length - 1))
    return false;
  // Validate the checksum of section #2 (a.k.a. the rest)
  if (length <= kDaikin2Section1Length + 1 ||
      state[length - 1] != sumBytes(state + kDaikin2Section1Length,
                                    length - kDaikin2Section1Length - 1))
    return false;
  return true;
}

// Calculate and set the checksum values for the internal state.
void IRDaikin2::checksum() {
  remote_state[kDaikin2Section1Length - 1] = sumBytes(
      remote_state, kDaikin2Section1Length - 1);
  remote_state[kDaikin2StateLength -1 ] = sumBytes(
      remote_state + kDaikin2Section1Length, kDaikin2Section2Length - 1);
}

void IRDaikin2::stateReset() {
  for (uint8_t i = 0; i < kDaikin2StateLength; i++) remote_state[i] = 0x0;

  remote_state[0] = 0x11;
  remote_state[1] = 0xDA;
  remote_state[2] = 0x27;
  remote_state[4] = 0x01;
  remote_state[6] = 0xC0;
  remote_state[7] = 0x70;
  remote_state[8] = 0x08;
  remote_state[9] = 0x0C;
  remote_state[10] = 0x80;
  remote_state[11] = 0x04;
  remote_state[12] = 0xB0;
  remote_state[13] = 0x16;
  remote_state[14] = 0x24;
  remote_state[17] = 0xBE;
  remote_state[18] = 0xD0;
  // remote_state[19] is a checksum byte, it will be set by checksum().
  remote_state[20] = 0x11;
  remote_state[21] = 0xDA;
  remote_state[22] = 0x27;
  remote_state[25] = 0x08;
  remote_state[28] = 0xA0;
  remote_state[35] = 0xC1;
  remote_state[36] = 0x80;
  remote_state[37] = 0x60;
  // remote_state[38] is a checksum byte, it will be set by checksum().
  disableOnTimer();
  disableOffTimer();
  disableSleepTimer();
  checksum();
}

uint8_t *IRDaikin2::getRaw() {
  checksum();  // Ensure correct settings before sending.
  return remote_state;
}

void IRDaikin2::setRaw(const uint8_t new_code[]) {
  for (uint8_t i = 0; i < kDaikin2StateLength; i++)
    remote_state[i] = new_code[i];
}

void IRDaikin2::on() {
  remote_state[25] |= kDaikinBitPower;
  remote_state[6] &= ~kDaikin2BitPower;
}

void IRDaikin2::off() {
  remote_state[25] &= ~kDaikinBitPower;
  remote_state[6] |= kDaikin2BitPower;
}

void IRDaikin2::setPower(const bool state) {
  if (state)
    on();
  else
    off();
}

bool IRDaikin2::getPower() {
  return (remote_state[25] & kDaikinBitPower) &&
         !(remote_state[6] & kDaikin2BitPower);
}

uint8_t IRDaikin2::getMode() { return remote_state[25] >> 4; }

void IRDaikin2::setMode(const uint8_t desired_mode) {
  uint8_t mode = desired_mode;
  switch (mode) {
    case kDaikinCool:
    case kDaikinHeat:
    case kDaikinFan:
    case kDaikinDry:
      break;
    default:
      mode = kDaikinAuto;
  }
  remote_state[25] &= 0b10001111;
  remote_state[25] |= (mode << 4);
  // Redo the temp setting as Cool mode has a different min temp.
  if (mode == kDaikinCool) this->setTemp(this->getTemp());
}

// Set the temp in deg C
void IRDaikin2::setTemp(const uint8_t desired) {
  // The A/C has a different min temp if in cool mode.
  uint8_t temp = std::max(
      (this->getMode() == kDaikinCool) ? kDaikin2MinCoolTemp : kDaikinMinTemp,
      desired);
  temp = std::min(kDaikinMaxTemp, temp);
  remote_state[26] = temp * 2;
}

// Set the speed of the fan, 1-5 or kDaikinFanAuto or kDaikinFanQuiet
void IRDaikin2::setFan(const uint8_t fan) {
  // Set the fan speed bits, leave low 4 bits alone
  uint8_t fanset;
  if (fan == kDaikinFanQuiet || fan == kDaikinFanAuto)
    fanset = fan;
  else if (fan < kDaikinFanMin || fan > kDaikinFanMax)
    fanset = kDaikinFanAuto;
  else
    fanset = 2 + fan;
  remote_state[28] &= 0x0F;
  remote_state[28] |= (fanset << 4);
}

uint8_t IRDaikin2::getFan() {
  uint8_t fan = remote_state[28] >> 4;
  if (fan != kDaikinFanQuiet && fan != kDaikinFanAuto) fan -= 2;
  return fan;
}

uint8_t IRDaikin2::getTemp() { return remote_state[26] / 2; }

void IRDaikin2::setSwingVertical(const uint8_t position) {
  switch (position) {
    case kDaikin2SwingVHigh:
    case 2:
    case 3:
    case 4:
    case 5:
    case kDaikin2SwingVLow:
    case kDaikin2SwingVBreeze:
    case kDaikin2SwingVCirculate:
    case kDaikin2SwingVAuto:
      remote_state[18] &= 0xF0;
      remote_state[18] |= (position & 0x0F);
  }
}

uint8_t IRDaikin2::getSwingVertical() { return remote_state[18] & 0x0F; }

void IRDaikin2::setSwingHorizontal(const uint8_t position) {
  remote_state[17] = position;
}

uint8_t IRDaikin2::getSwingHorizontal() { return remote_state[17]; }

void IRDaikin2::setCurrentTime(const uint16_t numMins) {
  uint16_t mins = numMins;
  if (numMins > 24 * 60) mins = 0;  // If > 23:59, set to 00:00
  remote_state[5] = (uint8_t)(mins & 0xFF);
  // only keep 4 bits
  remote_state[6] &= 0xF0;
  remote_state[6] |= (uint8_t)((mins >> 8) & 0x0F);
}

uint16_t IRDaikin2::getCurrentTime() {
  return ((remote_state[6] & 0x0F) << 8) + remote_state[5];
}

// starttime: Number of minutes after midnight.
// Note: Timer location is shared with sleep timer.
void IRDaikin2::enableOnTimer(const uint16_t starttime) {
  clearSleepTimerFlag();
  remote_state[25] |= kDaikinBitOnTimer;  // Set the On Timer flag.
  remote_state[30] = (uint8_t)(starttime & 0xFF);
  // only keep 4 bits
  remote_state[31] &= 0xF0;
  remote_state[31] |= (uint8_t)((starttime >> 8) & 0x0F);
}

void IRDaikin2::clearOnTimerFlag() {
  remote_state[25] &= ~kDaikinBitOnTimer;
}

void IRDaikin2::disableOnTimer() {
  enableOnTimer(kDaikinUnusedTime);
  clearOnTimerFlag();
  clearSleepTimerFlag();
}

uint16_t IRDaikin2::getOnTime() {
  return ((remote_state[31] & 0x0F) << 8) + remote_state[30];
}

bool IRDaikin2::getOnTimerEnabled() {
  return remote_state[25] & kDaikinBitOnTimer;
}

// endtime: Number of minutes after midnight.
void IRDaikin2::enableOffTimer(const uint16_t endtime) {
  remote_state[25] |= kDaikinBitOffTimer;  // Set the Off Timer flag.
  remote_state[32] = (uint8_t)((endtime >> 4) & 0xFF);
  remote_state[31] &= 0x0F;
  remote_state[31] |= (uint8_t)((endtime & 0xF) << 4);
}

void IRDaikin2::disableOffTimer() {
  enableOffTimer(kDaikinUnusedTime);
  remote_state[25] &= ~kDaikinBitOffTimer;  // Clear the Off Timer flag.
}

uint16_t IRDaikin2::getOffTime() {
  return (remote_state[32] << 4) + (remote_state[31] >> 4);
}

bool IRDaikin2::getOffTimerEnabled() {
  return remote_state[25] & kDaikinBitOffTimer;
}

uint8_t IRDaikin2::getBeep() {
  return remote_state[7] >> 6;
}

void IRDaikin2::setBeep(const uint8_t beep) {
  remote_state[7] &= ~kDaikin2BeepMask;
  remote_state[7] |= ((beep << 6) & kDaikin2BeepMask);
}

uint8_t IRDaikin2::getLight() {
  return (remote_state[7] & kDaikin2LightMask) >> 4;
}

void IRDaikin2::setLight(const uint8_t light) {
  remote_state[7] &= ~kDaikin2LightMask;
  remote_state[7] |= ((light << 4) & kDaikin2LightMask);
}

void IRDaikin2::setMold(const bool on) {
  if (on)
    remote_state[8] |= kDaikin2BitMold;
  else
    remote_state[8] &= ~kDaikin2BitMold;
}

bool IRDaikin2::getMold() {
  return remote_state[8] & kDaikin2BitMold;
}

// Auto clean setting.
void IRDaikin2::setClean(const bool on) {
  if (on)
    remote_state[8] |= kDaikin2BitClean;
  else
    remote_state[8] &= ~kDaikin2BitClean;
}

bool IRDaikin2::getClean() {
  return remote_state[8] & kDaikin2BitClean;
}

// Fresh Air settings.
void IRDaikin2::setFreshAir(const bool on) {
  if (on)
    remote_state[8] |= kDaikin2BitFreshAir;
  else
    remote_state[8] &= ~kDaikin2BitFreshAir;
}

bool IRDaikin2::getFreshAir() {
  return remote_state[8] & kDaikin2BitFreshAir;
}

void IRDaikin2::setFreshAirHigh(const bool on) {
  if (on)
    remote_state[8] |= kDaikin2BitFreshAirHigh;
  else
    remote_state[8] &= ~kDaikin2BitFreshAirHigh;
}

bool IRDaikin2::getFreshAirHigh() {
  return remote_state[8] & kDaikin2BitFreshAirHigh;
}

void IRDaikin2::setEyeAuto(bool on) {
  if (on)
    remote_state[13] |= kDaikin2BitEyeAuto;
  else
    remote_state[13] &= ~kDaikin2BitEyeAuto;
}

bool IRDaikin2::getEyeAuto() {
  return remote_state[13] & kDaikin2BitEyeAuto;
}

void IRDaikin2::setEye(bool on) {
  if (on)
    remote_state[36] |= kDaikin2BitEye;
  else
    remote_state[36] &= ~kDaikin2BitEye;
}

bool IRDaikin2::getEye() {
  return remote_state[36] & kDaikin2BitEye;
}

void IRDaikin2::setEcono(bool on) {
  if (on)
    remote_state[36] |= kDaikinBitEcono;
  else
    remote_state[36] &= ~kDaikinBitEcono;
}

bool IRDaikin2::getEcono() {
  return remote_state[36] & kDaikinBitEcono;
}

// sleeptime: Number of minutes.
// Note: Timer location is shared with On Timer.
void IRDaikin2::enableSleepTimer(const uint16_t sleeptime) {
  enableOnTimer(sleeptime);
  clearOnTimerFlag();
  remote_state[36] |= kDaikin2BitSleepTimer;  // Set the Sleep Timer flag.
}

void IRDaikin2::clearSleepTimerFlag() {
  remote_state[36] &= ~kDaikin2BitSleepTimer;
}

void IRDaikin2::disableSleepTimer() {
  disableOnTimer();
}

uint16_t IRDaikin2::getSleepTime() {
  return getOnTime();
}

bool IRDaikin2::getSleepTimerEnabled() {
  return remote_state[36] & kDaikin2BitSleepTimer;
}

void IRDaikin2::setQuiet(const bool on) {
  if (on) {
    remote_state[33] |= kDaikinBitSilent;
    // Powerful & Quiet mode being on are mutually exclusive.
    setPowerful(false);
  } else {
    remote_state[33] &= ~kDaikinBitSilent;
  }
}

bool IRDaikin2::getQuiet() { return remote_state[33] & kDaikinBitSilent; }

void IRDaikin2::setPowerful(const bool on) {
  if (on) {
    remote_state[33] |= kDaikinBitPowerful;
    // Powerful & Quiet mode being on are mutually exclusive.
    setQuiet(false);
  } else {
    remote_state[33] &= ~kDaikinBitPowerful;
  }
}

bool IRDaikin2::getPowerful() { return remote_state[33] & kDaikinBitPowerful; }

void IRDaikin2::setPurify(const bool on) {
  if (on)
    remote_state[36] |= kDaikin2BitPurify;
  else
    remote_state[36] &= ~kDaikin2BitPurify;
}

bool IRDaikin2::getPurify() { return remote_state[36] & kDaikin2BitPurify; }

// Convert a standard A/C mode into its native mode.
uint8_t IRDaikin2::convertMode(const stdAc::opmode_t mode) {
  return IRDaikinESP::convertMode(mode);
}

// Convert a standard A/C Fan speed into its native fan speed.
uint8_t IRDaikin2::convertFan(const stdAc::fanspeed_t speed) {
  return IRDaikinESP::convertFan(speed);
}

// Convert a standard A/C vertical swing into its native version.
uint8_t IRDaikin2::convertSwingV(const stdAc::swingv_t position) {
  switch (position) {
    case stdAc::swingv_t::kHighest:
    case stdAc::swingv_t::kHigh:
    case stdAc::swingv_t::kMiddle:
    case stdAc::swingv_t::kLow:
    case stdAc::swingv_t::kLowest:
      return (uint8_t)position + kDaikin2SwingVHigh;
    default:
      return kDaikin2SwingVAuto;
  }
}

// Convert a native vertical swing to it's common equivalent.
stdAc::swingv_t IRDaikin2::toCommonSwingV(const uint8_t setting) {
  switch (setting) {
    case kDaikin2SwingVHigh: return stdAc::swingv_t::kHighest;
    case kDaikin2SwingVHigh + 1: return stdAc::swingv_t::kHigh;
    case kDaikin2SwingVHigh + 2:
    case kDaikin2SwingVHigh + 3: return stdAc::swingv_t::kMiddle;
    case kDaikin2SwingVLow - 1: return stdAc::swingv_t::kLow;
    case kDaikin2SwingVLow: return stdAc::swingv_t::kLowest;
    default: return stdAc::swingv_t::kAuto;
  }
}

// Convert a native horizontal swing to it's common equivalent.
stdAc::swingh_t IRDaikin2::toCommonSwingH(const uint8_t setting) {
  switch (setting) {
    case kDaikin2SwingHSwing:
    case kDaikin2SwingHAuto: return stdAc::swingh_t::kAuto;
    default: return stdAc::swingh_t::kOff;
  }
}

// Convert the A/C state to it's common equivalent.
stdAc::state_t IRDaikin2::toCommon(void) {
  stdAc::state_t result;
  result.protocol = decode_type_t::DAIKIN2;
  result.model = -1;  // No models used.
  result.power = this->getPower();
  result.mode = IRDaikinESP::toCommonMode(this->getMode());
  result.celsius = true;
  result.degrees = this->getTemp();
  result.fanspeed = IRDaikinESP::toCommonFanSpeed(this->getFan());
  result.swingv = this->toCommonSwingV(this->getSwingVertical());
  result.swingh = this->toCommonSwingH(this->getSwingHorizontal());
  result.quiet = this->getQuiet();
  result.light = this->getLight();
  result.turbo = this->getPowerful();
  result.clean = this->getMold();
  result.econo = this->getEcono();
  result.filter = this->getPurify();
  result.beep = this->getBeep();
  result.sleep = this->getSleepTimerEnabled() ? this->getSleepTime() : -1;
  // Not supported.
  result.clock = -1;
  return result;
}

// Convert the internal state into a human readable string.
String IRDaikin2::toString() {
  String result = "";
  result.reserve(310);  // Reserve some heap for the string to reduce fragging.
  result += IRutils::acBoolToString(getPower(), F("Power"), false);
  result += IRutils::acModeToString(getMode(), kDaikinAuto,
                                    kDaikinCool, kDaikinHeat,
                                    kDaikinDry, kDaikinFan);
  result += F(", Temp: ");
  result += uint64ToString(getTemp());
  result += F("C, Fan: ");
  result += uint64ToString(getFan());
  switch (getFan()) {
    case kDaikinFanAuto:
      result += F(" (Auto)");
      break;
    case kDaikinFanQuiet:
      result += F(" (Quiet)");
      break;
    case kDaikinFanMin:
      result += F(" (Min)");
      break;
    case kDaikinFanMax:
      result += F(" (Max)");
      break;
  }
  result += F(", Swing (V): ");
  result += uint64ToString(getSwingVertical());
  switch (getSwingVertical()) {
    case kDaikin2SwingVHigh:
      result += F(" (Highest)");
      break;
    case 2:
    case 3:
    case 4:
    case 5:
      break;
    case kDaikin2SwingVLow:
      result += F(" (Lowest)");
      break;
    case kDaikin2SwingVBreeze:
      result += F(" (Breeze)");
      break;
    case kDaikin2SwingVCirculate:
      result += F(" (Circulate)");
      break;
    case kDaikin2SwingVAuto:
      result += F(" (Auto)");
      break;
    default:
      result += F(" (Unknown)");
  }
  result += F(", Swing (H): ");
  result += uint64ToString(getSwingHorizontal());
  switch (getSwingHorizontal()) {
    case kDaikin2SwingHAuto:
      result += F(" (Auto)");
      break;
    case kDaikin2SwingHSwing:
      result += F(" (Swing)");
      break;
  }
  result += F(", Clock: ");
  result += IRDaikinESP::renderTime(getCurrentTime());
  result += F(", On Time: ");
  if (getOnTimerEnabled())
    result += IRDaikinESP::renderTime(getOnTime());
  else
    result += F("Off");
  result += F(", Off Time: ");
  if (getOffTimerEnabled())
    result += IRDaikinESP::renderTime(getOffTime());
  else
    result += F("Off");
  result += F(", Sleep Time: ");
  if (getSleepTimerEnabled())
    result += IRDaikinESP::renderTime(getSleepTime());
  else
    result += F("Off");
  result += F(", Beep: ");
  result += uint64ToString(getBeep());
  switch (getBeep()) {
    case kDaikinBeepLoud:
      result += F(" (Loud)");
      break;
    case kDaikinBeepQuiet:
      result += F(" (Quiet)");
      break;
    case kDaikinBeepOff:
      result += F(" (Off)");
      break;
    default:
      result += F(" (UNKNOWN)");
  }
  result += F(", Light: ");
  result += uint64ToString(getLight());
  switch (getLight()) {
    case kDaikinLightBright:
      result += F(" (Bright)");
      break;
    case kDaikinLightDim:
      result += F(" (Dim)");
      break;
    case kDaikinLightOff:
      result += F(" (Off)");
      break;
    default:
      result += F(" (UNKNOWN)");
  }
  result += IRutils::acBoolToString(getMold(), F("Mold"));
  result += IRutils::acBoolToString(getClean(), F("Clean"));
  result += F(", Fresh Air: ");
  if (getFreshAir())
    result += (getFreshAirHigh() ? "High" : "On");
  else
    result += F("Off");
  result += IRutils::acBoolToString(getEye(), F("Eye"));
  result += IRutils::acBoolToString(getEyeAuto(), F("Eye Auto"));
  result += IRutils::acBoolToString(getQuiet(), F("Quiet"));
  result += IRutils::acBoolToString(getPowerful(), F("Powerful"));
  result += IRutils::acBoolToString(getPurify(), F("Purify"));
  result += IRutils::acBoolToString(getEcono(), F("Econo"));
  return result;
}

#if DECODE_DAIKIN2
// Decode the supplied Daikin2 A/C message.
// Args:
//   results: Ptr to the data to decode and where to store the decode result.
//   nbits:   Nr. of bits to expect in the data portion. (kDaikin2Bits)
//   strict:  Flag to indicate if we strictly adhere to the specification.
// Returns:
//   boolean: True if it can decode it, false if it can't.
//
// Supported devices:
// - Daikin FTXZ25NV1B, FTXZ35NV1B, FTXZ50NV1B Aircon
// - Daikin ARC477A1 remote
//
// Status: BETA / Work as expected.
//
// Ref:
//   https://github.com/mharizanov/Daikin-AC-remote-control-over-the-Internet/tree/master/IRremote
bool IRrecv::decodeDaikin2(decode_results *results, uint16_t nbits,
                           bool strict) {
  if (results->rawlen < 2 * (nbits + kHeader + kFooter) + kHeader - 1)
    return false;

  // Compliance
  if (strict && nbits != kDaikin2Bits) return false;

  uint16_t offset = kStartOffset;
  const uint8_t ksectionSize[kDaikin2Sections] = {kDaikin2Section1Length,
                                                  kDaikin2Section2Length};

  // Leader
  if (!matchMark(results->rawbuf[offset++], kDaikin2LeaderMark,
                 kDaikin2Tolerance)) return false;
  if (!matchSpace(results->rawbuf[offset++], kDaikin2LeaderSpace,
                  kDaikin2Tolerance)) return false;

  // Sections
  uint16_t pos = 0;
  for (uint8_t section = 0; section < kDaikin2Sections; section++) {
    uint16_t used;
    // Section Header + Section Data + Section Footer
    used = matchGeneric(results->rawbuf + offset, results->state + pos,
                        results->rawlen - offset, ksectionSize[section] * 8,
                        kDaikin2HdrMark, kDaikin2HdrSpace,
                        kDaikin2BitMark, kDaikin2OneSpace,
                        kDaikin2BitMark, kDaikin2ZeroSpace,
                        kDaikin2BitMark, kDaikin2Gap,
                        section >= kDaikin2Sections - 1,
                        kDaikin2Tolerance, kDaikinMarkExcess, false);
    if (used == 0) return false;
    offset += used;
    pos += ksectionSize[section];
  }
  // Compliance
  if (strict) {
    // Re-check we got the correct size/length due to the way we read the data.
    if (pos * 8 != kDaikin2Bits) return false;
    // Validate the checksum.
    if (!IRDaikin2::validChecksum(results->state)) return false;
  }

  // Success
  results->decode_type = DAIKIN2;
  results->bits = nbits;
  // No need to record the state as we stored it as we decoded it.
  // As we use result->state, we don't record value, address, or command as it
  // is a union data type.
  return true;
}
#endif  // DECODE_DAIKIN2

#if SEND_DAIKIN216
// Send a Daikin 216 bit A/C message.
//
// Args:
//   data: An array of kDaikin216StateLength bytes containing the IR command.
//
// Status: Alpha/Untested on a real device.
//
// Supported devices:
// - Daikin ARC433B69 remote.
//
// Ref:
//   https://github.com/crankyoldgit/IRremoteESP8266/issues/689
//   https://github.com/danny-source/Arduino_DY_IRDaikin
void IRsend::sendDaikin216(const unsigned char data[], const uint16_t nbytes,
                           const uint16_t repeat) {
  if (nbytes < kDaikin216Section1Length)
    return;  // Not enough bytes to send a partial message.

  for (uint16_t r = 0; r <= repeat; r++) {
    // Section #1
    sendGeneric(kDaikin216HdrMark, kDaikin216HdrSpace, kDaikin216BitMark,
                kDaikin216OneSpace, kDaikin216BitMark, kDaikin216ZeroSpace,
                kDaikin216BitMark, kDaikin216Gap, data,
                kDaikin216Section1Length,
                kDaikin216Freq, false, 0, kDutyDefault);
    // Section #2
    sendGeneric(kDaikin216HdrMark, kDaikin216HdrSpace, kDaikin216BitMark,
                kDaikin216OneSpace, kDaikin216BitMark, kDaikin216ZeroSpace,
                kDaikin216BitMark, kDaikin216Gap,
                data + kDaikin216Section1Length,
                nbytes - kDaikin216Section1Length,
                kDaikin216Freq, false, 0, kDutyDefault);
  }
}
#endif  // SEND_DAIKIN216

// Class for handling Daikin 216 bit / 27 byte A/C messages.
//
// Code by crankyoldgit.
//
// Supported Remotes: Daikin ARC433B69 remote
//
// Ref:
//   https://github.com/crankyoldgit/IRremoteESP8266/issues/689
//   https://github.com/danny-source/Arduino_DY_IRDaikin
IRDaikin216::IRDaikin216(uint16_t pin) : _irsend(pin) { stateReset(); }

void IRDaikin216::begin() { _irsend.begin(); }

#if SEND_DAIKIN216
void IRDaikin216::send(const uint16_t repeat) {
  checksum();
  _irsend.sendDaikin216(remote_state, kDaikin216StateLength, repeat);
}
#endif  // SEND_DAIKIN216

// Verify the checksum is valid for a given state.
// Args:
//   state:  The array to verify the checksum of.
//   length: The size of the state.
// Returns:
//   A boolean.
bool IRDaikin216::validChecksum(uint8_t state[], const uint16_t length) {
  // Validate the checksum of section #1.
  if (length <= kDaikin216Section1Length - 1 ||
      state[kDaikin216Section1Length - 1] != sumBytes(
          state, kDaikin216Section1Length - 1))
    return false;
  // Validate the checksum of section #2 (a.k.a. the rest)
  if (length <= kDaikin216Section1Length + 1 ||
      state[length - 1] != sumBytes(state + kDaikin216Section1Length,
                                    length - kDaikin216Section1Length - 1))
    return false;
  return true;
}

// Calculate and set the checksum values for the internal state.
void IRDaikin216::checksum() {
  remote_state[kDaikin216Section1Length - 1] = sumBytes(
      remote_state, kDaikin216Section1Length - 1);
  remote_state[kDaikin216StateLength - 1] = sumBytes(
      remote_state + kDaikin216Section1Length, kDaikin216Section2Length - 1);
}

void IRDaikin216::stateReset() {
  for (uint8_t i = 0; i < kDaikin216StateLength; i++) remote_state[i] = 0x00;
  remote_state[0] =  0x11;
  remote_state[1] =  0xDA;
  remote_state[2] =  0x27;
  remote_state[3] =  0xF0;
  // remote_state[7] is a checksum byte, it will be set by checksum().
  remote_state[8] =  0x11;
  remote_state[9] =  0xDA;
  remote_state[10] = 0x27;
  remote_state[23] = 0xC0;
  // remote_state[26] is a checksum byte, it will be set by checksum().
}

uint8_t *IRDaikin216::getRaw() {
  checksum();  // Ensure correct settings before sending.
  return remote_state;
}

void IRDaikin216::setRaw(const uint8_t new_code[]) {
  for (uint8_t i = 0; i < kDaikin216StateLength; i++)
    remote_state[i] = new_code[i];
}


void IRDaikin216::on() {
  remote_state[kDaikin216BytePower] |= kDaikinBitPower;
}

void IRDaikin216::off() {
  remote_state[kDaikin216BytePower] &= ~kDaikinBitPower;
}

void IRDaikin216::setPower(const bool state) {
  if (state)
    on();
  else
    off();
}

bool IRDaikin216::getPower() {
  return remote_state[kDaikin216BytePower] & kDaikinBitPower;
}

uint8_t IRDaikin216::getMode() {
  return (remote_state[kDaikin216ByteMode] & kDaikin216MaskMode) >> 4;
}

void IRDaikin216::setMode(const uint8_t mode) {
  switch (mode) {
    case kDaikinAuto:
    case kDaikinCool:
    case kDaikinHeat:
    case kDaikinFan:
    case kDaikinDry:
      remote_state[kDaikin216ByteMode] &= ~kDaikin216MaskMode;
      remote_state[kDaikin216ByteMode] |= (mode << 4);
      break;
    default:
      this->setMode(kDaikinAuto);
  }
}

// Convert a standard A/C mode into its native mode.
uint8_t IRDaikin216::convertMode(const stdAc::opmode_t mode) {
  return IRDaikinESP::convertMode(mode);
}

// Set the temp in deg C
void IRDaikin216::setTemp(const uint8_t temp) {
  uint8_t degrees = std::max(temp, kDaikinMinTemp);
  degrees = std::min(degrees, kDaikinMaxTemp);
  remote_state[kDaikin216ByteTemp] &= ~kDaikin216MaskTemp;
  remote_state[kDaikin216ByteTemp] |= (degrees << 1);
}

uint8_t IRDaikin216::getTemp(void) {
  return (remote_state[kDaikin216ByteTemp] & kDaikin216MaskTemp) >> 1;
}

// Set the speed of the fan, 1-5 or kDaikinFanAuto or kDaikinFanQuiet
void IRDaikin216::setFan(const uint8_t fan) {
  // Set the fan speed bits, leave low 4 bits alone
  uint8_t fanset;
  if (fan == kDaikinFanQuiet || fan == kDaikinFanAuto)
    fanset = fan;
  else if (fan < kDaikinFanMin || fan > kDaikinFanMax)
    fanset = kDaikinFanAuto;
  else
    fanset = 2 + fan;
  remote_state[kDaikin216ByteFan] &= ~kDaikin216MaskFan;
  remote_state[kDaikin216ByteFan] |= (fanset << 4);
}

uint8_t IRDaikin216::getFan() {
  uint8_t fan = remote_state[kDaikin216ByteFan] >> 4;
  if (fan != kDaikinFanQuiet && fan != kDaikinFanAuto) fan -= 2;
  return fan;
}

// Convert a standard A/C Fan speed into its native fan speed.
uint8_t IRDaikin216::convertFan(const stdAc::fanspeed_t speed) {
  return IRDaikinESP::convertFan(speed);
}

void IRDaikin216::setSwingVertical(const bool on) {
  if (on)
    remote_state[kDaikin216ByteSwingV] |= kDaikin216MaskSwingV;
  else
    remote_state[kDaikin216ByteSwingV] &= ~kDaikin216MaskSwingV;
}

bool IRDaikin216::getSwingVertical(void) {
  return remote_state[kDaikin216ByteSwingV] & kDaikin216MaskSwingV;
}

void IRDaikin216::setSwingHorizontal(const bool on) {
  if (on)
    remote_state[kDaikin216ByteSwingH] |= kDaikin216MaskSwingH;
  else
    remote_state[kDaikin216ByteSwingH] &= ~kDaikin216MaskSwingH;
}

bool IRDaikin216::getSwingHorizontal(void) {
  return remote_state[kDaikin216ByteSwingH] & kDaikin216MaskSwingH;
}

// This is a horrible hack till someone works out the quiet mode bit.
void IRDaikin216::setQuiet(const bool on) {
  if (on) {
    this->setFan(kDaikinFanQuiet);
    // Powerful & Quiet mode being on are mutually exclusive.
    this->setPowerful(false);
  } else if (this->getFan() == kDaikinFanQuiet) {
    this->setFan(kDaikinFanAuto);
  }
}

// This is a horrible hack till someone works out the quiet mode bit.
bool IRDaikin216::getQuiet(void) {
  return this->getFan() == kDaikinFanQuiet;
}

void IRDaikin216::setPowerful(const bool on) {
  if (on) {
    remote_state[kDaikin216BytePowerful] |= kDaikinBitPowerful;
    // Powerful & Quiet mode being on are mutually exclusive.
    this->setQuiet(false);
  } else {
    remote_state[kDaikin216BytePowerful] &= ~kDaikinBitPowerful;
  }
}

bool IRDaikin216::getPowerful() {
  return remote_state[kDaikin216BytePowerful] & kDaikinBitPowerful;
}

// Convert the A/C state to it's common equivalent.
stdAc::state_t IRDaikin216::toCommon(void) {
  stdAc::state_t result;
  result.protocol = decode_type_t::DAIKIN216;
  result.model = -1;  // No models used.
  result.power = this->getPower();
  result.mode = IRDaikinESP::toCommonMode(this->getMode());
  result.celsius = true;
  result.degrees = this->getTemp();
  result.fanspeed = IRDaikinESP::toCommonFanSpeed(this->getFan());
  result.swingv = this->getSwingVertical() ? stdAc::swingv_t::kAuto :
                                             stdAc::swingv_t::kOff;
  result.swingh = this->getSwingHorizontal() ? stdAc::swingh_t::kAuto :
                                               stdAc::swingh_t::kOff;
  result.quiet = this->getQuiet();
  result.turbo = this->getPowerful();
  // Not supported.
  result.light = false;
  result.clean = false;
  result.econo = false;
  result.filter = false;
  result.beep = false;
  result.sleep = -1;
  result.clock = -1;
  return result;
}

// Convert the internal state into a human readable string.
String IRDaikin216::toString() {
  String result = "";
  result.reserve(120);  // Reserve some heap for the string to reduce fragging.
  result += IRutils::acBoolToString(getPower(), F("Power"), false);
  result += IRutils::acModeToString(getMode(), kDaikinAuto,
                                    kDaikinCool, kDaikinHeat,
                                    kDaikinDry, kDaikinFan);
  result += F(", Temp: ");
  result += uint64ToString(this->getTemp());
  result += F("C, Fan: ");
  result += uint64ToString(this->getFan());
  switch (this->getFan()) {
    case kDaikinFanAuto:
      result += F(" (AUTO)");
      break;
    case kDaikinFanQuiet:
      result += F(" (QUIET)");
      break;
    case kDaikinFanMin:
      result += F(" (MIN)");
      break;
    case kDaikinFanMax:
      result += F(" (MAX)");
      break;
  }
  result += IRutils::acBoolToString(getSwingHorizontal(),
                                    F("Swing (Horizontal)"));
  result += IRutils::acBoolToString(getSwingVertical(), F("Swing (Vertical)"));
  result += IRutils::acBoolToString(getQuiet(), F("Quiet"));
  result += IRutils::acBoolToString(getPowerful(), F("Powerful"));
  return result;
}

#if DECODE_DAIKIN216
// Decode the supplied Daikin 216 bit A/C message.
// Args:
//   results: Ptr to the data to decode and where to store the decode result.
//   nbits:   Nr. of bits to expect in the data portion. (kDaikin216Bits)
//   strict:  Flag to indicate if we strictly adhere to the specification.
// Returns:
//   boolean: True if it can decode it, false if it can't.
//
// Supported devices:
// - Daikin ARC433B69 remote.
//
// Status: BETA / Should be working.
//
// Ref:
//   https://github.com/crankyoldgit/IRremoteESP8266/issues/689
//   https://github.com/danny-source/Arduino_DY_IRDaikin
bool IRrecv::decodeDaikin216(decode_results *results, const uint16_t nbits,
                             const bool strict) {
  if (results->rawlen < 2 * (nbits + kHeader + kFooter) - 1)
    return false;

  // Compliance
  if (strict && nbits != kDaikin216Bits) return false;

  uint16_t offset = kStartOffset;
  const uint8_t ksectionSize[kDaikin216Sections] = {kDaikin216Section1Length,
                                                    kDaikin216Section2Length};
  // Sections
  uint16_t pos = 0;
  for (uint8_t section = 0; section < kDaikin216Sections; section++) {
    uint16_t used;
    // Section Header + Section Data + Section Footer
    used = matchGeneric(results->rawbuf + offset, results->state + pos,
                        results->rawlen - offset, ksectionSize[section] * 8,
                        kDaikin216HdrMark, kDaikin216HdrSpace,
                        kDaikin216BitMark, kDaikin216OneSpace,
                        kDaikin216BitMark, kDaikin216ZeroSpace,
                        kDaikin216BitMark, kDaikin216Gap,
                        section >= kDaikin216Sections - 1,
                        kDaikinTolerance, kDaikinMarkExcess, false);
    if (used == 0) return false;
    offset += used;
    pos += ksectionSize[section];
  }
  // Compliance
  if (strict) {
    if (pos * 8 != kDaikin216Bits) return false;
    // Validate the checksum.
    if (!IRDaikin216::validChecksum(results->state)) return false;
  }

  // Success
  results->decode_type = decode_type_t::DAIKIN216;
  results->bits = nbits;
  // No need to record the state as we stored it as we decoded it.
  // As we use result->state, we don't record value, address, or command as it
  // is a union data type.
  return true;
}
#endif  // DECODE_DAIKIN216

#if SEND_DAIKIN160
// Send a Daikin 160 bit A/C message.
//
// Args:
//   data: An array of kDaikin160StateLength bytes containing the IR command.
//
// Status: STABLE / Confirmed working.
//
// Supported devices:
// - Daikin ARC423A5 remote.
//
// Ref:
//   https://github.com/crankyoldgit/IRremoteESP8266/issues/731
void IRsend::sendDaikin160(const unsigned char data[], const uint16_t nbytes,
                           const uint16_t repeat) {
  if (nbytes < kDaikin160Section1Length)
    return;  // Not enough bytes to send a partial message.

  for (uint16_t r = 0; r <= repeat; r++) {
    // Section #1
    sendGeneric(kDaikin160HdrMark, kDaikin160HdrSpace, kDaikin160BitMark,
                kDaikin160OneSpace, kDaikin160BitMark, kDaikin160ZeroSpace,
                kDaikin160BitMark, kDaikin160Gap, data,
                kDaikin160Section1Length,
                kDaikin160Freq, false, 0, kDutyDefault);
    // Section #2
    sendGeneric(kDaikin160HdrMark, kDaikin160HdrSpace, kDaikin160BitMark,
                kDaikin160OneSpace, kDaikin160BitMark, kDaikin160ZeroSpace,
                kDaikin160BitMark, kDaikin160Gap,
                data + kDaikin160Section1Length,
                nbytes - kDaikin160Section1Length,
                kDaikin160Freq, false, 0, kDutyDefault);
  }
}
#endif  // SEND_DAIKIN160

// Class for handling Daikin 160 bit / 20 byte A/C messages.
//
// Code by crankyoldgit.
//
// Supported Remotes: Daikin ARC423A5 remote
//
// Ref:
//   https://github.com/crankyoldgit/IRremoteESP8266/issues/731
IRDaikin160::IRDaikin160(uint16_t pin) : _irsend(pin) { stateReset(); }

void IRDaikin160::begin() { _irsend.begin(); }

// Verify the checksum is valid for a given state.
// Args:
//   state:  The array to verify the checksum of.
//   length: The size of the state.
// Returns:
//   A boolean.
bool IRDaikin160::validChecksum(uint8_t state[], const uint16_t length) {
  // Validate the checksum of section #1.
  if (length <= kDaikin160Section1Length - 1 ||
      state[kDaikin160Section1Length - 1] != sumBytes(
          state, kDaikin160Section1Length - 1))
    return false;
  // Validate the checksum of section #2 (a.k.a. the rest)
  if (length <= kDaikin160Section1Length + 1 ||
      state[length - 1] != sumBytes(state + kDaikin160Section1Length,
                                    length - kDaikin160Section1Length - 1))
    return false;
  return true;
}

// Calculate and set the checksum values for the internal state.
void IRDaikin160::checksum() {
  remote_state[kDaikin160Section1Length - 1] = sumBytes(
      remote_state, kDaikin160Section1Length - 1);
  remote_state[kDaikin160StateLength - 1] = sumBytes(
      remote_state + kDaikin160Section1Length, kDaikin160Section2Length - 1);
}

void IRDaikin160::stateReset() {
  for (uint8_t i = 0; i < kDaikin160StateLength; i++) remote_state[i] = 0x00;
  remote_state[0] =  0x11;
  remote_state[1] =  0xDA;
  remote_state[2] =  0x27;
  remote_state[3] =  0xF0;
  remote_state[4] =  0x0D;
  // remote_state[6] is a checksum byte, it will be set by checksum().
  remote_state[7] =  0x11;
  remote_state[8] =  0xDA;
  remote_state[9] =  0x27;
  remote_state[11] = 0xD3;
  remote_state[12] = 0x30;
  remote_state[13] = 0x11;
  remote_state[16] = 0x1E;
  remote_state[17] = 0x0A;
  remote_state[18] = 0x08;
  // remote_state[19] is a checksum byte, it will be set by checksum().
}

uint8_t *IRDaikin160::getRaw() {
  checksum();  // Ensure correct settings before sending.
  return remote_state;
}

void IRDaikin160::setRaw(const uint8_t new_code[]) {
  for (uint8_t i = 0; i < kDaikin160StateLength; i++)
    remote_state[i] = new_code[i];
}

#if SEND_DAIKIN160
void IRDaikin160::send(const uint16_t repeat) {
  checksum();
  _irsend.sendDaikin160(remote_state, kDaikin160StateLength, repeat);
}
#endif  // SEND_DAIKIN160

void IRDaikin160::on() {
  remote_state[kDaikin160BytePower] |= kDaikinBitPower;
}

void IRDaikin160::off() {
  remote_state[kDaikin160BytePower] &= ~kDaikinBitPower;
}

void IRDaikin160::setPower(const bool state) {
  if (state)
    on();
  else
    off();
}

bool IRDaikin160::getPower() {
  return remote_state[kDaikin160BytePower] & kDaikinBitPower;
}

uint8_t IRDaikin160::getMode() {
  return (remote_state[kDaikin160ByteMode] & kDaikin160MaskMode) >> 4;
}

void IRDaikin160::setMode(const uint8_t mode) {
  switch (mode) {
    case kDaikinAuto:
    case kDaikinCool:
    case kDaikinHeat:
    case kDaikinFan:
    case kDaikinDry:
      remote_state[kDaikin160ByteMode] &= ~kDaikin160MaskMode;
      remote_state[kDaikin160ByteMode] |= (mode << 4);
      break;
    default:
      this->setMode(kDaikinAuto);
  }
}

// Convert a standard A/C mode into its native mode.
uint8_t IRDaikin160::convertMode(const stdAc::opmode_t mode) {
  return IRDaikinESP::convertMode(mode);
}

// Set the temp in deg C
void IRDaikin160::setTemp(const uint8_t temp) {
  uint8_t degrees = std::max(temp, kDaikinMinTemp);
  degrees = std::min(degrees, kDaikinMaxTemp) * 2 - 20;
  remote_state[kDaikin160ByteTemp] &= ~kDaikin160MaskTemp;
  remote_state[kDaikin160ByteTemp] |= degrees;
}

uint8_t IRDaikin160::getTemp(void) {
  return (((remote_state[kDaikin160ByteTemp] & kDaikin160MaskTemp) / 2 ) + 10);
}

// Set the speed of the fan, 1-5 or kDaikinFanAuto or kDaikinFanQuiet
void IRDaikin160::setFan(const uint8_t fan) {
  uint8_t fanset;
  if (fan == kDaikinFanQuiet || fan == kDaikinFanAuto)
    fanset = fan;
  else if (fan < kDaikinFanMin || fan > kDaikinFanMax)
    fanset = kDaikinFanAuto;
  else
    fanset = 2 + fan;
  // Set the fan speed bits, leave *upper* 4 bits alone
  remote_state[kDaikin160ByteFan] &= ~kDaikin160MaskFan;
  remote_state[kDaikin160ByteFan] |= fanset;
}

uint8_t IRDaikin160::getFan() {
  uint8_t fan = remote_state[kDaikin160ByteFan] & kDaikin160MaskFan;
  if (fan != kDaikinFanQuiet && fan != kDaikinFanAuto) fan -= 2;
  return fan;
}

// Convert a standard A/C Fan speed into its native fan speed.
uint8_t IRDaikin160::convertFan(const stdAc::fanspeed_t speed) {
  switch (speed) {
    case stdAc::fanspeed_t::kMin: return kDaikinFanMin;
    case stdAc::fanspeed_t::kLow: return kDaikinFanMin + 1;
    case stdAc::fanspeed_t::kMedium: return kDaikinFanMin + 2;
    case stdAc::fanspeed_t::kHigh: return kDaikinFanMax - 1;
    case stdAc::fanspeed_t::kMax: return kDaikinFanMax;
    default:
      return kDaikinFanAuto;
  }
}

void IRDaikin160::setSwingVertical(const uint8_t position) {
  switch (position) {
    case kDaikin160SwingVLowest:
    case kDaikin160SwingVLow:
    case kDaikin160SwingVMiddle:
    case kDaikin160SwingVHigh:
    case kDaikin160SwingVHighest:
    case kDaikin160SwingVAuto:
      remote_state[kDaikin160ByteSwingV] &= ~kDaikin160MaskSwingV;
      remote_state[kDaikin160ByteSwingV] |= (position << 4);
      break;
    default:
      setSwingVertical(kDaikin160SwingVAuto);
  }
}

uint8_t IRDaikin160::getSwingVertical(void) {
  return remote_state[kDaikin160ByteSwingV] >> 4;
}

// Convert a standard A/C vertical swing into its native version.
uint8_t IRDaikin160::convertSwingV(const stdAc::swingv_t position) {
  switch (position) {
    case stdAc::swingv_t::kHighest:
    case stdAc::swingv_t::kHigh:
    case stdAc::swingv_t::kMiddle:
    case stdAc::swingv_t::kLow:
    case stdAc::swingv_t::kLowest:
      return kDaikin160SwingVHighest + 1 - (uint8_t)position;
    default:
      return kDaikin160SwingVAuto;
  }
}

// Convert a native vertical swing to it's common equivalent.
stdAc::swingv_t IRDaikin160::toCommonSwingV(const uint8_t setting) {
  switch (setting) {
    case kDaikin160SwingVHighest: return stdAc::swingv_t::kHighest;
    case kDaikin160SwingVHigh:    return stdAc::swingv_t::kHigh;
    case kDaikin160SwingVMiddle:  return stdAc::swingv_t::kMiddle;
    case kDaikin160SwingVLow:     return stdAc::swingv_t::kLow;
    case kDaikin160SwingVLowest:  return stdAc::swingv_t::kLowest;
    default:
      return stdAc::swingv_t::kAuto;
  }
}

// Convert the A/C state to it's common equivalent.
stdAc::state_t IRDaikin160::toCommon(void) {
  stdAc::state_t result;
  result.protocol = decode_type_t::DAIKIN160;
  result.model = -1;  // No models used.
  result.power = this->getPower();
  result.mode = IRDaikinESP::toCommonMode(this->getMode());
  result.celsius = true;
  result.degrees = this->getTemp();
  result.fanspeed = IRDaikinESP::toCommonFanSpeed(this->getFan());
  result.swingv = this->toCommonSwingV(this->getSwingVertical());
  // Not supported.
  result.swingh = stdAc::swingh_t::kOff;
  result.quiet = false;
  result.turbo = false;
  result.light = false;
  result.clean = false;
  result.econo = false;
  result.filter = false;
  result.beep = false;
  result.sleep = -1;
  result.clock = -1;
  return result;
}

// Convert the internal state into a human readable string.
String IRDaikin160::toString() {
  String result = "";
  result.reserve(150);  // Reserve some heap for the string to reduce fragging.
  result += F("Power: ");
  if (this->getPower())
    result += F("On");
  else
    result += F("Off");
  result += F(", Mode: ");
  result += uint64ToString(this->getMode());
  switch (getMode()) {
    case kDaikinAuto:
      result += F(" (AUTO)");
      break;
    case kDaikinCool:
      result += F(" (COOL)");
      break;
    case kDaikinHeat:
      result += F(" (HEAT)");
      break;
    case kDaikinDry:
      result += F(" (DRY)");
      break;
    case kDaikinFan:
      result += F(" (FAN)");
      break;
    default:
      result += F(" (UNKNOWN)");
  }
  result += F(", Temp: ");
  result += uint64ToString(this->getTemp());
  result += F("C, Fan: ");
  result += uint64ToString(this->getFan());
  switch (this->getFan()) {
    case kDaikinFanAuto:
      result += F(" (AUTO)");
      break;
    case kDaikinFanQuiet:
      result += F(" (QUIET)");
      break;
    case kDaikinFanMin:
      result += F(" (MIN)");
      break;
    case kDaikinFanMax:
      result += F(" (MAX)");
      break;
  }
  result += F(", Vent Position (V): ");
  result += uint64ToString(getSwingVertical());
  switch (getSwingVertical()) {
    case kDaikin160SwingVHighest: result += F(" (Highest)"); break;
    case kDaikin160SwingVHigh:    result += F(" (High)"); break;
    case kDaikin160SwingVMiddle:  result += F(" (Middle)"); break;
    case kDaikin160SwingVLow:     result += F(" (Low)"); break;
    case kDaikin160SwingVLowest:  result += F(" (Lowest)"); break;
    case kDaikin160SwingVAuto:    result += F(" (Auto)"); break;
    default:
      result += F(" (Unknown)");
  }
  return result;
}

#if DECODE_DAIKIN160
// Decode the supplied Daikin 160 bit A/C message.
// Args:
//   results: Ptr to the data to decode and where to store the decode result.
//   nbits:   Nr. of bits to expect in the data portion. (kDaikin160Bits)
//   strict:  Flag to indicate if we strictly adhere to the specification.
// Returns:
//   boolean: True if it can decode it, false if it can't.
//
// Supported devices:
// - Daikin ARC423A5 remote.
//
// Status: STABLE / Confirmed working.
//
// Ref:
//   https://github.com/crankyoldgit/IRremoteESP8266/issues/731
bool IRrecv::decodeDaikin160(decode_results *results, const uint16_t nbits,
                             const bool strict) {
  if (results->rawlen < 2 * (nbits + kHeader + kFooter) - 1)
    return false;

  // Compliance
  if (strict && nbits != kDaikin160Bits) return false;

  uint16_t offset = kStartOffset;
  const uint8_t ksectionSize[kDaikin160Sections] = {kDaikin160Section1Length,
                                                    kDaikin160Section2Length};

  // Sections
  uint16_t pos = 0;
  for (uint8_t section = 0; section < kDaikin160Sections; section++) {
    uint16_t used;
    // Section Header + Section Data (7 bytes) + Section Footer
    used = matchGeneric(results->rawbuf + offset, results->state + pos,
                        results->rawlen - offset, ksectionSize[section] * 8,
                        kDaikin160HdrMark, kDaikin160HdrSpace,
                        kDaikin160BitMark, kDaikin160OneSpace,
                        kDaikin160BitMark, kDaikin160ZeroSpace,
                        kDaikin160BitMark, kDaikin160Gap,
                        section >= kDaikin160Sections - 1,
                        kDaikinTolerance, kDaikinMarkExcess, false);
    if (used == 0) return false;
    offset += used;
    pos += ksectionSize[section];
  }
  // Compliance
  if (strict) {
    // Validate the checksum.
    if (!IRDaikin160::validChecksum(results->state)) return false;
  }

  // Success
  results->decode_type = decode_type_t::DAIKIN160;
  results->bits = nbits;
  // No need to record the state as we stored it as we decoded it.
  // As we use result->state, we don't record value, address, or command as it
  // is a union data type.
  return true;
}
#endif  // DECODE_DAIKIN160
