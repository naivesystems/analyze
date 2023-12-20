int f(unsigned int x)
{
  int* a = (int*)malloc(16);
  free(a);
  free(a);
}
