# rothOS
 The **ro**drigo and m**th**us **O**perating **S**ystem:

An Operating System kernel simulation written in C

<br>

## About the program file structure
the synthetic program files must have lines separated exclusively by Line Feeds (no carriage returns allowed)
#### how to check for Carriage Returns
you can run the following command to check if your program contains CR characters
```bash
xxd -b <programFilename> | grep 00001101 &> /dev/null && echo "has CRs" || echo "has no CRs"
```
