/**
 * LTC298X
 * 
 * Copyright 2020 pegrei [https://github.com/pegrei]
 * 
 * Based on Linduino.
 * Copyright 2018 Analog Devices, Inc.
 * 
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#include "LTC298X.h"

/* -------------------------------------------------------------------------- */
/*                                   PRIVATE                                  */
/* -------------------------------------------------------------------------- */
/* -------------------------- SPI RAM data transfer ------------------------- */
uint16_t LTC298X::get_start_address(uint16_t base_address, uint8_t channel_number)
{
    return base_address + 4 * (channel_number-1);
}

void LTC298X::spi_transfer_block(uint8_t *tx, uint8_t *rx, uint8_t length)
{
    int8_t i;

    digitalWrite(_cs, LOW);

    for (i=(length-1);  i >= 0; i--)
    rx[i] = SPI.transfer(tx[i]);

    digitalWrite(_cs, HIGH);
}

/* -------------------------------------------------------------------------- */
/*                                   PUBLIC                                   */
/* -------------------------------------------------------------------------- */
uint32_t LTC298X::transfer_four_bytes(uint8_t ram_read_or_write, uint16_t start_address, uint32_t input_data)
{
    uint32_t output_data;
    uint8_t tx[7], rx[7];

    tx[6] = ram_read_or_write;
    tx[5] = highByte(start_address);
    tx[4] = lowByte(start_address);
    tx[3] = (uint8_t)(input_data >> 24);
    tx[2] = (uint8_t)(input_data >> 16);
    tx[1] = (uint8_t)(input_data >> 8);
    tx[0] = (uint8_t) input_data;

    this->spi_transfer_block(tx, rx, 7);

    output_data = (uint32_t) rx[3] << 24 |
                  (uint32_t) rx[2] << 16 |
                  (uint32_t) rx[1] << 8  |
                  (uint32_t) rx[0];

    return output_data;
}

uint8_t LTC298X::transfer_byte(uint8_t ram_read_or_write, uint16_t start_address, uint8_t input_data)
{
    uint8_t tx[4], rx[4];

    tx[3] = ram_read_or_write;
    tx[2] = (uint8_t)(start_address >> 8);
    tx[1] = (uint8_t)start_address;
    tx[0] = input_data;
    this->spi_transfer_block(tx, rx, 4);
    return rx[0];
}

LTC298X::LTC298X(uint8_t cs, uint8_t interrupt)
{
    this->_cs = cs;
    this->_interrupt = interrupt;
}

void LTC298X::begin(void)
{
	digitalWrite(_cs, HIGH);
	pinMode(_cs, OUTPUT);
  pinMode(_interrupt, INPUT);
	SPI.begin();
}

/* ---------------------------------- Setup --------------------------------- */
void LTC298X::assign_channel(uint8_t channel_number, uint32_t channel_assignment_data)
{
    uint16_t start_address = get_start_address(CH_ADDRESS_BASE, channel_number);
    this->transfer_four_bytes(WRITE_TO_RAM, start_address, channel_assignment_data);
}

uint32_t LTC298X::get_channel_config(uint8_t channel_number)
{
    uint16_t start_address = get_start_address(CH_ADDRESS_BASE, channel_number);
    uint32_t config = transfer_four_bytes(READ_FROM_RAM, start_address, 0);

    return config;
}

/* ----------------------------- Measure channel ---------------------------- */
void LTC298X::select_conversion_channels(uint32_t channels)
{
  this->transfer_four_bytes(WRITE_TO_RAM, MULTI_CONV_MASK, channels);
}

void LTC298X::start_multi_conversion(void)
{
  this->transfer_byte(WRITE_TO_RAM, COMMAND_STATUS_REGISTER, CONVERSION_CONTROL_BYTE);
}

void LTC298X::convert_channel(uint8_t channel_number)
{
  this->transfer_byte(WRITE_TO_RAM, COMMAND_STATUS_REGISTER, CONVERSION_CONTROL_BYTE | channel_number);
}

void LTC298X::wait_for_process_to_finish(void)
{
  bool process_finished = 0;
  while (process_finished == 0)
  {
    process_finished  = digitalRead(_interrupt);
  }
}

bool LTC298X::is_finished(void)
{
  bool process_finished  = digitalRead(_interrupt);
  return process_finished;
}

void LTC298X::measure_channel(uint8_t channel_number, uint32_t &data, uint8_t &fault)
{
  this->transfer_byte(WRITE_TO_RAM, COMMAND_STATUS_REGISTER, CONVERSION_CONTROL_BYTE | channel_number);
  this->wait_for_process_to_finish();
  this->get_result(channel_number, data, fault);
}

/* ------------------------------- Get results ------------------------------ */
void LTC298X::get_result(uint8_t channel_number, uint32_t &data, uint8_t &fault)
{
  uint32_t raw;
  uint16_t start_address = get_start_address(CONVERSION_RESULT_MEMORY_BASE, channel_number);

  raw = transfer_four_bytes(READ_FROM_RAM, start_address, 0);
  // 24 LSB's are conversion result
  data = raw & 0xFFFFFF;
  // 8 MSB's show the fault data
  fault = raw >> 24;
}

void LTC298X::get_raw_result(uint8_t channel_number, int32_t &data)
{
  uint16_t start_address = get_start_address(VOUT_CH_BASE, channel_number);

  data = transfer_four_bytes(READ_FROM_RAM, start_address, 0);
}

void LTC298X::print_fault_data(uint8_t fault_byte)
{
  Serial.print(F("  FAULT DATA = "));
  Serial.println(fault_byte, BIN);

  if (fault_byte & SENSOR_HARD_FAILURE)
    Serial.println(F("  - SENSOR HARD FALURE"));
  if (fault_byte & ADC_HARD_FAILURE)
    Serial.println(F("  - ADC_HARD_FAILURE"));
  if (fault_byte & CJ_HARD_FAILURE)
    Serial.println(F("  - CJ_HARD_FAILURE"));
  if (fault_byte & CJ_SOFT_FAILURE)
    Serial.println(F("  - CJ_SOFT_FAILURE"));
  if (fault_byte & SENSOR_ABOVE)
    Serial.println(F("  - SENSOR_ABOVE"));
  if (fault_byte & SENSOR_BELOW)
    Serial.println(F("  - SENSOR_BELOW"));
  if (fault_byte & ADC_RANGE_ERROR)
    Serial.println(F("  - ADC_RANGE_ERROR"));
  if (!(fault_byte & VALID))
    Serial.println(F("INVALID READING !!!!!!"));
  if (fault_byte == 0b11111111)
    Serial.println(F("CONFIGURATION ERROR !!!!!!"));
}