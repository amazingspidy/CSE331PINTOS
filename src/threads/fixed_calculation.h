#define f (1 << 14)

int integer_to_fixed(int n);
int fixed_to_integer(int x);
int round_to_integer(int x);
int add_fixed_fixed(int x, int y);
int sub_fixed_fixed(int x, int y);
int add_fixed_integer(int x, int n);
int sub_fixed_integer(int x, int n);
int sub_integer_fixed(int n, int x);
int mul_fixed_fixed(int x, int y);
int mul_fixed_integer(int x, int n);
int div_fixed_fixed(int x, int y);
int div_fixed_integer(int x, int n);


int integer_to_fixed(int n) {
    return n * f;
}

int fixed_to_integer(int x) {
    return x / f;
}

int round_to_integer(int x) {
    if (x >=0) {
        return (x + f/2) / f;
    }
    else {
        return (x - f/2) / f;
    }
}

int add_fixed_fixed(int x, int y) {
    return x + y;
}

int sub_fixed_fixed(int x, int y) {
    return x - y;
}

int add_fixed_integer(int x, int n) {
    return x + n * f;
}

int sub_fixed_integer(int x, int n) {
    return x - n * f;
}

int sub_integer_fixed(int n, int x) {
    return n*f - x;
}

int mul_fixed_fixed(int x, int y) {
    return ((int64_t)x) * y / f;
}

int mul_fixed_integer(int x, int n) {
    return x * n;
}

int div_fixed_fixed(int x, int y) {
    return ((int64_t)x) * f / y;
}

int div_fixed_integer(int x, int n) {
    return x / n;
}