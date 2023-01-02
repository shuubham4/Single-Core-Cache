# Single-Core-Cache
Design of a single core cache which is modifiable as writeback/ write through, write allocate/ write no allocate at runtime. 

To run the program run the following commands at terminal:
1. make
2. make sim
3. sim -bs 128 -us 8192 -a 1 -wb/-wt -wa/nw spice.trace     OR,
4. sim -bs 128 -is 8192 -ds 8192 -a 1 -wb/-wt -wa/nw spice.trace

-bs -> block size in Bytes
-us -> unified cache size in Bytes
-is -> instruction cache size in Bytes
-ds -> data cache size in Bytes
-a -> assoicativity
-wb/-wt -> writeback/ writethrough
-wa/-nw -> write allocate/ write no allocate
spice.trace -> should be replaced by the relevant trace file.
