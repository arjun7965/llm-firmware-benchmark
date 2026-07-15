# Trusted Reference

The reference creates a single priority-inheritance mutex during startup,
forwards the required telemetry and safety wait policies once each, and leaves
ownership and priority donation to the supplied RTOS mutex.
