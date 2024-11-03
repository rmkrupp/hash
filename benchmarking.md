# Benchmarking

## Test

Sum of 100 runs over ~100k pre-generated 64-byte keys

## Results

### ALICE (Intel(R) Core(TM) Ultra 7 165H)

```
159.384 [debug build]
 26.971 -O2
 27.778 -O2 msse-4.1
 27.106 -O2 -mtune=native
 26.984 -O2 -march=native
 27.462 -O2 -march=native -mtune=native
 29.864 -O3
 31.597 -O3 -msse4.1
 26.044 -O3 -mtune=native
 22.262 -O3 -march=native
 22.528 -O3 -march=native -mtune=native
```

```
-O3 -march=native == -O3 -march=native -mtune=native
(all -O2 options) == -O3 -mtune=native
-O3
```

### DESKTOP (Intel((R) Core(TM) i7-4790K)

```
 99.947 -O2
 97.794 -O2 -mtune=native
 99.013 -O2 -march=native
 98.852 -O2 -march=native -mtune=native
110.407 -O3
 92.289 -O3 -mtune=native
103.721 -O3 -march=native
104.797 -O3 -mtune=native -march=native
```

```
-03 -mtune=native
-O2 -mtune=native
-O2 == -O2 -march=narch == -O2 -march=native -mtune=native
-O3 -march=natve == -O3 -mtune=native -march=native
-O3
```

### WHITE LAPTOP (Intel(R) Core(TM) M-5Y71)

```
202.700 -O2
215.488 -O2 -mtune=native
206.882 -O2 -march=native
216.538 -O2 -march=native -mtune=native
219.817 -O3
208.819 -O3 -mtune=native
205.680 -O3 -march=native
199.754 -O3 -march=native -mtune=native
```

```
-O3 -march=native -mtune=native
-O2
-O2 -march=native == -O3 -march=native == -O3 -mtune=native
-O2 -mtune=native == -02 -march=native -mtune=native
```
