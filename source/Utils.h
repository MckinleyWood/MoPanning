#pragma once

struct FrequencyBand 
{
    float frequency; // Band frequency in Hertz
    float amplitude; // Range 0 - 1 ...?
    float pan_index; // -1 = far left, +1 = far right
    int trackIndex;
};

struct TrackSlot
{
    std::array<std::vector<FrequencyBand>, 2> buffers;
    std::atomic<int> activeIndex { 0 }; // Which buffer reader should use
};
