# Trusted Reference

`stream_decoder.rs` incrementally retains at most one bounded frame candidate.
It scans each input byte once in normal operation and rescans only a rejected
candidate while searching for the next possible magic prefix.
