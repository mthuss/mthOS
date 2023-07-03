# rothOS
 The **ro**drigo and m**th**us **O**perating **S**ystem:

An Operating System simulator written in C

<br>

## About the program file structure
the synthetic program files must have lines separated exclusively by Line Feeds (no carriage returns allowed)
#### How to check for Carriage Returns
you can run the following command to check if your program contains CR characters
```bash
xxd -b <programFilename> | grep 00001101 &> /dev/null && echo "has CRs" || echo "has no CRs"
```

#### Remove Carriage Returns from file
To remove all carriage return characters from your program files, run the following command:
```bash
sed -i s/$(echo -e "\x0D")//g <programFilename>
```
