# SIDER Compiler Tool

Designed to generate a database of strings. There are several ways to generate such a database:

- merging - From one or more existing databases
- building - From one or more C++ source files
- mixed - Combination of merging and building

This utility searches for strings ``SID("text")`` in C++ files and generates a new String Id from that text. The format of the database file is simple, in text form, one line per StringId, which are in the following format:

``STRING_ID text``.

An example of the file is shown below:

```
0F182EC3 float
41F1B620 let
49616B19 print
A033F4CF export
A2DBC5A5 label
AAEFA679 native
ACDF5E8A file
B41C89F1 define
C7CB275C int32
E12771E6 lambda
E4C86BEE println
FFF840ED import
```


## Usage:

The processing of two C++ files is shown below:

```sider FILE1 FILE2```

The following options are allowed:

- ```--help```     display this help and exit;
- ```--version```  output version information and exit
- ```-i```, ```--input``` The source database
- ```-o```, ```--output``` The result database

## Examples:

To create the database 2 from the database 1 and the (.h,.cpp) files 3 and 4:

```sider -i FILE1 -o FILE2 FILE3 FILE4```

To create the database3 from the database 1 and 2:

```sider -i FILE1 -i FILE2 -o FILE3```
