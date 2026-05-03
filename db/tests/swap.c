int zip0 = 15213;
int zip1 = 15217;

void swap (int *a, int *b) {
    int t0 = *a;
    int t1 = *b;

    *b = t0;
    *a = t1;
}

int main () {
    swap(&zip0, &zip1);
}