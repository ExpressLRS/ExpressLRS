# FHSS channel exclusions

A regulatory domain can define inclusive ranges of channel indexes to exclude
from its regular frequency grid. `freq_count` remains the original grid size;
the runtime channel count is the number left after exclusions.

At initialization, FHSS builds a compact map from retained-channel indexes to
original grid indexes. It selects sync from the retained channels and builds
the existing pseudorandom sequence using the retained count. Every hop block
contains each retained channel once. Domains without exclusions keep their
existing frequencies and sequences.

Normal hops, initial/sync frequencies, same-band Gemini, and dual-band Gemini
all use the same mapping. Same-band Gemini offsets by half of the retained
channel count, rather than half of the original frequency grid. Consequently,
its physical frequency separation can vary across a split band.

Frequency lookup remains constant-time: a map lookup, when exclusions exist,
followed by the existing frequency calculation. No exclusion searches occur
in the hopping path. The map costs 80 bytes per band (160 bytes on dual-band
radios). Metadata is rejected if the original grid exceeds 80 channels, ranges
are reversed or outside the grid, or fewer than two channels remain. Invalid
metadata stops initialization rather than enabling excluded frequencies.

The original frequency bounds remain available to radio initialization and
calibration. A domain's `freq_center`, also used by the continuous-wave test,
must be chosen in an allowed range separately.

Exclusions do not establish RF compliance: channel bandwidth, frequency
correction, band-edge emissions, dwell time and output power still need to be
evaluated for the selected domain and hardware.
