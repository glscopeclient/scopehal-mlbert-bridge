#include <stdio.h>
#include "mlbertapi.h"
#include <stdlib.h>

int main()
{
    auto inst = mlBert_createInstance();
    if (!inst)
    {
        printf("fail\n");
        exit(1);
    }

    printf("ohai\n");
}
