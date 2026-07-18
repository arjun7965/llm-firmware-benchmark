# Trusted Reference

The reference creates one bounded FIFO and a count-matched semaphore. It sends
before giving, waits on a bounded semaphore operation before receiving, and
does not spin, allocate, or retry behind the RTOS boundary.
