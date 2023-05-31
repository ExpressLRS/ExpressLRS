#pragma once

#include "stdint.h"

#define BAND_NAME_LENGTH                8
#define IS_FACTORY_BAND                 1
#define CHANNEL_COUNT                   8
#define FREQ_TABLE_SIZE 48

#define RACE_MODE                       2
#define RACE_MODE_POWER                 14 // dBm
#define NUM_POWER_LEVELS                5
#define POWER_LEVEL_LABEL_LENGTH        3

const uint8_t channelFreqLabel[FREQ_TABLE_SIZE] = {
    'B', 'A', 'N', 'D', '_', 'A', ' ', ' ', // A
    'B', 'A', 'N', 'D', '_', 'B', ' ', ' ', // B
    'B', 'A', 'N', 'D', '_', 'E', ' ', ' ', // E
    'F', 'A', 'T', 'S', 'H', 'A', 'R', 'K', // F
    'R', 'A', 'C', 'E', ' ', ' ', ' ', ' ', // R
    'R', 'A', 'C', 'E', '_', 'L', 'O', 'W', // L
};

const uint8_t bandLetter[6] = {'A', 'B', 'E', 'F', 'R', 'L'};

const uint16_t channelFreqTable[FREQ_TABLE_SIZE] = {
    5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725, // A
    5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866, // B
    5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945, // E
    5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880, // F
    5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917, // R
    5362, 5399, 5436, 5473, 5510, 5547, 5584, 5621  // L
};

const uint8_t powerLevelsLut[NUM_POWER_LEVELS] = {1, RACE_MODE, 14, 20, 26};

const uint8_t powerLevelsLabel[NUM_POWER_LEVELS * POWER_LEVEL_LABEL_LENGTH] = {'0', ' ', ' ',
                                                                         'R', 'C', 'E',
                                                                         '2', '5', ' ',
                                                                         '1', '0', '0',
                                                                         '4', '0', '0'};

uint8_t getFreqTableChannels(void);
uint8_t getFreqTableBands(void);
uint16_t getFreqByIdx(uint8_t idx);
uint8_t channelFreqLabelByIdx(uint8_t idx);
uint8_t getBandLetterByIdx(uint8_t idx);
