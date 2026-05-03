int main() {
    int *foo = malloc(3 * sizeof(int));
    int *bar = malloc(3 * sizeof(int));
    int *result = malloc(3 * sizeof(int));
    free(result);
    free(result);
    for (int i = 0; i < 3; i++) {
        result[i] = foo[i] + bar[i];
    }
    return result[3];
}