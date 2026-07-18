# Trusted Reference

The reference performs bounded, rate-monotonic release decisions without a
catch-up loop. A late dispatch releases each task at most once, gives it a fresh
relative deadline, and schedules its next period from the dispatch tick.
