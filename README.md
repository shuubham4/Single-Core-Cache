# Single-Core-Cache
Design of a single core cache which is modifiable as writeback/ write through, write allocate/ write no allocate at runtime. 

To run the program run the following commands at terminal:
1. make
2. make sim
3. sim -n 4 -bs 128 -us 8192 -a 1 radix-4c.trace
