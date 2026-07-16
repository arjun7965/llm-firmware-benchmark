# Trusted Timer Capture/Compare Reference

The validator-owned implementation applies the declared half-range rule to
simultaneous capture and overflow status, retains one capture event, and arms a
bounded compare deadline that survives a 16-bit counter wrap.
